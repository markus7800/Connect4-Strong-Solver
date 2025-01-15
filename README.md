# Connect4 Strong Solver

This project is an open-source implementation of the symbolic search approaches described in
```
Edelkamp, S., & Kissmann, P. (2011, August). On the complexity of BDDs for state space search: A case study in Connect Four.
In Proceedings of the AAAI Conference on Artificial Intelligence (Vol. 25, No. 1, pp. 18-23).
```
and
```
Edelkamp, S., & Kissmann, P. (2008). Symbolic classification of general two-player games.
In KI 2008: Advances in Artificial Intelligence: 31st Annual German Conference on AI, KI 2008, Kaiserslautern, Germany, September 23-26, 2008. Proceedings 31 (pp. 185-192). Springer Berlin Heidelberg.
```
to count the number of unique positions and to produce a strong solution of a Connect4 game of a given board size, respectively.

The number of positions for the conventional 7 x 6 board can be computed in ~3h with 16 GB RAM.

The strong solution of the 7 x 6 instance takes ~47 hours and 128 GB RAM.
It is published on [Zenodo](https://zenodo.org/uploads/14582823).

## Overview

### Binary Decision Diagrams Library

In `src/bdd`, you can find a simple library to work with [binary decision diagrams](https://en.wikipedia.org/wiki/Binary_decision_diagram).  
It is designed to give the user great control over memory.  
The user specifies the number of all allocatable BDD nodes with the `log2_tablesize` parameter.  
For instance, `log2_tablesize=28` fits into 16 GB RAM and allows you to allocate 2^28 = 268,435,456 nodes simultaniously.  
If you increase, `log2_tablesize` by one, it doubles the required RAM and number of allocatable nodes.  
The maximum number is  `log2_tablesize=32`.

The user has to manually keep track of BDD node references and manually perform garbage collection to free and reuse nodes, see [src/bdd/uniquetable.h]().

### Queens
To demonstrate the usage of the library, we also include an implementation of the [n-queens puzzle](https://en.wikipedia.org/wiki/Eight_queens_puzzle).

Go to its directory, `cd src/queens`, and compile it with `make`.  
Run with `queens.out log2_tablesize N`, where `N` is the board size.
E.g. `queens.out 22 8` or `queens.out 27 12`.  
You may play around with trading of speed versus memory usage. E.g. `queens.out 24 12`.


### Connect4

Go to `src/connect4`.

#### Counting number of unique positions
Compile with `make count`.
Options:
- `COMPRESSED_ENCODING`: whether to use compressed encoding (default: `1`)
- `ALLOW_ROW_ORDER`: whether to sort variables by row instead of column if `heigth > width` (default: `0`)
- `FULLBDD`: whether to compute BDD that represents all unique positions (= union of positions per ply, default: `0`)
- `SUBTRACT_TERM`: whether to use Connect4 termination criterion (default: `1`)
- `ENTER_TO_CONTINUE`: whether to require to press enter to start computation (default: `0`)
- `WRITE_TO_FILE`: whether to write results to `.csv` file (default: `1`)
- `IN_OP_GC`: whether to allow automatic garbage collection during BDD operations (default: `1`)
- `IN_OP_GC_THRES`: fill-level of node pool above which garbage collection is triggerd if `IN_OP_GC=1` (default: `0.9999`)
- `IN_OP_GC_EXCL`: whether to exclusively perform automatic garbage collection instead of at manually set points (default: `0`)

Run with `./build/counts.out log2_tablesize width height`.

For more information about the encoding see ??.
Generally, if you have enough memory available, `log2_tablesize` should be chosen such that the maximum fill-level of the node pool does not exceed 50 %.
Otherwise, performance degrades.
If the garbage collection manually set program points is not enough, then you may encounter a `uniquetable too small` error, i.e. there are no more nodes available to allocate.
In this case you may try to set `IN_OP_GC=1`.
Then, if the fill level exceeds `IN_OP_GC_THRES` during a BDD operation, garbage collection will be triggered.
This wrecks performance, because all caches are also cleared, which are heavily used during operations.
For `IN_OP_GC=1` (and gariage collection in general) to work, the program has to be written in a way such that all BDD nodes that are used later on will be kept alive with reference counting, i.e. `keep_alive(node)`.
Lastly, setting `FULLBDD=1` increases computation and memory usage slighlty, because a union over all the BDD encoding the position at each ply is iteratively performed and kept alive.
Set only if you are intersted in the number of nodes required to encode all positions, not just the position counts.

Some examples to demonstrate flags:

- `make count COMPRESSED_ENCODING=1 FULLBDD=0 IN_OP_GC=0`
    - `./build/count.out 29 7 6`: uses <32 GB RAM, fill-level: 28.50 %, finishes in 6290.382 seconds.
    - `./build/count.out 28 7 6`: uses <16 GB RAM, fill-level: 56.99 %, finsihes in 6879.484 seconds.

- `make count COMPRESSED_ENCODING=1 FULLBDD=0 IN_OP_GC=1`
    - `./build/count.out 27 7 6`: uses <8 GB RAM, IN_OP_GC triggerd 3 times, finishes 10816.522 seconds.

- `make count COMPRESSED_ENCODING=0 FULLBDD=0 IN_OP_GC=0`
    - `./build/count.out 29 7 6` uses <32 GB RAM, fill-level: .
    - `./build/count.out 28 7 6` uses <16 GB RAM, fill-level: 88.86 %, finishes in 11715.221 seconds.

- `make count COMPRESSED_ENCODING=1 FULLBDD=1 IN_OP_GC=0`
    - `./build/count.out 29 7 6` uses <32 GB RAM, fill-level: 48.21 %, finishes in 8021.748 seconds.

- `make count COMPRESSED_ENCODING=0 FULLBDD=1 IN_OP_GC=0`
    - `./build/count.out 29 7 6` uses <32 GB RAM, fill-level: 75.40 %, finishes in 12807.400 seconds.

- `make count COMPRESSED_ENCODING=1 FULLBDD=1 IN_OP_GC=0 SUBTRACT_TERM=0`
    - `./build/count.out 29 7 6` uses <32 GB RAM, fill-level: 0.17 %, finishes in 67.398 seconds.

## Scripts to Reproduce Paper

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


### Writing your own program

This project comes with its own [binary decision diagram](https://en.wikipedia.org/wiki/Binary_decision_diagram) library implementation.
Its focus is on pre-allocation of all BDD nodes (constant memory usage), fine control over garbage collection, and to make Connect4 work.
So it may not be the most user-friendly BDD library, however, it is very performant and may be re-used for your needs.
We do not provide documentation, but comments in the source code and studying `queens.c` and `connect.c` should help.

## Results
We were able to independently verify the numbers computed by [John Tromp](https://tromp.github.io/c4/c4.html) and produce novel counts for boards up to size `width + height = 14`.

The computation were run on a Intel(R) Xeon(R) Platinum 8358 CPU @ 2.60GHz with 238.4GB RAM.


|   height\width |   1 |         2 |              3 |                  4 |                 5 |                  6 |                   7 |                   8 |                   9 |                 10 |                  11 |                     12 |                      13 |
|---------------:|----:|----------:|---------------:|-------------------:|------------------:|-------------------:|--------------------:|--------------------:|--------------------:|-------------------:|--------------------:|-----------------------:|------------------------:|
|              1 |   2 |         5 |             13 |                 35 |                96 |                267 |                 750 |               2,118 |               6,010 |             17,120 |              48,930 |                140,243 |                 402,956 |
|              2 |   3 |        18 |            116 |                741 |             4,688 |             29,737 |             189,648 |           1,216,721 |           7,844,298 |         50,780,523 |         329,842,064 |          2,148,495,091 |          14,027,829,516 |
|              3 |   4 |        58 |            869 |             12,031 |           158,911 |          2,087,325 |          27,441,956 |         362,940,958 |       4,816,325,017 |     64,137,689,503 |     856,653,299,180 |     11,470,572,730,124 |     153,906,772,806,519 |
|              4 |   5 |       179 |          6,000 |            161,029 |         3,945,711 |         94,910,577 |       2,265,792,710 |      54,233,186,631 |   1,295,362,125,552 | 30,932,968,221,097 | 738,548,749,700,312 | 17,631,656,694,578,592 | 420,788,402,285,901,824 |
|              5 |   6 |       537 |         38,310 |          1,706,255 |        69,763,700 |      2,818,972,642 |     112,829,665,923 |   4,499,431,376,127 | 178,942,601,291,926 |                N/A |                 N/A |                    N/A |                     N/A |
|              6 |   7 |     1,571 |        235,781 |         15,835,683 |     1,044,334,437 |     69,173,028,785 |   4,531,985,219,092 | 290,433,534,225,566 |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|              7 |   8 |     4,587 |      1,417,322 |        135,385,909 |    14,171,315,454 |  1,523,281,696,228 | 161,965,120,344,045 |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|              8 |   9 |    13,343 |      8,424,616 |      1,104,642,469 |   182,795,971,462 | 31,936,554,362,084 |                 N/A |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|              9 |  10 |    38,943 |     49,867,996 |      8,754,703,921 | 2,284,654,770,108 |                N/A |                 N/A |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|             10 |  11 |   113,835 |    294,664,010 |     67,916,896,758 |               N/A |                N/A |                 N/A |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|             11 |  12 |   333,745 |  1,741,288,730 |    519,325,538,608 |               N/A |                N/A |                 N/A |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|             12 |  13 |   980,684 | 10,300,852,227 |  3,928,940,117,357 |               N/A |                N/A |                 N/A |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |
|             13 |  14 | 2,888,780 | 61,028,884,959 | 29,499,214,177,403 |               N/A |                N/A |                 N/A |                 N/A |                 N/A |                N/A |                 N/A |                    N/A |                     N/A |

### solution
|   ply |    states (won) |   states (drawn) |   states (lost) |   states (total) |   states (terminal) |   nodes (won) |   nodes (drawn) |   nodes (lost) |   nodes (total) |   nodes (won+drawn+lost) |
|------:|----------------:|-----------------:|----------------:|-----------------:|--------------------:|--------------:|----------------:|---------------:|----------------:|-------------------------:|
|     0 |               1 |                0 |               0 |                1 |                   0 |            52 |               1 |              1 |               1 |                       54 |
|     1 |               1 |                2 |               4 |                7 |                   0 |            52 |              66 |             94 |              94 |                      212 |
|     2 |              27 |               12 |              10 |               49 |                   0 |           263 |             152 |            169 |             188 |                      584 |
|     3 |              35 |               58 |             145 |              238 |                   0 |           413 |             409 |            560 |             297 |                    1,382 |
|     4 |             690 |              200 |             230 |            1,120 |                   0 |         1,316 |           1,159 |          1,046 |             531 |                    3,521 |
|     5 |           1,080 |              697 |           2,486 |            4,263 |                   0 |         2,815 |           2,916 |          3,410 |             888 |                    9,141 |
|     6 |          10,889 |            1,943 |           3,590 |           16,422 |                   0 |         8,022 |           6,467 |          6,333 |           1,537 |                   20,822 |
|     7 |          17,507 |            5,944 |          31,408 |           54,859 |                 728 |        17,287 |          15,373 |         19,432 |           2,546 |                   52,092 |
|     8 |         124,624 |           14,676 |          44,975 |          184,275 |               1,892 |        44,790 |          32,494 |         37,598 |           5,081 |                  114,882 |
|     9 |         197,749 |           42,896 |         317,541 |          558,186 |              19,412 |        96,605 |          75,634 |        103,390 |           8,144 |                  275,629 |
|    10 |       1,122,696 |           97,532 |         442,395 |        1,662,623 |              44,225 |       225,472 |         151,745 |        196,692 |          15,481 |                  573,909 |
|    11 |       1,734,122 |          255,780 |       2,578,781 |        4,568,683 |             273,261 |       473,318 |         332,980 |        494,548 |          24,888 |                1,300,846 |
|    12 |       8,191,645 |          541,825 |       3,502,631 |       12,236,101 |             573,323 |     1,005,142 |         632,233 |        908,352 |          41,577 |                2,545,727 |
|    13 |      12,333,735 |        1,286,746 |      17,308,630 |       30,929,111 |           2,720,636 |     2,009,688 |       1,299,574 |      2,084,118 |          68,726 |                5,393,380 |
|    14 |      49,756,539 |        2,583,292 |      23,097,764 |       75,437,595 |           5,349,954 |     3,910,552 |       2,349,278 |      3,653,045 |         113,229 |                9,912,875 |
|    15 |      73,263,172 |        5,596,074 |      97,682,013 |      176,541,259 |          20,975,690 |     7,341,883 |       4,488,590 |      7,605,748 |         205,450 |               19,436,221 |
|    16 |     255,117,922 |       10,681,110 |     128,792,359 |      394,591,391 |          38,918,821 |    13,074,218 |       7,678,007 |     12,587,399 |         337,797 |               33,339,624 |
|    17 |     369,230,362 |       21,226,658 |     467,761,723 |      858,218,743 |         130,632,515 |    22,817,526 |      13,520,818 |     23,655,811 |         614,419 |               59,994,155 |
|    18 |   1,112,643,249 |       38,582,237 |     612,658,408 |    1,763,883,894 |         229,031,670 |    36,961,565 |      21,706,747 |     36,621,547 |         972,846 |               95,289,859 |
|    19 |   1,589,752,959 |       70,754,712 |   1,907,752,131 |    3,568,259,802 |         670,491,437 |    59,529,067 |      35,041,660 |     61,958,941 |       1,720,046 |              156,529,668 |
|    20 |   4,132,585,341 |      122,495,056 |   2,491,075,548 |    6,746,155,945 |       1,108,210,254 |    87,231,324 |      52,350,828 |     88,954,106 |       2,580,881 |              228,536,258 |
|    21 |   5,849,074,428 |      208,240,707 |   6,616,029,910 |   12,673,345,045 |       2,858,601,535 |   129,146,009 |      77,086,251 |    135,108,055 |       4,279,667 |              341,340,315 |
|    22 |  13,031,002,559 |      342,506,047 |   8,637,315,382 |   22,010,823,988 |       4,434,627,684 |   170,154,453 |     106,020,104 |    178,871,366 |       5,996,668 |              455,045,923 |
|    23 |  18,317,405,077 |      543,074,854 |  19,402,748,258 |   38,263,228,189 |      10,130,180,393 |   231,613,852 |     141,963,009 |    243,423,743 |       9,260,656 |              617,000,604 |
|    24 |  34,623,818,387 |      845,872,717 |  25,361,122,355 |   60,830,813,459 |      14,654,767,176 |   272,893,906 |     177,820,871 |    296,421,995 |      12,082,764 |              747,136,772 |
|    25 |  48,376,711,901 |    1,256,717,558 |  47,632,685,500 |   97,266,114,959 |      29,672,303,474 |   342,815,670 |     216,077,224 |    360,753,249 |      17,366,699 |              919,646,143 |
|    26 |  76,568,242,258 |    1,846,266,966 |  62,314,059,815 |  140,728,569,039 |      39,696,898,910 |   359,305,861 |     244,685,750 |    404,232,494 |      21,043,741 |            1,008,224,105 |
|    27 | 106,274,173,915 |    2,578,399,088 |  96,436,935,052 |  205,289,508,055 |      71,042,927,249 |   419,723,892 |     269,575,051 |    439,379,836 |      28,022,290 |            1,128,678,779 |
|    28 | 138,476,323,812 |    3,567,644,646 | 126,013,643,486 |  268,057,611,944 |      86,949,129,149 |   389,020,517 |     274,939,257 |    454,821,971 |      31,799,618 |            1,118,781,745 |
|    29 | 190,301,585,678 |    4,687,144,532 | 157,638,115,456 |  352,626,845,666 |     136,563,138,602 |   427,026,716 |     274,526,874 |    440,003,517 |      39,362,941 |            1,141,557,107 |
|    30 | 199,698,237,436 |    6,071,049,190 | 204,609,218,821 |  410,378,505,447 |     150,692,335,491 |   347,160,424 |     251,906,206 |    423,584,106 |      41,997,064 |            1,022,650,736 |
|    31 | 269,818,663,336 |    7,481,813,611 | 201,906,000,786 |  479,206,477,733 |     205,243,451,746 |   362,858,636 |     228,078,024 |    362,445,054 |      48,323,733 |              953,381,714 |
|    32 | 221,858,140,210 |    9,048,082,187 | 258,000,224,786 |  488,906,447,183 |     200,299,011,722 |   256,335,118 |     188,918,083 |    328,791,220 |      48,313,654 |              774,044,421 |
|    33 | 291,549,830,422 |   10,381,952,902 | 194,705,107,378 |  496,636,890,702 |     232,494,602,432 |   260,638,140 |     155,709,824 |    246,069,381 |      51,955,012 |              662,417,345 |
|    34 | 180,530,409,295 |   11,668,229,290 | 241,273,091,751 |  433,471,730,336 |     195,427,938,799 |   157,385,411 |     117,478,018 |    215,831,346 |      48,778,662 |              490,694,775 |
|    35 | 226,007,657,501 |   12,225,240,861 | 132,714,989,361 |  370,947,887,723 |     188,065,840,647 |   160,409,773 |      88,337,570 |    137,452,152 |      48,332,633 |              386,199,495 |
|    36 |  98,839,977,654 |   12,431,825,174 | 155,042,098,394 |  266,313,901,222 |     131,014,104,050 |    80,553,314 |      60,594,699 |    120,050,832 |      41,909,940 |              261,198,845 |
|    37 | 114,359,332,473 |   11,509,102,126 |  57,747,247,782 |  183,615,682,381 |     100,184,819,358 |    83,790,619 |      41,853,456 |     61,427,031 |      37,278,648 |              187,071,106 |
|    38 |  32,161,409,500 |   10,220,085,105 |  61,622,970,744 |  104,004,465,349 |      54,716,901,301 |    32,744,625 |      24,888,904 |     53,915,478 |      28,733,225 |              111,549,007 |
|    39 |  33,666,235,957 |    7,792,641,079 |  13,697,133,737 |   55,156,010,773 |      31,270,711,562 |    33,780,894 |      15,576,173 |     20,081,029 |      22,165,837 |               69,438,096 |
|    40 |   4,831,822,472 |    5,153,271,363 |  12,710,802,660 |   22,695,896,495 |      11,972,173,842 |     9,782,495 |       7,772,258 |     17,351,899 |      14,031,885 |               34,906,652 |
|    41 |   4,282,128,782 |    2,496,557,393 |   1,033,139,763 |    7,811,825,938 |       4,282,128,782 |     7,938,927 |       3,499,912 |      4,036,774 |       7,901,773 |               15,475,613 |
|    42 |               0 |      713,298,878 |     746,034,021 |    1,459,332,899 |         746,034,021 |             1 |       1,783,048 |      2,731,785 |       2,777,005 |                4,514,834 |

states (won)      2,317,028,267,398
states (drawn)      123,343,183,724
states (lost)     2,091,613,767,970
states (total)    4,531,985,219,092

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
|   width |   height |          #positions |          comp.time |   GC perc. |   RAM used |   #allocatable |    #allocated |   perc. allocated |
|--------:|---------:|--------------------:|-------------------:|-----------:|-----------:|---------------:|--------------:|------------------:|
|       1 |        1 |                   2 |         0:00:00.67 |     27.48% |     6.85GB |           2^27 |            61 |             0.00% |
|       1 |        2 |                   3 |         0:00:00.74 |     24.62% |     6.85GB |           2^27 |           222 |             0.00% |
|       1 |        3 |                   4 |         0:00:00.81 |     22.77% |     6.85GB |           2^27 |           468 |             0.00% |
|       1 |        4 |                   5 |         0:00:00.88 |     21.23% |     6.85GB |           2^27 |           825 |             0.00% |
|       1 |        5 |                   6 |         0:00:00.94 |     19.36% |     6.85GB |           2^27 |         1,306 |             0.00% |
|       1 |        6 |                   7 |         0:00:01.01 |     18.00% |     6.85GB |           2^27 |         1,923 |             0.00% |
|       1 |        7 |                   8 |         0:00:01.08 |     16.92% |     6.85GB |           2^27 |         2,688 |             0.00% |
|       1 |        8 |                   9 |         0:00:01.15 |     15.79% |     6.85GB |           2^27 |         3,613 |             0.00% |
|       1 |        9 |                  10 |         0:00:01.60 |     34.47% |     6.85GB |           2^27 |         4,710 |             0.00% |
|       1 |       10 |                  11 |         0:00:02.02 |     45.06% |     6.85GB |           2^27 |         5,991 |             0.00% |
|       1 |       11 |                  12 |         0:00:02.47 |     52.25% |     6.85GB |           2^27 |         7,468 |             0.01% |
|       1 |       12 |                  13 |         0:00:12.27 |     57.63% |    27.38GB |           2^29 |         9,153 |             0.00% |
|       1 |       13 |                  14 |         0:00:26.77 |     60.75% |    54.76GB |           2^30 |        11,058 |             0.00% |
|       2 |        1 |                   5 |         0:00:00.73 |     24.67% |     6.85GB |           2^27 |           203 |             0.00% |
|       2 |        2 |                  18 |         0:00:00.87 |     20.94% |     6.85GB |           2^27 |           800 |             0.00% |
|       2 |        3 |                  58 |         0:00:01.02 |     17.93% |     6.85GB |           2^27 |         1,957 |             0.00% |
|       2 |        4 |                 179 |         0:00:01.16 |     15.91% |     6.85GB |           2^27 |         3,777 |             0.00% |
|       2 |        5 |                 537 |         0:00:02.02 |     45.05% |     6.85GB |           2^27 |         6,330 |             0.00% |
|       2 |        6 |               1,571 |         0:00:02.91 |     56.82% |     6.85GB |           2^27 |         9,700 |             0.01% |
|       2 |        7 |               4,587 |         0:00:03.82 |     63.13% |     6.85GB |           2^27 |        13,971 |             0.01% |
|       2 |        8 |              13,343 |         0:00:04.78 |     66.98% |     6.85GB |           2^27 |        19,227 |             0.01% |
|       2 |        9 |              38,943 |         0:00:05.69 |     69.59% |     6.85GB |           2^27 |        25,552 |             0.02% |
|       2 |       10 |             113,835 |         0:00:06.77 |     71.62% |     6.85GB |           2^27 |        33,030 |             0.02% |
|       2 |       11 |             333,745 |         0:00:30.29 |     73.50% |    27.38GB |           2^29 |        41,745 |             0.01% |
|       2 |       12 |             980,684 |         0:01:06.19 |     74.78% |    54.76GB |           2^30 |        51,781 |             0.00% |
|       3 |        1 |                  13 |         0:00:00.87 |     22.92% |     6.85GB |           2^27 |           426 |             0.00% |
|       3 |        2 |                 116 |         0:00:01.04 |     18.44% |     6.85GB |           2^27 |         1,836 |             0.00% |
|       3 |        3 |                 869 |         0:00:01.44 |     26.32% |     6.85GB |           2^27 |         4,567 |             0.00% |
|       3 |        4 |               6,000 |         0:00:03.01 |     57.20% |     6.85GB |           2^27 |        10,781 |             0.01% |
|       3 |        5 |              38,310 |         0:00:04.53 |     65.07% |     6.85GB |           2^27 |        18,520 |             0.01% |
|       3 |        6 |             235,781 |         0:00:05.80 |     68.64% |     6.85GB |           2^27 |        29,974 |             0.02% |
|       3 |        7 |           1,417,322 |         0:00:07.36 |     68.99% |     6.85GB |           2^27 |        75,162 |             0.06% |
|       3 |        8 |           8,424,616 |         0:00:09.67 |     67.38% |     6.85GB |           2^27 |       172,756 |             0.13% |
|       3 |        9 |          49,867,996 |         0:00:14.60 |     62.21% |     6.85GB |           2^27 |       422,983 |             0.32% |
|       3 |       10 |         294,664,010 |         0:01:09.28 |     67.59% |    27.38GB |           2^29 |     1,010,556 |             0.19% |
|       3 |       11 |       1,741,288,730 |         0:02:38.47 |     66.14% |    54.76GB |           2^30 |     2,381,296 |             0.22% |
|       4 |        1 |                  35 |         0:00:00.88 |     20.84% |     6.85GB |           2^27 |           745 |             0.00% |
|       4 |        2 |                 741 |         0:00:01.17 |     15.98% |     6.85GB |           2^27 |         3,426 |             0.00% |
|       4 |        3 |              12,031 |         0:00:02.91 |     56.68% |     6.85GB |           2^27 |         8,853 |             0.01% |
|       4 |        4 |             161,029 |         0:00:09.31 |     82.53% |     6.85GB |           2^27 |        17,799 |             0.01% |
|       4 |        5 |           1,706,255 |         0:00:13.97 |     80.92% |     6.85GB |           2^27 |       129,589 |             0.10% |
|       4 |        6 |          15,835,683 |         0:00:22.41 |     71.30% |     6.85GB |           2^27 |       561,346 |             0.42% |
|       4 |        7 |         135,385,909 |         0:00:43.05 |     52.97% |     6.85GB |           2^27 |     1,648,551 |             1.23% |
|       4 |        8 |       1,104,642,469 |         0:01:43.80 |     32.62% |     6.85GB |           2^27 |     4,307,162 |             3.21% |
|       4 |        9 |       8,754,703,921 |         0:07:13.75 |     36.64% |    27.38GB |           2^29 |    11,478,742 |             2.14% |
|       4 |       10 |      67,916,896,758 |         0:21:19.35 |     30.47% |    54.76GB |           2^30 |    29,923,914 |             2.79% |
|       5 |        1 |                  96 |         0:00:00.95 |     19.58% |     6.85GB |           2^27 |         1,172 |             0.00% |
|       5 |        2 |               4,688 |         0:00:02.02 |     45.08% |     6.85GB |           2^27 |         5,666 |             0.00% |
|       5 |        3 |             158,911 |         0:00:04.33 |     65.09% |     6.85GB |           2^27 |        15,062 |             0.01% |
|       5 |        4 |           3,945,711 |         0:00:14.31 |     82.08% |     6.85GB |           2^27 |       129,415 |             0.10% |
|       5 |        5 |          69,763,700 |         0:00:28.89 |     62.67% |     6.85GB |           2^27 |     1,230,084 |             0.92% |
|       5 |        6 |       1,044,334,437 |         0:02:31.63 |     24.05% |     6.85GB |           2^27 |     8,235,134 |             6.14% |
|       5 |        7 |      14,171,315,454 |         0:10:33.32 |     14.14% |     6.85GB |           2^27 |    25,600,589 |            19.07% |
|       5 |        8 |     182,795,971,462 |         0:43:21.22 |     14.76% |    27.38GB |           2^29 |    69,165,125 |            12.88% |
|       5 |        9 |   2,284,654,770,108 |         3:01:45.83 |     14.94% |   109.52GB |           2^31 |   201,469,478 |             9.38% |
|       6 |        1 |                 267 |         0:00:01.03 |     18.24% |     6.85GB |           2^27 |         1,719 |             0.00% |
|       6 |        2 |              29,737 |         0:00:03.02 |     57.38% |     6.85GB |           2^27 |         8,652 |             0.01% |
|       6 |        3 |           2,087,325 |         0:00:05.80 |     68.28% |     6.85GB |           2^27 |        32,107 |             0.02% |
|       6 |        4 |          94,910,577 |         0:00:24.33 |     70.16% |     6.85GB |           2^27 |       706,477 |             0.53% |
|       6 |        5 |       2,818,972,642 |         0:02:17.06 |     28.26% |     6.85GB |           2^27 |     7,852,100 |             5.85% |
|       6 |        6 |      69,173,028,785 |         0:26:54.42 |     13.25% |     6.85GB |           2^27 |    61,634,539 |            45.92% |
|       6 |        7 |   1,523,281,696,228 |         4:49:49.95 |      8.94% |    27.38GB |           2^29 |   365,099,251 |            68.01% |
|       6 |        8 |  31,936,554,362,084 |        19:05:58.63 |     10.03% |   109.52GB |           2^31 | 1,023,026,899 |            47.64% |
|       7 |        1 |                 750 |         0:00:01.10 |     17.32% |     6.85GB |           2^27 |         2,398 |             0.00% |
|       7 |        2 |             189,648 |         0:00:03.89 |     63.26% |     6.85GB |           2^27 |        12,480 |             0.01% |
|       7 |        3 |          27,441,956 |         0:00:07.59 |     68.44% |     6.85GB |           2^27 |        97,803 |             0.07% |
|       7 |        4 |       2,265,792,710 |         0:00:55.87 |     49.33% |     6.85GB |           2^27 |     2,418,545 |             1.80% |
|       7 |        5 |     112,829,665,923 |         0:12:39.10 |     16.95% |     6.85GB |           2^27 |    27,594,037 |            20.56% |
|       7 |        6 |   4,531,985,219,092 |         3:19:39.28 |     12.56% |    27.38GB |           2^29 |   238,538,432 |            44.43% |
|       7 |        7 | 161,965,120,344,045 | 1 day, 22:15:50.00 |     10.66% |   109.52GB |           2^31 | 1,797,440,896 |            83.70% |
|       8 |        1 |               2,118 |         0:00:01.19 |     16.52% |     6.85GB |           2^27 |         3,221 |             0.00% |
|       8 |        2 |           1,216,721 |         0:00:04.89 |     67.05% |     6.85GB |           2^27 |        17,246 |             0.01% |
|       8 |        3 |         362,940,958 |         0:00:09.66 |     64.10% |     6.85GB |           2^27 |       238,318 |             0.18% |
|       8 |        4 |      54,233,186,631 |         0:02:35.56 |     31.73% |     6.85GB |           2^27 |     5,820,061 |             4.34% |
|       8 |        5 |   4,499,431,376,127 |         0:54:34.82 |     18.28% |    27.38GB |           2^29 |    70,093,438 |            13.06% |
|       8 |        6 | 290,433,534,225,566 |        17:06:50.59 |     11.63% |    54.76GB |           2^30 |   625,763,115 |            58.28% |
|       9 |        1 |               6,010 |         0:00:01.63 |     35.18% |     6.85GB |           2^27 |         4,200 |             0.00% |
|       9 |        2 |           7,844,298 |         0:00:05.83 |     69.04% |     6.85GB |           2^27 |        23,046 |             0.02% |
|       9 |        3 |       4,816,325,017 |         0:00:13.50 |     56.83% |     6.85GB |           2^27 |       478,748 |             0.36% |
|       9 |        4 |   1,295,362,125,552 |         0:09:14.50 |     40.31% |    27.38GB |           2^29 |    11,398,845 |             2.12% |
|       9 |        5 | 178,942,601,291,926 |         2:58:41.23 |     16.62% |    54.76GB |           2^30 |   163,165,045 |            15.20% |
|      10 |        1 |              17,120 |         0:00:02.08 |     45.90% |     6.85GB |           2^27 |         5,347 |             0.00% |
|      10 |        2 |          50,780,523 |         0:00:06.87 |     70.01% |     6.85GB |           2^27 |        36,276 |             0.03% |
|      10 |        3 |      64,137,689,503 |         0:00:51.52 |     63.88% |    27.38GB |           2^29 |       840,337 |             0.16% |
|      10 |        4 |  30,932,968,221,097 |         0:24:05.94 |     40.29% |    54.76GB |           2^30 |    24,226,909 |             2.26% |
|      11 |        1 |              48,930 |         0:00:02.55 |     52.75% |     6.85GB |           2^27 |         6,674 |             0.00% |
|      11 |        2 |         329,842,064 |         0:00:30.24 |     72.48% |    27.38GB |           2^29 |        57,846 |             0.01% |
|      11 |        3 |     856,653,299,180 |         0:01:57.51 |     64.38% |    54.76GB |           2^30 |     1,615,299 |             0.15% |
|      12 |        1 |             140,243 |         0:00:11.52 |     56.78% |    27.38GB |           2^29 |         8,193 |             0.00% |
|      12 |        2 |       2,148,495,091 |         0:01:05.07 |     73.30% |    54.76GB |           2^30 |        92,737 |             0.01% |
|      13 |        1 |             402,956 |         0:00:26.28 |     60.12% |    54.76GB |           2^30 |         9,916 |             0.00% |
