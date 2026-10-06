# 08 – Build flags and small hot-path fixes

## Idea

Four small, independent clean-ups of the inner loops.

1. **No dereference on a cache hit.** Every cache hit was followed by
   `isdisabled(get_node(result))`: load the result node just to check that it had not been
   freed. Nodes are only freed inside `gc`, and `gc` removes every cache entry that refers
   to a freed node (or clears the cache), so the check can never fail. A hit now returns
   the index straight from the cache entry.
2. **Integer threshold for in-op GC.** With `IN_OP_GC=1`, `in_op_gc` runs after every
   node creation and computed `(double) num_nodes / capacity > IN_OP_GC_THRES` each time.
   The threshold is now converted to a node count once and compared as an integer.
3. **Release flags.** `solve` and `count` are built with `-DNDEBUG` and
   `-mcpu=native` / `-march=native`. `make solve DEBUG=1` keeps the asserts. The probing
   programs keep their flags (some of them use `assert` for real error handling).
4. **Checks that must survive `-DNDEBUG`** are now explicit (`BDD_CHECK`): the
   `win + draw + loss == total` invariant of every ply, allocation failures, the
   external-reference counter, and, new, **overflow in the position counts**.

On the last point: `satcount` multiplied with `1 << k`, where `1` is a 32-bit `int`, and
summed in `uint64_t` without checks. The published counts are far from the limit
(9x6: 1.9e16 of 1.8e19), but each added row multiplies the count by roughly 50–70, so a
9x8-class board would wrap around silently. Counting now uses `add_count` /
`shift_count`, which stop the program with a message instead.

## Justification

* Item 1 removes a memory access to a node the operation did not otherwise need. On a
  table that fits the CPU caches this is a few cycles; on a table of tens of GB it is a
  cache miss (and a TLB miss) per cache hit, in the most frequently executed path of
  the library.
* Item 3: the asserts in `make()` read the levels of both children of every node that
  is looked up, which costs the same kind of access.
* Items 2 and 4 are about not paying for, or not silently losing, something on every
  call.

## Experimental results

| case | 07 [s] | 08 [s] | 08 with asserts (`DEBUG=1`) [s] |
|---|---:|---:|---:|
| solve 6x4 | 13.35 | 12.99 | 13.01 |
| solve 5x5 | 17.43 | 16.73 | 16.90 |
| count 5x5 | 1.96 | 1.78 | |
| count 6x5 | 26.93 | 25.73 | 26.53 |

* 3–4 % faster in total. Asserts on or off makes no measurable difference on these
  boards (at most 2–5 million nodes, everything is cache-resident), so the gain here
  comes from the cache-hit path.
* The argument for both items is about cache misses on large tables, which this suite
  does not exercise; the 6x5 / 6x6 numbers in the [running table](README.md) are the
  relevant measurement.
* The overflow checks cost nothing measurable (counting is 2–4 % of the run).
* Regression: all 12 cases pass including `--bytes`, with and without asserts.
