
import pandas as pd
import argparse
import matplotlib.pyplot as plt

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--latex", action="store_true", help="print tables in latex")
    parser.add_argument("--md", action="store_true", help="print tables in markdown")
    parser.add_argument("--all", action="store_true", help="print entire table instead of only position counts")
    parser.add_argument("--nodes", action="store_true", help="print node count instead of state count")
    parser.add_argument("--subtract-term", action="store_true", help="subtract terminal states from won/lost count")

    parser.add_argument("file", help="file to print solve table")

    args = parser.parse_args()

    df = pd.read_csv(args.file)
    df = df[::-1].reset_index().drop("index", axis=1)

    first_player = df["ply"] % 2 == 0
    second_player = df["ply"] % 2 == 1

    if args.subtract_term:
        df["loss_count"] = df["loss_count"] - df["term_count"]
    
    df["states (won)"] = df["win_count"]
    df.loc[second_player, "states (won)"] = df["loss_count"][second_player]

    df["states (drawn)"] = df["draw_count"]
    
    df["states (lost)"] = df["loss_count"]
    df.loc[second_player, "states (lost)"] = df["win_count"][second_player]


    df["states (total)"] = df["total_count"]
    df["states (terminal)"] = df["term_count"]


    df["nodes (won)"] = df["win_nodecount"]
    df.loc[second_player, "nodes (won)"] = df["loss_nodecount"][second_player]

    df["nodes (drawn)"] = df["draw_nodecount"]
    
    df["nodes (lost)"] = df["loss_nodecount"]
    df.loc[second_player, "nodes (lost)"] = df["win_nodecount"][second_player]
    df["ply"] = df.index

    df["nodes (total)"] = df["total_nodecount"]

    df["nodes (won+drawn+lost)"] = df["nodes (won)"] + df["nodes (drawn)"] + df["nodes (lost)"]


    print("Classification from first players perspective")
    df_states = df[["ply", "states (won)", "states (drawn)", "states (lost)", "states (total)", "states (terminal)"]]
    df_nodes = df[["nodes (won)", "nodes (drawn)", "nodes (lost)", "nodes (total)", "nodes (won+drawn+lost)"]]
    
    if args.all:
        _df = df_states.join(df_nodes)
    elif args.nodes:
        _df  = df_nodes
    else:
        _df = df_states
    str_df = _df.map(lambda x: "{:,}".format(x))

    if args.latex:
        print(str_df.to_latex(index=False))
    elif args.md:
        print(str_df.to_markdown(index=False, stralign="right"))
    else:
        print(str_df)

    if not args.nodes:
        print(df[["states (won)", "states (drawn)", "states (lost)", "states (total)"]].sum(axis=0).map(lambda x: "{:,}".format(x)))


    # plt.figure()
    # plt.plot(df["nodes (won)"], label="nodes (won)")
    # plt.plot(df["nodes (draw)"], label="nodes (draw)")
    # plt.plot(df["nodes (lost)"], label="nodes (lost)")
    # plt.yscale("log")
    # plt.legend()
    # plt.savefig("states.pdf")


