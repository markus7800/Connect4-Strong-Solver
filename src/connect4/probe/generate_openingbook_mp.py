import sys
import subprocess
import os

folder = sys.argv[1]
width = int(sys.argv[2])
height = int(sys.argv[3])
n_workers = int(sys.argv[4])

SUBGROUP_SIZE = 4
DEPTH = 8

assert(n_workers % 4 == 0)

procs = []
for subgroup in range(n_workers // 4):
    proc = subprocess.Popen([
        f"./generate_ob_w{width}_h{height}.out", str(folder), str(n_workers), str(subgroup)],
        )
    procs.append(proc)
    # proc.wait()

for proc in procs:
    proc.wait()

for proc in procs:
    print()

results = []
for subgroup in range(n_workers // 4):
    file = folder + f"/openingbook_w{width}_h{height}_d{DEPTH}_g{subgroup}.csv"
    with open(file, "r") as f:
        for line in f:
            key, score = line.split(",")
            key = int(key)
            score = int(score)
            results.append((key, score))
    os.remove(file)

results = sorted(results)

with open(folder + f"/openingbook_w{width}_h{height}_d{DEPTH}.csv", "w") as f:
    for key, score in results:
        f.write(f"{key}, {score}\n")
