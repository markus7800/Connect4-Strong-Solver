# 18 – Optional compact node layout: 12-byte nodes that are their own hash table

## Idea

In the default layout a node costs 16 bytes plus 4 bytes of bucket array, and four of the
sixteen bytes (`next`) only serve the unique table. `make ... COMPACT_NODES=1` selects a
second layout without `next` and without bucket array:

```c
typedef struct BBDNode {      // 12 bytes
    nodeindex_t low, high;
    uint32_t var : 10, mark : 1, visit : 1, overflow : 1, extref : 19;
} bddnode_t;
```

The node array itself is the hash table. It is divided into blocks of five slots (60
bytes, about one cache line). A node `(var, low, high)` has two *home blocks*, given by
two hash values, and is stored in the one with more free slots at the time it is
created ("two-choice hashing"). `make` searches the two home blocks; a slot is free if
`low == high`, which no real node has.

* If both home blocks are full, the node goes into the next block after its first home
  block that has room, and an `overflow` bit is set in the blocks it skipped. A lookup
  only continues past its home blocks while it sees that bit.
* A node never moves, so its index (the slot number) stays valid, as the rest of the
  library requires.
* Freeing a node just marks its slot free. There are no chains to repair. GC clears and
  recomputes the overflow bits in the rare case that any were set.
* There is no free list and no allocation order: where a node lives is decided by its
  hash.

Three things had to change with it:

* **Saving.** The default save routine keeps the file position of a written node in
  `next`. The compact layout flags the nodes of the BDD and writes them in index order
  (root last); the file position of a node is then its rank among the flagged nodes,
  answered by a bitmap with running counts (1.5 bits per slot, freed afterwards). The
  nodes therefore appear in a different order in the file. The reader no longer relies
  on "children before parents" and builds the BDD recursively from the root, for files
  of either layout.
* **The cache key of `image_rename_and`.** It contained the index of the relation node in
  24 bits, on the assumption that relations are built first and get small indices. With
  hashed placement that is false. The nodes of a relation are now numbered when the
  relation is first used (and the relation is pinned), and the number goes into the key.
* **Usable load.** Two-choice hashing stays fast up to a very high load but not to 100 %:
  a simulation gives 3 % overflowing inserts at 90 % load, 5.5 % at 95 %, with chains
  that grow quickly beyond that. The layout uses at most 95 % of the slots.

The default layout is unchanged and remains the default.

## Justification

The live nodes are the part of the memory that no setting can shrink
([17](17-7x6-on-32gb.md)). Per node that can be alive at the peak:

| | default | compact |
|---|---:|---:|
| node | 16 B | 12 B |
| unique table | 4 B (2 B with `BUCKET_SHIFT=1`) | – |
| usable fraction of the pool | ~1.0 | 0.95 |
| **bytes per usable node** | **20 (18)** | **12.6** |

Why two-choice blocks rather than linear probing: with linear probing a freed slot
cannot simply be reused as "empty" (lookups of other nodes pass over it), and since
nodes cannot be moved, the clusters formed while the table was full would stay forever.
With two fixed home blocks a lookup never depends on other slots, except through the
explicit overflow bit.

What it costs, by design:

* **Locality.** The default layout hands out nodes in address order, so nodes created
  together sit together. Here every node is at a random position: every node access
  is a cache miss as soon as the table exceeds the CPU cache, and the whole array is
  touched from the start, even if only a few nodes are alive.
* **GC** scans the whole array (there is no high-water mark).
* **Index range.** Indices number slots, so 32 bits address at most 0.95 x 2^32 = 4.08 G
  nodes instead of 4.29 G. On a machine where the index and not the RAM is the limit
  (the 256 GB machine), the default layout holds more.

## Experimental results

**Correctness.** The unit tests run for both layouts. The regression suite passes with
`COMPACT_NODES=1`; since the node order in the files differs, `scripts/regress.py --bytes`
renumbers the nodes of each file in the canonical order and compares that with the
golden hashes: all solution BDDs are identical.

**Small boards, large table** (2^24 nodes, the table is 5–10 % full):

| case | default [s] | compact [s] |
|---|---:|---:|
| solve 6x4 | 7.09 | 13.03 |
| solve 5x5 | 7.67 | 15.12 |
| count 6x5 | 9.79 | 18.46 |

About 2x slower: this is the locality cost in its purest form. The compact layout is
the wrong choice when memory is plentiful.

**Small boards, smallest budget that completes:**

| case | default | compact |
|---|---|---|
| solve 6x4 | `20M`, 6.5 s | `14M`, 8.0 s |
| solve 5x5 | `26M`, 6.4 s | `20M`, 8.6 s |

**6x6 solve in a tight table:**

| layout | budget | slots / nodes | max alive | time | RSS |
|---|---|---:|---:|---:|---:|
| default | 2.5 GiB (`2500M`) | 130.3 M | 105.4 M | 2845 s | 2.47 GB |
| compact | 1.75 GiB (`1800M`) | 134.2 M | 105.7 M | 3723 s | 2.12 GB |

The same computation in 30 % less table memory, 1.3x slower. (The first attempt at this
run failed at ply 26 with the pool full: that was the `image_rename_and` cache key
described above, which made the operation fall back to the unrestricted pre-image. It
did not show on the small boards, whose tables are smaller than 2^24 slots.)

**What it means for 7x6 on the 32 GB Mac.** 1.21 G live nodes need at least 1.27 G slots;
with some headroom:

| layout | command | resident memory |
|---|---|---:|
| default | `make solve BUCKET_SHIFT=1 CACHE_SHIFT=3`, `solve.out 24G 7 6` | 25.8 GB |
| compact | `make solve COMPACT_NODES=1 CACHE_SHIFT=3`, `solve.out 18G 7 6` | 19.3 GB |
| compact | same build, `solve.out 20G 7 6` | 21.5 GB |

The compact layout is what makes the run fit into the roughly 21 GB that are available
next to a desktop session; with the applications closed the default layout fits too and
should be faster. Neither 7x6 run had been completed when this was written.
