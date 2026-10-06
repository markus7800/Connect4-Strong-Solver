# 11 – Build the four-in-a-line condition once

## Idea

A position is terminal if the player who just moved has four stones in a line. To split
a set of positions into terminal and non-terminal ones, the code looped over all lines
(69 on a 7x6 board), built the BDD `a` of "these four cells belong to the player", and
applied it to the set right away:

```
for each line:   current = current \ a          (subtract)
for each line:   term    = term ∪ (current ∧ a) (intersect, a second loop in the solve)
```

That is 69 (or 138) operations over the full `current` BDD per ply, each producing a
new intermediate BDD of the size of `current`, with forced GCs in between.

The condition itself, `four(player) = ∨ a_line`, does not depend on `current` or on the
ply. It is now built once per player at start-up (`search/terminal.c`), and the split is

```
non-terminal = current \ four(player)
terminal     = current ∧ four(player)
```

`four(player)` is assembled per direction (columns, rows, both diagonals). The four parts
are merged into one BDD if the union stays below 1/64 of the node capacity; otherwise
they are applied one after the other (four operations instead of 69).

The per-encoding code shrinks to one function, `cell_of_player`, and the 300 duplicated
lines of line loops in the two encoding files are gone.

## Justification

* **Time:** one pass over `current` instead of 69. After [06](06-terminal-rules-and-diff.md)
  the profile was dominated by exactly these passes.
* **Memory:** the 69 intermediate versions of `current` were garbage that had to be
  collected inside the loop (`gc_level` 1 and 2 existed for this). A single `diff`
  produces the result and nothing else.
* **Is the condition BDD small enough?** Edelkamp and Kissmann showed that the BDD of the
  termination criterion grows exponentially with the board, which is why one might
  hesitate to build it. For the boards in reach it is tiny compared to the position
  sets (measured below): under a million nodes up to 7x7.

Risk: none for correctness beyond the usual; the result is the same set, so files stay
bit-identical.

## Experimental results

Size of `four(player 0)` in nodes (player 1 is within 15 %):

| board | encoding | column | row | diagonal / | diagonal \ | merged |
|---|---|---:|---:|---:|---:|---:|
| 5x5 | compressed | 52 | 2,860 | 161 | 116 | 8,265 |
| 6x5 | compressed | 62 | 8,170 | 434 | 229 | 19,789 |
| 6x6 | compressed | 92 | 32,903 | 1,815 | 861 | 105,638 |
| 7x6 | compressed | 107 | 62,939 | 4,512 | 1,341 | 185,052 |
| 7x6 | default | 86 | 47,566 | 2,644 | 808 | 138,017 |
| 8x6 | compressed | 122 | 92,975 | 7,209 | 1,821 | 264,466 |
| 7x7 | compressed | 142 | 268,752 | 20,549 | 5,354 | 973,080 |
| 7x7 | default | 114 | 208,362 | 13,196 | 3,416 | 740,950 |

The merged BDD grows by about 1.4x per column and 5x per row. For comparison, the
largest reachable-set BDD of 7x6 has 52 million nodes.

Timing:

| case | 10: time / GC [s] | 11: time / GC [s] | speed-up |
|---|---:|---:|---:|
| solve 5x4 compressed | 0.95 / 0.31 | 0.65 / 0.11 | 1.5x |
| solve 5x4 default | 1.33 / 0.45 | 1.02 / 0.18 | 1.3x |
| solve 6x4 compressed | 11.67 / 3.44 | 8.36 / 1.23 | 1.4x |
| solve 5x5 compressed | 14.99 / 4.80 | 9.97 / 1.74 | 1.5x |
| count 5x5 compressed | 1.60 / 0.19 | 0.94 / 0.05 | 1.7x |
| count 5x5 default | 2.33 / 0.27 | 1.26 / 0.07 | 1.8x |
| count 6x5 compressed | 23.75 / 2.69 | 10.42 / 0.42 | 2.3x |

* Part of the gain is GC time: the forced collections inside the line loop are gone.
* The peak number of allocated nodes is unchanged for `solve` and 15–25 % higher for
  `count` (4.73 M instead of 4.06 M on 6x5). This is a sampling effect: the peak is
  recorded when a GC runs, and there are fewer GCs between which garbage can be
  observed. The number of live nodes did not change.
* Compared to the baseline: `solve 5x5` 27.0 s -> 10.0 s, `count 6x5` 52.0 s -> 10.4 s.
* Regression: all 12 cases pass including `--bytes`.
