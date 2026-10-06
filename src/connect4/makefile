CC = clang
$(info Output directory: build $(shell mkdir -p build))

# for solve and count

# write results to file?
WRITE_TO_FILE = 1
# should require to press enter to start?
ENTER_TO_CONTINUE = 0

# for solve and count

# should GC be performed during ops (when the node pool is nearly full)?
IN_OP_GC = 1
# fill-level at which IN_OP_GC is triggered
IN_OP_GC_THRES = 0.9
# (no effect anymore: between the steps GC only runs if the node pool is more than half full)
IN_OP_GC_EXCL = 0
# run GC after every step of the computation? (for measuring the number of nodes that are needed)
FORCE_GC = 0
# between the steps, GC runs if more than GC_GROWTH times the nodes that survived the last GC are allocated
GC_GROWTH = 2.0

# for solve, count, bestmove, wdl, openingbook 

# should order by row if height > width?
ALLOW_ROW_ORDER = 0
# use compressed Connect4 encoding?
COMPRESSED_ENCODING = 1

# apply the moves of each column separately instead of all moves at once?
PARTITIONED_IMAGE = 0

# for count

# compute a BDD encoding all positions (OR over all BDDs for single plies)
FULLBDD = 0
# SUBTRACT_TERM = 0 disables terminal condition (connect4 win rule)
SUBTRACT_TERM = 1

# for solve

# save BDDs (strong solution) to disk?
SAVE_BDD_TO_DISK = 1
# keep the BDDs of the reachable positions per ply (folder reach_w{WIDTH}_h{HEIGHT}) after the computation?
KEEP_REACH = 0

# for bestmove, full_ab_search, wdl, openingbook, perft

# width of connect4 board
WIDTH = 7
# height of connect4 board
HEIGHT = 6

# for full_ab_search
DTM = 1                 # compute distance to win/loss? or win/draw/loss score instead

CFLAGS = -O3 -flto -Wall -O3

# for solve and count: disable asserts and optimise for the CPU of this machine
# (make solve DEBUG=1 keeps the asserts)
DEBUG = 0
ifeq ($(shell uname -m),arm64)
NATIVE = -mcpu=native
else ifeq ($(shell uname -m),aarch64)
NATIVE = -mcpu=native
else
NATIVE = -march=native
endif
ifeq ($(DEBUG),1)
SEARCHCFLAGS = $(CFLAGS) $(NATIVE)
else
SEARCHCFLAGS = $(CFLAGS) $(NATIVE) -DNDEBUG
endif

BDDFILES = ../bdd/caches.c ../bdd/counts.c ../bdd/memorypool.c ../bdd/node.c ../bdd/ops.c ../bdd/uniquetable.c ../bdd/save.c

SEARCHFLAGS = \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) \
-DWRITE_TO_FILE=$(WRITE_TO_FILE)

# experimental variable orders for the compressed encoding (not compatible with the probing programs)
# COL_ORDER: 0 = left to right, 1 = centre out, 2 = outside in; ROW_DIR: 0 = bottom up, 1 = top down
COL_ORDER = 0
ROW_DIR = 0

ENCODING = \
-DCOL_ORDER=$(COL_ORDER) \
-DROW_DIR=$(ROW_DIR) \
-DPARTITIONED_IMAGE=$(PARTITIONED_IMAGE) \
-DALLOW_ROW_ORDER=$(ALLOW_ROW_ORDER) \
-DCOMPRESSED_ENCODING=$(COMPRESSED_ENCODING)

WIDTH_HEIGHT = \
-DWIDTH=$(WIDTH) \
-DHEIGHT=$(HEIGHT)

# the op cache has 2^(log2_tablesize - CACHE_SHIFT) entries of 16 bytes
CACHE_SHIFT = 2
# the unique table has 2^(log2_tablesize - BUCKET_SHIFT) buckets of 4 bytes
BUCKET_SHIFT = 0
# if > 0: also run GC during ops when more than IN_OP_GC_GROWTH times the nodes alive after the last GC are allocated
IN_OP_GC_GROWTH = 0
# keep cache entries of surviving nodes when garbage collecting? 0 = never, 1 = for GC during ops, 2 = always
GC_KEEPS_CACHE = 1

GCFLAGS = \
-DOPCACHE_LOG2_REDUCTION=$(CACHE_SHIFT) \
-DUNIQUETABLE_LOG2_REDUCTION=$(BUCKET_SHIFT) \
-DIN_OP_GC_GROWTH=$(IN_OP_GC_GROWTH) \
-DGC_KEEPS_CACHE=$(GC_KEEPS_CACHE) \
-DFORCE_GC=$(FORCE_GC) \
-DGC_GROWTH=$(GC_GROWTH) \
-DIN_OP_GC=$(IN_OP_GC) \
-DIN_OP_GC_EXCL=$(IN_OP_GC_EXCL) \
-DIN_OP_GC_THRES=$(IN_OP_GC_THRES)

count:
	$(CC) search/count.c $(BDDFILES) $(SEARCHCFLAGS) $(GCFLAGS) $(ENCODING) $(SEARCHFLAGS) -DFULLBDD=$(FULLBDD) -DSUBTRACT_TERM=$(SUBTRACT_TERM) -o build/count.out

solve:
	$(CC) search/solve.c $(BDDFILES) $(SEARCHCFLAGS) $(GCFLAGS) $(ENCODING) $(SEARCHFLAGS) -DSAVE_BDD_TO_DISK=$(SAVE_BDD_TO_DISK) -DKEEP_REACH=$(KEEP_REACH) -o build/solve.out

bestmove:
	$(CC) probe/bestmove.c $(CFLAGS) $(ENCODING) $(WIDTH_HEIGHT) -o build/bestmove_w$(WIDTH)_h$(HEIGHT).out

full_ab_search:
	$(CC) probe/full_ab_search.c $(CFLAGS) $(WIDTH_HEIGHT) -DDTM=$(DTM) -o build/full_ab_search_w$(WIDTH)_h$(HEIGHT).out

wdl:
	$(CC) probe/wdl.c $(CFLAGS) $(ENCODING) $(WIDTH_HEIGHT) -o build/wdl_w$(WIDTH)_h$(HEIGHT).out

openingbook:
	$(CC) probe/generate_openingbook.c $(CFLAGS) $(ENCODING) $(WIDTH_HEIGHT) -o build/generate_openingbook_w$(WIDTH)_h$(HEIGHT).out -lpthread

perft:
	$(CC) probe/perft.c $(CFLAGS) $(WIDTH_HEIGHT) -o build/perft_w$(WIDTH)_h$(HEIGHT).out 

clean:
	rm -rf build