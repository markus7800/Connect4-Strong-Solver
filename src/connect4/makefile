CC = clang
$(info Output directory: build $(shell mkdir -p build))

# for solve and count

# write results to file?
WRITE_TO_FILE = 1
# should require to press enter to start?
ENTER_TO_CONTINUE = 0

# for solve and count

# should GC be performed during ops?
IN_OP_GC = 0
# fill-level at which IN_OP_GC is triggered
IN_OP_GC_THRES = 0.9999
# exclusively perform IN_OP_GC?
IN_OP_GC_EXCL = 0

# for solve and count

# should order by row if height > width?
ALLOW_ROW_ORDER = 0
# use compressed Connect4 encoding?
COMPRESSED_ENCODING = 1

# for count

# compute a BDD encoding all positions (OR over all BDDs for single plies)
FULLBDD = 0
# SUBTRACT_TERM = 0 disables terminal condition (connect4 win rule)
SUBTRACT_TERM = 1

# for solve

# save BDDs (strong solution) to disk?
SAVE_BDD_TO_DISK = 1

# for bestmove, full_ab_search, wdl, openingbook, perft

# width of connect4 board
WIDTH = 7
# height of connect4 board
HEIGHT = 6

# for full_ab_search
DTM = 1                 # compute distance to win/loss? or win/draw/loss score instead

CFLAGS = -O3 -flto -Wall -O3

BDDFILES = ../bdd/caches.c ../bdd/counts.c ../bdd/memorypool.c ../bdd/node.c ../bdd/ops.c ../bdd/uniquetable.c ../bdd/save.c ../bdd/nodeindexmap.c

SEARCHFLAGS = \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) \
-DWRITE_TO_FILE=$(WRITE_TO_FILE) \
-DALLOW_ROW_ORDER=$(ALLOW_ROW_ORDER) \
-DCOMPRESSED_ENCODING=$(COMPRESSED_ENCODING)

WIDTH_HEIGHT = \
-DWIDTH=$(WIDTH) \
-DHEIGHT=$(HEIGHT)

GCFLAGS = \
-DIN_OP_GC=$(IN_OP_GC) \
-DIN_OP_GC_EXCL=$(IN_OP_GC_EXCL) \
-DIN_OP_GC_THRES=$(IN_OP_GC_THRES)

count:
	$(CC) search/count.c $(BDDFILES) $(CFLAGS) $(GCFLAGS) $(SEARCHFLAGS) -DFULLBDD=$(FULLBDD) -DSUBTRACT_TERM=$(SUBTRACT_TERM) -o build/count.out

solve:
	$(CC) search/solve.c $(BDDFILES) $(CFLAGS) $(GCFLAGS) $(SEARCHFLAGS) -DSAVE_BDD_TO_DISK=$(SAVE_BDD_TO_DISK) -o build/solve.out

bestmove:
	$(CC) probe/bestmove.c $(CFLAGS) $(WIDTH_HEIGHT) -o build/bestmove_w$(WIDTH)_h$(HEIGHT).out

full_ab_search:
	$(CC) probe/full_ab_search.c $(CFLAGS) $(WIDTH_HEIGHT) -DDTM=$(DTM) -o build/full_ab_search_w$(WIDTH)_h$(HEIGHT).out

wdl:
	$(CC) probe/wdl.c $(CFLAGS) $(WIDTH_HEIGHT) -o build/wdl_w$(WIDTH)_h$(HEIGHT).out

openingbook:
	$(CC) probe/generate_openingbook.c $(CFLAGS) $(WIDTH_HEIGHT) -o build/generate_openingbook_w$(WIDTH)_h$(HEIGHT).out -lpthread

perft:
	$(CC) probe/perft.c $(CFLAGS) $(WIDTH_HEIGHT) -o build/perft_w$(WIDTH)_h$(HEIGHT).out 

clean:
	rm -rf build