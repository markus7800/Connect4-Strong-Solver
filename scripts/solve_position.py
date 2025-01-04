import subprocess
from pathlib import Path
import os
import argparse

def get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE):
    return f"w{WIDTH}_h{HEIGHT}_ls{LOG_TB_SIZE}"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", type=int)
    parser.add_argument("HEIGHT", type=int)
    parser.add_argument("LOG_TB_SIZE", type=int)
    parser.add_argument("COMPRESSED_ENCODING", nargs='?', type=int, default=1)
    parser.add_argument("ALLOW_ROW_ORDER", nargs='?', type=int, default=0)
    parser.add_argument("--exists-ok", action="store_true", default=False)
    args = parser.parse_args()


    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    LOG_TB_SIZE = args.LOG_TB_SIZE
    COMPRESSED_ENCODING = int(args.COMPRESSED_ENCODING)
    ALLOW_ROW_ORDER = int(args.ALLOW_ROW_ORDER)
    print(f"{WIDTH=} {HEIGHT=} {LOG_TB_SIZE=} {COMPRESSED_ENCODING=} {ALLOW_ROW_ORDER=}")

    folder = Path(f"results/solve_w{WIDTH}_h{HEIGHT}_results_compenc_{COMPRESSED_ENCODING}_allowrow_{ALLOW_ROW_ORDER}")
    folder.mkdir(exist_ok=args.exists_ok)

    subprocess.run([
        "make", "solve",
        "WRITE_TO_FILE=1",
        "ENTER_TO_CONTINUE=0",
        "IN_OP_GC=1", # we use this compilation flag if RAM is too small and GC has to be triggered within op
        "IN_OP_GC_THRES=0.9",
        f"COMPRESSED_ENCODING={COMPRESSED_ENCODING}",
        f"ALLOW_ROW_ORDER={ALLOW_ROW_ORDER}",
        "SAVE_BDD_TO_DISK=1"
    ], cwd="src/connect4")

    with open(folder.joinpath(f"log_w{WIDTH}_h{HEIGHT}_ls{LOG_TB_SIZE}.txt"), "w") as f:
        res = subprocess.run(["../../src/connect4/build/solve.out", str(LOG_TB_SIZE), str(WIDTH), str(HEIGHT)], check=False, stdout=f, stderr=f, cwd=folder)