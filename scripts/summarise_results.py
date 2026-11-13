#!/usr/bin/env python3
"""Print a readable summary of aggregate results CSVs in a folder."""

import argparse
import csv
from pathlib import Path


FIELDS = (
    "width",
    "height",
    "count",
    "time",
    "log2_tbsize",
    "max_fill_level",
    "max_nodes_alloc",
)


def format_duration(seconds):
    seconds = round(seconds, 1)
    whole_seconds = int(seconds)
    fractional = seconds - whole_seconds
    days, remainder = divmod(whole_seconds, 24 * 60 * 60)
    hours, remainder = divmod(remainder, 60 * 60)
    minutes, seconds_part = divmod(remainder, 60)
    seconds_text = f"{seconds_part + fractional:.1f}s"

    if days:
        return f"{days}d {hours}h {minutes}m {seconds_text}"
    if hours:
        return f"{hours}h {minutes}m {seconds_text}"
    if minutes:
        return f"{minutes}m {seconds_text}"
    return seconds_text


def read_results(folder):
    files = sorted(folder.glob("results_w*_h*_ls*.csv"))
    if not files:
        raise ValueError(f"No aggregate result CSVs found in {folder}")

    results = []
    for path in files:
        with path.open(newline="") as result_file:
            reader = csv.DictReader(result_file, skipinitialspace=True)
            if reader.fieldnames is None:
                raise ValueError(f"{path}: missing CSV header")
            reader.fieldnames = [name.strip() for name in reader.fieldnames]
            missing = set(FIELDS) - set(reader.fieldnames)
            if missing:
                raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")

            for line_number, row in enumerate(reader, start=2):
                try:
                    results.append(
                        {
                            "width": int(row["width"].strip()),
                            "height": int(row["height"].strip()),
                            "count": int(row["count"].strip()),
                            "time": float(row["time"].strip()),
                            "log2_tbsize": int(row["log2_tbsize"].strip()),
                            "max_fill_level": float(row["max_fill_level"].strip()),
                            "max_nodes_alloc": int(row["max_nodes_alloc"].strip()),
                        }
                    )
                except (AttributeError, ValueError) as error:
                    raise ValueError(f"{path}:{line_number}: invalid result value: {error}") from error

    return sorted(
        results,
        key=lambda result: (
            result["width"],
            result["height"],
            result["log2_tbsize"],
        ),
    )


def print_table(results):
    rows = [
        [
            f'{result["width"]}x{result["height"]}',
            f'2^{result["log2_tbsize"]}',
            f'{result["count"]:,}',
            format_duration(result["time"]),
            f'{result["max_fill_level"]:.2%}',
            f'{result["max_nodes_alloc"]:,}',
        ]
        for result in results
    ]
    headers = ["Board", "Table", "Positions", "Time", "Max fill", "Max nodes"]
    widths = [
        max(len(header), *(len(row[index]) for row in rows))
        for index, header in enumerate(headers)
    ]

    print(f"{len(results)} result(s) across {len({row[0] for row in rows})} board size(s)\n")
    print("  ".join(header.ljust(widths[index]) for index, header in enumerate(headers)))
    print("  ".join("-" * width for width in widths))
    for row in rows:
        print(
            "  ".join(
                value.rjust(widths[index]) if index >= 2 else value.ljust(widths[index])
                for index, value in enumerate(row)
            )
        )


def main():
    default_folder = Path(__file__).resolve().parent.parent / "results_ls35"
    parser = argparse.ArgumentParser(
        description="Summarise aggregate result CSVs (per-ply CSVs are excluded)."
    )
    parser.add_argument(
        "folder",
        nargs="?",
        type=Path,
        default=default_folder,
        help=f"folder containing result CSVs (default: {default_folder})",
    )
    args = parser.parse_args()

    try:
        results = read_results(args.folder)
    except (OSError, ValueError) as error:
        parser.exit(2, f"{parser.prog}: error: {error}\n")

    print_table(results)


if __name__ == "__main__":
    main()
