# 05 – RAM budget instead of powers of two

## Idea

The only size knob was `log2_tablesize`: the node pool, the bucket array and the caches
all had `2^k` (or `2^k / 4`) elements. Going from `k` to `k + 1` doubles the memory, so a
machine with 32 GB could use 2^29 nodes (the old 28 GB) or nothing bigger.

Only the *hash-indexed* arrays need a power-of-two size (the index is `hash & mask`). The
node pool is a plain array addressed by node index and can have any length. So:

* The pool capacity is an arbitrary number of nodes (up to 2^32 - 1).
* The bucket array has the largest power of two that does not exceed the capacity, i.e.
  between one and two nodes per bucket when the pool is completely full.
* The cache has `buckets >> CACHE_SHIFT` entries.

The first program argument accepts either form:

```
solve.out 28  6 6      # 2^28 nodes, as before
solve.out 12G 6 6      # as many nodes as fit into 12 GiB
solve.out 800M 5 5
```

For a budget, the largest capacity is chosen such that pool + buckets + cache fit. If the
capacity hits the 32-bit index limit, the rest of the budget goes into a larger cache.

All three arrays are now allocated with `mmap` (anonymous, zero-filled, backed lazily).
On Linux the mapping is marked `MADV_HUGEPAGE`.

`print_RAM_info` prints the real sizes (it used to over-estimate with a stale formula).

## Justification

* **Use the machine you have.** With 24 bytes per node, a power-of-two table on a 32 GB
  machine is either 2^30 nodes (24 GiB) or too big. The 7x6 solve is expected to need
  1.1–1.2 billion live nodes, just above 2^30 = 1.07 billion; a 28 GiB budget gives
  1.34 billion.
* **Bytes per node drop a little further** between powers of two, because the bucket
  array and the cache do not grow with the pool: 20–24 bytes per node.
* **Huge pages.** Every unique-table lookup and almost every node access is a random
  access into arrays of tens of GB. With 4 KiB pages each of them is also a TLB miss with
  a multi-level page walk; with 2 MiB pages the page tables for 100 GB shrink from 200 MB
  to well under 1 MB and mostly stay cached. macOS on Apple Silicon already uses 16 KiB
  pages and has no equivalent switch, so this only affects the Linux machines.

Risk: `mmap` instead of `calloc` is a portability change (POSIX only, as is the rest of
the code base, which already uses `mmap` for probing).

## Experimental results

Sizes chosen for a few budgets (`CACHE_SHIFT=2`):

| argument | allocatable nodes | buckets | cache entries | bytes per node |
|---|---:|---:|---:|---:|
| `24` | 16,777,216 | 2^24 | 2^22 | 24.0 |
| `300M` | 15,466,496 | 2^23 | 2^21 | 20.3 |
| `28G` | 1,342,177,280 | 2^30 | 2^28 | 22.4 |
| `100G` | 4,294,967,295 | 2^32 | 2^30 | 24.0 |
| `120G` | 4,294,967,295 | 2^32 | 2^31 | 28.0 |
| `250G` | 4,294,967,295 | 2^32 | 2^32 | 36.0 |

So 2^32 nodes, which needed 228 GB at the baseline, now need 103 GB; on the 256 GB
machine the spare memory turns into a 4x larger cache.

Regression suite, new case `solve 5x5` with a `150M` budget (7.4 million nodes, not a
power of two) next to the 2^24 run:

| case | time [s] | GC [s] | peak nodes | RSS |
|---|---:|---:|---:|---:|
| solve 5x5, `24` | 26.20 | 5.36 | 2,385,028 | 0.17 GB |
| solve 5x5, `150M` | 25.17 | 4.54 | 2,385,028 | 0.07 GB |

* Same results, same peak; the smaller table is slightly faster because its bucket array
  and cache (32 MB each instead of 64 MB) fit the CPU caches better.
* All 12 cases and the unit tests pass, `--bytes` included.
* **Not measured:** the effect of `MADV_HUGEPAGE`. It needs a Linux machine and a table
  much larger than the TLB reach; it should be measured on the 128/256 GB servers
  (compare with `echo never > /sys/kernel/mm/transparent_hugepage/enabled`).
