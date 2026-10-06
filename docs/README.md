# Optimisation log

Each change to the solver is one commit with one document here: the idea, why it should
help, and what was measured. The plan behind the sequence is to first cut the bytes per
BDD node, then the time per operation, then the number of nodes the algorithm keeps
alive, and finally the size of the BDDs themselves.

Regression test for every change: `python3 scripts/regress.py` (see
[00-baseline](00-baseline.md)).

| # | change | headline result |
|---|---|---|
| [00](00-baseline.md) | baseline and regression harness | 53 bytes per node; binary apply 38 %, `make` 20 %, GC 22 % of the time on 5x5 |
| [01](01-intrusive-unique-table.md) | unique table chains through the nodes | 53 -> 45 bytes per node, GC 12–30 % faster, RSS follows actual usage |
| [02](02-mark-sweep-gc.md) | mark-and-sweep GC, 16-byte node, 10-bit variables | 45 -> 41 bytes per node; GC on `solve` ~25 % slower until the GC schedule changes |
| [03](03-unified-cache.md) | one 16-byte operation cache, entries can survive GC | 41 -> 32 bytes per node, no slowdown on small boards |

## Running totals

Machine: Apple M2 Pro, 32 GB, single thread. `bytes/node` is the memory allocated per
allocatable node for `solve` (nodes, unique table, caches, scratch maps).

Small boards (tables of 2^24 nodes, timings ±5 % because other jobs were running):

| after | bytes/node | solve 5x5 [s] | solve 6x4 [s] | count 6x5 [s] |
|---|---:|---:|---:|---:|
| 00 baseline | 53 | 27.0 | 21.6 | 52.0 |
| 01 intrusive unique table | 45 | 27.2 | 21.8 | 47.9 |
| 02 mark-and-sweep GC | 41 | 28.2 | 22.3 | 48.0 |
| 03 unified cache | 32 | 27.4 | 21.2 | 52.9 |

Larger boards (tables of 2^26 nodes):

| after | solve 6x5: time [s] | GC [s] | peak nodes | RSS | count 6x6: time [s] | GC [s] | peak nodes | RSS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 00 baseline | 466.3 | 79.3 | 28.75 M | 3.31 GB | 859.1 | 82.1 | 40.28 M | 2.98 GB |
