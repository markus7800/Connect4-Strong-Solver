import subprocess
from pathlib import Path
import os
from multiprocessing.dummy import Pool # use threads
from tqdm import tqdm
import pandas as pd
import shutil

def executable_name(compressed_encoding, allow_row_order):
    return f"build/count_{int(compressed_encoding)}_{int(allow_row_order)}.out"

def folder_name(compressed_encoding, allow_row_order):
    return f"count_results/compenc_{int(compressed_encoding)}_allowrow_{int(allow_row_order)}/"

class Setup:
    def __init__(self, compressed_encoding, allow_row_order, boardsizes):
        self.compressed_encoding = compressed_encoding
        self.allow_row_order = allow_row_order
        self.boardsizes = boardsizes

def count(args):
    compressed_encoding, allow_row_order, width, height, log2_tablesize = args

    folder = Path(folder_name(compressed_encoding, allow_row_order))

    log_file_name = f"logs/log_w{width}_h{height}_ls{log2_tablesize}.txt"
    res_file_name = f"results_w{width}_h{height}_ls{log2_tablesize}.csv"
    ply_file_name = f"results_ply_w{width}_h{height}_ls{log2_tablesize}.csv"

    prefix = f"compressed_encoding={int(compressed_encoding)} allow_row_order={int(allow_row_order)}"
    if folder.joinpath("data").joinpath(ply_file_name).exists():
        tqdm.write(f"{prefix}: Count for width={width} x height={height} already done.")
        return

    if width >= height and not allow_row_order:
        return
        
    
    tqdm.write(f"{prefix}: Start count for width={width} x height={height} ... ")

    with open(folder.joinpath(log_file_name), "w") as f:
        
        res = subprocess.run(["../../"+executable_name(compressed_encoding, allow_row_order), str(log2_tablesize), str(width), str(height)], check=False, stdout=f, stderr=f, cwd=folder)
        
        if res.returncode != 0:
            tqdm.write(f"{prefix}: Warning: error during count of width={width} x height={height}!")
            os.rename(folder.joinpath(ply_file_name), folder.joinpath("data").joinpath(ply_file_name))
        else:
            os.rename(folder.joinpath(ply_file_name), folder.joinpath("data").joinpath(ply_file_name))
            os.rename(folder.joinpath(res_file_name), folder.joinpath("data").joinpath(res_file_name))

            tqdm.write(f"{prefix}: Finished count for width={width} x height={height}. ")


# boardsizes = 0 ... small, 1 ... medium, 2 ... big
def count_all(setups, RAM, max_worker):
    assert RAM >= 8
    # working directory is src/connect4

    payloads = []

    # compile
    for setup in setups:
        subprocess.run([
            "make", "count",
            "WRITE_TO_FILE=1",
            "ENTER_TO_CONTINUE=0",
            "IN_OP_GC=0",
            "FULLBDD=0",
            "SUBTRACT_TERM=1",
            f"COMPRESSED_ENCODING={int(setup.compressed_encoding)}",
            f"ALLOW_ROW_ORDER={int(setup.allow_row_order)}",
        ])
        os.rename("build/count.out", executable_name(setup.compressed_encoding, setup.allow_row_order))

        folder = Path(folder_name(setup.compressed_encoding, setup.allow_row_order))
        folder.mkdir(parents=True, exist_ok=True)
        folder.joinpath("logs").mkdir(exist_ok=True)
        folder.joinpath("data").mkdir(exist_ok=True)


        setup_payloads = []
        boardsizes = setup.boardsizes
        for width in range(1,14):
            for height in range(1,14):
                if height + width <= 12:
                    setup_payloads.append([width,height,26])
                elif height <= 4 or width <= 4:
                    if (width,height) == (13,4):
                        if boardsizes >= 1: setup_payloads.append([width, height, 29])
                    elif (width,height) == (4,13):
                        if boardsizes == 2: setup_payloads.append([width, height, 30])
                    elif (width == 4 and height >= 10) or (width >= 1 and height == 4):
                        if boardsizes >= 1: setup_payloads.append([width, height, 28])
                    else:
                        setup_payloads.append([width, height, 26])
                elif height + width == 13:
                    if boardsizes >= 1: setup_payloads.append([width, height, 29])
                elif height + width == 14:
                    if (width,height) in [(6,8),(7,7)]:
                        # (5,9) is on the limit for compressed encoding with 30
                        if boardsizes == 2: setup_payloads.append([width, height, 31])
                    else:
                        if boardsizes == 2: setup_payloads.append([width, height, 30])

        payloads += [[setup.compressed_encoding, setup.allow_row_order] + payload for payload in setup_payloads]

    # TODO: check for duplicates
    payloads.sort(key = lambda t: t[2] * t[3], reverse=True)

    # work on payloads
    for log2_tablesize in range(24,32+1):
        sub_payloads = [payload for payload in payloads if payload[-1] == log2_tablesize]
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
        
        if RAM % RAM_per_worker != 0:
            print(f"RAM {RAM} is not multiple of RAM_per_worker {RAM_per_worker}.")
        n_workers = min(RAM // RAM_per_worker, max_worker)
        with Pool(n_workers) as p:
            list(tqdm(
                p.imap_unordered(count, sub_payloads),
                total=len(sub_payloads),
                desc=f"workers={n_workers} log2_tablesize={log2_tablesize}",
                bar_format="{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}]"
            ))


    for payload in payloads:
        compressed_encoding, allow_row_order, width, height, log2_tablesize = payload
        if width >= height and not allow_row_order:
            folder = Path(folder_name(compressed_encoding, allow_row_order))
            folder_from_other_run = Path(folder_name(compressed_encoding, not allow_row_order))

            log_file_name = f"logs/log_w{width}_h{height}_ls{log2_tablesize}.txt"
            res_file_name = f"results_w{width}_h{height}_ls{log2_tablesize}.csv"
            ply_file_name = f"results_ply_w{width}_h{height}_ls{log2_tablesize}.csv"
            
            if folder_from_other_run.joinpath(log_file_name).exists():
                shutil.copy(folder_from_other_run.joinpath(log_file_name), folder.joinpath(log_file_name))
                if Path(folder_from_other_run.joinpath("data").joinpath(res_file_name)).exists():
                    shutil.copy(folder_from_other_run.joinpath("data").joinpath(res_file_name), folder.joinpath("data").joinpath(res_file_name))
                    shutil.copy(folder_from_other_run.joinpath("data").joinpath(ply_file_name), folder.joinpath("data").joinpath(ply_file_name))


    # cleanup work
    for setup in setups:
        folder = Path(folder_name(setup.compressed_encoding, setup.allow_row_order))
        frames = []
        for file in os.listdir(folder.joinpath("data")):
            if file.startswith("results_w"):
                frames.append(pd.read_csv(folder.joinpath("data").joinpath(file)))

        if len(frames) > 0:
            results = pd.concat(frames)
            results = results.sort_values(["width", "height"])
            results = results.reset_index().drop("index", axis=1)
            results.to_csv(folder.joinpath("all_results.csv"), index=False)

        os.remove(executable_name(setup.compressed_encoding, setup.allow_row_order))


import argparse
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("RAM", type=int)
    parser.add_argument("max_worker", type=int)
    args = parser.parse_args()

    RAM = args.RAM
    max_worker = args.max_worker

    boardsizes=2
    count_all(
        [
            Setup(compressed_encoding=False, allow_row_order=True, boardsizes=boardsizes),
            Setup(compressed_encoding=True, allow_row_order=False, boardsizes=boardsizes),
            # Setup(compressed_encoding=True, allow_row_order=True, boardsizes=boardsizes),
            # Setup(compressed_encoding=False, allow_row_order=False, boardsizes=boardsizes)
        ],
        RAM=RAM,
        max_worker=max_worker
    )
