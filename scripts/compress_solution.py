import subprocess
import os
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", type=int)
    parser.add_argument("HEIGHT", type=int)
    parser.add_argument("COMPRESSED_ENCODING", nargs='?', type=int)
    parser.add_argument("ALLOW_ROW_ORDER", nargs='?', type=int)
    args = parser.parse_args()

    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    COMPRESSED_ENCODING = int(args.COMPRESSED_ENCODING)
    ALLOW_ROW_ORDER = int(args.ALLOW_ROW_ORDER)
    print(f"{WIDTH=} {HEIGHT=} {COMPRESSED_ENCODING=} {ALLOW_ROW_ORDER=}")

    folder = f"results/solve_w{WIDTH}_h{HEIGHT}_results_compenc_{COMPRESSED_ENCODING}_allowrow_{ALLOW_ROW_ORDER}"

    archive_name = f"strong_solution_w{WIDTH}_h{HEIGHT}_archive.7z"

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", "../../"+archive_name, "-r",
        f"*_lost.{COMPRESSED_ENCODING}{ALLOW_ROW_ORDER}.bin",
        f"*_win.{COMPRESSED_ENCODING}{ALLOW_ROW_ORDER}.bin",
        f"*.csv",
        f"*.txt",
    ], cwd=folder)


    with open("compile_wdl.sh", "w") as f:
        f.write(f"#!/bin/bash\ngcc src/connect4/probe/wdl.c -O3 -flto -Wall -O3 -DWIDTH={WIDTH} -DHEIGHT={HEIGHT} -DCOMPRESSED_ENCODING={COMPRESSED_ENCODING} -DALLOW_ROW_ORDER={ALLOW_ROW_ORDER} -o wdl.out")
    subprocess.run(["chmod", "+x", "compile_wdl.sh"])

    with open("compile_bestmove.sh", "w") as f:
        f.write(f"#!/bin/bash\ngcc src/connect4/probe/bestmove.c -O3 -flto -Wall -O3 -DWIDTH={WIDTH} -DHEIGHT={HEIGHT} -DCOMPRESSED_ENCODING={COMPRESSED_ENCODING} -DALLOW_ROW_ORDER={ALLOW_ROW_ORDER} -o bestmove.out")
    subprocess.run(["chmod", "+x", "compile_bestmove.sh"])

    with open("readme.md", "w") as f:
        f.write(f"""
# Build

Run `./compile_wdl.sh` or `bash compile_wdl.sh` to build wdl evaluation (requires gcc).
                
Similarly, run `./compile_bestmove.sh` to build bestmove evaluation.

# Example Usage

`./wdl.out solution_w7_h6 "332"` to evaluate position after playing in the fourth-column twice and then playing in the third column.  
It provides a win-draw-loss evaluation by directly reading from the binary decision diagrams stored in the *.bin files.

Output for 7x6 board:
```
input move sequence: 332
Connect4 width=7 x height=6
 . . . . . . .
 . . . . . . .
 . . . . . . .
 . . . . . . .  stones played: 3
 . . . o . . .  side to move: o
 . . x x . . .  is terminal: 0
 0 1 2 3 4 5 6

Overall evaluation = 1 (forced win)

move evaluation:
  0   1   2   3   4   5   6 
 -1   1  -1  -1   1  -1  -1 

 1 ... move leads to forced win,
 0 ... move leads to forced draw,
-1 ... move leads to forced loss
```
                
Run `./bestmove.out solution_w7_h6 "332"` to find the best move with respect to shortest win or longest loss.  
This will perform an alpha-beta pruning type search guided by the win-draw-loss evaluation.  
It also uses the openingbook stored in the openingbook_w<WIDTH>_h<HEIGHT>_d8.csv file.  
Also see `./bestmove.out --help`.
                
# More Information

Visit https://github.com/markus7800/Connect4-Strong-Solver.
        """)

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", archive_name,
        "src/connect4/probe/ab.c",
        "src/connect4/probe/bestmove.c",
        "src/connect4/probe/board_constants.c",
        "src/connect4/probe/board.c",
        "src/connect4/probe/openingbook.c",
        "src/connect4/probe/probing.c",
        "src/connect4/probe/read.c",
        "src/connect4/probe/tt.c",
        "src/connect4/probe/utils.c",
        "src/connect4/probe/wdl.c",
        "compile_wdl.sh",
        "compile_bestmove.sh",
        "readme.md"
    ], stdout=subprocess.PIPE)

    os.remove("compile_wdl.sh")
    os.remove("compile_bestmove.sh")
    os.remove("readme.md")