import subprocess
from pathlib import Path
import os
import pandas as pd
import matplotlib.pyplot as plt
import argparse

def get_prefix(compressed_encoding, subtract_term):
    return f"compenc_{int(compressed_encoding)}_subterm_{int(subtract_term)}"

def get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE):
    return f"w{WIDTH}_h{HEIGHT}_ls{LOG_TB_SIZE}"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", nargs='?', type=int, default=7)
    parser.add_argument("HEIGHT", nargs='?', type=int, default=6)
    parser.add_argument("LOG_TB_SIZE", nargs='?', type=int, default=29)
    parser.add_argument("--skip-search", action="store_true")
    parser.add_argument("--exists-ok", action="store_true")
    args = parser.parse_args()


    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    LOG_TB_SIZE = args.LOG_TB_SIZE
    print(f"{WIDTH=} {HEIGHT=} {LOG_TB_SIZE=}")

    folder = Path(f"results/count_w{WIDTH}_h{HEIGHT}_results")
    if not args.skip_search:
        folder.mkdir(exist_ok=args.exists_ok)

        for compressed_encoding, subtract_term in [(True, False), (False, False), (True, True), (False, True)]:
            subprocess.run([
                "make", "count",
                "WRITE_TO_FILE=1",
                "ENTER_TO_CONTINUE=0",
                "IN_OP_GC=0",
                "FULLBDD=1", # FULL BDD
                f"SUBTRACT_TERM={int(subtract_term)}",
                f"COMPRESSED_ENCODING={int(compressed_encoding)}",
                f"ALLOW_ROW_ORDER=0",
            ], cwd="src/connect4")

            prefix = get_prefix(compressed_encoding, subtract_term)
            with open(folder.joinpath(f"{prefix}_log.txt"), "w") as f:
                res = subprocess.run(["../../src/connect4/build/count.out", str(LOG_TB_SIZE), str(WIDTH), str(HEIGHT)], check=False, stdout=f, stderr=f, cwd=folder)

            suffix = get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE)
            os.rename(folder.joinpath(f"results_ply_{suffix}.csv"), folder.joinpath(f"{prefix}_results_ply_{suffix}.csv"))
            os.rename(folder.joinpath(f"results_{suffix}.csv"), folder.joinpath(f"{prefix}_results_{suffix}.csv"))


            os.remove("src/connect4/build/count.out")

    def get_node_counts(compressed_encoding, subtract_term):
        prefix = get_prefix(compressed_encoding, subtract_term)
        res = pd.read_csv(folder.joinpath(f"{prefix}_results_ply_{suffix}.csv"))
        res = res[:-1].convert_dtypes()
        return res["nodecount"]

    def get_states_counts(compressed_encoding, subtract_term):
        prefix = get_prefix(compressed_encoding, subtract_term)
        res = pd.read_csv(folder.joinpath(f"{prefix}_results_ply_{suffix}.csv"))
        res = res[:-1].convert_dtypes()
        return res["poscount"]
    suffix = get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE)
    fig, ax = plt.subplots(1,1, figsize=(6,4))
    ax.set_yscale("log")
    ax.plot(get_states_counts(False, False), c="tab:blue", marker="+", label="Ignored Terminals (States)")
    ax.plot(get_states_counts(False, True), c="tab:orange", marker="s", markerfacecolor='none', label="Stopped at Terminals (States)")
    ax.plot(get_node_counts(False, False), c="tab:blue", marker="o", markerfacecolor='none', label="Ignored Terminals (Nodes)")
    ax.plot(get_node_counts(False, True), c="tab:orange", marker="x", label="Stopped at Terminals (Nodes)")
    ax.set_ylabel("Number of States or BDD Nodes")
    ax.grid()
    ax.legend()
    plt.tight_layout()
    plt.savefig(folder.joinpath("bdd_node_count_term_vs_nonterm.pdf"))

    fig, ax = plt.subplots(1,1, figsize=(6,4))
    #ax.set_yscale("log")
    ax.plot(get_node_counts(True, True), c="tab:blue", marker="s", markerfacecolor='none', label="Stopped at Terminals (Compressed)")
    ax.plot(get_node_counts(False, True), c="tab:orange", marker="x", label="Stopped at Terminals (Standard)")
    ax.set_ylabel("Number of BDD Nodes")
    ax.set_xlabel("Ply")
    ax.grid()
    ax.legend()
    plt.tight_layout()
    plt.savefig(folder.joinpath("bdd_node_count_compenc_vs_standard.pdf"))

