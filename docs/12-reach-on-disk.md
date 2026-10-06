# 12 – Reachable sets wait on disk

## Idea

The solve has two passes. The forward pass computes the BDD of all reachable positions
for every ply (`width x height + 1` BDDs). The retrograde pass then walks from the last
ply to the first and, for each ply, classifies those positions into won, drawn and lost.

The forward pass kept all its BDDs alive in memory (`bdd_per_ply[]`) until the
retrograde pass reached them. But the retrograde pass needs the reachable set of ply `p`
only while it works on ply `p`, long after the forward pass is done.

Now the forward pass writes each reachable set to `reach_w{W}_h{H}/` with the ordinary
save routine and keeps only the current one in memory. The retrograde pass reads the set
of its ply back, checks its node count against the one recorded when it was written, and
deletes the file (`-DKEEP_REACH=1` keeps them).

A new statistic supports this and the following changes: `GC max number of nodes alive
after gc`, the largest number of nodes that survived a garbage collection. Unlike the
existing "max allocated nodes" it does not count garbage, so it shows what the
computation actually needs; it is sampled whenever a GC runs.

## Justification

* For 7x6 the 43 reachable-set BDDs add up to about 0.6 billion nodes (table in the
  README: the "nodes (total)" column), with little sharing between plies. During the
  retrograde pass of the middle plies, where the won/drawn/lost BDDs are largest, about
  half of them are still waiting. That is 20–30 % of the node budget doing nothing.
* A mark-and-sweep GC visits every live node. BDDs that are merely waiting were re-marked
  by every collection.
* The cost is I/O that is small next to the run: 9 bytes per node written once and read
  once, about 6 GB in total for 7x6.

Risk: reading must reproduce the same BDD. The file is canonical and `make` rebuilds
canonical nodes, so the result is the same function; the node count check catches a
truncated or mixed-up file.

## Experimental results

| case | 11: time / GC [s] | 12: time / GC [s] | peak allocated 11 | 12 | max alive after GC 11 | 12 |
|---|---:|---:|---:|---:|---:|---:|
| solve 5x4 compressed | 0.65 / 0.11 | 0.52 / 0.07 | 0.22 M | 0.20 M | 0.13 M | 0.11 M |
| solve 6x4 compressed | 8.36 / 1.23 | 7.09 / 0.86 | 1.76 M | 1.69 M | 0.84 M | 0.76 M |
| solve 5x5 compressed | 9.97 / 1.74 | 8.94 / 1.11 | 2.08 M | 1.89 M | 1.28 M | 1.02 M |

* 10–20 % fewer live nodes at the peak and about 10 % less time (mostly GC: fewer nodes
  to mark).
* The effect is larger on bigger boards, where the middle plies dominate; see the 6x5
  and 6x6 rows of the [running table](README.md).
* `count` is unaffected (it never kept old plies).
* Regression: all 12 cases pass including `--bytes`.
