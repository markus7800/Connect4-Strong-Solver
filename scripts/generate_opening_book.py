import subprocess
from pathlib import Path
import os
import argparse
import time


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", nargs='?', type=int)
    parser.add_argument("HEIGHT", nargs='?', type=int)
    parser.add_argument("COMPRESSED_ENCODING", nargs='?', type=int)
    parser.add_argument("ALLOW_ROW_ORDER", nargs='?', type=int)
    parser.add_argument("N_WORKERS", nargs='?', type=int)
    args = parser.parse_args()

    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    COMPRESSED_ENCODING = int(args.COMPRESSED_ENCODING)
    ALLOW_ROW_ORDER = int(args.ALLOW_ROW_ORDER)
    ROW_ORDER = int(bool(ALLOW_ROW_ORDER) and (HEIGHT > WIDTH))
    N_WORKERS = args.N_WORKERS
    print(f"{WIDTH=} {HEIGHT=} {COMPRESSED_ENCODING=} {ALLOW_ROW_ORDER=} {ROW_ORDER=} {N_WORKERS=}")


    subprocess.run([
        "make", "openingbook",
        f"WIDTH={WIDTH}",
        f"HEIGHT={HEIGHT}",
        f"COMPRESSED_ENCODING={COMPRESSED_ENCODING}",
        f"ALLOW_ROW_ORDER={ALLOW_ROW_ORDER}",
    ], cwd="src/connect4")

    t0 = time.time()
    subprocess.run([
        "python3", "probe/generate_openingbook_mp.py",
        f"../../results/solve_w{WIDTH}_h{HEIGHT}_results_compenc_{COMPRESSED_ENCODING}_row_{ROW_ORDER}/solution_w{WIDTH}_h{HEIGHT}",
        str(WIDTH),
        str(HEIGHT),
        str(N_WORKERS)
    ], cwd="src/connect4")
    t1 = time.time()

    os.remove(f"src/connect4/build/generate_openingbook_w{WIDTH}_h{HEIGHT}.out")
    print(f"\nFinished in {t1-t0:.3f} seconds.")