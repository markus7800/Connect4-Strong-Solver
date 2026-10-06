# 06 – Terminal rules for apply, and a difference operation

## Idea

`apply_binary(f, g, op)` recursed until *both* operands were constants. For `and`, `or`
and `iff` the result is already known when only one operand is a constant, or when both
operands are the same node:

| operation | terminal cases |
|---|---|
| `and(f, g)` | `0` if `f = 0` or `g = 0`; `g` if `f = 1`; `f` if `g = 1` or `f = g` |
| `or(f, g)` | `1` if `f = 1` or `g = 1`; `g` if `f = 0`; `f` if `g = 0` or `f = g` |
| `iff(f, g)` | `1` if `f = g`; `g` if `f = 1`; `f` if `g = 1` |
| `diff(f, g)` | `0` if `f = 0` or `g = 1` or `f = g`; `f` if `g = 0` |

All checks compare node indices only, before any node is dereferenced.

Three related changes:

* The operands of the commutative operations are ordered by index before the cache
  lookup, so `and(f, g)` and `and(g, f)` share one cache entry.
* `image(S, T)` returns `0` as soon as either operand is `0`.
* New operation `diff(f, g) = f ∧ ¬g`. The solver wrote `and(f, not(g))`, which first
  builds a complete negated copy of `g`. `diff` recurses over the pair directly. It
  replaces all four subtraction sites in each encoding and the two in `solve.c`
  (`current \ win`, `current \ draw`).

## Justification

A BDD operation costs one cache probe and usually one `make` per visited *pair* of
nodes. Without terminal rules, `and(f, 1)` visits every node of `f` and rebuilds it node
by node through the unique table, only to arrive at `f` again. Such pairs are not rare
corner cases; they are the bulk of the work in this program:

* the reachable-set BDDs are sparse (most paths lead to `0`), so the recursion of
  `and(current, ...)` keeps running into `(subgraph, 0)`;
* the terminal test intersects `current` with 69 small "four in a line" BDDs (7x6). Each
  of them constrains a handful of variables; everywhere else it is `1` and the recursion
  degenerates into copying `current` onto itself;
* the `or` inside `image` (existential quantification) frequently meets `(f, 0)`.

`diff` saves the `not`: for `current \ win` that was a traversal and a full copy of the
largest BDD of the ply. The copy is garbage immediately afterwards, so it also inflated
the number of allocated nodes between GCs.

Risk: wrong terminal rules give wrong results silently. `tests/bdd_test.c` now checks
`and`, `or`, `iff`, `not` and `diff` of random functions against explicit truth tables,
all identities above, and `diff(f, g) == and(f, not(g))` as nodes.

## Experimental results

| case | 05: time [s] | terminal rules [s] | + `diff` [s] | speed-up |
|---|---:|---:|---:|---:|
| solve 5x4 compressed | 1.65 | 1.07 | 1.09 | 1.5x |
| solve 5x4 default | 2.90 | 1.89 | 1.78 | 1.6x |
| solve 6x4 compressed | 20.97 | 14.66 | 14.02 | 1.5x |
| solve 5x5 compressed | 26.20 | 18.40 | 18.10 | 1.45x |
| count 5x5 compressed | 3.47 | 2.09 | 2.18 | 1.6x |
| count 5x5 default | 5.37 | 2.92 | 2.92 | 1.8x |
| count 6x5 compressed | 51.42 | 27.97 | 27.91 | 1.8x |

* GC time is unchanged (5.4 s of the 18.1 s for `solve 5x5`), so the time outside GC
  dropped from 20.8 s to 12.6 s (1.65x) for `solve` and from 48.6 s to 25.1 s (1.9x) for
  `count 6x5`.
* Almost all of it comes from the terminal rules. `diff` adds 2–4 % on `solve`: the
  negated copies of `win` and `draw` are a small part of the run on these boards, and
  the 69 line BDDs are tiny. Its value is in memory (no transient copy of the largest
  BDD) rather than time.
* Peak node counts at the GC sample points are unchanged.
* Regression: all 12 cases pass including `--bytes`; unit tests pass.
