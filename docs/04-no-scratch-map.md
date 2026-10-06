# 04 – No scratch maps for counting and saving

## Idea

Two auxiliary hash maps were allocated for the whole run, sized relative to the node
table:

| structure | used for | size | bytes per node |
|---|---|---|---:|
| `nodeindexmap_t` (`solve`) | node count of a BDD, node index -> file index while saving | N/2 entries of 12 B + N/2 buckets | 8 |
| `uniquetable_t` as a set (`count`) | node count of a BDD | N/2 entries of 8 B + N/2 buckets | 6 |

Both jobs can be done with state that already exists in the nodes.

* **Node count.** A depth-first traversal sets the `visit` bit of every node it reaches and
  counts them; a second traversal clears the bits. `nodecount(ix)` needs no memory and
  no size argument.
* **Save.** The file format numbers nodes by their position in the file, children before
  parents, so writing a node needs the file positions of its two children. During the
  post-order traversal the position of a written node is stored in its own `next` field
  (`visit` marks nodes that are done). `next` is the unique-table chain link, so the
  chains are destroyed while saving; `rebuild_uniquetable()` restores them afterwards with
  the same sequential pass GC uses for sweeping. Nothing calls `make` in between.
* **Load.** File positions are dense (0 .. n-1), so `_read_from_file` maps them through a
  plain `uint32_t` array of the file's length (4 bytes per node of *that BDD*, freed
  afterwards) instead of a hash map. A variant with a variable map exists for reading
  files that were written with one; the solver will use it to reload sets from disk.

`nodeindexmap.{c,h}` and the generic set functions of `uniquetable.c` are removed.
`tests/bdd_test.c` is new: it checks the operations against truth tables, GC against node
counts, and that save -> read -> save reproduces both the node and the file.

## Justification

* **Memory:** 8 bytes per allocatable node in `solve` (32 -> 24), 6 in `count` (30 -> 24).
  More importantly the cost no longer scales with the *table*: the old map had to be
  big enough for the largest BDD that would ever be saved, and it sat there the whole
  run. With the 7x6 solve on 32 GB in mind, a 450-million-node BDD must be saved while
  three such BDDs are alive; a hash map for it (several GB) would not fit, zero extra
  bytes do.
* **Time:** the old save reset the whole map (`8 x N` bytes written) and did two hash
  lookups per node. The new one does no hashing; its fixed cost is one pass over the
  used part of the node pool for the rebuild, which the old code paid in a similar form
  for the reset.

Risk: a `make` between the start of a save and the rebuild would see broken chains. Save
is a leaf routine that calls no BDD operation, and the rebuild is inside it, so the
window is closed by construction. The file contents depend only on traversal order,
which is unchanged (low before high, node after its children).

## Experimental results

| case | 03: time / GC [s] | 04: time / GC [s] | RSS 03 | RSS 04 |
|---|---:|---:|---:|---:|
| solve 6x4 (2^24) | 21.18 / 3.81 | 21.08 / 3.82 | 0.28 GB | 0.16 GB |
| solve 5x5 (2^24) | 27.42 / 5.45 | 26.58 / 5.46 | 0.29 GB | 0.17 GB |
| count 6x5 (2^24) | 52.94 / 2.81 | 51.86 / 2.80 | 0.30 GB | 0.20 GB |

* Bytes per allocatable node: **32 -> 24** for `solve`, 30 -> 24 for `count`. This is the
  end of the layout work on the fixed structures: 16 (node) + 4 (bucket) + 4 (cache).
  Relative to the baseline of 53 the same RAM now holds 2.2x as many nodes.
* Time is unchanged within noise on these boards (saving and counting are a few percent
  of the run, see the profile in [00](00-baseline.md)).
* Regression: all 11 cases pass including `--bytes`; unit tests pass.
