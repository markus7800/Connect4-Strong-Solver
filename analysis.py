#%%
import os
import pandas as pd
# %%
folder = "lambda_results"
frames = []
for file in os.listdir(folder):
    if file.startswith("results_w"):
        print(file)
        frames.append(pd.read_csv(folder + "/" + file))
# %%
results = pd.concat(frames)
results = results.sort_values(["width", "height"])
results = results.reset_index()
results
# %%
p = results.pivot(index="height", columns="width", values="count")
p = p.convert_dtypes()
p = p.applymap(lambda x: f"{int(x):,}" if not pd.isna(x) else "N/A")
p.index.name = "height/width"
p
# %%
print(p.to_markdown(tablefmt="github", index=True, stralign="right"))

# %%
import datetime

results["#positions"] = results["count"].apply(lambda x: f"{int(x):,}" if not pd.isna(x) else "N/A")
results["comp.time"] = results["time"].apply(lambda x: str(datetime.timedelta(seconds=x))[:-4])
results["GC perc."] = results["GC"].apply(lambda x: "{:.2f}%".format(x*100))
results["RAM used"] = results["bytes"].apply(lambda x: "{:.2f}GB".format(x/1e9))
results["#allocatable"] = results["log2tbsize"].apply(lambda x: "2^{:d}".format(x))
results["perc. allocated"] = results["usage"].apply(lambda x: "{:.2f}%".format(x*100))

# %%
pretty_results = results[["width","height", "#positions", "comp.time", "GC perc.", "RAM used", "#allocatable", "perc. allocated"]]
print(pretty_results.to_markdown(tablefmt="github", index=False, stralign="right"))
# %%
