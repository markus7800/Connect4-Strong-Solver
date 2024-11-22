import os
import sys


folder = sys.argv[1]
width = int(sys.argv[2])
height = int(sys.argv[3])

total_size = 0
for ply in range(width*height+1):
    lost_size = os.stat(folder + "/" + f"bdd_w{width}_h{height}_{ply}_lost.bin").st_size
    draw_size = os.stat(folder + "/" + f"bdd_w{width}_h{height}_{ply}_draw.bin").st_size
    win_size = os.stat(folder + "/" + f"bdd_w{width}_h{height}_{ply}_win.bin").st_size
    print(width, height, ply, "lost", lost_size // 9, "draw", draw_size // 9, "win", win_size // 9)
    total_size += lost_size + draw_size + win_size
    rename_suffix = ""
    if lost_size > draw_size and lost_size > win_size:
        total_size -= lost_size
        rename_suffix = "lost"
    if draw_size > lost_size and draw_size > win_size:
        total_size -= draw_size
        rename_suffix = "draw"
    if win_size > draw_size and win_size > lost_size:
        total_size -= win_size
        rename_suffix = "win"
    rename_file = folder + "/" + f"bdd_w{width}_h{height}_{ply}_{rename_suffix}.bin"
    os.rename(rename_file, rename_file + ".redundant")

print(total_size / 10**9)
