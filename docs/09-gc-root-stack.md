# 09 – A root stack for garbage collection during operations

## Idea

With `IN_OP_GC=1`, a garbage collection can start in the middle of a BDD operation, when
the pool is nearly full. The recursion then holds intermediate results that no finished
BDD refers to yet: in

```c
l = apply(low branch);
h = apply(high branch);     // a GC in here must not free l
u = make(var, l, h);
```

`l` has to survive while `h` is computed. Until now every frame protected its results
with `keepalive(l)` / `undo_keepalive(l)`, i.e. by incrementing and decrementing a counter
*inside the node* `l` (and again for `h` and for `u`): six writes to three different
nodes per recursion step.

This change adds a second kind of GC root, a small array used as a stack:

```c
l = apply(low branch);   gc_stack_push(l);
h = apply(high branch);  gc_stack_push(h);
u = make(var, l, h);
gc_stack_pop(2);                 // l and h are reachable from u now
in_op_gc(op, u);                 // pushes u only if a GC actually starts
```

`gc` marks everything reachable from nodes with external references (as before) and
from the entries of the stack. The stack depth is bounded by twice the recursion depth
(at most a few thousand entries, 32 KB reserved).

`keepalive` remains the way to protect the operands of an operation and all BDDs the
solver holds on to; only the per-step protection inside the recursions changed.

## Justification

In-op GC is what makes a run possible when the table is only slightly larger than the
live data, which is the situation for 7x6 on 32 GB and was the situation for 7x6 on
128 GB. In that mode the old protocol added six node writes to every step of every
operation, whether or not a GC ever happened. The nodes involved were just returned by
a recursive call or by a cache hit, so on a large table they may not be in the CPU
cache, and the writes dirty cache lines that would otherwise only be read.

The stack replaces them with two stores into an array that stays in the L1 cache. The
aim is that `IN_OP_GC=1` costs nothing compared to `IN_OP_GC=0`, so that it can become
the normal mode and forced GCs between steps can go.

Risk: a missing push would let a GC free a node that is still needed. The unit tests run
with `IN_OP_GC=1` in a 2^20 table and hit dozens of in-op GCs while checking results
against truth tables; the regression below adds two solver runs under real pressure.

## Experimental results

Correctness under pressure (`IN_OP_GC=1`, threshold 0.9, table 2^21, `--bytes`):

| case | in-op GCs | result |
|---|---:|---|
| solve 5x5 | 11 | state counts and solution files identical to golden |
| solve 6x4 | 4 | identical |

Overhead of the protocol when no GC triggers (`IN_OP_GC=1` vs `IN_OP_GC=0`, 2^24 table):

| case | `IN_OP_GC=0` [s] | old protocol [s] | root stack [s] |
|---|---:|---:|---:|
| solve 6x4 | 12.99 | 13.08 | 13.53 |
| solve 5x5 | 16.73 | 16.75 | 17.75 |
| count 6x5 | 25.73 | 26.45 | 27.58 |

On these boards neither protocol has a measurable cost: all nodes are cache-resident, so
the extra writes of the old protocol are cheap. (The last column was measured with four
jobs running on the machine instead of three, which accounts for the 4–6 %.) The case
this change is made for, a table much larger than the caches, is measured on 6x5 with
2^26 nodes and added below.
