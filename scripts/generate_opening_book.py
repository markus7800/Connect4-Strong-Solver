import subprocess
from pathlib import Path
import os
import argparse
import time


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", nargs='?', type=int, default=7)
    parser.add_argument("HEIGHT", nargs='?', type=int, default=6)
    parser.add_argument("N_WORKERS", nargs='?', type=int, default=16)
    args = parser.parse_args()

    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    N_WORKERS = args.N_WORKERS


    subprocess.run([
        "make", "openingbook",
        f"WIDTH={WIDTH}",
        f"HEIGHT={HEIGHT}",
    ], cwd="src/connect4")

    t0 = time.time()
    subprocess.run([
        "python3", "probe/generate_openingbook_mp.py",
        f"../../results/solve_w{WIDTH}_h{HEIGHT}_results/solution_w{WIDTH}_h{HEIGHT}",
        str(WIDTH),
        str(HEIGHT),
        str(N_WORKERS)
    ], cwd="src/connect4")
    t1 = time.time()

    os.remove(f"src/connect4/build/generate_openingbook_w{WIDTH}_h{HEIGHT}.out")
    print(f"\nFinished in {t1-t0:.3f} seconds.")