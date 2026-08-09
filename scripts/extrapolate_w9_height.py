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
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


DEFAULT_PATTERN = "results_w9*.csv"


def load_results(pattern: str, base_dir: Path) -> pd.DataFrame:
    files = sorted(base_dir.glob(pattern))
    if not files:
        raise FileNotFoundError(f"No files matching {pattern!r} found in {base_dir}")

    rows = []
    for path in files:
        df = pd.read_csv(path)
        if df.empty:
            continue
        row = df.iloc[0]
        if "height" not in row or "count" not in row:
            raise ValueError(f"CSV {path.name} is missing 'height' or 'count' columns")
        rows.append({
            "file": path.name,
            "height": int(row["height"]),
            "count": int(row["count"]),
        })

    if not rows:
        raise ValueError(f"No usable rows found in {pattern!r}")

    data = pd.DataFrame(rows).sort_values("height").reset_index(drop=True)
    return data


def fit_exponential(data: pd.DataFrame) -> dict:
    x = data["height"].to_numpy(dtype=float)
    y = data["count"].to_numpy(dtype=float)

    if np.any(y <= 0):
        raise ValueError("Counts must all be positive for an exponential fit")

    slope, intercept = np.polyfit(x, np.log(y), 1)
    a = math.exp(intercept)
    b = slope
    fitted = np.exp(intercept + slope * x)
    pred_h7 = math.exp(intercept + slope * 7.0)
    pred_h8 = math.exp(intercept + slope * 8.0)

    return {
        "x": x,
        "y": y,
        "fit": fitted,
        "slope": b,
        "intercept": intercept,
        "a": a,
        "pred_h7": pred_h7,
        "pred_h8": pred_h8,
    }


def plot_fit(data: pd.DataFrame, fit: dict, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    x = fit["x"]
    y = fit["y"]
    h_fit = np.linspace(x.min() - 0.5, 8.5, 400)
    fit_line = np.exp(fit["intercept"] + fit["slope"] * h_fit)
    pred_h7 = fit["pred_h7"]
    pred_h8 = fit["pred_h8"]

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(x, y, "o", label="Measured counts", markersize=7, color="C0")
    ax.plot(h_fit, fit_line, "-", color="C3", linewidth=2, label=f"Exp fit: y = {fit['a']:.3e} e^({fit['slope']:.4f} h)")
    ax.axvline(8.0, color="0.5", linestyle="--", linewidth=1.2, alpha=0.8)
    ax.scatter([7.0], [pred_h7], color="C1", s=60, zorder=5, label=f"Extrapolated h=7: {pred_h7:,.0f}")
    ax.scatter([8.0], [pred_h8], color="C2", s=60, zorder=5, label=f"Extrapolated h=8: {pred_h8:,.0f}")
    ax.set_title("Connect4 width=9 unique-position counts vs. height")
    ax.set_xlabel("Height")
    ax.set_ylabel("Number of unique positions")
    ax.set_yscale("log")
    ax.grid(True, which="both", linestyle="--", linewidth=0.6, alpha=0.5)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_dir / "w9_exponential_fit.png", dpi=200)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Fit an exponential model to width-9 unique-position counts and extrapolate to height 8.")
    parser.add_argument("--pattern", default=DEFAULT_PATTERN, help="Glob pattern for CSV result files to read (default: %(default)s)")
    parser.add_argument("--base-dir", type=Path, default=Path("."), help="Directory containing the CSV files (default: current directory)")
    parser.add_argument("--output-dir", type=Path, default=Path("plots"), help="Directory to write PNG plots into (default: %(default)s)")
    args = parser.parse_args()

    data = load_results(args.pattern, args.base_dir)
    fit = fit_exponential(data)
    plot_fit(data, fit, args.output_dir)

    print(f"Heights observed: {data['height'].tolist()}")
    print(f"Counts observed: {data['count'].tolist()}")
    print(f"Fit: count ≈ {fit['a']:.6e} * exp({fit['slope']:.6f} * height)")
    print(f"Extrapolated count at height 8: {fit['pred_h8']:.6e}")
    print(f"Plots written to: {args.output_dir.resolve()}")


if __name__ == "__main__":
    main()
