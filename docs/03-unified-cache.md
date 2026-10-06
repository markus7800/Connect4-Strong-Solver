# 03 – One operation cache

## Idea

The library had three caches, each with N/4 entries (N = number of allocatable nodes):

| cache | entry | used by | bytes per node |
|---|---|---|---:|
| unary | `{arg, op, result (64 bit)}` = 16 B | `not`, satcount | 4 |
| binary | `{arg1, arg2, op, result}` = 16 B | `and`, `or`, `iff`, `exists` | 4 |
| ternary | `{arg1, arg2, arg3, op, result}` = 20 B | `image` | 5 |

They are replaced by a single cache of 16-byte entries:

```c
typedef struct OpCacheEntry {
    nodeindex_t a, b;     // operands
    nodeindex_t result;
    uint32_t tag;         // bits 0-7: operation (0 = empty), bits 8-31: extra key
} opcache_entry_t;
```

* `image(S, T, varset)` keeps its third operand in the key through the tag: every
  `variable_set_t` gets a small id, and `tag = IMAGE_OP | id << 8`.
* Counting operations (`OP_COUNT_FLAG` in the operation number) use `(tag, a)` as key and
  store their 64-bit result in the `b` and `result` fields.
* Operation numbers are now global (`USER_OP`, `USER_COUNT_OP` for the Connect4-specific
  ones); before, `CONNECT4_SAT_OP` and `IMAGE_OP` were both 7 and only worked because
  they lived in different caches.
* The cache size is decoupled from the node capacity: `2^(log2_tablesize - CACHE_SHIFT)`
  entries, `CACHE_SHIFT = 2` by default (`make ... CACHE_SHIFT=1` doubles it).
* The cache comes from `calloc` and is cleared with `memset`; an all-zero entry is empty.

**Cache survival at GC.** GC used to wipe all caches, because a cached index might belong
to a node that was just freed. With mark-and-sweep it is cheap to be precise instead:
between mark and sweep, an entry is dropped only if one of its nodes is unmarked. This is
controlled by `GC_KEEPS_CACHE`:

| value | behaviour |
|---|---|
| 0 | always clear |
| 1 (default) | keep valid entries when the GC was triggered *inside* an operation (`IN_OP_GC`), clear otherwise |
| 2 | always keep valid entries |

## Justification

* **Memory:** 13 -> 4 bytes per allocatable node (41 -> 32 in total). The three caches
  were never busy at the same time: while an image runs, the binary cache only serves
  the inner `or`; the unary cache is idle outside `not` and satcount.
* **Why keep entries only for in-op GC:** filtering costs a pass over the cache with up to
  three random node reads per entry. When the solver calls `gc` between two unrelated
  steps, the old entries are rarely asked for again and the pass is wasted. When the GC
  interrupts a running operation, the cache holds exactly the sub-results that operation
  will ask for next; losing them is what made `IN_OP_GC` so slow.

Risk: one cache means the operations now evict each other. Whether N/4 entries are
enough at scale is an empirical question (see below and the running table).

## Experimental results

Small suite, same machine, table sizes as before. Three other jobs were running, so the
absolute numbers are noisier than in the previous steps.

| case | 02: time / GC [s] | policy 0 (clear) | policy 2 (always keep) | policy 1 (default) |
|---|---:|---:|---:|---:|
| solve 6x4 | 22.31 / 4.96 | 20.67 / 3.69 | 25.25 / 8.41 | 21.18 / 3.81 |
| solve 5x5 | 28.22 / 6.60 | 26.16 / 5.24 | 30.53 / 10.69 | 27.42 / 5.45 |
| count 6x5 | 48.03 / 3.36 | 49.36 / 2.63 | 49.17 / 6.37 | 52.94 / 2.81 |

(Policy 1 equals policy 0 here because these runs have no in-op GC; the difference
between the two columns is run-to-run noise.)

* Bytes per allocatable node: **41 -> 32** (`count`: 39 -> 30). RSS at 2^24: 0.43 -> 0.29 GB.
* One cache instead of three does not cost time on these boards; clearing became cheaper
  (one `memset`), which shows as lower GC time.
* Always keeping entries (policy 2) saves about 8 % of the time outside GC on `solve 5x5`
  (19.8 s vs 20.9 s) but doubles the GC time, a net loss with the current schedule of
  five forced GCs per ply. Hence the default of keeping entries only for in-op GC.
* With `CACHE_SHIFT=1` (twice the cache) nothing improves at this size.
* In-op GC on a board this small (5x5 at 2^21, 11 in-op GCs) is too rare to separate the
  policies: 25.2 s (keep) vs 24.4 s (clear).
* Regression: all 11 cases pass, including `--bytes`, for policies 0, 1 and 2.

The measurements that matter for this change need a table larger than the CPU caches and
real memory pressure: 6x5 at 2^26 (does a single N/4 cache hold up?) and 6x5 at 2^24 with
`IN_OP_GC` (does keeping entries help when GC interrupts operations?). Both are appended
below when the runs finish.
