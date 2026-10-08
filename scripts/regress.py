#!/usr/bin/env python3
"""
Regression and benchmark harness for the BDD solver.

Builds `solve.out` / `count.out` for each encoding configuration, runs a set of small
boards in a scratch directory (results/regress, git-ignored) and compares

  * solve: per-ply win/draw/loss/term/total state counts against tests/golden/*.csv
  * count: total number of positions against the published table (README)
  * optionally (--bytes): sha256 of every solution .bin file against tests/golden/*.sha256

It also reports wall time, GC time, peak number of allocated nodes and peak RSS,
which is what the docs/NN-*.md experiment tables are built from.

Examples:
  python3 scripts/regress.py                    # default small suite
  python3 scripts/regress.py --bytes            # additionally compare .bin hashes
  python3 scripts/regress.py --suite bench      # 6x6 solve / count, slow
  python3 scripts/regress.py --update-golden    # (re)generate golden data
  python3 scripts/regress.py --json out.json    # dump metrics
"""

import argparse
import csv
import hashlib
import json
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "connect4"
GOLDEN = ROOT / "tests" / "golden"
WORK = ROOT / "results" / "regress"

# Published position counts (README table / John Tromp), key = (width, height)
KNOWN_COUNTS = {
    (4, 4): 161029,
    (5, 4): 3945711,
    (4, 5): 1706255,
    (5, 5): 69763700,
    (6, 4): 94910577,
    (6, 5): 2818972642,
    (6, 6): 69173028785,
    (7, 5): 112829665923,
    (7, 6): 4531985219092,
    (8, 6): 290433534225566,
    (7, 7): 161965120344045,
    (9, 6): 18651052587059938,
}

# (kind, width, height, table, compressed_encoding, allow_row_order)
# `table` is passed verbatim as the first program argument (log2 of the node capacity).
SUITES = {
    "small": [
        ("solve", 4, 4, "22", 1, 0),
        ("solve", 5, 4, "22", 1, 0),
        ("solve", 5, 4, "22", 0, 0),
        ("solve", 4, 5, "22", 1, 1),
        ("solve", 4, 5, "22", 0, 1),
        ("solve", 6, 4, "24", 1, 0),
        ("solve", 5, 5, "24", 1, 0),
        ("solve", 5, 5, "150M", 1, 0),  # RAM budget instead of log2, capacity is not a power of two
        ("count", 5, 5, "22", 1, 0),
        ("count", 5, 5, "22", 0, 0),
        ("count", 4, 5, "22", 1, 1),
        ("count", 6, 5, "24", 1, 0),
    ],
    "medium": [
        ("solve", 6, 5, "26", 1, 0),
        ("count", 6, 6, "26", 1, 0),
    ],
    "bench": [
        ("solve", 6, 6, "29", 1, 0),
        ("count", 7, 6, "28", 1, 0),
    ],
    # the 7x6 strong solve: many hours, 28 GiB; golden data is the table published in the README
    # (run with --make SAVE_BDD_TO_DISK=0 unless you have 120 GB of disk to spare)
    "7x6": [
        ("solve", 7, 6, "28G", 1, 0),
    ],
    # hours and many GB
    "ladder": [
        ("count", 8, 6, "12G", 1, 0),
        ("count", 7, 7, "20G", 1, 0),
    ],
}

STATE_COLUMNS = ["ply", "win_count", "draw_count", "loss_count", "term_count", "total_count"]


def case_name(case):
    kind, w, h, _, comp, row = case
    return f"{kind}_w{w}_h{h}_c{comp}_r{row}"


def build(kind, comp, row, extra_make, src=SRC):
    cmd = [
        "make", kind,
        "WRITE_TO_FILE=1", "ENTER_TO_CONTINUE=0",
        f"COMPRESSED_ENCODING={comp}", f"ALLOW_ROW_ORDER={row}",
    ] + extra_make
    res = subprocess.run(cmd, cwd=src, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr)
        sys.exit(f"build failed: {' '.join(cmd)}")
    return src / "build" / f"{kind}.out"


def run_unit_tests():
    """Builds and runs tests/bdd_test.c (with asserts enabled) for both node layouts."""
    wd = WORK / "unit"
    wd.mkdir(parents=True, exist_ok=True)
    bddfiles = sorted(str(p) for p in (ROOT / "src" / "bdd").glob("*.c") if p.name != "uint_test.c")
    # (COMPACT_NODES, INDEX_BITS, extra flags)
    configs = [
        (0, 32, []),
        (1, 32, []),
        # with 40-bit indices, also use the file format for BDDs with 2^32 or more nodes for all larger test BDDs
        (1, 40, ["-DWIDE_FILE_MIN_NODES=1000"]),
    ]
    for compact, bits, extra in configs:
        exe = wd / f"bdd_test_{compact}_{bits}.out"
        cmd = ["clang", str(ROOT / "tests" / "bdd_test.c")] + bddfiles + ["-O2", "-Wall", "-DIN_OP_GC=1", f"-DCOMPACT_NODES={compact}", f"-DINDEX_BITS={bits}"] + extra + ["-o", str(exe)]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(res.stdout)
            print(res.stderr)
            sys.exit("unit test build failed")
        res = subprocess.run([str(exe)], cwd=wd, capture_output=True, text=True)
        lines = [l for l in res.stdout.splitlines() if l.startswith(("ok", "FAILED", "all unit"))]
        print(f"unit tests, COMPACT_NODES={compact} INDEX_BITS={bits}:")
        print("\n".join("  " + l for l in lines))
        if res.returncode != 0:
            print(res.stdout[-2000:])
            print(res.stderr[-2000:])
            sys.exit("unit tests failed")


def run_wide_index_test():
    """Builds and runs tests/wide_index_test.c: 40-bit node indices in a table with 2^33 slots."""
    wd = WORK / "unit"
    wd.mkdir(parents=True, exist_ok=True)
    bddfiles = sorted(str(p) for p in (ROOT / "src" / "bdd").glob("*.c") if p.name != "uint_test.c")
    exe = wd / "wide_index_test.out"
    cmd = ["clang", str(ROOT / "tests" / "wide_index_test.c")] + bddfiles + ["-O2", "-Wall", "-DIN_OP_GC=0", "-DCOMPACT_NODES=1", "-DINDEX_BITS=40", "-o", str(exe)]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr)
        sys.exit("wide index test build failed")
    res = subprocess.run([str(exe)], cwd=wd, capture_output=True, text=True)
    print("\n".join(l for l in res.stdout.splitlines() if l.startswith(("ok", "FAILED", "all wide", "nodes seen"))))
    if res.returncode != 0:
        print(res.stdout[-2000:])
        print(res.stderr[-2000:])
        sys.exit("wide index test failed")


def canonical_sha(path):
    """sha256 of a BDD file after renumbering its nodes in post-order (low before high, node after its children).
    This is the order in which the default node layout writes the file; the compact layout writes the nodes in
    another order. Two files of the same BDD have the same canonical hash."""
    with open(path, "rb") as f:
        raw = f.read()
    n = len(raw) // 9
    var = raw[0::9]
    low = [int.from_bytes(raw[9 * i + 1:9 * i + 5], "little") for i in range(n)]
    high = [int.from_bytes(raw[9 * i + 5:9 * i + 9], "little") for i in range(n)]
    new = [-1] * n
    out = bytearray()
    count = 0
    def emit(i, l, h):
        nonlocal count
        new[i] = count
        count += 1
        out.append(var[i])
        out.extend(l.to_bytes(4, "little"))
        out.extend(h.to_bytes(4, "little"))
    # constants first, as in the file
    emit(0, 0, 0)
    if n > 1:
        emit(1, 1, 1)
    stack = [n - 1]
    while stack:
        i = stack[-1]
        if new[i] >= 0:
            stack.pop()
            continue
        l, h = low[i], high[i]
        if new[l] < 0:
            stack.append(l)
        elif new[h] < 0:
            stack.append(h)
        else:
            emit(i, new[l], new[h])
            stack.pop()
    return hashlib.sha256(bytes(out)).hexdigest()


def time_cmd():
    if platform.system() == "Darwin":
        return ["/usr/bin/time", "-l"]
    return ["/usr/bin/time", "-v"]


def parse_rss(stderr):
    # macOS: "  123456  maximum resident set size" (bytes)
    m = re.search(r"(\d+)\s+maximum resident set size", stderr)
    if m:
        return int(m.group(1))
    # Linux: "Maximum resident set size (kbytes): 1234"
    m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", stderr)
    if m:
        return int(m.group(1)) * 1024
    return None


def parse_stdout(stdout):
    out = {}
    m = re.search(r"\nFinished in ([0-9.]+) seconds\.\nGC time", stdout)
    if m:
        out["time_s"] = float(m.group(1))
    m = re.search(r"GC time: ([0-9.]+) seconds", stdout)
    if m:
        out["gc_time_s"] = float(m.group(1))
    m = re.search(r"GC max number of allocated nodes: (\d+)", stdout)
    if m:
        out["peak_nodes"] = int(m.group(1))
    m = re.search(r"GC max number of nodes alive after gc: (\d+)", stdout)
    if m:
        out["peak_alive"] = int(m.group(1))
    m = re.search(r"GC max fill-level: ([0-9.]+) %", stdout)
    if m:
        out["peak_fill_pct"] = float(m.group(1))
    m = re.search(r"RAM needed ([0-9.]+) GB", stdout)
    if m:
        out["ram_reported_gb"] = float(m.group(1))
    return out


def read_solve_rows(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f, skipinitialspace=True):
            rows.append([int(r[c]) for c in STATE_COLUMNS])
    rows.sort()
    return rows


def read_node_totals(path):
    tot = {"win_nodecount": 0, "draw_nodecount": 0, "loss_nodecount": 0}
    with open(path) as f:
        for r in csv.DictReader(f, skipinitialspace=True):
            for k in tot:
                tot[k] += int(r[k])
    return tot


def sha_folder(folder):
    lines = []
    for p in sorted(folder.glob("*.bin")):
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        lines.append(f"{h.hexdigest()}  {p.name}")
    return lines


def run_case(case, binary, args):
    kind, w, h, table, comp, row = case
    name = case_name(case)
    label = name if table.isdigit() else f"{name}_t{table}"
    wd = WORK / (label + args.tag)
    if wd.exists():
        shutil.rmtree(wd)
    wd.mkdir(parents=True)
    exe = wd / f"{kind}.out"
    shutil.copy2(binary, exe)

    res = subprocess.run(time_cmd() + [str(exe), table, str(w), str(h)], cwd=wd, capture_output=True, text=True)
    (wd / "stdout.txt").write_text(res.stdout)
    (wd / "stderr.txt").write_text(res.stderr)

    metrics = {"case": label, "table": table, "ok": True, "notes": []}
    metrics.update(parse_stdout(res.stdout))
    metrics["peak_rss_bytes"] = parse_rss(res.stderr)
    if res.returncode != 0:
        metrics["ok"] = False
        metrics["notes"].append(f"exit code {res.returncode}")
        return metrics

    if kind == "solve":
        csvs = list(wd.glob("results_solve_*.csv"))
        if len(csvs) != 1:
            metrics["ok"] = False
            metrics["notes"].append("no results csv")
            return metrics
        rows = read_solve_rows(csvs[0])
        metrics.update(read_node_totals(csvs[0]))
        golden_csv = GOLDEN / f"{name}.csv"
        if args.update_golden:
            GOLDEN.mkdir(parents=True, exist_ok=True)
            with open(golden_csv, "w") as f:
                f.write(",".join(STATE_COLUMNS) + "\n")
                for r in rows:
                    f.write(",".join(map(str, r)) + "\n")
        else:
            with open(golden_csv) as f:
                golden = [[int(x) for x in line.split(",")] for line in list(f)[1:]]
            if rows != golden:
                metrics["ok"] = False
                bad = [r[0] for r, g in zip(rows, golden) if r != g]
                metrics["notes"].append(f"state counts differ at plies {bad[:10]}")
        total = sum(r[5] for r in rows)
        if (w, h) in KNOWN_COUNTS and total != KNOWN_COUNTS[(w, h)]:
            metrics["ok"] = False
            metrics["notes"].append(f"total {total} != known {KNOWN_COUNTS[(w, h)]}")

        folder = wd / f"solution_w{w}_h{h}"
        if folder.exists():
            metrics["solution_bytes"] = sum(p.stat().st_size for p in folder.glob("*.bin"))
            if args.update_golden or args.bytes:
                lines = sha_folder(folder)
                golden_sha = GOLDEN / f"{name}.sha256"
                if args.update_golden:
                    golden_sha.write_text("\n".join(lines) + "\n")
                elif golden_sha.read_text().splitlines() != lines:
                    # the files may store the same BDDs with the nodes in a different order (COMPACT_NODES=1)
                    golden_by_name = {l.split("  ")[1]: l.split("  ")[0] for l in golden_sha.read_text().splitlines()}
                    files = sorted(folder.glob("*.bin"))
                    same = len(files) == len(golden_by_name) and all(canonical_sha(p) == golden_by_name.get(p.name) for p in files)
                    if same:
                        metrics["notes"].append("node order in .bin files differs, BDDs identical")
                    else:
                        metrics["ok"] = False
                        metrics["notes"].append(".bin files differ from golden")
            if not args.keep:
                shutil.rmtree(folder)
    else:
        csvs = [p for p in wd.glob("results_w*.csv")]
        if len(csvs) != 1:
            metrics["ok"] = False
            metrics["notes"].append("no results csv")
            return metrics
        with open(csvs[0]) as f:
            r = next(csv.DictReader(f, skipinitialspace=True))
        total = int(r["count"])
        metrics["count"] = total
        metrics["time_s"] = float(r["time"])
        if total != KNOWN_COUNTS[(w, h)]:
            metrics["ok"] = False
            metrics["notes"].append(f"count {total} != known {KNOWN_COUNTS[(w, h)]}")
    return metrics


def fmt(v, kind):
    if v is None:
        return "-"
    if kind == "s":
        return f"{v:.2f}"
    if kind == "M":
        return f"{v / 1e6:.2f}"
    if kind == "GB":
        return f"{v / 2**30:.2f}"
    return str(v)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--suite", default="small", help="comma separated: " + ", ".join(SUITES))
    ap.add_argument("--only", default=None, help="regex on case name")
    ap.add_argument("--bytes", action="store_true", help="compare sha256 of solution .bin files with golden")
    ap.add_argument("--update-golden", action="store_true")
    ap.add_argument("--keep", action="store_true", help="keep solution folders in the scratch dir")
    ap.add_argument("--json", default=None, help="write metrics to this file")
    ap.add_argument("--check", nargs=2, metavar=("RESULTS_CSV", "GOLDEN_CSV"), default=None,
                    help="only compare the state counts of a results_solve_*.csv (also of a run that is still in progress) with a golden file")
    ap.add_argument("--wide", action="store_true", help="only run the test of node indices above 2^32 (tests/wide_index_test.c, needs a few GB of RAM)")
    ap.add_argument("--unit", action="store_true", help="only run the library unit tests (tests/bdd_test.c)")
    ap.add_argument("--tag", default="", help="suffix for the scratch directories (to run several suites concurrently)")
    ap.add_argument("--src", default=None, help="build from this src/connect4 directory (e.g. a git worktree of another commit)")
    ap.add_argument("--table", default=None, help="override the table size argument of every case")
    ap.add_argument("--make", action="append", default=[], help="extra make variable, e.g. --make IN_OP_GC=1")
    args = ap.parse_args()

    if args.check:
        rows = read_solve_rows(args.check[0])
        with open(args.check[1]) as f:
            golden = {int(line.split(",")[0]): [int(x) for x in line.split(",")] for line in list(f)[1:]}
        bad = [r[0] for r in rows if golden.get(r[0]) != r]
        print(f"{len(rows)} of {len(golden)} plies present, {len(rows) - len(bad)} identical to golden" + (f", DIFFERENT at plies {bad}" if bad else ""))
        sys.exit(1 if bad else 0)

    if args.wide:
        run_wide_index_test()
        return

    if not args.src:
        run_unit_tests()
    if args.unit:
        return

    cases = []
    for s in args.suite.split(","):
        cases += SUITES[s]
    if args.only:
        cases = [c for c in cases if re.search(args.only, case_name(c))]
    if args.table:
        cases = [(k, w, h, args.table, c, r) for (k, w, h, _, c, r) in cases]

    # group by build configuration so that each binary is built once
    results = []
    built = {}
    for case in cases:
        kind, _, _, _, comp, row = case
        key = (kind, comp, row)
        if key not in built:
            binary = build(kind, comp, row, args.make, Path(args.src).resolve() if args.src else SRC)
            cached = WORK / "bin" / f"{kind}_c{comp}_r{row}{args.tag}.out"
            cached.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(binary, cached)
            built[key] = cached
        m = run_case(case, built[key], args)
        results.append(m)
        status = "ok  " if m["ok"] else "FAIL"
        print(f"{status} {m['case']:<24} time {fmt(m.get('time_s'), 's'):>9} s  gc {fmt(m.get('gc_time_s'), 's'):>8} s  "
              f"peak nodes {fmt(m.get('peak_nodes'), 'M'):>9} M  alive {fmt(m.get('peak_alive'), 'M'):>9} M  rss {fmt(m.get('peak_rss_bytes'), 'GB'):>6} GB  "
              f"{'; '.join(m['notes'])}", flush=True)

    if args.json:
        Path(args.json).write_text(json.dumps(results, indent=1))

    failed = [m["case"] for m in results if not m["ok"]]
    if failed:
        sys.exit(f"\n{len(failed)} case(s) failed: {', '.join(failed)}")
    print(f"\nall {len(results)} cases passed")


if __name__ == "__main__":
    main()
