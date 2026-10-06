# 02 – Mark-and-sweep GC and a 16-byte node

## Idea

Garbage collection used reference counts: every node stored the number of its parents
(`parentcount`, 4 bytes), `make` incremented the count of both children, and `gc` walked
all nodes, recursively "disabled" those with count 0 and decremented their children.

This change replaces the parent count by a plain reachability rule:

> a node is alive if it was kept alive explicitly (`keepalive`), or if it can be reached
> from such a node through `low` / `high` edges.

* `keepalive` / `undo_keepalive` / `reassign_and_keepalive` keep their meaning and their
  call sites, but now count *external* references only (`extref`).
* `gc()` = **mark** (scan the pool for `extref > 0`, set the mark bit on everything below;
  the recursion depth is bounded by the number of variables) + **sweep** (the sequential
  rebuild pass from [01](01-intrusive-unique-table.md), keyed on the mark bit).
* `make` no longer touches the children of the node it creates.

With the parent count gone, the remaining per-node state fits into one word and the node
shrinks from 20 to 16 bytes:

```c
typedef struct BBDNode {
    nodeindex_t low, high, next;
    uint32_t var    : 10;   // was uint8_t
    uint32_t mark   : 1;    // GC
    uint32_t visit  : 1;    // scratch bit for traversals (used from change 04 on)
    uint32_t extref : 20;   // external references
} bddnode_t;
```

The variable field grows from 8 to 10 bits on the way (`variable_t` is `uint16_t`,
`MAX_VARIABLES = 1022`). The old limit of 255 variables ruled out the default encoding on
8x8-class boards (2 x (2 x 64 + 1) = 258 variables). The file format is unaffected: saved
variables are `(v + 1) / 2`, which stays below 256; the save routine checks this.

## Justification

* **Memory:** 4 bytes per node (45 -> 41), 8.6 GB at `log2 = 31`.
* **Node creation:** two read-modify-writes on the children per new node disappear. The
  children were just returned by a recursive call or by the cache, so only their index is
  known; on a table that is larger than the CPU caches, each of those writes is a
  potential cache miss.
* **Simpler liveness:** "reachable from a root" is easier to reason about than counts that
  must be kept consistent, and it is what the later changes need (dropping and reloading
  whole BDDs, a save routine that temporarily reuses `next`).
* `extref` has 20 bits. The in-op GC protocol keeps at most two references per recursion
  frame on the same node, so the count stays below a few thousand; an assert guards it.

What it costs: the price of a GC changes from "proportional to the garbage" to
"proportional to the live nodes" (every live node is visited by the mark phase, in random
order). The solver currently forces five GCs per ply while holding the reachable sets of
all plies alive, which is the worst case for that trade.

## Experimental results

Same machine and flags as before; two baseline jobs were running concurrently, so a few
percent are noise.

| case | time 01 [s] | time 02 [s] | GC 01 [s] | GC 02 [s] | non-GC 01 [s] | non-GC 02 [s] |
|---|---:|---:|---:|---:|---:|---:|
| solve 5x4 compressed | 1.62 | 1.99 | 0.44 | 0.58 | 1.18 | 1.41 |
| solve 6x4 compressed | 21.81 | 22.31 | 4.04 | 4.96 | 17.77 | 17.35 |
| solve 5x5 compressed | 27.19 | 28.22 | 5.10 | 6.60 | 22.09 | 21.62 |
| count 5x5 compressed | 3.37 | 3.59 | 0.33 | 0.30 | 3.04 | 3.29 |
| count 6x5 compressed | 47.85 | 48.03 | 3.67 | 3.36 | 44.18 | 44.67 |

* Bytes per allocatable node: **45 -> 41** (`count`: 43 -> 39).
* GC time: up 23–29 % on `solve` (the predicted cost: all plies stay alive and are
  re-marked by every forced GC), down 8–9 % on `count`.
* Time outside GC: unchanged within noise. These boards peak at 2–5 million nodes
  (30–80 MB), which fits in the M2's caches, so the saved writes cannot show here.
* Net on these boards: 0–4 % slower in total for 9 % less memory. The trade is expected
  to turn positive once the GC schedule stops forcing collections (change 12) and on
  tables that exceed the caches; the running table in [README](README.md) tracks it.
* Regression: all 11 cases pass, including `--bytes`.

### Larger tables (added after change 03)

6x5 solve and 6x6 count with 2^26 nodes, where the node pool (up to 0.6 GB in use) no
longer fits the CPU caches. Baseline vs. the state after changes 01–03:

| case | GC baseline [s] | GC after 01–03 [s] | total baseline [s] | total after 01–03 [s] |
|---|---:|---:|---:|---:|
| solve 6x5 | 79.3 | 69.8 | 466.3 | 444.0 |
| count 6x6 | 82.1 | 36.8 | 859.1 | 798.9 |

At this size the GC is faster than the reference-counting one, also for `solve`: the
sequential sweep and the absence of the disable cascade outweigh the marking of live
nodes. The slowdown seen on the small boards does not carry over.
