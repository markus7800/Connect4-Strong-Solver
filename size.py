import os

folder = "/Users/markus/Downloads/Connect4-PositionCount-Solve"

width = 7
height = 6

total_size = 0
for ply in range(width*height+1):
    lost_size = os.stat(folder + "/" + f"bdd_w{width}_h{height}_{ply}_lost.bin").st_size
    draw_size = os.stat(folder + "/" + f"bdd_w{width}_h{height}_{ply}_draw.bin").st_size
    win_size = os.stat(folder + "/" + f"bdd_w{width}_h{height}_{ply}_win.bin").st_size
    print(width, height, ply, "lost", lost_size // 9, "draw", draw_size // 9, "win", win_size // 9)
    total_size += lost_size + draw_size + win_size
    total_size -= max([lost_size, draw_size, win_size])

print(total_size / 10**9)
