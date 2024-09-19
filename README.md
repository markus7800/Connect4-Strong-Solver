# Connect4 Position Counter

This project is an open-source implementation of the approach described in
```
Edelkamp, S., & Kissmann, P. (2011, August). On the complexity of BDDs for state space search: A case study in Connect Four.
In Proceedings of the AAAI Conference on Artificial Intelligence (Vol. 25, No. 1, pp. 18-23).
```
to determinte the number of unique positions in a Connect4 game of a given board size.

The number of positions for the conventional 7 x 6 board can be computed in ~3h with 14GB RAM (log2tbsize=28).

## Usage

Compile with `make` (see the `src/makefile` for compilation options):
```
cd src
make
```

Run with
```
connect4.out log2tbsize width height
```
`log2tbsize` is the parameter that controls how much RAM is used and how many BDD nodes can be allocated (`1 << log2tbsize`).
The maximum size is `log2tbsize=32` which requires a bit more than 200GB RAM.
It is recommended to set `log2tbsize` as large as possible, because it reduces hash collisions.

Garbage collection was tuned to work well with `connect4.out 29 7 6`.  
Smaller board sizes spend more time in GC than necessary.

### Queens
We also inlcude an implementation of the [n-queens puzzle](https://en.wikipedia.org/wiki/Eight_queens_puzzle).

Compile it with `make queens` and run  with `queens.out log2tbsize N`.  E.g. `queens.out 22 8` or `queens.out 27 12`.  
You may play around with trading of speed versus memory usage.

### Writing your own program

This project comes with its own [binary decision diagram](https://en.wikipedia.org/wiki/Binary_decision_diagram) library implementation.
Its focus is on pre-allocation of all BDD nodes (constant memory usage), fine control over garbage collection, and to make Connect4 work.
So it may not be the most user-friendly BDD library, however, it is very performant and may be re-used for your needs.
We do not provide documentation, but comments in the source code and studying `queens.c` and `connect.c` should help.

## Results
We were able to independently verify the numbers computed by [John Tromp](https://tromp.github.io/c4/c4.html) and produce novel counts for boards up to size `width + height = 14`.

The computation were run on a Intel(R) Xeon(R) Platinum 8358 CPU @ 2.60GHz with 238.4GB RAM.



|   height/width |   1 |       2 |             3 |              4 |                 5 |                   6 |                   7 |                   8 |                   9 |                 10 |              11 |            12 |      13 |
|----------------|-----|---------|---------------|----------------|-------------------|---------------------|---------------------|---------------------|---------------------|--------------------|-----------------|---------------|---------|
|              1 |   2 |       5 |            13 |             35 |                96 |                 267 |                 750 |               2,118 |               6,010 |             17,120 |          48,930 |       140,243 | 402,956 |
|              2 |   3 |      18 |           116 |            741 |             4,688 |              29,737 |             189,648 |           1,216,721 |           7,844,298 |         50,780,523 |     329,842,064 | 2,148,495,091 |     N/A |
|              3 |   4 |      58 |           869 |         12,031 |           158,911 |           2,087,325 |          27,441,956 |         362,940,958 |       4,816,325,017 |     64,137,689,503 | 856,653,299,180 |           N/A |     N/A |
|              4 |   5 |     179 |         6,000 |        161,029 |         3,945,711 |          94,910,577 |       2,265,792,710 |      54,233,186,631 |   1,295,362,125,552 | 30,932,968,221,097 |             N/A |           N/A |     N/A |
|              5 |   6 |     537 |        38,310 |      1,706,255 |        69,763,700 |       2,818,972,642 |     112,829,665,923 |   4,499,431,376,127 | 178,942,601,291,926 |                N/A |             N/A |           N/A |     N/A |
|              6 |   7 |   1,571 |       235,781 |     15,835,683 |     1,044,334,437 |      69,173,028,785 |   4,531,985,219,092 | 290,433,534,225,566 |                 N/A |                N/A |             N/A |           N/A |     N/A |
|              7 |   8 |   4,587 |     1,417,322 |    135,385,909 |    14,171,315,454 |   1,523,281,696,228 | 161,965,120,344,045 |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |
|              8 |   9 |  13,343 |     8,424,616 |  1,104,642,469 |   182,795,971,462 | >19,483,954,541,313 |                 N/A |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |
|              9 |  10 |  38,943 |    49,867,996 |  8,754,703,921 | 2,284,654,770,108 |                 N/A |                 N/A |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |
|             10 |  11 | 113,835 |   294,664,010 | 67,916,896,758 |               N/A |                 N/A |                 N/A |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |
|             11 |  12 | 333,745 | 1,741,288,730 |            N/A |               N/A |                 N/A |                 N/A |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |
|             12 |  13 | 980,684 |           N/A |            N/A |               N/A |                 N/A |                 N/A |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |
|             13 |  14 |     N/A |           N/A |            N/A |               N/A |                 N/A |                 N/A |                 N/A |                 N/A |                N/A |             N/A |           N/A |     N/A |

### Counts per ply

By default, we only construct one BDD per ply (layer).
We record the number of positions at a given ply and the number of nodes of the BDD encoding these positions.
These numbers can be found in the results folder.

In the 7 x 6 case, our approximately numbers match Edelkamp, S., & Kissmann, P.

We also implemented the compilation option `FULLBDD=1` which computes the OR over the BDDs of each ply.
This encodes all Connect4 positions.
For the 7 x 6 position this BDD has 95124612 nodes in our implementation.

This is a bit much more than the 84,088,763 number of nodes which Edelkamp, S., & Kissmann, P. claimed in 
```
Edelkamp, S., & Kissmann, P. (2008). Symbolic classification of general two-player games.
In KI 2008: Advances in Artificial Intelligence: 31st Annual German Conference on AI, KI 2008, Kaiserslautern, Germany, September 23-26, 2008. Proceedings 31 (pp. 185-192). Springer Berlin Heidelberg.
```
Of course this number depends on the variable ordering which Edelkamp, S., & Kissmann, P. did not publish.

### Experiment information
|   width |   height |          #positions |          comp.time |   GC perc. |   RAM used |   #allocatable |   perc. allocated |
|---------|----------|---------------------|--------------------|------------|------------|----------------|-------------------|
|       1 |        1 |                   2 |         0:00:00.72 |     26.56% |     6.85GB |           2^27 |             0.00% |
|       1 |        2 |                   3 |         0:00:00.83 |     23.48% |     6.85GB |           2^27 |             0.00% |
|       1 |        3 |                   4 |         0:00:00.89 |     20.98% |     6.85GB |           2^27 |             0.00% |
|       1 |        4 |                   5 |         0:00:01.02 |     19.58% |     6.85GB |           2^27 |             0.00% |
|       1 |        5 |                   6 |         0:00:01.09 |     17.50% |     6.85GB |           2^27 |             0.00% |
|       1 |        6 |                   7 |         0:00:01.20 |     16.64% |     6.85GB |           2^27 |             0.00% |
|       1 |        7 |                   8 |         0:00:01.29 |     15.23% |     6.85GB |           2^27 |             0.01% |
|       1 |        8 |                   9 |         0:00:01.41 |     14.71% |     6.85GB |           2^27 |             0.01% |
|       1 |        9 |                  10 |         0:00:01.90 |     31.18% |     6.85GB |           2^27 |             0.01% |
|       1 |       10 |                  11 |         0:00:02.36 |     41.42% |     6.85GB |           2^27 |             0.02% |
|       1 |       11 |                  12 |         0:00:02.89 |     48.74% |     6.85GB |           2^27 |             0.02% |
|       1 |       12 |                  13 |         0:00:13.43 |     53.53% |    27.38GB |           2^29 |             0.01% |
|       1 |       13 |                  14 |         0:00:30.30 |     56.54% |    54.76GB |           2^30 |             0.00% |
|       2 |        1 |                   5 |         0:00:00.84 |     24.46% |     6.85GB |           2^27 |             0.00% |
|       2 |        2 |                  18 |         0:00:01.01 |     19.54% |     6.85GB |           2^27 |             0.00% |
|       2 |        3 |                  58 |         0:00:01.22 |     16.75% |     6.85GB |           2^27 |             0.00% |
|       2 |        4 |                 179 |         0:00:01.39 |     14.52% |     6.85GB |           2^27 |             0.01% |
|       2 |        5 |                 537 |         0:00:02.46 |     42.83% |     6.85GB |           2^27 |             0.02% |
|       2 |        6 |               1,571 |         0:00:03.45 |     54.15% |     6.85GB |           2^27 |             0.03% |
|       2 |        7 |               4,587 |         0:00:04.42 |     59.60% |     6.85GB |           2^27 |             0.05% |
|       2 |        8 |              13,343 |         0:00:05.43 |     63.11% |     6.85GB |           2^27 |             0.07% |
|       2 |        9 |              38,943 |         0:00:06.54 |     65.75% |     6.85GB |           2^27 |             0.10% |
|       2 |       10 |             113,835 |         0:00:07.62 |     66.71% |     6.85GB |           2^27 |             0.14% |
|       2 |       11 |             333,745 |         0:00:33.03 |     68.85% |    27.38GB |           2^29 |             0.05% |
|       2 |       12 |             980,684 |         0:01:27.56 |     73.46% |    54.76GB |           2^30 |             0.03% |
|       3 |        1 |                  13 |         0:00:00.92 |     21.63% |     6.85GB |           2^27 |             0.00% |
|       3 |        2 |                 116 |         0:00:01.20 |     16.30% |     6.85GB |           2^27 |             0.00% |
|       3 |        3 |                 869 |         0:00:01.72 |     23.58% |     6.85GB |           2^27 |             0.01% |
|       3 |        4 |               6,000 |         0:00:03.44 |     54.38% |     6.85GB |           2^27 |             0.03% |
|       3 |        5 |              38,310 |         0:00:04.98 |     61.56% |     6.85GB |           2^27 |             0.06% |
|       3 |        6 |             235,781 |         0:00:06.62 |     65.62% |     6.85GB |           2^27 |             0.10% |
|       3 |        7 |           1,417,322 |         0:00:08.46 |     67.27% |     6.85GB |           2^27 |             0.16% |
|       3 |        8 |           8,424,616 |         0:00:10.81 |     66.21% |     6.85GB |           2^27 |             0.24% |
|       3 |        9 |          49,867,996 |         0:00:15.53 |     62.51% |     6.85GB |           2^27 |             0.34% |
|       3 |       10 |         294,664,010 |         0:01:09.23 |     67.26% |    27.38GB |           2^29 |             0.15% |
|       3 |       11 |       1,741,288,730 |         0:03:27.85 |     65.28% |    54.76GB |           2^30 |             0.20% |
|       4 |        1 |                  35 |         0:00:01.04 |     19.95% |     6.85GB |           2^27 |             0.00% |
|       4 |        2 |                 741 |         0:00:01.42 |     14.75% |     6.85GB |           2^27 |             0.01% |
|       4 |        3 |              12,031 |         0:00:03.43 |     53.43% |     6.85GB |           2^27 |             0.03% |
|       4 |        4 |             161,029 |         0:00:10.39 |     80.18% |     6.85GB |           2^27 |             0.07% |
|       4 |        5 |           1,706,255 |         0:00:15.52 |     80.85% |     6.85GB |           2^27 |             0.14% |
|       4 |        6 |          15,835,683 |         0:00:23.76 |     72.88% |     6.85GB |           2^27 |             0.38% |
|       4 |        7 |         135,385,909 |         0:00:52.93 |     48.00% |     6.85GB |           2^27 |             1.73% |
|       4 |        8 |       1,104,642,469 |         0:02:59.47 |     22.63% |     6.85GB |           2^27 |             7.69% |
|       4 |        9 |       8,754,703,921 |         0:16:35.88 |     20.45% |    27.38GB |           2^29 |             8.51% |
|       4 |       10 |      67,916,896,758 |         1:34:04.22 |     12.20% |    54.76GB |           2^30 |            18.22% |
|       5 |        1 |                  96 |         0:00:01.11 |     17.83% |     6.85GB |           2^27 |             0.00% |
|       5 |        2 |               4,688 |         0:00:02.35 |     41.67% |     6.85GB |           2^27 |             0.02% |
|       5 |        3 |             158,911 |         0:00:04.85 |     61.35% |     6.85GB |           2^27 |             0.06% |
|       5 |        4 |           3,945,711 |         0:00:14.95 |     79.93% |     6.85GB |           2^27 |             0.14% |
|       5 |        5 |          69,763,700 |         0:00:31.90 |     62.51% |     6.85GB |           2^27 |             0.92% |
|       5 |        6 |       1,044,334,437 |         0:02:17.40 |     26.13% |     6.85GB |           2^27 |             6.47% |
|       5 |        7 |      14,171,315,454 |         0:17:25.51 |     10.75% |     6.85GB |           2^27 |            38.27% |
|       5 |        8 |     182,795,971,462 |         2:27:07.62 |      8.57% |    27.38GB |           2^29 |            53.90% |
|       5 |        9 |   2,284,654,770,108 |        20:22:22.74 |      7.46% |   109.52GB |           2^31 |            75.79% |
|       6 |        1 |                 267 |         0:00:01.18 |     16.47% |     6.85GB |           2^27 |             0.00% |
|       6 |        2 |              29,737 |         0:00:03.38 |     52.86% |     6.85GB |           2^27 |             0.03% |
|       6 |        3 |           2,087,325 |         0:00:06.37 |     64.58% |     6.85GB |           2^27 |             0.10% |
|       6 |        4 |          94,910,577 |         0:00:25.59 |     69.86% |     6.85GB |           2^27 |             0.53% |
|       6 |        5 |       2,818,972,642 |         0:02:17.36 |     27.09% |     6.85GB |           2^27 |             5.85% |
|       6 |        6 |      69,173,028,785 |         0:26:43.80 |      9.86% |     6.85GB |           2^27 |            45.92% |
|       6 |        7 |   1,523,281,696,228 |         5:22:58.83 |      7.32% |    27.38GB |           2^29 |            78.38% |
|       7 |        1 |                 750 |         0:00:01.29 |     15.49% |     6.85GB |           2^27 |             0.01% |
|       7 |        2 |             189,648 |         0:00:04.35 |     59.34% |     6.85GB |           2^27 |             0.05% |
|       7 |        3 |          27,441,956 |         0:00:08.25 |     64.90% |     6.85GB |           2^27 |             0.16% |
|       7 |        4 |       2,265,792,710 |         0:00:56.41 |     49.08% |     6.85GB |           2^27 |             1.80% |
|       7 |        5 |     112,829,665,923 |         0:12:38.91 |     13.70% |     6.85GB |           2^27 |            20.56% |
|       7 |        6 |   4,531,985,219,092 |         3:18:08.08 |      9.02% |    27.38GB |           2^29 |            44.43% |
|       7 |        7 | 161,965,120,344,045 | 1 day, 16:01:33.98 |      8.58% |   219.04GB |           2^32 |            41.85% |
|       8 |        1 |               2,118 |         0:00:01.38 |     14.32% |     6.85GB |           2^27 |             0.01% |
|       8 |        2 |           1,216,721 |         0:00:05.35 |     62.93% |     6.85GB |           2^27 |             0.07% |
|       8 |        3 |         362,940,958 |         0:00:10.96 |     62.02% |     6.85GB |           2^27 |             0.24% |
|       8 |        4 |      54,233,186,631 |         0:02:35.89 |     30.48% |     6.85GB |           2^27 |             4.34% |
|       8 |        5 |   4,499,431,376,127 |         0:55:18.68 |     14.90% |    27.38GB |           2^29 |            13.06% |
|       8 |        6 | 290,433,534,225,566 |        16:17:53.60 |      7.68% |    54.76GB |           2^30 |            58.28% |
|       9 |        1 |               6,010 |         0:00:01.85 |     31.51% |     6.85GB |           2^27 |             0.01% |
|       9 |        2 |           7,844,298 |         0:00:06.47 |     65.23% |     6.85GB |           2^27 |             0.10% |
|       9 |        3 |       4,816,325,017 |         0:00:14.57 |     54.69% |     6.85GB |           2^27 |             0.36% |
|       9 |        4 |   1,295,362,125,552 |         0:09:39.57 |     39.01% |    27.38GB |           2^29 |             2.12% |
|       9 |        5 | 178,942,601,291,926 |         3:02:56.78 |     12.90% |    54.76GB |           2^30 |            15.23% |
|      10 |        1 |              17,120 |         0:00:02.40 |     42.29% |     6.85GB |           2^27 |             0.02% |
|      10 |        2 |          50,780,523 |         0:00:07.39 |     65.70% |     6.85GB |           2^27 |             0.14% |
|      10 |        3 |      64,137,689,503 |         0:00:56.79 |     62.29% |    27.38GB |           2^29 |             0.16% |
|      10 |        4 |  30,932,968,221,097 |         0:26:36.90 |     37.84% |    54.76GB |           2^30 |             2.27% |
|      11 |        1 |              48,930 |         0:00:02.87 |     48.79% |     6.85GB |           2^27 |             0.02% |
|      11 |        2 |         329,842,064 |         0:00:32.49 |     68.31% |    27.38GB |           2^29 |             0.05% |
|      11 |        3 |     856,653,299,180 |         0:02:11.86 |     62.01% |    54.76GB |           2^30 |             0.16% |
|      12 |        1 |             140,243 |         0:00:12.99 |     52.72% |    27.38GB |           2^29 |             0.01% |
|      12 |        2 |       2,148,495,091 |         0:01:10.72 |     69.04% |    54.76GB |           2^30 |             0.03% |
|      13 |        1 |             402,956 |         0:00:31.19 |     56.95% |    54.76GB |           2^30 |             0.00% |
