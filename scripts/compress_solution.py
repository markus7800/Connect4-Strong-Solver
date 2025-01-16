import subprocess
import os
import argparse
import re

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
    ROW_ORDER = int(bool(ALLOW_ROW_ORDER) and (HEIGHT > WIDTH))
    print(f"{WIDTH=} {HEIGHT=} {COMPRESSED_ENCODING=} {ALLOW_ROW_ORDER=} {ROW_ORDER=}")

    folder = f"results/solve_w{WIDTH}_h{HEIGHT}_results_compenc_{COMPRESSED_ENCODING}_row_{ROW_ORDER}"

    archive_name = f"strong_solution_w{WIDTH}_h{HEIGHT}_archive.7z"

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", "../../"+archive_name, "-r",
        f"*_loss.{COMPRESSED_ENCODING}{ROW_ORDER}.bin",
        f"*_win.{COMPRESSED_ENCODING}{ROW_ORDER}.bin",
        f"*.csv",
        f"*.txt",
    ], cwd=folder)


    with open("compile_wdl.sh", "w") as f:
        f.write(f"#!/bin/bash\ngcc src/connect4/probe/wdl.c -O3 -flto -Wall -O3 -DWIDTH={WIDTH} -DHEIGHT={HEIGHT} -DCOMPRESSED_ENCODING={COMPRESSED_ENCODING} -DALLOW_ROW_ORDER={ALLOW_ROW_ORDER} -o wdl.out")
    subprocess.run(["chmod", "+x", "compile_wdl.sh"])
    subprocess.run(["./compile_wdl.sh"])
    res_wdl = subprocess.run(["./wdl.out", f"{folder}/solution_w{WIDTH}_h{HEIGHT}", "332"], capture_output=True)
    os.remove("wdl.out")
    print("Example WDL:")
    print(res_wdl.stdout.decode())

    with open("compile_bestmove.sh", "w") as f:
        f.write(f"#!/bin/bash\ngcc src/connect4/probe/bestmove.c -O3 -flto -Wall -O3 -DWIDTH={WIDTH} -DHEIGHT={HEIGHT} -DCOMPRESSED_ENCODING={COMPRESSED_ENCODING} -DALLOW_ROW_ORDER={ALLOW_ROW_ORDER} -o bestmove.out")
    subprocess.run(["chmod", "+x", "compile_bestmove.sh"])
    subprocess.run(["./compile_bestmove.sh"])


    res_bestmove = subprocess.run(["./bestmove.out", f"{folder}/solution_w{WIDTH}_h{HEIGHT}", "332"], capture_output=True)
    os.remove("bestmove.out")
    print("Example best move:")
    print(res_bestmove.stdout.decode())


    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

    res_wdl_str = ansi_escape.sub("", res_wdl.stdout.decode().replace("\x08", "\b"))
    res_bestmove_str = ansi_escape.sub("", res_bestmove.stdout.decode().replace("\x08", "\b"))

    with open("readme.md", "w") as f:
        f.write(f"""
# Build

Run `./compile_wdl.sh` or `bash compile_wdl.sh` to build win-draw-loss evaluation (requires gcc).
                
Similarly, run `./compile_bestmove.sh` to build bestmove evaluation.

# Example Usage

`./wdl.out solution_w{WIDTH}_h{HEIGHT} "332"` to evaluate position after playing in the fourth-column twice and then playing in the third column.  
It provides a win-draw-loss evaluation by directly reading from the binary decision diagrams stored in the *.bin files.

Example Output:
```
{res_wdl_str}
```
                
Run `./bestmove.out solution_w{WIDTH}_h{HEIGHT} "332"` to find the best move with respect to fastest win or slowest loss.  
This will perform an alpha-beta search guided by the win-draw-loss evaluation.  
It also uses the openingbook stored in the solution_w{WIDTH}_h{HEIGHT}/openingbook_w{WIDTH}_h{HEIGHT}_d8.csv file.  
Also see `./bestmove.out --help`.

Example Output:
```
{res_bestmove_str}
```

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