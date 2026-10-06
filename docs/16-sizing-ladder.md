# 16 – How far does this go? Forward-pass tightening and sizing measurements

## Idea

Two parts.

**A small change to the forward pass.** One step of the reachability computation is

```
nonterminal = current \ four(last mover)
next        = successors(nonterminal)
```

`current` was kept alive until `next` was complete, although it is not needed once
`nonterminal` exists. It is released before the image is computed now, in `count` and in
the forward pass of `solve`. During the image, two large BDDs are alive instead of three.

**Measurements for planning larger boards.** With the library and the solver in their
new state, what does a computation need, as a multiple of a quantity that can be
extrapolated? Two rules of thumb come out of "smallest table that still completes"
experiments:

| computation | needs alive at the peak |
|---|---|
| `count` | 1.5 x the largest reachable-set BDD |
| `solve` | 1.03–1.06 x the largest per-ply `W + D + L` |

## Justification

Whether a board is in reach has so far been judged by trying it with the largest table
and watching it fail ("9x7 blows up at 2^32"). The requirement of the old code was the
live data times a factor of 2–3.5 that depended on GC timing. Now the factor is small
and stable, so sizes measured on smaller boards can be extrapolated.

## Experimental results

**Forward-pass change**, smallest table that completes the 6x6 count (largest
reachable-set BDD: 11.80 M nodes):

| version | nodes in the table | result | max alive |
|---|---:|---|---:|
| before | 25.69 M (`520M`) | out of nodes | |
| before | 30.93 M (`600M`) | ok | 25.58 M (2.17 x) |
| after | 16.78 M (`370M`) | out of nodes | |
| after | 19.14 M (`420M`) | ok | 17.22 M (1.46 x) |

**Counting**, new code, Apple M2 Pro:

| board | time | largest reachable-set BDD | max alive (large table) | RSS |
|---|---:|---:|---:|---:|
| 6x6 | 131 s (baseline 859 s) | 11.80 M | 12.00 M | 1.07 GB |
| 7x6 | 626 s | 51.96 M | 52.30 M | 4.22 GB |
| 8x6 | 2920 s | 146.70 M | 147.20 M | 10.01 GB |

(7x6 and 8x6 were measured before the forward-pass change. All totals match the
published counts. For comparison, the README lists 6300–6900 s for the 7x6 count on a
Ryzen 5950X with the original code.)

**Solving:**

| board | time | largest per-ply `W + D + L` | max alive, tight table | smallest RAM that worked |
|---|---:|---:|---:|---:|
| 6x5 | 135 s (baseline 466 s) | 11.28 M | 11.58 M (1.03 x) | 0.24 GiB |
| 6x6 | 1722 s (baseline 7262 s) | 99.69 M | 105.42 M (1.06 x) | 2.5 GiB (baseline: 12.35 GB) |
| 7x6 | see [17](17-7x6-on-32gb.md) | 1141.56 M | | |

### Extrapolation

Largest reachable-set BDD, from the runs above and the earlier width-9 runs in this
repository (`results_ply_w9_*.csv`):

| | height 4 | height 5 | height 6 | height 7 |
|---|---:|---:|---:|---:|
| width 6 | | 1.41 M | 11.80 M | |
| width 7 | | | 51.96 M | |
| width 8 | | | 146.70 M | |
| width 9 | 2.64 M | 34.75 M | 322.38 M | > 1393 M (run aborted at ply 37 of 63) |

One more row multiplies the BDD by 8–13 (less as the board grows), one more column by
2.2–4.4. With "1.5 x the largest BDD" and 24 bytes per node:

| count | estimated largest BDD | nodes needed | RAM | verdict |
|---|---:|---:|---:|---|
| 7x7 | ~0.4 G | ~0.6 G | ~15 GB | fits the 32 GB Mac |
| 8x7 | ~1.0–1.3 G | ~1.5–2.0 G | 36–48 GB | fits 64 GB |
| 9x7 | ~2.3–2.9 G | ~3.4–4.3 G | 80–103 GB | at the limit of the 32-bit node index; worth trying on the large machine |
| 8x8 | ~7–10 G | ~10–15 G | > 250 GB | needs wider node indices |

The original code needed about 3.4 x the largest BDD in allocated nodes (9x6: 1.09 G
for a 322 M BDD), which is why 9x7 was out of reach at 2^32.

For the strong solve, the largest `W + D + L` grew by 8.8x from 6x5 to 6x6 and by 11.5x
from 6x6 to 7x6:

| solve | estimated largest `W + D + L` | nodes needed | verdict |
|---|---:|---:|---|
| 7x6 | 1.14 G (known) | 1.21 G | 28 GiB, see [17](17-7x6-on-32gb.md) |
| 7x7, 8x6 | ~8–12 G | ~9–13 G | 2–3 x beyond the 32-bit index (4.29 G nodes) |

### What the next board needs

A 7x7 or 8x6 strong solve does not fit under the 32-bit node index, whatever the amount
of RAM. The remaining factor of 2–3 cannot come from the measures tried here:
don't-care minimisation gave 11–14 % ([14](14-dont-care-minimisation.md)) and the
variable order is already the best of those tested ([15](15-variable-order.md)). The
options, in order of effort:

1. **Wider node indices** (40 bits). A node still fits into 16 bytes (three 40-bit
   fields and a 7-bit variable), buckets and cache entries grow; about 24 bytes per node
   in total. 10 G nodes then need about 240 GB: the 256 GB machine is at the edge, a
   512 GB machine is comfortable. The file format needs 5-byte indices as well.
2. **Run time.** 7x6 takes on the order of ten hours single-threaded on the M2; ten
   times the nodes means days to a week. This is where parallel BDD operations start to
   pay for their complexity.
3. **Out-of-core** (keeping the BDDs of a ply on disk and processing them level by
   level) would remove the RAM limit altogether, at the cost of a different operation
   engine.

Counting is the cheaper frontier: 8x7 and probably 9x7 are new results that the
current code should reach on existing hardware.
