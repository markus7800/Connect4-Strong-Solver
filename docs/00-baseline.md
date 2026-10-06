# 00 – Baseline and regression harness

## Idea

Before changing anything, pin down what "correct" and "fast" mean, so that every later
change can be judged against the same numbers.

* `scripts/regress.py` builds `solve.out` / `count.out` for each encoding configuration,
  runs a suite of small boards in a scratch directory (`results/regress/`, git-ignored)
  and checks the results.
* `tests/golden/` holds the reference data, generated from commit `281e420` (the state of
  `main` before this work).

## Justification

The solver's outputs are summary statistics, and those are the regression oracle:

| check | what is compared |
|---|---|
| solve, per ply | `win`, `draw`, `loss`, `term`, `total` state counts vs `tests/golden/<case>.csv` |
| solve, total | sum of `total` over all plies vs the published position count |
| count | total number of positions vs the published table (README / John Tromp) |
| `--bytes` (optional) | sha256 of every solution `.bin` vs `tests/golden/<case>.sha256` |

The byte check is stricter than needed: a reduced ordered BDD is canonical and the save
routine writes nodes in a deterministic post-order, so the file only depends on the
represented set and the variable order, not on node indices or allocation order. Any
change that keeps both must reproduce the files bit for bit. It stops applying once a
change deliberately alters the stored function (don't-care minimisation) or the order.

BDD node counts and file sizes are reported but are *not* part of the oracle; they are
expected to go down.

The suite covers both encodings (`COMPRESSED_ENCODING=0/1`), column and row order
(`ALLOW_ROW_ORDER=1` with height > width), `solve` and `count`.

## How to run

```bash
python3 scripts/regress.py                  # small suite, ~2 minutes at baseline
python3 scripts/regress.py --bytes          # also compare .bin hashes
python3 scripts/regress.py --suite medium   # 6x5 solve, 6x6 count
python3 scripts/regress.py --suite bench    # 6x6 solve, 7x6 count
python3 scripts/regress.py --src <worktree>/src/connect4 --tag _base   # measure another commit
```

Metrics per case: wall time and GC time as printed by the program, the peak number of
allocated nodes (`GC max number of allocated nodes`, sampled at GC points), and peak RSS
from `/usr/bin/time`.

## Experimental results (baseline)

Machine: Apple M2 Pro (10 cores), 32 GB RAM, macOS 15.2, Apple clang 17, single thread.
Build flags as in the makefile (`-O3 -flto`, `IN_OP_GC=0`).

| case | table | time [s] | GC time [s] | peak nodes | W+D+L nodes (all plies) | solution size |
|---|---|---:|---:|---:|---:|---:|
| solve 4x4 compressed | 2^22 | 0.36 | 0.20 | 48,575 | 64,621 | 0.6 MB |
| solve 5x4 compressed | 2^22 | 1.62 | 0.45 | 268,703 | 825,306 | 7.4 MB |
| solve 5x4 default | 2^22 | 2.67 | 0.62 | 467,355 | 1,372,664 | 12.4 MB |
| solve 4x5 compressed, row order | 2^22 | 1.52 | 0.49 | 248,395 | 476,033 | 4.3 MB |
| solve 4x5 default, row order | 2^22 | 2.00 | 0.59 | 370,482 | 786,907 | 7.1 MB |
| solve 6x4 compressed | 2^24 | 21.56 | 4.61 | 2,158,979 | 7,591,361 | 68.3 MB |
| solve 5x5 compressed | 2^24 | 27.03 | 6.07 | 2,385,028 | 8,427,149 | 75.8 MB |
| count 5x5 compressed | 2^22 | 3.43 | 0.39 | 843,527 | | |
| count 5x5 default | 2^22 | 4.79 | 0.48 | 1,230,084 | | |
| count 4x5 compressed, row order | 2^22 | 0.39 | 0.10 | 100,854 | | |
| count 6x5 compressed | 2^24 | 52.03 | 5.29 | 5,214,931 | | |

Memory is allocated and initialised up front, so at baseline the RSS is a function of the
table size only: 0.21 GB at 2^22, 0.83 GB at 2^24 (53 bytes per allocatable node for
`solve`, 51 for `count`). The baselines for 6x5 / 6x6 are added to `docs/README.md` when
their runs finish (the 6x6 solve takes more than two hours).

### Where the bytes go

| item | bytes per allocatable node |
|---|---:|
| `bddnode_t` (`var`, `parentcount`, `low`, `high`, `next`) | 20 |
| unique table: `buckets` 4 + `entries {next, value}` 8 | 12 |
| three op caches with N/4 entries each (16 + 16 + 20 bytes) | 13 |
| `nodeindexmap` with N/2 entries in `solve` (node-count set in `count`: 6) | 8 |
| **total** | **53** (`count`: 51) |

The 9x6 count run at `log2=32` reported 219,043,332,096 bytes = 51.0 bytes per node, which
matches.

### Where the time goes

Sampling profile of `solve 5x5` (`sample`, 15 s, top of stack):

| function | share |
|---|---:|
| `apply_binary` (and / or / iff) | 38 % |
| `make` (unique table lookup + insert) | 20 % |
| `gc` (+ `disable_node_rec`) | 22 % |
| `apply_image` | 8 % |
| `_connect4_satcount` | 3 % |
| `_collect_nodes_into_map` + `reset_map` | 2 % |
| `apply_not` | 1.5 % |

On a board this small the forced GCs (five per ply from ply 10 on, each clearing all
caches) are over-represented, but the ranking is the same on larger boards except that GC
shrinks: binary apply dominates, not the image.
