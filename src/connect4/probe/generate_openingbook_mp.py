import sys
import subprocess
import os

# this script is used for generating the openingbook with multi-processing
# python3 generate_openingbook_mp.py folder width height n_workers
# folder    ... is the folder that contains strong solutions (.bin files)
# width     ... width of connect4 board
# height    ... height of connect4 board
# n_workers ... the number of total workers (has to be multiple of 4, will spawn n_workers / 4 processes with 4 threads each)

folder = sys.argv[1]
width = int(sys.argv[2])
height = int(sys.argv[3])
n_workers = int(sys.argv[4])

SUBGROUP_SIZE = 4
DEPTH = 8

assert(n_workers % 4 == 0)

procs = []

# start processes (calling generate_openingbook c program)
for subgroup in range(n_workers // 4):
    proc = subprocess.Popen([
        f"./build/generate_openingbook_w{width}_h{height}.out", str(folder), str(n_workers), str(subgroup)],
        )
    procs.append(proc)

# join processes
for proc in procs:
    proc.wait()

for proc in procs:
    print()

# read results of each subprocess and sort them by position key
# deletes files produced by subprocesses
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

# write results to final openingbook file
file = folder + f"/openingbook_w{width}_h{height}_d{DEPTH}.csv"
print(f"Writing combined solutions sorted to {file}.")
with open(file, "w") as f:
    for key, score in results:
        f.write(f"{key}, {score}\n")
