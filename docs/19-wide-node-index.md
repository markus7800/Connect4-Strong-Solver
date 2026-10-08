# 19 – 40-bit node indices (optional), and what larger boards would take

## Idea

A node index had 32 bits, so a table could hold at most 4.29 billion nodes (4.08 billion
in the compact layout): 103 GB in the default layout, 49 GB in the compact one. More RAM
than that could only be spent on the cache.

`make ... COMPACT_NODES=1 INDEX_BITS=40` builds with 40-bit indices:

```c
typedef uint64_t nodeindex_t;           // 40 bits used

typedef struct BBDNode {                // 16 bytes
    uint64_t low : 40, var : 10, mark : 1, visit : 1, overflow : 1;
    uint64_t high : 40, extref : 24;
} bddnode_t;

typedef struct __attribute__((packed)) OpCacheEntry {   // 20 bytes
    uint64_t a : 40, b : 40, result : 40;
    uint32_t tag;
    uint8_t unused;
} opcache_entry_t;
```

* It is built on the compact layout ([18](18-compact-node-layout.md)) only. A 40-bit
  version of the default layout would need a third 40-bit field for the chain link, a
  20-byte node plus a 5-byte bucket, about 25 bytes per node before the cache. The
  compact layout needs two fields, which fit into 16 bytes together with all flags.
* Hashing uses a 64-bit hash and a 128-bit multiplication to pick the home blocks,
  because there can be more than 2^32 blocks.
* The largest index value (`NODEINDEX_NONE`) replaces the hard-coded `(uint32_t) -1`
  as the "freed" marker.
* **Files.** Positions inside a BDD file are still 4 bytes as long as the BDD has fewer
  than 2^32 nodes, so such files stay readable by the probing programs. A larger BDD is
  written with 5-byte positions (11 bytes per node) to `<name>.wide`. The reader accepts
  both; the probing programs do not read `.wide` files yet.
* The op cache is still indexed with 32 bits (at most 2^32 entries, 86 GB).

Everything else is unchanged: with `INDEX_BITS=32` (the default) the compiled code is
the same as before.

## Justification and drawbacks

The option exists to use machines with more memory than the 32-bit layouts can address.
It has costs, which is why it is a compile-time option and not the default:

| | default | compact | compact, 40 bit |
|---|---:|---:|---:|
| node / slot | 16 B + 4 B bucket | 12 B | 16 B |
| usable fraction | ~1.0 | 0.95 | 0.95 |
| cache entry | 16 B | 16 B | 20 B |
| bytes per usable node (cache 1/8 of the table) | 22 | 14.6 | 19.3 |
| largest table | 4.29 G nodes (103 GB) | 4.08 G (49 GB) | 1.04 T (any machine) |

* **33 % more memory per node** than the 32-bit compact layout, so it only pays for
  tables beyond 4.08 G nodes.
* **Speed** is that of the compact layout: nodes are scattered by hash, 1.3x slower
  than the default layout in a tight table (measured on 6x6), about 2x in a sparse one.
  The 40-bit fields themselves cost nothing measurable: on the small suite the 40-bit
  build ran within 3 % of the 32-bit compact build (measured on a loaded machine, so
  only the ratio means anything).
* **Solution files of BDDs with 2^32 or more nodes** are in a new format that the probing
  programs cannot read yet.
* **It cannot be tested at its intended scale here.** What was tested:
  * the unit tests and the whole regression suite for the 40-bit build (all solution
    BDDs identical to the golden ones), with the wide file format forced for every BDD
    above 1000 nodes in the unit tests;
  * `tests/wide_index_test.c` (`python3 scripts/regress.py --wide`): a table with 2^33
    slots (128 GB of address space, 3.6 GB touched), in which half of the nodes get an
    index above 2^32; operations against truth tables, `image_rename`, satcount, save
    and read.
  Not tested: garbage collection, and any run at all, in a table that is actually larger
  than 2^32 nodes. GC does not depend on the index width, but the first large run
  should be one with a known result.

## What larger boards would take

All numbers below are extrapolations from the measurements in this repository; expect
errors of a factor of 1.5 for the next size and more beyond that. Runtimes are single
threaded on hardware like the M2 Pro.

### Counting positions

The memory rule is 1.5 x the largest reachable-set BDD ([16](16-sizing-ladder.md)). That
BDD grew by these factors:

| one more row | width 5 | width 6 | width 9 |
|---|---:|---:|---:|
| height 3 -> 4 | | | 29 |
| height 4 -> 5 | 10.5 | 11.9 | 13.2 |
| height 5 -> 6 | 7.3 | 8.4 | 9.3 |

| one more column at height 6 | 4 -> 5 | 5 -> 6 | 6 -> 7 | 7 -> 8 | 8 -> 9 |
|---|---:|---:|---:|---:|---:|
| | 18.0 | 8.1 | 4.4 | 2.8 | 2.2 |

Both factors shrink as the board grows; I continue the row factor with about 7 (6 -> 7),
5.5 (7 -> 8) and 4.8 (8 -> 9). Run time is taken as the sum of all reachable-set BDDs
times 2–4 µs per node (7x6: 1.0 µs, 8x6: 1.4 µs in the default layout, rising with table
size and in the compact layout).

| count | largest BDD | nodes needed | RAM | layout | run time |
|---|---:|---:|---:|---|---:|
| 7x6 (measured) | 0.052 G | 0.078 G | 4 GB | default | 10 min |
| 8x6 (measured) | 0.147 G | 0.22 G | 10 GB | default | 49 min |
| 7x7 | ~0.34 G | ~0.5 G | ~12 GB | default | ~3 h |
| 8x7 | ~1.0 G | ~1.5 G | ~35 GB | default | ~10 h |
| 9x7 | ~2.3 G (> 1.39 G is known) | ~3.4 G | ~80 GB | default, 32 bit | ~1.5 days |
| **8x8** | ~5.5 G (4–7.5) | ~8 G (6–11) | **~160 GB (120–220)** | compact, 40 bit | **~4 days (2.5–7)** |
| 9x8 | ~13 G (9–17) | ~19 G | ~380 GB (270–510) | compact, 40 bit | ~2 weeks |
| **9x9** | ~60 G (35–90) | ~90 G | **~1.8 TB (1–2.7)** | compact, 40 bit | **~2–5 months** |

Two further requirements for the last two rows: the totals exceed 2^64 from 9x8 on
(9x6 has 1.9e16 positions, each row multiplies by about 65–70), so the counters need
128 bits (the program currently stops with an overflow message); and a run of months
needs checkpointing, which does not exist.

### Strong solutions

The memory rule is 1.06–1.08 x the largest per-ply `W + D + L` (7x6 measured: 1.234 G
nodes alive for 1.142 G). Unlike the reachable sets, the result BDDs grow by about
10x per column *and* per row:

| largest per-ply `W + D + L` | one more column | one more row |
|---|---:|---:|
| 5x4 -> 6x4, 5x4 -> 5x5 | 8.5 | 9.1 |
| 5x5 -> 6x5, 5x5 -> 5x6 | 11.4 | 7.5 |
| 6x4 -> 7x4, 6x4 -> 6x5 | 7.2 | 12.1 |
| 6x6 -> 7x6, 6x5 -> 6x6 | 11.5 | 8.8 |

Run time: the 7x6 solve took 67,917 s for 13.06 G result nodes in total, 5.2 µs per
node (in a table that was 96 % full; 3–4 µs in the plies without memory pressure).

| solve | largest `W + D + L` | nodes needed | RAM | run time | disk (win + loss) |
|---|---:|---:|---:|---:|---:|
| 6x6 (measured) | 0.100 G | 0.105 G | 2.5 GB | 29–47 min | 6 GB |
| 7x6 (measured) | 1.14 G | 1.23 G | 24 GiB | 18.9 h | 90 GB |
| **7x7** | ~10 G (9–11.5) | ~11 G | **~210 GB (190–250)** | **~10–14 days** | ~1 TB |
| 8x6 | ~11.5 G (10–12.5) | ~12.5 G | ~240 GB (210–270) | ~10–14 days | ~1 TB |
| 8x7 | ~110 G | ~120 G | ~2.3 TB | ~6 months | ~10 TB |
| **8x8** | ~900 G (500–1500) | ~1000 G | **~19 TB (10–30)** | **~4 years** | ~80 TB |
| **9x9** | ~55,000 G | | **~1 PB** | centuries | |

### Reading the tables

* With 40-bit indices the 256 GB machine comes within reach of **counting 8x8** and,
  just, of **solving 7x7** (8x6 is more likely to need a little more than 256 GB). These
  are the two results the option is for. Both are days to two weeks of single-threaded
  run time.
* **Counting 9x9** is a 1–3 TB, multi-month computation: possible on a rented
  large-memory machine, but it needs parallel operations and checkpointing to be
  practical, and 128-bit counters.
* **Solving 8x8 or 9x9** is not a matter of index width or of RAM that can be bought.
  The result BDDs grow tenfold per row and per column; 8x8 needs about 800 times the
  nodes of 7x6. Neither don't-care minimisation ([14](14-dont-care-minimisation.md))
  nor variable order ([15](15-variable-order.md)) changes that by more than a few
  percent. A strong solution of 8x8 would need a representation other than one BDD
  per result and ply, or a disk-based computation over tens of terabytes.
