import subprocess
import os
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("folder", type=str)
    args = parser.parse_args()


    folder = args.folder

    archive_name = f"{folder.rstrip("/")}.7z"

    subprocess.run([
        "7za", "a", "-t7z", "-mx9", archive_name, "-r",
        f"{folder}*.csv",
        f"{folder}*.txt",
    ])
    