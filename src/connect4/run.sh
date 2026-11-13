#!/bin/bash
#SBATCH --partition=CPU-amd
#SBATCH --nodes=1
#SBATCH --time=168:00:00
#SBATCH --mem=650G

# checkpoints go to the job-specific scratch dir (fast local disk), results stay in the submit directory
scratch_dir="/scratch/${USER}"
if [ ! -d "${scratch_dir}" ]; then
    mkdir ${scratch_dir}
    chmod 700 ${scratch_dir}
fi
export CHECKPOINT_DIR=${scratch_dir}

make count \
  WRITE_TO_FILE=1 \
  ENTER_TO_CONTINUE=0 \
  IN_OP_GC=0 \
  FULLBDD=0 \
  SUBTRACT_TERM=1 \
  COMPRESSED_ENCODING=1 \
  ALLOW_ROW_ORDER=1 \
  CHECKPOINT=1

LS=35
./build/count.out $LS 8 8

# the checkpoint is removed by count when a board is finished; remove the scratch dir
# (an interrupted job loses keeps checkpoint)
rm -rf ${scratch_dir}

# all boards with width + height <= 15
# boards whose results_w{W}_h{H}_ls{LS}.csv already exists are skipped, so a resubmitted job continues
# (with CHECKPOINT=1 the interrupted board itself resumes from its last ply)

# LS=35

# for w in $(seq 1 14); do
#     for h in $(seq 1 $((15 - w))); do
#         if [ -f results_w${w}_h${h}_ls${LS}.csv ]; then
#             echo "skip ${w}x${h}"
#             continue
#         fi
#         ./build/count.out $LS $w $h
#     done
# done
