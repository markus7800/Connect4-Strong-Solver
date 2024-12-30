import subprocess
from pathlib import Path
import os
import pandas as pd
import matplotlib.pyplot as plt
import argparse

def get_prefix(compressed_encoding, subtract_term):
    return f"compenc={int(compressed_encoding)}_subterm={int(subtract_term)}"

def get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE):
    return f"w{WIDTH}_h{HEIGHT}_ls{LOG_TB_SIZE}"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("WIDTH", nargs='?', type=int, default=7)
    parser.add_argument("HEIGHT", nargs='?', type=int, default=6)
    parser.add_argument("LOG_TB_SIZE", nargs='?', type=int, default=29)
    args = parser.parse_args()


    WIDTH = args.WIDTH
    HEIGHT = args.HEIGHT
    LOG_TB_SIZE = args.LOG_TB_SIZE
    print(f"{WIDTH=} {HEIGHT=} {LOG_TB_SIZE=}")

    folder = Path(f"count_w{WIDTH}_h{HEIGHT}_results")
    folder.mkdir(exist_ok=True)

    for compressed_encoding, subtract_term in [(True, False), (False, False), (True, True), (False, True)]:
        subprocess.run([
            "make", "count",
            "WRITE_TO_FILE=1",
            "ENTER_TO_CONTINUE=0",
            "IN_OP_GC=0",
            "FULLBDD=1", # FULL BDD
            f"SUBTRACT_TERM={int(subtract_term)}",
            f"COMPRESSED_ENCODING={int(compressed_encoding)}",
            f"ALLOW_ROW_ORDER=0",
        ])

        prefix = get_prefix(compressed_encoding, subtract_term)
        with open(folder.joinpath(f"{prefix}_log.txt"), "w") as f:
            res = subprocess.run(["../build/count.out", str(LOG_TB_SIZE), str(WIDTH), str(HEIGHT)], check=False, stdout=f, stderr=f, cwd=folder)

        suffix = get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE)
        os.rename(folder.joinpath(f"results_ply_{suffix}.csv"), folder.joinpath(f"{prefix}_results_ply_{suffix}.csv"))
        os.rename(folder.joinpath(f"results_{suffix}.csv"), folder.joinpath(f"{prefix}_results_{suffix}.csv"))


        os.remove("build/count.out")

    def get_node_counts(compressed_encoding, subtract_term):
        prefix = get_prefix(compressed_encoding, subtract_term)
        res = pd.read_csv(folder.joinpath(f"{prefix}_results_ply_{suffix}.csv"))
        res = res[:-1].convert_dtypes()
        return res["nodecount"]

    def get_states_counts(compressed_encoding, subtract_term):
        prefix = get_prefix(compressed_encoding, subtract_term)
        res = pd.read_csv(folder.joinpath(f"{prefix}_results_ply_{suffix}.csv"))
        res = res[:-1].convert_dtypes()
        return res["poscount"]
    suffix = get_suffix(WIDTH, HEIGHT, LOG_TB_SIZE)
    fig, axs = plt.subplots(2,1, figsize=(6,7))
    axs[0].set_yscale("log")
    axs[0].plot(get_states_counts(False, False), c="tab:blue", marker="+", label="Ignored Terminals (States)")
    axs[0].plot(get_states_counts(False, True), c="tab:orange", marker="s", markerfacecolor='none', label="Stopped at Terminals (States)")
    axs[0].plot(get_node_counts(False, False), c="tab:blue", marker="o", markerfacecolor='none', label="Ignored Terminals (Nodes)")
    axs[0].plot(get_node_counts(False, True), c="tab:orange", marker="x", label="Stopped at Terminals (Nodes)")
    axs[0].set_ylabel("Number of States or BDD Nodes")
    axs[0].grid()
    axs[0].legend()

    #axs[1].set_yscale("log")
    # axs[1].plot(get_node_counts(True, False), c="tab:blue", marker="+", label="Ignored Terminals (Compressend)")
    axs[1].plot(get_node_counts(True, True), c="tab:blue", marker="s", markerfacecolor='none', label="Stopped at Terminals (Compressed)")
    # axs[1].plot(get_node_counts(False, False), c="tab:orange", marker="o", markerfacecolor='none', label="Ignored Terminals (Default)")
    axs[1].plot(get_node_counts(False, True), c="tab:orange", marker="x", label="Stopped at Terminals (Default)")
    axs[1].set_ylabel("Number of BDD Nodes")
    axs[1].set_xlabel("Ply")
    axs[1].grid()
    axs[1].legend()

    plt.tight_layout()
    plt.savefig(folder.joinpath("bdd_node_count.pdf"))
