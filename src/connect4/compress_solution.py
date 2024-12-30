import subprocess
import os
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", nargs='?', type=int, default=7)
    parser.add_argument("HEIGHT", nargs='?', type=int, default=6)
    args = parser.parse_args()

    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    print(f"{WIDTH=} {HEIGHT=}")

    folder = f"solve_w{WIDTH}_h{HEIGHT}_results/"


    subprocess.run([
        "7za", "a", "-t7z", "-mx9", "archive.7z", "-r",
        f"{folder}*_lost.10.bin",
        f"{folder}*_win.10.bin",
        f"{folder}*.csv",
        f"{folder}*.txt",
    ])


    with open("compile.sh", "w") as f:
        f.write(f"#!/bin/bash\ngcc probe/wdl.c -O3 -flto -Wall -O3 -DWIDTH={WIDTH} -DHEIGHT={HEIGHT} -o wdl_w{WIDTH}_h{HEIGHT}.out")
    subprocess.run(["chmod", "+x", "compile.sh"])

    with open("wdl.sh", "w") as f:
        f.write(f"#!/bin/bash\n./wdl_w{WIDTH}_h{HEIGHT}.out {folder}solution_w{WIDTH}_h{HEIGHT} \"$1\"")
    subprocess.run(["chmod", "+x", "wdl.sh"])

    # TODO: add README

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", "archive.7z",
        "probe/wdl.c",
        "probe/board.c",
        "probe/board_constants.c",
        "probe/probing.c",
        "probe/read.c",
        "compile.sh",
        "wdl.sh"
    ], stdout=subprocess.PIPE)

    os.remove("compile.sh")
    os.remove("wdl.sh")