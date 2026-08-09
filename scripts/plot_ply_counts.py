#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "matplotlib>=3.9.4",
#     "numpy>=2.0.2",
#     "pandas>=2.3.3",
# ]
# ///

import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd


DEFAULT_PATTERN = "results_ply*.csv"


def load_plies(pattern: str, base_dir: Path) -> pd.DataFrame:
    files = sorted(base_dir.glob(pattern))
    if not files:
        raise FileNotFoundError(f"No files matching {pattern!r} found in {base_dir}")

    frames = []
    for path in files:
        df = pd.read_csv(path)
        required = {"width", "height", "ply", "poscount"}
        missing = required - set(df.columns)
        if missing:
            raise ValueError(f"CSV {path.name} is missing required columns: {sorted(missing)}")

        chunk = df[["width", "height", "ply", "poscount"]].copy()
        chunk["source_file"] = path.name
        chunk["height"] = chunk["height"].astype(int)
        chunk["ply"] = chunk["ply"].astype(int)
        chunk["poscount"] = pd.to_numeric(chunk["poscount"], errors="raise")
        frames.append(chunk)

    if not frames:
        raise ValueError(f"No usable rows found in {pattern!r}")

    return pd.concat(frames, ignore_index=True)


def plot_counts(data: pd.DataFrame, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    out_path = output_dir / "results_ply_all_heights.png"

    fig, ax = plt.subplots(figsize=(12, 7))
    for height, group in data.groupby("height", sort=True):
        ax.plot(
            group["ply"],
            group["poscount"],
            marker="o",
            linewidth=2,
            markersize=3,
            alpha=0.9,
            label=f"h={int(height)}",
        )

    ax.set_title("Intermediate ply counts for all width=9 result files")
    ax.set_xlabel("Ply")
    ax.set_ylabel("Position count")
    ax.set_yscale("log")
    ax.grid(True, which="both", linestyle="--", linewidth=0.6, alpha=0.5)
    ax.legend(title="Board height", loc="best", ncol=3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=200)
    plt.close(fig)

    return out_path


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot all intermediate ply counts from results_ply*.csv files on one graph.")
    parser.add_argument("--pattern", default=DEFAULT_PATTERN, help="Glob pattern for CSV files to read (default: %(default)s)")
    parser.add_argument("--base-dir", type=Path, default=Path("."), help="Directory containing the CSV files (default: current directory)")
    parser.add_argument("--output-dir", type=Path, default=Path("plots"), help="Directory to write the plot into (default: %(default)s)")
    args = parser.parse_args()

    data = load_plies(args.pattern, args.base_dir)
    out_path = plot_counts(data, args.output_dir)

    print(f"Loaded {len(data)} rows from {data['source_file'].nunique()} files")
    print(f"Heights: {sorted(data['height'].unique().tolist())}")
    print(f"Plot saved to: {out_path.resolve()}")


if __name__ == "__main__":
    main()
