#%%
import os
import pandas as pd
# %%
folder = "full"
results = pd.read_csv(folder + "/" + "all_results.csv")
results = results.sort_values(["width", "height"])
results = results.reset_index().drop("index", axis=1)
results
# %%
p = results.pivot(index="height", columns="width", values="count")
p = p.convert_dtypes()
p = p.map(lambda x: f"{int(x):,}" if not pd.isna(x) else "N/A")
p.index.name = "height/width"
p

# %%
print(p.to_markdown(index=True, stralign="right"))


# %%
import datetime

results["#positions"] = results["count"].apply(lambda x: f"{int(x):,}" if not pd.isna(x) else "N/A")
results["comp.time"] = results["time"].apply(lambda x: str(datetime.timedelta(seconds=x))[:-4])
results["GC perc."] = results["GC_perc"].apply(lambda x: "{:.2f}%".format(x*100))
results["RAM used"] = results["bytes_alloc"].apply(lambda x: "{:.2f}GB".format(x/1e9))
results["#allocatable"] = results["log2_tbsize"].apply(lambda x: "2^{:d}".format(x))
results["#allocated"] = results["max_nodes_alloc"].apply(lambda x: f"{int(x):,}")
results["perc. allocated"] = results["max_fill_level"].apply(lambda x: "{:.2f}%".format(x*100))

# %%
pretty_results = results[["width","height", "#positions", "comp.time", "GC perc.", "RAM used", "#allocatable", "#allocated", "perc. allocated"]]
print(pretty_results.to_markdown(index=False, stralign="right"))
# %%
