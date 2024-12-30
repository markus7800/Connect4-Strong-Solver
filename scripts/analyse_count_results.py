
import pandas as pd
import argparse
from pathlib import Path
import datetime
import math

def read_results(folder):
    folder = Path(folder)
    results = pd.read_csv(folder.joinpath("all_results.csv"))
    results = results.sort_values(["width", "height","log2_tbsize"])
    results = results.drop_duplicates(subset=["width","height"], keep="first") # use entry with lowest log2_tbsize
    results = results.reset_index().drop("index", axis=1)
    results["min_log2_tbsize"] = results["max_nodes_alloc"].apply(lambda x: int(math.ceil(math.log2(x))))
    return results

def pivot_table(results, values):
    p = results.pivot(index="height", columns="width", values=values)
    p = p.convert_dtypes()
    return p

def format_pivot_table(p, formatter=lambda x: x):
    p = p.map(lambda x: formatter(x) if not pd.isna(x) else "N/A")
    return p

formatters = {
    "count": ("#positions", lambda x: f"{int(x):,}"),
    "time": ("comp.time", lambda x: str(datetime.timedelta(seconds=x))[:-4]),
    "GC_perc": ("GC perc.", lambda x: "{:.2f}%".format(x*100)),
    "bytes_alloc": ("RAM used", lambda x: "{:.2f}GB".format(x/1e9)),
    "log2_tbsize": ("#allocatable", lambda x: "2^{:d}".format(int(x))),
    "max_nodes_alloc": ("#allocated", lambda x: f"{int(x):,}"),
    "max_fill_level": ("perc. allocated", lambda x: "{:.2f}%".format(x*100)),
    "min_log2_tbsize": ("min_log2_tbsize", lambda x: f"{int(x):,}"),
}

def list_table(results):
    results = results.copy()
    cols = []
    for col in ["count", "time", "GC_perc", "bytes_alloc", "log2_tbsize", "max_nodes_alloc", "max_fill_level", "min_log2_tbsize"]:
        name, formatter = formatters[col]
        cols.append(name)
        results[name] = results[col].apply(formatter)

    return results[["width","height", *cols]]

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("folder", help="folder to analyse")
    parser.add_argument("-pivot", help="dimension of pivot table (count | time | GC_perc | bytes_alloc | log2_tbsize | max_nodes_alloc | max_fill_level)", default="count")
    parser.add_argument("--list", action="store_true", help="print list of all results")
    parser.add_argument("-comp", help="folder of results to compare to")

    args = parser.parse_args()

    results = read_results(args.folder)

    name, formatter = formatters[args.pivot]
    if args.comp is not None:
        print("Compare by dimension", name)
        print()
        
        green = '\033[92m'
        red = '\033[91m'
        endc = '\033[0m'

        comp_results = read_results(args.comp)
        p = pivot_table(results, args.pivot)
        p2 = pivot_table(comp_results, args.pivot)
        p2 = p2.reindex(p.index)

        print(args.comp)
        p_formatted = format_pivot_table(p2, formatter)
        p_formatted.index.name = "height\\width"
        print(p_formatted.to_markdown(index=True, stralign="right"))
        print()

        # less is better for every dimension
        p_formatted = format_pivot_table(p, formatter)
        better_mask = (p2.isna() & ~p.isna()) | (p < p2)
        p_formatted[better_mask] = p_formatted[better_mask].map(lambda x: green + str(x) + endc)
        worse_mask = (p.isna() & ~p2.isna()) | (p > p2)
        p_formatted[worse_mask] = p_formatted[worse_mask].map(lambda x: red + str(x) + endc)

        print(args.folder)
        p_formatted.index.name = "height\\width"
        print(p_formatted.to_markdown(index=True, stralign="right"))

    else:
        print("Pivot table for", name)
        p = pivot_table(results, args.pivot)

        p = format_pivot_table(p, formatter)
        p.index.name = "height\\width"
        print(p.to_markdown(index=True, stralign="right"))

    if args.list:
        pretty_results = list_table(results)
        print(pretty_results.to_markdown(index=False, stralign="right"))



