# 14 – Don't-care minimisation (experiment, not adopted)

## Idea

The won / drawn / lost BDDs of a ply are exact: they are `0` for every assignment that is
not a reachable position of that ply. Nobody ever asks about those assignments. The
probing programs only query positions that occur in play, and the solver itself only
asks "is this successor in `L'`?" for successors of reachable positions.

So `W` could be replaced by any `W~` with `W~ ∧ R = W` (`R` = reachable positions of the
ply, the *care set*), and the hope was that a suitable `W~` is much smaller: the exact
BDD has to encode which assignments are positions at all (the column structure of the
encoding, the number of stones of each colour), `W~` does not.

Two things were added to try this:

* `minimise(f, care)` in the library, the *restrict* operator of Coudert and Madre: walk
  `f` and `care` together and drop the test of a variable whenever `care` is `0` for one
  of its two values.
* `-DMINIMISE_STATS=1` for `solve`, which reports for every ply the size of each result
  BDD and of its minimised versions for different care sets, plus the size of the union
  of each result set over all plies processed so far. The results of the solve are not
  changed.

## Justification

This was the candidate for a multi-fold reduction of the largest BDDs, which is what a
7x7 or 8x6 solve would need (the plan estimated them at ten times 7x6). Constant-factor
work on the library cannot deliver that; a smaller representation of the same
information could.

There was a concrete reason for optimism: for the reachable sets the README reports
that one BDD for *all* plies of 7x6 has 60 million nodes, while the per-ply BDDs add up
to 620 million. Most of a per-ply BDD is "counting stones".

## Experimental results

Sum over all plies, in nodes; ratios relative to the exact BDDs:

| board | W+D+L exact | minimised, care = reachable | W | D | L |
|---|---:|---:|---:|---:|---:|
| 5x5 | 8.43 M | 6.96 M (0.83) | 0.82 | 0.91 | 0.76 |
| 6x4 | 7.59 M | 6.50 M (0.86) | 0.84 | 0.93 | 0.80 |
| 6x5 | 104.86 M | 91.14 M (0.87) | 0.87 | 0.97 | 0.79 |

At the ply that determines the memory peak:

| board | ply | W+D+L exact | minimised, care = reachable |
|---|---:|---:|---:|
| 5x5 | 19 | 0.99 M | 0.85 M (0.86) |
| 6x4 | 17 | 0.93 M | 0.83 M (0.89) |
| 6x5 | 21 | 11.28 M | 10.06 M (0.89) |

Smaller care sets, as they would apply to the stored files (the probing programs read
the win and the loss file of a ply):

| board | win + loss files today | W care reachable, L care not-won | W care non-terminal, L care not-won |
|---|---:|---:|---:|
| 5x5 | 5.68 M | 4.13 M (0.73) | 3.80 M (0.67) |
| 6x4 | 5.08 M | 3.88 M (0.76) | 3.63 M (0.71) |
| 6x5 | 74.61 M | 55.06 M (0.74) | 50.27 M (0.67) |

Union over all plies (one BDD per result instead of one per ply):

| board | sum of per-ply W | W, all plies in one BDD | largest per-ply W | reachable: sum / union |
|---|---:|---:|---:|---:|
| 5x5 | 2.75 M | 1.22 M (0.44) | 0.35 M | 1.40 M / 0.49 M |
| 6x4 | 2.50 M | 0.99 M (0.40) | 0.33 M | 0.85 M / 0.24 M |
| 6x5 | 35.65 M | 14.03 M (0.39) | 3.98 M | 11.80 M / 2.54 M |

(Drawn and lost behave the same: 0.40 and 0.39 on 6x5.)

What this says:

* **Restrict buys 11–14 % at the memory peak**, and the gain shrinks slightly as the
  board grows. That does not change which boards fit. It is not integrated into the
  solver: the solver would still have to build the exact sets to count them, so the
  peak would not even drop by the full 11 %.
* **The stored files could be 26–33 % smaller**, at the price of files that are only
  meaningful for the positions in the respective care set (a lost-file that is only
  valid for positions that are not won must be probed after the win-file, which the
  current probing code does not guarantee everywhere). Not adopted for that reason.
* **One BDD for all plies is 2.5x smaller than the per-ply BDDs together**, but 3.5x
  larger than the largest of them. So it would cut the size of a published solution by
  60 % (for 7x6: roughly 35 GB instead of 90 GB for win + loss, before compression),
  but it does not help the solver, which only ever needs two plies at a time. Building
  it means holding a 3.5x larger BDD in memory; for 7x6 that is a job for the large
  machine, as a post-processing step over the per-ply files.
* The hypothesis "the exact BDDs mostly encode which assignments are positions" is
  wrong for the result sets. For the reachable sets the union over plies is 4.6x smaller
  than the sum; for the result sets it is 2.5x, and per ply the don't-cares are worth
  little. Most of the nodes of `W`, `D`, `L` encode game-theoretic information.

Restrict is the simplest don't-care heuristic; it never merges two different sub-BDDs
that happen to agree on the care set. Stronger heuristics exist (two-sided matching,
"safe minimisation") and might find more, but the union experiment bounds what a
per-ply minimisation that still has to separate the plies' positions can plausibly
gain. I do not expect a 3x reduction from this direction.

Unit tests: `and(minimise(f, c), c) == and(f, c)` for random functions and care sets of
different density (on random functions the result is smaller in about half of the cases
and larger in a third, which is the known behaviour of the heuristic).
