# 10 – One set of board variables: image with renaming

## Idea

A move relates two positions, so the transition relation `T(x, x')` needs two copies of
the board variables: `x` (board0) for the position before the move and `x'` (board1) for
the position after it. The standard image

```
S'(x') = ∃x. S(x) ∧ T(x, x')
```

produces the successors in the *primed* variables. Renaming them back costs a traversal,
which the original code avoided by alternating: the set of ply `i` lived in board0, ply
`i + 1` in board1, ply `i + 2` in board0 again, with two relations `trans0`, `trans1`.

The catch is that a set and its image never share a node, even though a Connect4 move
changes one column and leaves the rest of the board alone. Every image rebuilds the
whole BDD in the other variable set.

The new library operation `image_rename(S, T, vars)` computes the image and the renaming
in one recursion. It relies on the variable order the encodings already use, where each
primed variable is the direct successor of its unprimed partner:

| top variable of the pair `(S, T)` | action |
|---|---|
| unprimed `x`, in `vars` | quantify: `or(rec(S|x=0, T|x=0), rec(S|x=1, T|x=1))` |
| primed `x'` (only `T` depends on it) | `make(x, rec(S, T|x'=0), rec(S, T|x'=1))` (the node is labelled with the unprimed variable) |

Since `x` is quantified away exactly where `x'` takes its place, the order of the result
is consistent. All sets now live in board0 variables; `trans0` gives successors and
`trans1` (the same relation with the roles of the boards swapped) gives predecessors.

**Identity shortcut.** The relation contains frame conditions `x = x'` for every cell a
move does not touch. Once the recursion has passed the column of the move, the rest of
`T` is exactly "all remaining pairs are equal", and the image of `S` under the identity
is `S`. `prepare_image_rename` builds these identity suffixes once (one node per
variable), and the recursion returns the current sub-BDD of `S` unchanged when it meets
one. So below the moved column the result *is* the corresponding part of `S`: no
recursion, no new nodes.

The alternative from the literature, a disjunctive partition `∨_col image(S, T_col)`, is
available as `PARTITIONED_IMAGE=1` for comparison.

## Justification

* **Sharing.** Successor sets, predecessor sets and the sets they were computed from now
  share every sub-BDD below the changed column. With alternating boards a pre-image of
  the lost positions (the largest BDDs in the solve) was a full second copy.
* **Work.** The recursion only has to descend through the columns up to the move; with
  the monolithic relation and two boards it walked the entire BDD of `S` for every image.
* **Less to keep in sync.** No more "is this ply in board0 or board1" in the solver, and
  the special `board0_board1_or` for the full BDD in `count` is a plain `or` now.

Why not partition by column? Partitioning pays off when the monolithic relation is too
big to build or when quantification can be scheduled early. Here the relation is tiny
(a few thousand nodes) and each partition's image is as large as the result, so the
union over columns adds full-size intermediate BDDs.

Risk: the operation assumes a relation that does not let primed variables float where
`S` still has variables above (otherwise the shortcut would be wrong); the shortcut
checks the level condition explicitly. `tests/bdd_test.c` compares `image_rename` against
`exists(and(S, T))` followed by a naive renaming, as nodes, for transition-like relations
with frames and for arbitrary relations.

## Experimental results

Solution files are bit-identical: the sets are the same functions, only encoded in
board0 variables throughout, and the save routine already mapped both boards to the same
file variables.

| case | 09: time [s] | 10: time [s] | partitioned [s] | 09: peak nodes | 10: peak nodes | partitioned |
|---|---:|---:|---:|---:|---:|---:|
| solve 5x4 compressed | 1.05 | 0.95 | 1.11 | 0.27 M | 0.22 M | 0.30 M |
| solve 5x4 default | 1.63 | 1.33 | 1.76 | 0.47 M | 0.41 M | 0.61 M |
| solve 6x4 compressed | 12.99 | 11.67 | 15.03 | 2.16 M | 1.75 M | 2.61 M |
| solve 5x5 compressed | 16.73 | 14.99 | 17.93 | 2.39 M | 2.07 M | 2.94 M |
| count 5x5 compressed | 1.78 | 1.60 | 2.07 | 0.84 M | 0.58 M | 1.13 M |
| count 5x5 default | 2.73 | 2.33 | 2.76 | 1.23 M | 0.88 M | 1.63 M |
| count 6x5 compressed | 25.73 | 23.75 | 29.49 | 5.21 M | 4.06 M | 7.74 M |

* 8–18 % faster and 13–31 % fewer allocated nodes at the peak, for both encodings and
  both variable orders. The time gain is bounded by the share of the image in the run
  (8 % in the baseline profile); most of the remaining time is the terminal test, which
  is next.
* The column partition is correct but worse on both axes: 15–25 % slower and 40–90 %
  more nodes at the peak than the single relation, and worse than the two-board code it
  would have replaced. It stays in the code as a switch but is not used.
* Regression: all 12 cases pass including `--bytes`, for both variants; unit tests pass.
