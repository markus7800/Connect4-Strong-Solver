
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

do_sub_12 = True
do_13 = True
do_14 = True
do_14_big = True

if do_sub_12:
    for width in range(1,12+1):
        for height in range(1, 12+1):
            if height + width > 12:
                continue

            cnt += 1
            if cnt % mul != off:
                continue

            print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
            with open(f"log_w{width}_h{height}.txt", "w") as f:
                subprocess.run(["src/connect4.out", "27", str(width), str(height)], check=False, stdout=f, stderr=f)
            print("finished.")

if do_13:
    for width in range(1,12+1):
        for height in range(1, 12+1):
            if height + width != 13:
                continue
            cnt += 1
            if cnt % mul != off:
                continue

            print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
            with open(f"log_w{width}_h{height}.txt", "w") as f:
                subprocess.run(["src/connect4.out", "29", str(width), str(height)], check=False, stdout=f, stderr=f)
            print("finished.")

if do_14:
    for (width, height) in [
        (1, 13),
        (2, 12),
        (3, 11),
        (4, 10),
        (8, 6),
        (9, 5),
        (10, 4),
        (11, 3),
        (12, 2),
        (13, 1)
        ]:
            cnt += 1
            if cnt % mul != off:
                continue

            print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
            with open(f"log_w{width}_h{height}.txt", "w") as f:
                subprocess.run(["src/connect4.out", "30", str(width), str(height)], check=False, stdout=f, stderr=f)
            print("finished.")

if do_14_big:
    cnt = -1
    for width, height, log2tbsize in [(5, 9, 31), (7, 7, 31)]:
        cnt += 1
        if cnt % min(mul,2) != off:
            continue

        print(f"{cnt}. Count for width={width} x height={height} ... ", end="", flush=True)
        with open(f"log_w{width}_h{height}.txt", "w") as f:
            subprocess.run(["src/connect4.out",  str(log2tbsize), str(width), str(height)], check=False, stdout=f, stderr=f)
        print("finished.")

# (6, 8, 32) does not work

