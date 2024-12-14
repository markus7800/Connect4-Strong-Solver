import subprocess
from pathlib import Path
import os
from multiprocessing.dummy import Pool # use threads
from tqdm import tqdm
import pandas as pd

def count(args):
    width, height, log2_tablesize = args
    
    res_file = f"results_w{width}_h{height}.csv"
    ply_file = f"results_ply_w{width}_h{height}.csv"

    if Path("data/"+res_file).exists():
        tqdm.write(f"Count for width={width} x height={height} already done.")
        return
    else:
        tqdm.write(f"Count for width={width} x height={height} ... ")

    with open(f"logs/log_w{width}_h{height}.txt", "w") as f:
        
        res = subprocess.run(["../../build/count.out", str(log2_tablesize), str(width), str(height)], check=False, stdout=f, stderr=f)
        
        if res.returncode != 0:
            tqdm.write(f"Warning: error during count of width={width} x height={height}!")
            if Path(ply_file).is_file():
                os.remove(ply_file)
        else:
            os.rename(ply_file, "data/"+ply_file)
            os.rename(res_file, "data/"+res_file)

def is_power_of_two(n: int):
    return bool(n & (n-1) == 0)

def count_all(compressed_encoding, allow_row_order, RAM):
    assert RAM >= 8 and is_power_of_two(RAM)
    # working directory is src/connect4
    subprocess.run([
        "make", "count",
        "WRITE_TO_FILE=1",
        "ENTER_TO_CONTINUE=0",
        "IN_OP_GC=0",
        "FULLBDD=0",
        "SUBTRACT_TERM=1",
        f"COMPRESSED_ENCODING={int(compressed_encoding)}",
        f"ALLOW_ROW_ORDER={int(allow_row_order)}",
    ])

    folder = Path(f"count_results/compenc_{int(compressed_encoding)}_allowrow_{int(allow_row_order)}")
    folder.mkdir(parents=True, exist_ok=True)
    os.chdir(folder)
    Path("logs").mkdir(exist_ok=True)
    Path("data").mkdir(exist_ok=True)

    payloads = []
    for width in range(1,14):
        for height in range(1,14):
            if height + width <= 12:
                payloads.append((width, height, 26))
            # elif height <= 4 or width <= 4:
            #     payloads.append((width, height, 27))

            # if height + width == 13:
            #     payloads.append((width, height, 29))
            # if height + width == 14:
            #     if (width,height) in [(6,8),(7,7)]:
            #         # (5,9) is on the limit for compressed encoding with 30
            #         payloads.append((width, height, 31))
            #     else:
            #         payloads.append((width, height, 30))

    
    for log2_tablesize in range(24,32+1):
        sub_payloads = [payload for payload in payloads if payload[2] == log2_tablesize]
        if len(sub_payloads) == 0:
            continue
        # 27 ... 8
        # 28 ... 16
        # 29 ... 32
        # 30 ... 64
        # 31 ... 128
        RAM_per_worker = 2 ** (log2_tablesize - 24)
        if RAM_per_worker > RAM:
            print(f"Not enough RAM for {log2_tablesize} :/")
            return
        
        assert RAM % RAM_per_worker == 0
        n_workers = RAM // RAM_per_worker
        with Pool(n_workers) as p:
            list(tqdm(p.imap(count, sub_payloads), total=len(sub_payloads), desc=f"workers={n_workers} log2_tablesize={log2_tablesize}"))



    frames = []
    for file in os.listdir("data"):
        if file.startswith("results_w"):
            frames.append(pd.read_csv("data/" + file))

    results = pd.concat(frames)
    results = results.sort_values(["width", "height"])
    results = results.reset_index().drop("index", axis=1)
    results.to_csv("all_results.csv", index=False)

    os.chdir("../..")
    os.remove("build/count.out")



if __name__ == "__main__":
    RAM = 16
    count_all(compressed_encoding=False, allow_row_order=True, RAM=RAM)
    count_all(compressed_encoding=True, allow_row_order=False, RAM=RAM)

    count_all(compressed_encoding=False, allow_row_order=False, RAM=RAM)
    count_all(compressed_encoding=True, allow_row_order=True, RAM=RAM)