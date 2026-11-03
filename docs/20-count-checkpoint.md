# 20 – Checkpoints for `count` (optional), smaller read buffer for 40-bit builds

## Idea

The large counts (9x7, 9x8, 8x8, ...) run for days on the cluster, and a job that is
killed (time limit, node failure, out of memory in a late ply) had to start again from
ply 0. The forward pass only carries a little state from one ply to the next: the BDD of
the current ply, the full BDD (`FULLBDD=1`), the running total and the elapsed time.

`make count CHECKPOINT=1` stores this state after every ply in `checkpoint_w{W}_h{H}/`:

```
checkpoint_w9_h8/
  ply_33.bin      BDD of ply 33 (ply_33.bin.wide if it has 2^32 or more nodes)
  full_33.bin     full BDD (only with FULLBDD=1)
  state.txt       width height ply total total_t satcount nodecount full_nodecount + build options
```

* **Writing.** The BDDs are written with the existing `_safe_to_file_with_varmap` (only
  board0 variables, mapped to 1, 2, 3, ... as in `solve`). Then `state.txt` is written
  to a temporary file, `fsync`ed and renamed, and only after that the files of the
  previous ply are removed. A crash at any point leaves one complete checkpoint.
* **Resuming** is automatic: a run that finds `state.txt` checks that it was made for
  the same board and the same `COMPRESSED_ENCODING`, `ALLOW_ROW_ORDER`, `SUBTRACT_TERM`,
  `FULLBDD`, reads the BDDs, checks their node count and satcount against the stored
  values and continues with the next ply. The table size and the node layout may differ
  from the run that wrote the checkpoint. The rows of later plies that are already in
  `results_ply_*.csv` (crash between the row and the checkpoint) are removed and
  computed again; the rest of the file is kept.
* When the last ply is done the checkpoint is removed, so the next run starts from ply 0.

`CHECKPOINT=0` (the default) compiles to the previous code.

Reading a BDD (`_read_from_file_with_varmap`, also used by `solve`) changed in two ways:

* The temporary map from file position to node index takes `INDEX_BITS / 8` bytes per
  node: 5 instead of 8 bytes with `INDEX_BITS=40` (a `nodeindex_t` is 64 bits there),
  4 as before otherwise.
* The nodes are built in the order of the file instead of recursively from the root
  (the last node). Every node of a file is part of the BDD, so the result is the same,
  but the memory-mapped file is read mostly sequentially, which matters once the file
  is larger than the page cache or lies on a network file system.

## Justification: size of a large checkpoint

For a ply BDD of 8 083 017 773 nodes (more than 2^32, so only possible with
`COMPACT_NODES=1 INDEX_BITS=40`):

| | size |
|---|---:|
| file (`.wide`, 11 bytes per node) | 88.9 GB (82.8 GiB) |
| disk while switching to the next ply (old + new file) | up to ~180 GB, plus the full BDD with `FULLBDD=1` |
| temporary RAM for writing (rank bitmap, 1.5 bits per table slot, 2^34 slots) | 3.2 GB |
| temporary RAM for reading (5 bytes per node) | 40.4 GB (64.7 GB before this change) |
| node table to hold the BDD (16 bytes per slot, 95 % usable) | ≥ 136 GB, in practice the run's own table |

When resuming, the read buffer comes on top of the node table, but at that moment the
op cache is empty and no other large BDD is alive, so the peak is normally below the
peak of the ply that follows. The mapped file itself is page cache and can be evicted.

## Results

Kill-and-resume test on 6x5 (each binary was killed twice at a random ply with
`alarm`, then run to completion; per-ply counts and node counts compared with an
uninterrupted run without checkpoints):

| build | killed after ply | resumed from | per-ply rows | total |
|---|---|---|---|---|
| default layout | 21, 23 | 21, 23 | identical | 2 818 972 642 |
| `COMPACT_NODES=1` | 20, 21 | 20, 21 | identical | 2 818 972 642 |
| `COMPACT_NODES=1 INDEX_BITS=40 FULLBDD=1` | 20, 21 | 20, 21 | identical, full BDD 2 542 289 nodes with satcount = total | 2 818 972 642 |

A checkpoint made with `FULLBDD=1` is refused by a `FULLBDD=0` build with a message.

Overhead on 6x5 (2^22 nodes, every ply ~0.4 s): 12.2 s without, 13.1 s with checkpoints
(0.9 s for 11.8 M nodes / 106 MB written, ~13 M nodes per second). At that rate the
8.08 G-node checkpoint above takes ~10 minutes, against plies of many hours at that size.

`python3 scripts/regress.py --bytes` (solution files byte-identical), `--unit`, `--wide`
and the solve cases with `COMPACT_NODES=1 INDEX_BITS=40` pass.

Not tested: a resume of a real `.wide` checkpoint (BDD with more than 2^32 nodes); the
`.wide` read path itself is covered by `--wide`.
