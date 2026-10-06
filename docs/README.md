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

## Running totals

Machine: Apple M2 Pro, 32 GB, single thread. `bytes/node` is the memory allocated per
allocatable node for `solve` (nodes, unique table, caches, scratch maps).

| after | bytes/node | solve 5x5 [s] | solve 6x4 [s] | count 6x5 [s] |
|---|---:|---:|---:|---:|
| 00 baseline | 53 | 27.0 | 21.6 | 52.0 |
