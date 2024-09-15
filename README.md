# Connect4 Position Counter

This project is an open-source implementation of the approach described in
```
Edelkamp, S., & Kissmann, P. (2011, August). On the complexity of BDDs for state space search: A case study in Connect Four.
In Proceedings of the AAAI Conference on Artificial Intelligence (Vol. 25, No. 1, pp. 18-23).
```
to determinte the number of unique positions in a Connect4 game of given board size.


## Results
We were able to independently verify the numbers computed by [John Tromp](https://tromp.github.io/c4/c4.html) and produce novel counts for boards up to size `width + height = 14`.

The computation were run on a Intel(R) Xeon(R) Platinum 8358 CPU @ 2.60GHz with 238.4GB RAM.

It is possible to compute the number of positions for the conventional 7 x 6 board in ~3h20min with 28GB RAM.

### Counts per ply

We also recorded the number of positions at a given ply (layer) and the number of nodes of the BDD encoding these positions.
These numbers can be found in the results folder.

In the 7 x 6 case, our numbers approximately match Edelkamp, S., & Kissmann, P.
However, the BDD encoding all 7 x 6 positions (not of a single ply) has _ nodes.
This is much more than the 84,088,763 which Edelkamp, S., & Kissmann, P. claimed in 
```
Edelkamp, S., & Kissmann, P. (2008). Symbolic classification of general two-player games.
In KI 2008: Advances in Artificial Intelligence: 31st Annual German Conference on AI, KI 2008, Kaiserslautern, Germany, September 23-26, 2008. Proceedings 31 (pp. 185-192). Springer Berlin Heidelberg.
```
In contrast, we found that the maximum number of nodes to encode all positions of a single ply is 86,532,594 (ply 33).

### Experiment information