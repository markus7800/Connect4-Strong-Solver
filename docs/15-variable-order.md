# 15 – Variable order experiments

## Idea

The size of a BDD depends on the order of its variables. The compressed encoding orders
them by column from left to right and, within a column, by row from bottom to top (the
alternative that existed before is row-major order for boards that are higher than
wide). Two more degrees of freedom are easy to try because the solver addresses cells
logically (`X[col][row]`) and only the creation order of the variables changes:

* `COL_ORDER`: `0` left to right, `1` centre column first and then outwards
  (7 columns: 3, 4, 2, 5, 1, 6, 0), `2` outer columns first and then inwards;
* `ROW_DIR`: `0` bottom up, `1` top down.

A solution written with a non-default order is not readable by the probing programs,
which assume the default bit layout. The options are meant for experiments.

## Justification

Variable order is the classic lever for BDD size and costs nothing at run time. For
Connect4 the central columns take part in the most winning lines, which suggested
testing them first; within a column, the "top of the stack" marker of the compressed
encoding could plausibly be better placed before the stones than after them.

## Experimental results

All runs reproduce the correct win / draw / loss counts. Nodes of the result BDDs,
summed over all plies, relative to the default order:

| board | centre out | outside in | top down | centre out + top down |
|---|---:|---:|---:|---:|
| 5x4 | 0.68 | | | |
| 5x5 | 0.64 | 1.08 | 1.07 | 0.63 |
| 5x6 | 0.59 | | | |
| 4x6 | 0.92 | | | |
| 6x4 | 0.94 | 1.72 | 1.07 | 0.96 |
| 6x5 | 0.96 | 2.25 | 1.09 | 0.99 |
| 7x4 | **1.26** | | | |

Largest per-ply `W + D + L` (what determines the memory peak) and run time:

| board | default: nodes / time | centre out: nodes / time |
|---|---:|---:|
| 5x5 | 0.99 M / 7.8 s | 0.62 M / 4.2 s |
| 5x6 | 7.39 M / 72.4 s | 4.27 M / 38.5 s |
| 6x5 | 11.28 M / 120.0 s | 10.89 M / 105.5 s |
| 7x4 | 6.70 M / 60.6 s | 8.44 M / 72.2 s |

* **Top down** is 7–9 % worse than bottom up.
* **Outside in** is up to 2.25x worse.
* **Centre out** depends on the width: 36–41 % smaller for 5 columns, 4–8 % smaller for
  4 and 6 columns, 26 % *larger* for 7 columns.

The pattern has a simple explanation. A horizontal or diagonal line spans four adjacent
columns, and a BDD is small when variables that interact are close in the order. Left
to right keeps every such window within four consecutive columns, which is the minimum.
Centre out only wins on narrow boards, where the middle columns belong to (almost)
every window: with 5 columns, columns 1–3 are in both windows, and putting them first
helps. With 7 columns only column 3 is in every window, and the windows at the edges
get spread across the whole order (columns 0–3 sit at positions 6, 4, 2, 0).

**Conclusion:** for the boards this work aims at (7 or more columns), the existing
left-to-right, bottom-up order stays. The options remain available; `COL_ORDER=1` is
worth using for 5-column boards if the solution does not have to be probed.
