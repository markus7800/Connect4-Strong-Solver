# 13 – Retrograde pass: restricted pre-images, fewer live BDDs, GC on demand

## Idea

For every ply the retrograde pass splits the reachable positions `R` into won (`W`),
drawn (`D`) and lost (`L`), using the drawn and lost sets `D'`, `L'` of the next ply:

```
W = positions with a move into L'
D = positions that are not won, with a move into D'
L = everything else (including the terminal positions)
```

Four things change.

**1. Restricted pre-image.** `W` was computed as `pre(L') ∧ N`, with `N` the non-terminal
positions: first the set of *all* predecessors of `L'`, then the intersection. The
unrestricted pre-image contains unreachable positions and was larger than both its
input and the result (6x5, ply 21: 5.7 M nodes, against 3.95 M for `L'` and 3.98 M for
`W`). The new library operation `image_rename_and(S, T, C)` computes
`image_rename(S, T) ∧ C` in one recursion: it carries the restriction `C` along and stops
wherever `C` is `0`, so the unrestricted set is never built.

**2. No terminal bookkeeping.** A terminal position has no move into a reachable position
(the game is over), and `D'`, `L'` only contain reachable positions. So restricting the
pre-images to `R` already excludes terminal positions; the non-terminal set is only
needed to count them (and for the full board). The loss set becomes a single difference:

```
R  = load reachable set
W  = pre(L') within R            drop L'
X  = R \ W                       drop W, R      (not won: drawn or lost)
D  = pre(D') within X            drop D'
L  = X \ D                       drop X
```

Before, `L = (X \ D) ∪ term` kept `X`, `D`, the intermediate `X \ D` and the growing `L`
alive together, which turned out to be the peak of the whole run.

**3. GC when it pays.** The solver forced a GC after almost every step (five per ply).
Now there are three triggers:

* at a step boundary, if more than `GC_GROWTH` (2.0) times the nodes that survived the
  previous GC are allocated;
* inside an operation, if the pool is 90 % full (`IN_OP_GC=1` is the default now);
* `FORCE_GC=1` restores "after every step", for measuring.

**4. In-op GC uses the whole pool.** If a GC inside an operation could not get below the
90 % mark, the program used to abort ("Could not decrease number of nodes") although
10 % of the pool was still free. Now the trigger moves to halfway between the number of
live nodes and the capacity after each GC, so the remaining space is used with a few
more collections, and the run only stops when the pool is really full.

## Justification

* The largest BDDs of the solve are `W`, `D`, `L` of the middle plies. Every additional
  BDD of that size that is alive at the same time costs as much memory as one of them.
  The step order above needs, at its two worst moments, `{D', L', W}` plus the small `R`,
  and `{X, D, L}` with `X` about the size of `L`.
* Forced GCs were needed when the terminal test produced 69 intermediate BDDs per ply.
  With that gone they mostly re-marked the same live nodes. But "never GC until the pool
  is half full" is not the answer either, see the measurement below: garbage spreads
  the live nodes over more memory and operations slow down. Growth relative to the last
  live count keeps the footprint proportional to what is alive.

Risk: point 2 rests on an argument about the game (a position that contains a four of
the player who did not move last is unreachable), not on the BDD library. It is checked
by the regression: the win/draw/loss counts and the solution files are identical for
all boards, both encodings and both variable orders.

## Experimental results

**GC schedule** (6x4 / 5x5 solve with 2^24 nodes, time in s):

| schedule | solve 6x4 | solve 5x5 |
|---|---:|---:|
| GC only when the pool is half full | 11.22 | 10.55 |
| GC after every step (`FORCE_GC=1`) | 7.55 | 8.79 |
| growth 1.5 | 7.23 | 8.24 |
| growth 2.0 (default) | 7.36 | 8.80 |
| growth 3.0 | 7.23 | 8.64 |
| growth 5.0 | 7.55 | 9.03 |

Letting garbage pile up is 20–50 % slower than collecting eagerly; anything between
1.5 and 3 is equally good.

**Small suite** (2^24 nodes):

| case | 12: time / GC [s] | 13: time / GC [s] | max alive at checkpoints, 12 | 13 |
|---|---:|---:|---:|---:|
| solve 6x4 | 7.09 / 0.86 | 7.01 / 0.39 | 0.76 M | 0.66 M |
| solve 5x5 | 8.94 / 1.11 | 7.73 / 0.45 | 1.02 M | 0.76 M |

**How many nodes does 6x5 really need?** Smallest RAM budget that completes the solve
(the largest per-ply `W + D + L` of 6x5 is 11.27 M nodes):

| budget | nodes | result | in-op GCs | time [s] | max alive |
|---|---:|---|---:|---:|---:|
| baseline, 2^24 at 0.9 (0.83 GiB) | 16.78 M | fails | | | |
| baseline, 2^25 at 0.7 (1.66 GiB) | 33.55 M | ok | 4 | 496 | |
| `300M` | 15.47 M | ok | 22 | 213 | 11.95 M |
| `260M` | 12.85 M | ok | 61 | 217 | 11.65 M |
| `250M` | 12.19 M | ok | 91 | 198 | 11.58 M |
| `240M` | 11.53 M | out of nodes | | | |

* The run needs 11.6 M nodes alive at its worst moment, 1.03 times the largest
  `W + D + L`, and it completes in a pool that is 95 % full at that point. With the
  abort-at-90 % rule the same code needed the `280M` budget.
* In RAM: 0.24 GiB instead of more than 0.83 GiB for the baseline, and at that size it
  is still 2.5x faster than the baseline with all the memory it wants.
* For comparison, the same run with a comfortable 2^26 table takes 135–146 s; under
  maximum pressure it takes about 1.5x as long.

**Keeping cache entries when GC interrupts an operation** (the open question from
[03](03-unified-cache.md)), 6x4 and 5x5 with a `26M` budget, 40–90 in-op GCs per run:

| `GC_KEEPS_CACHE` | solve 6x4 [s] | solve 5x5 [s] |
|---|---:|---:|
| 0: clear | 5.80, 5.77 | 6.10, 6.75 |
| 1: keep on in-op GC (default) | 5.26, 5.25 | 6.25, 6.18 |
| 2: always keep | 5.64, 5.25 | 6.46, 5.59 |

A small gain on 6x4 (9 %), nothing clear on 5x5. The default stays at 1.

* Side observation: these small boards run faster in a tight table (5.3 s for 6x4 with
  1.2 M nodes) than in a large one (7.0 s with 16.8 M nodes), because bucket array and
  cache then fit into the CPU caches.
* Regression: all 12 cases and the tight-budget runs pass including `--bytes`.
