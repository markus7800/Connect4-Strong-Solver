
import os
import subprocess
import sys

if len(sys.argv) == 1:
    mul = 1
    off = 0
elif len(sys.argv) == 3:
    mul = int(sys.argv[1])
    off = int(sys.argv[2])
else:
    print("usage: run.py mul off")
    exit()
print(f"mul={mul} off={off}")

cnt = -1
do_sub_12 = False

if do_sub_12:
    for width in range(1,12+1):
        for height in range(1, 12+1):
            if height + width > 12:
                continue

            cnt += 1
            if cnt % mul != off:
                continue

            print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
            subprocess.run(["src/connect4.out", "27", str(width), str(height)], check=True, capture_output=True)
            print("finished.")

do_13 = False
if do_13:
    for width in range(1,12+1):
        for height in range(1, 12+1):
            if height + width != 13:
                continue
            cnt += 1
            if cnt % mul != off:
                continue

            print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
            subprocess.run(["src/connect4.out", "29", str(width), str(height)], check=False)
            print("finished.")

do_14 = False
if do_14:
    for width in range(1,13+1):
        for height in range(1, 13+1):
            if height + width != 14:
                continue
            cnt += 1
            if cnt % mul != off:
                continue

            print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
            with open(f"log_w{width}_h{height}.txt", "w") as f:
                subprocess.run(["src/connect4.out", "30", str(width), str(height)], check=False, stdout=f, stderr=f)
            print("finished.")

redo_14 = True
if redo_14:
    for width, height in [(7,7),(6,8),(8,6)]:
        print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
        with open(f"log_w{width}_h{height}.txt", "w") as f:
            subprocess.run(["src/connect4.out", "32", str(width), str(height)], check=False, stdout=f, stderr=f)
        print("finished.")

# results = pandas.read_csv("results.csv")
# results = results[['width', 'height', 'count']]
# results.pivot(index='height', columns='width', values='count').convert_dtypes()