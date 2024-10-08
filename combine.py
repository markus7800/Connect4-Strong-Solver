#%%
import os
import pandas as pd

folder = "/Users/markus/Downloads/full/"

frames = []
for file in os.listdir(folder):
    if file.startswith("results_w"):
        frames.append(pd.read_csv(folder + file))
        # os.remove(file)

results = pd.concat(frames)
results = results.sort_values(["width", "height"])
results = results.reset_index().drop("index", axis=1)
results.to_csv(folder + "all_results.csv", index=False)

#%%
# _results = pd.read_csv(folder + "_all_results.csv")
# _results = _results[_results['width'] + _results['height'] > 12]

# results = pd.concat([results, _results])
# results = results.sort_values(["width", "height"])
# results = results.reset_index().drop("index", axis=1)

#%%
frames = []
for file in os.listdir(folder):
    if file.startswith("results_ply_w"):
        frames.append(pd.read_csv(folder + file))
        # os.rename(file, folder + "ply/" + file)
    
results = pd.concat(frames)
results = results.sort_values(["width", "height"])
results = results.reset_index().drop("index", axis=1)
results.to_csv(folder + "all_results_ply.csv", index=False)
# %%

# _results = pd.read_csv(folder + "_all_results_ply.csv")
# _results = _results[_results['width'] + _results['height'] > 12]

# results = pd.concat([results, _results])
# results = results.sort_values(["width", "height"])
# results = results.reset_index().drop("index", axis=1)
# %%
