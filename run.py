
import os
import subprocess

with open("results.csv", "w") as f:
    f.write("width,height,count,time,GC,bytes,RAM\n")


for width in range(1,12+1):
    for height in range(1, 12+1):
        if height + width != 13:
            continue
        
        print(f"Count for width={width} x height={height} ... ", end="", flush=True)
        subprocess.run(["src/connect4.out", "29", str(width), str(height)], check=False)
        print("finished.")

# for width in range(1,12+1):
#     for height in range(1, 12+1):
#         if height + width > 12:
#             continue
        
#         print(f"Count for width={width} x height={height} ... ", end="", flush=True)
#         subprocess.run(["src/connect4.out", "27", str(width), str(height)], check=True, capture_output=True)
#         print("finished.")

# results = pandas.read_csv("results.csv")
# results = results[['width', 'height', 'count']]
# results.pivot(index='height', columns='width', values='count').convert_dtypes()