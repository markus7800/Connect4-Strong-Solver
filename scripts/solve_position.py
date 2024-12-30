import subprocess
from pathlib import Path
import os
import argparse

def get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE):
    return f"w{WIDTH}_h{HEIGHT}_ls{LOG_TB_SIZE}"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", nargs='?', type=int, default=7)
    parser.add_argument("HEIGHT", nargs='?', type=int, default=6)
    parser.add_argument("LOG_TB_SIZE", nargs='?', type=int, default=31)
    args = parser.parse_args()


    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    LOG_TB_SIZE = args.LOG_TB_SIZE
    print(f"{WIDTH=} {HEIGHT=} {LOG_TB_SIZE=}")

    folder = Path(f"results/solve_w{WIDTH}_h{HEIGHT}_results")
    folder.mkdir(exist_ok=True)

    subprocess.run([
        "make", "solve",
        "WRITE_TO_FILE=1",
        "ENTER_TO_CONTINUE=0",
        "IN_OP_GC=1", # we use this compilation flag if RAM is too small and GC has to be triggered within op
        "IN_OP_GC_THRES=0.9",
        "COMPRESSED_ENCODING=1",
        "ALLOW_ROW_ORDER=0",
        "SAVE_BDD_TO_DISK=1"
    ], cwd="src/connect4")

    with open(folder.joinpath(f"log.txt"), "w") as f:
        res = subprocess.run(["../../src/connect4/build/solve.out", str(LOG_TB_SIZE), str(WIDTH), str(HEIGHT)], check=False, stdout=f, stderr=f, cwd=folder)