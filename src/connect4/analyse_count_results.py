
import pandas as pd
import argparse
from pathlib import Path
import datetime

def pivot_table(results, values, formatter=lambda x: x):
    p = results.pivot(index="height", columns="width", values=values)
    p = p.convert_dtypes()
    p = p.map(lambda x: formatter(x) if not pd.isna(x) else "N/A")
    return p

formatters = {
    "count": ("#positions", lambda x: f"{int(x):,}"),
    "time": ("comp.time", lambda x: str(datetime.timedelta(seconds=x))[:-4]),
    "GC_perc": ("GC perc.", lambda x: "{:.2f}%".format(x*100)),
    "bytes_alloc": ("RAM used", lambda x: "{:.2f}GB".format(x/1e9)),
    "log2_tbsize": ("#allocatable", lambda x: "2^{:d}".format(x)),
    "max_nodes_alloc": ("#allocated", lambda x: f"{int(x):,}"),
    "max_fill_level": ("perc. allocated", lambda x: "{:.2f}%".format(x*100)),
}
def list_table(results):
    results = results.copy()
    cols = []
    for col in ["count", "time", "GC_perc", "bytes_alloc", "log2_tbsize", "max_nodes_alloc", "max_fill_level"]:
        name, formatter = formatters[col]
        cols.append(name)
        results[name] = results[col].apply(formatter)

    return results[["width","height", *cols]]

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("folder", help="folder to analyse")
    parser.add_argument("-pivot", help="dimension of pivot table (count | time | GC_perc | bytes_alloc | log2_tbsize | max_nodes_alloc | max_fill_level)", default="count")
    parser.add_argument("--list", action="store_true", help="print list of all results")
    args = parser.parse_args()

    folder = Path(args.folder)
    results = pd.read_csv(folder.joinpath("all_results.csv"))
    results = results.sort_values(["width", "height"])
    results = results.reset_index().drop("index", axis=1)

    name, formatter = formatters[args.pivot]
    print("Pivot table for", name)
    p = pivot_table(results, args.pivot, formatter)

    p.index.name = "height\\width"
    print(p.to_markdown(index=True, stralign="right"))

    if args.list:
        pretty_results = list_table(results)
        print(pretty_results.to_markdown(index=False, stralign="right"))

