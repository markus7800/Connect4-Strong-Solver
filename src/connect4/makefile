CC = clang

WRITE_TO_FILE = 1       # write results to file?
ENTER_TO_CONTINUE = 0   # should require to press enter to start?
FULLBDD = 0             # compute a BDD encoding all positions (OR over all BDDs for single plies)
IN_OP_GC = 0		    # should GC be performed during ops?
ALLOW_ROW_ORDER = 0     # should order by row if height > width?
COMPRESSED_ENCODING = 1 # use compressed Connect4 encoding?

WIDTH = 7
HEIGHT = 6

CFLAGS = -O3 -flto -Wall -O3

BDDFILES = ../bdd/caches.c ../bdd/counts.c ../bdd/memorypool.c ../bdd/node.c ../bdd/ops.c ../bdd/uniquetable.c ../bdd/save.c ../bdd/nodeindexmap.c

C4FLAGS = \
-DFULLBDD=$(FULLBDD) \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) \
-DWRITE_TO_FILE=$(WRITE_TO_FILE) \
-DIN_OP_GC=$(IN_OP_GC) \
-DALLOW_ROW_ORDER=$(ALLOW_ROW_ORDER) \
-DCOMPRESSED_ENCODING=$(COMPRESSED_ENCODING)

count:
	$(CC) search/count.c $(BDDFILES) $(CFLAGS) $(C4FLAGS) -o count.out

solve:
	$(CC) search/solve.c $(BDDFILES) $(CFLAGS) $(C4FLAGS) -o solve.out

bestmove:
	$(CC) probe/bestmove.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o bestmove_w$(WIDTH)_h$(HEIGHT).out

wdl_search:
	$(CC) probe/wdl_search.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o wdl_search_w$(WIDTH)_h$(HEIGHT).out

wdl:
	$(CC) probe/wdl.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o wdl_w$(WIDTH)_h$(HEIGHT).out

openingbook:
	$(CC) probe/generate_openingbook.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o generate_ob_w$(WIDTH)_h$(HEIGHT).out -lpthread

perft:
	$(CC) probe/perft.c $(BDDFILES) $(CFLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o perft_w$(WIDTH)_h$(HEIGHT).out 