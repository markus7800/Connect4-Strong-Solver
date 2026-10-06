# 07 – Hash function

## Idea

Unique table and cache both hash a small number (variable or operation) and two node
indices. The old function combined them with Cantor's pairing function and reduced the
result modulo the prime 2^31 - 1:

```c
pair = (l + h) * (l + h + 1) / 2 + l;
pair = (pair + a) * (pair + a + 1) / 2 + pair;
hash = pair % 0x7FFFFFFF;
```

The new one concatenates the two indices to a 64-bit word and mixes it with two
multiplications and a xor-shift; the upper 32 bits of the product are the hash:

```c
x  = (uint64_t) b << 32 | c;
x ^= a * 0x9E3779B97F4A7C15;
x *= 0xFF51AFD7ED558CCD;
x ^= x >> 32;
x *= 0xC4CEB9FE1A85EC53;
hash = x >> 32;
```

It lives in `node.h` as `static inline`, so it is inlined without relying on LTO.

A compile-time switch `-DHASH_STATS` prints, at every GC, the distribution of bucket
sizes of the unique table next to the ideal values for a uniformly random hash.

## Justification

Two concerns with the old function, one confirmed and one not:

1. **Range (confirmed by construction).** `x % (2^31 - 1)` is always below 2^31. A table
   with 2^32 buckets (`log2 = 32`) therefore only ever used its lower half: twice the
   chain length for the same number of nodes, and half of a 17 GB array wasted. The same
   holds for an op cache with more than 2^31 entries, which the 256 GB machine can now
   have ([05](05-memory-budget.md)). The new hash produces all 32-bit values.
2. **Distribution (not confirmed).** Cantor pairing is a polynomial in the indices, and
   node indices are far from random (children are often allocated next to each other),
   so clustering seemed possible. The measurement below shows there was none.

Cost per call: the old version is three multiplications plus the modulo (compiled to a
multiply-and-shift sequence, not a division); the new one is three multiplications and
two shifts.

## Experimental results

Bucket statistics of the unique table at its fullest point (`-DHASH_STATS`):

| run | nodes / buckets | avg probes to find a node, old | new | ideal | longest chain, old | new |
|---|---:|---:|---:|---:|---:|---:|
| solve 5x5 at 2^21 (in-op GC) | 0.9999 | 1.5000–1.5007 | 1.5004 | 1.4999 | 9 | 9–10 |
| solve 6x4 at 2^22 | 0.5147 | 1.2569 | 1.2579 | 1.2574 | 7 | 7 |

Both functions are indistinguishable from a random hash at this size. The old one was
not the bottleneck I suspected; the range limit is its only real defect.

Timing (small suite):

| case | 06 [s] | 07 [s] |
|---|---:|---:|
| solve 6x4 | 14.02 | 13.35 |
| solve 5x5 | 18.10 | 17.43 |
| count 5x5 | 2.18 | 1.96 |
| count 6x5 | 27.91 | 26.93 |

* 3–5 % faster across the board, consistent in sign but close to the measurement noise.
* The benefit that motivated the change, full use of 2^32 buckets, only materialises
  at `log2 = 32` and could not be measured on this machine. With `n` nodes in the table,
  the average number of probes for a successful lookup goes from `1 + n / 2^32` (half
  the buckets unused) to `1 + n / 2^33`, i.e. from 2.0 to 1.5 for a full table.
* A unit test checks that every bit of the hash is set about half of the time.
* Regression: all 12 cases pass including `--bytes`.
