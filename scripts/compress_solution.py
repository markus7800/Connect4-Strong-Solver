import subprocess
import os
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", type=int)
    parser.add_argument("HEIGHT", type=int)
    parser.add_argument("COMPRESSED_ENCODING", nargs='?', type=bool, default=True)
    parser.add_argument("ALLOW_ROW_ORDER", nargs='?', type=bool, default=True)
    args = parser.parse_args()
    args = parser.parse_args()

    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    print(f"{WIDTH=} {HEIGHT=}")

    folder = f"results/solve_w{WIDTH}_h{HEIGHT}_results_compenc_{COMPRESSED_ENCODING}_allowrow_{ALLOW_ROW_ORDER}"

    archive_name = f"strong_solution_w{WIDTH}_h{HEIGHT}_archive.7z"

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", archive_name, "-r",
        f"{folder}/*_lost.10.bin",
        f"{folder}/*_win.10.bin",
        f"{folder}/*.csv",
        f"{folder}/*.txt",
    ])


    with open("compile.sh", "w") as f:
        f.write(f"#!/bin/bash\ngcc src/connect4/probe/wdl.c -O3 -flto -Wall -O3 -DWIDTH={WIDTH} -DHEIGHT={HEIGHT} -o wdl_w{WIDTH}_h{HEIGHT}.out")
    subprocess.run(["chmod", "+x", "compile.sh"])

    with open("wdl.sh", "w") as f:
        f.write(f"#!/bin/bash\n./wdl_w{WIDTH}_h{HEIGHT}.out {folder}/solution_w{WIDTH}_h{HEIGHT} \"$1\"")
    subprocess.run(["chmod", "+x", "wdl.sh"])

    with open("readme.md", "w") as f:
        f.write(f"""
# Build

Run `./compile.sh` or `bash compile.sh`.

# Example Usage

`./wdl.sh "332"` to evaluate position after playing in the fourth-column twice and then playing in the third column.

Output for 7x6 board:
```
Input moveseq: 332
Connect4 width=7 x height=6
 . . . . . . .
 . . . . . . .
 . . . . . . .
 . . . . . . .  stones played: 3
 . . . o . . .  side to move: o
 . . x x . . .  is terminal: 0
 0 1 2 3 4 5 6

Overall evaluation = 1 (forced win)

move evalution:
  0   1   2   3   4   5   6 
 -1   1  -1  -1   1  -1  -1 

 1 ... move leads to forced win,
 0 ... move leads to forced draw,
-1 ... move leads to forced loss
```

# More Information

Visit https://github.com/markus7800/Connect4-Strong-Solver.
        """)

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", archive_name,
        "src/connect4/probe/wdl.c",
        "src/connect4/probe/board.c",
        "src/connect4/probe/board_constants.c",
        "src/connect4/probe/probing.c",
        "src/connect4/probe/read.c",
        "compile.sh",
        "wdl.sh",
        "readme.md"
    ], stdout=subprocess.PIPE)

    os.remove("compile.sh")
    os.remove("wdl.sh")
    os.remove("readme.md")