# 01 – Intrusive unique table

## Idea

The unique table maps `(var, low, high)` to the node with these fields. It used to be a
separate chained hash map next to the node pool:

```
buckets[h]  ->  entries[i] = {next, value}  ->  nodes[value] = {var, parentcount, low, high, next}
   4 B              8 B                              20 B
```

`nodes[].next` was only used while a node sat in the free list; for an allocated node it
was a dead field. This change uses it as the bucket chain link instead, which makes the
`entries` array redundant:

```
buckets[h]  ->  nodes[i] = {var, parentcount, low, high, next}  ->  nodes[next] ...
   4 B                          20 B
```

* A bucket is the index of its first node; `0` terminates a chain. Index 0 is the constant
  `false`, which is never looked up through the table, so it is free to act as "nil". The
  two constants are no longer stored in buckets at all.
* The free list uses the same field and the same terminator.
* The pool gets a high-water mark `hwm`: indices `>= hwm` were never handed out. Allocation
  takes from the free list first and otherwise bumps `hwm`. Pool and buckets come from
  `calloc`, so memory that is never touched is never backed by the OS.
* GC no longer walks `entries`. It walks `nodes[2 .. hwm)` sequentially: first the existing
  recursive disable pass, then one pass from high to low index that puts every live node
  back into its bucket and threads every dead node into the free list. Going downwards
  leaves both kinds of list sorted by ascending index, so nodes freed by a GC are reused
  in address order. Dead nodes at the top of the pool lower `hwm` instead.

The reference-counting scheme (`parentcount`, `keepalive`) is unchanged here; that is the
next step.

## Justification

* **Memory:** 8 bytes per allocatable node disappear (53 -> 45). At `log2 = 31` that is
  17 GB.
* **Lookups:** `make` follows bucket -> node instead of bucket -> entry -> node, one
  dependent cache miss less per chain element.
* **GC:** the sweep reads the node array in order instead of chasing `entries[i].value`
  into it, and the bucket reset is a `memset`.
* **Start-up and small boards:** the old `init_memorypool` / `init_set` wrote every node,
  bucket and entry, so RSS was the full table size from the first second. With `calloc`
  plus the high-water mark, RSS follows the number of nodes actually used.

Risk: the save routine and the node-count set still have their own maps, so nothing else
depended on `entries`. The canonical-form argument from [00](00-baseline.md) applies:
node indices change, the saved files must not.

## Experimental results

Same machine and flags as the baseline. Timings were taken while the 6x6 baseline solve
was running on another core, so differences of a few percent are noise.

| case | time before [s] | time after [s] | GC before [s] | GC after [s] | RSS before | RSS after |
|---|---:|---:|---:|---:|---:|---:|
| solve 5x4 compressed (2^22) | 1.62 | 1.62 | 0.45 | 0.44 | 0.21 GB | 0.10 GB |
| solve 6x4 compressed (2^24) | 21.56 | 21.81 | 4.61 | 4.04 | 0.83 GB | 0.43 GB |
| solve 5x5 compressed (2^24) | 27.03 | 27.19 | 6.07 | 5.10 | 0.83 GB | 0.44 GB |
| count 5x5 compressed (2^22) | 3.43 | 3.37 | 0.39 | 0.33 | 0.20 GB | 0.11 GB |
| count 6x5 compressed (2^24) | 52.03 | 47.85 | 5.29 | 3.67 | 0.80 GB | 0.46 GB |

* Bytes per allocatable node: **53 -> 45** (`count`: 51 -> 43).
* GC time drops by 12–30 %; total time is unchanged within noise on these boards, where
  the node pool still fits in the CPU caches and the saved cache miss does not show.
* Peak node counts are identical, as they must be.
* Regression: all 11 cases pass, including `--bytes` (solution files bit-identical).
