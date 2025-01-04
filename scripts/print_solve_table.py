
import pandas as pd
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--latex", action="store_true", help="print tables in latex")
    parser.add_argument("--md", action="store_true", help="print tables in markdown")

    parser.add_argument("file", help="file to print solve table")

    args = parser.parse_args()

    df = pd.read_csv(args.file)
    df = df[::-1].reset_index().drop("index", axis=1)

    first_player = df["ply"] % 2 == 0
    second_player = df["ply"] % 2 == 1
    
    df["states (won)"] = df["win_count"]
    df.loc[second_player, "states (won)"] = df["lost_count"][second_player]

    df["states (draw)"] = df["draw_count"]
    
    df["states (lost)"] = df["lost_count"]
    df.loc[second_player, "states (lost)"] = df["win_count"][second_player]


    df["states"] = df["total_count"]
    df["terminal"] = df["term_count"]


    df["nodes (won)"] = df["win_nodecount"]
    df.loc[second_player, "nodes (won)"] = df["lost_nodecount"][second_player]

    df["nodes (draw)"] = df["draw_nodecount"]
    
    df["nodes (lost)"] = df["lost_nodecount"]
    df.loc[second_player, "nodes (lost)"] = df["win_nodecount"][second_player]
    df["ply"] = df.index


    print("Classification from first players perspective")
    df = df[["ply", "states (won)", "states (draw)", "states (lost)", "states", "terminal", "nodes (won)", "nodes (draw)", "nodes (lost)"]]
    str_df = df.map(lambda x: "{:,}".format(x))

    if args.latex:
        print(str_df.to_latex(index=False))
    elif args.md:
        print(str_df.to_markdown(index=False, stralign="right"))
    else:
        print(str_df)

    print(df[["states (won)", "states (draw)", "states (lost)", "states"]].sum(axis=0).map(lambda x: "{:,}".format(x)))


