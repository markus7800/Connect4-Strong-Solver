# Optimisation log

Each change to the solver is one commit with one document here: the idea, why it should
help, and what was measured. The plan behind the sequence is to first cut the bytes per
BDD node, then the time per operation, then the number of nodes the algorithm keeps
alive, and finally the size of the BDDs themselves.

Regression test for every change: `python3 scripts/regress.py` (see
[00-baseline](00-baseline.md)); it also runs the library unit tests in `tests/bdd_test.c`.

| # | change | headline result |
|---|---|---|
| [00](00-baseline.md) | baseline and regression harness | 53 bytes per node; binary apply 38 %, `make` 20 %, GC 22 % of the time on 5x5 |
| [01](01-intrusive-unique-table.md) | unique table chains through the nodes | 53 -> 45 bytes per node, GC 12–30 % faster, RSS follows actual usage |
| [02](02-mark-sweep-gc.md) | mark-and-sweep GC, 16-byte node, 10-bit variables | 45 -> 41 bytes per node; GC on `solve` ~25 % slower until the GC schedule changes |
| [03](03-unified-cache.md) | one 16-byte operation cache, entries can survive GC | 41 -> 32 bytes per node, no slowdown on small boards |
| [04](04-no-scratch-map.md) | node count and save without hash maps | 32 -> 24 bytes per node; saving a BDD needs no extra memory |
| [05](05-memory-budget.md) | table size as RAM budget (`28G`), any pool capacity, huge pages on Linux | 20–24 bytes per node; 28 GiB hold 1.34 G nodes, 2^32 nodes fit in 103 GB |
| [06](06-terminal-rules-and-diff.md) | terminal rules in `apply`, `diff` operation | 1.5x faster `solve`, 1.8x faster `count` |
| [07](07-hash.md) | multiply-mix hash with full 32-bit range | tables with 2^32 buckets are used fully; 3–5 % faster; old hash was already well distributed |
| [08](08-build-and-hot-path.md) | no node access on cache hits, release flags, overflow-checked counts | 3–4 % faster on small boards; counts can no longer wrap around silently |
| [09](09-gc-root-stack.md) | GC root stack for intermediate results (`IN_OP_GC`) | no node writes per recursion step; prerequisite for making in-op GC the default |
| [10](10-single-variable-set.md) | all sets in one variable set, `image_rename` with identity shortcut | 8–18 % faster, 13–31 % lower peak; column-partitioned image measured and rejected |
| [11](11-terminal-predicate.md) | four-in-a-line BDD built once per player | 1.4–2.3x faster; the predicate has 185 K nodes for 7x6, under 1 M for 7x7 |
| [12](12-reach-on-disk.md) | reachable sets are stored on disk between the two passes | 10–20 % fewer live nodes, ~10 % faster |
| [13](13-retrograde-pass.md) | restricted pre-images, one fewer large BDD alive, GC by growth, in-op GC uses the whole pool | 6x5 needs 11.6 M nodes (1.03x the largest W+D+L) and completes in 0.24 GiB |
| [14](14-dont-care-minimisation.md) | don't-care minimisation of the result BDDs (experiment) | not adopted: 11–14 % at the memory peak; one BDD for all plies would make a published solution 2.5x smaller |
| [15](15-variable-order.md) | column / row order options (experiment) | default order stays for 7+ columns; centre-out is 36–41 % smaller for 5 columns but 26 % larger for 7 |
| [16](16-sizing-ladder.md) | forward pass keeps two instead of three large BDDs; sizing measurements | count needs 1.5x the largest reachable-set BDD (was 2.2x); 7x6 count 626 s, 8x6 count 2920 s; outlook for 7x7 / 8x6 / 9x7 |
| [17](17-7x6-on-32gb.md) | 7x6 on the 32 GB Mac; `BUCKET_SHIFT`, `IN_OP_GC_GROWTH` options | needs 24–28 GiB; first 8 plies verified, then stopped: the machine had ~21 GB to give next to the desktop session |

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
| 04 no scratch maps | 24 | 26.6 | 21.1 | 51.9 |
| 05 memory budget | 20–24 | 26.2 | 21.0 | 51.4 |
| 06 terminal rules, `diff` | 20–24 | 18.1 | 14.0 | 27.9 |
| 07 hash | 20–24 | 17.4 | 13.4 | 26.9 |
| 08 hot path, build flags | 20–24 | 16.7 | 13.0 | 25.7 |
| 10 single variable set | 20–24 | 15.0 | 11.7 | 23.8 |
| 11 terminal predicate | 20–24 | 10.0 | 8.4 | 10.4 |
| 12 reach on disk | 20–24 | 8.9 | 7.1 | 10.4 |
| 13 retrograde pass, GC schedule | 20–24 | 7.7 | 7.0 | 9.8 |

Larger boards (tables of 2^26 nodes):

| after | solve 6x5: time [s] | GC [s] | peak nodes | RSS | count 6x6: time [s] | GC [s] | peak nodes | RSS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 00 baseline | 466.3 | 79.3 | 28.75 M | 3.31 GB | 859.1 | 82.1 | 40.28 M | 2.98 GB |
| 03 unified cache | 444.0 | 69.8 | 28.75 M | 1.43 GB | 798.9 | 36.8 | 40.28 M | 1.48 GB |
| 08 hot path, build flags | 259.1 | 62.4 | 28.75 M | 0.94 GB | 369.3 | 33.5 | 40.28 M | 1.10 GB |
| 12 reach on disk | 137.3 | 13.9 | 21.29 M | 0.85 GB | | | | |
| 13 retrograde pass, GC schedule | 146.1 | 6.5 | 24.99 M | 0.91 GB | 130.5 | 4.4 | 38.48 M | 1.07 GB |

"Peak nodes" is the largest number of allocated nodes seen at a GC, garbage included, so
it depends on the GC schedule. From change 13 on GC runs less often; the number of nodes
that are actually needed is the "max alive" figure in [13](13-retrograde-pass.md).

6x6 solve (2^28 nodes; the baseline ran with `IN_OP_GC=1` at 0.9, which is the default now):

| after | time [s] | GC [s] | RSS |
|---|---:|---:|---:|
| 00 baseline | 7262 | 935 | 12.35 GB |
| 13 retrograde pass, GC schedule | 1722 | 60 | 5.78 GB |
