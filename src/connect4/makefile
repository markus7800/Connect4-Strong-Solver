CC = clang

WRITE_TO_FILE = 1       # write results to file?
ENTER_TO_CONTINUE = 0   # should require to press enter to start?
FULLBDD = 0             # compute a BDD encoding all positions (OR over all BDDs for single plies)

IN_OP_GC = 0		    # should GC be performed during ops?
IN_OP_GC_THRES = 0.9999
IN_OP_GC_EXCL = 0

ALLOW_ROW_ORDER = 0     # should order by row if height > width?
COMPRESSED_ENCODING = 1 # use compressed Connect4 encoding?

# for solve.c
SAVE_BDD_TO_DISK = 1    # save BDDs (strong solution) to disk?

# for full_ab_search
DTM = 1					# compute distance to win/loss? or win/draw/loss score instead

WIDTH = 7
HEIGHT = 6

CFLAGS = -O3 -flto -Wall -O3

BDDFILES = ../bdd/caches.c ../bdd/counts.c ../bdd/memorypool.c ../bdd/node.c ../bdd/ops.c ../bdd/uniquetable.c ../bdd/save.c ../bdd/nodeindexmap.c

C4FLAGS = \
-DFULLBDD=$(FULLBDD) \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) \
-DWRITE_TO_FILE=$(WRITE_TO_FILE) \
-DIN_OP_GC=$(IN_OP_GC) \
-DIN_OP_GC_EXCL=$(IN_OP_GC_EXCL) \
-DIN_OP_GC_THRES=$(IN_OP_GC_THRES) \
-DALLOW_ROW_ORDER=$(ALLOW_ROW_ORDER) \
-DCOMPRESSED_ENCODING=$(COMPRESSED_ENCODING)

# TODO: tidy up c4flags and write encoding type to .bin files?

count:
	$(CC) search/count.c $(BDDFILES) $(CFLAGS) $(C4FLAGS) -o count.out

solve:
	$(CC) search/solve.c $(BDDFILES) $(CFLAGS) $(C4FLAGS) -DSAVE_BDD_TO_DISK=$(SAVE_BDD_TO_DISK) -o solve.out

bestmove:
	$(CC) probe/bestmove.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o bestmove_w$(WIDTH)_h$(HEIGHT).out

full_ab_search:
	$(CC) probe/full_ab_search.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -DDTM=$(DTM) -o full_ab_search_w$(WIDTH)_h$(HEIGHT).out

wdl:
	$(CC) probe/wdl.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o wdl_w$(WIDTH)_h$(HEIGHT).out

openingbook:
	$(CC) probe/generate_openingbook.c $(CFLAGS) $(C4FLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o generate_openingbook_w$(WIDTH)_h$(HEIGHT).out -lpthread

perft:
	$(CC) probe/perft.c $(CFLAGS) -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -o perft_w$(WIDTH)_h$(HEIGHT).out 