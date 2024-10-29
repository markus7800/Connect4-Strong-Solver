CC = clang

WRITE_TO_FILE = 1       # write results to file?
ENTER_TO_CONTINUE = 0   # should require to press enter to start?
FULLBDD = 0             # compute a BDD encoding all positions (OR over all BDDs for single plies)
IN_OP_GC = 0		    # should GC be performed during ops?
ALLOW_ROW_ORDER = 1     # should order by row if height > width?
COMPRESSED_ENCODING = 0 # use compressed Connect4 encoding?

CFLAGS = -O3 -flto -Wall -O3

BDDFILES = caches.c counts.c memorypool.c node.c ops.c uniquetable.c save.c nodeindexmap.c

C4FLAGS = \
-DFULLBDD=$(FULLBDD) \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) \
-DWRITE_TO_FILE=$(WRITE_TO_FILE) \
-DIN_OP_GC=$(IN_OP_GC) \
-DALLOW_ROW_ORDER=$(ALLOW_ROW_ORDER) \
-DCOMPRESSED_ENCODING=$(COMPRESSED_ENCODING)

QFLAGS = \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE)

connect4:
	$(CC) connect4.c $(BDDFILES) $(CFLAGS) $(C4FLAGS) -o connect4.out

connect4-solve:
	$(CC) connect4_solve.c $(BDDFILES) $(CFLAGS) $(C4FLAGS) -o connect4_solve.out
	$(CC) connect4_bestmove.c $(CFLAGS) $(C4FLAGS) -o connect4_bestmove.out

connect4-perft:
	$(CC) connect4_perft.c $(BDDFILES) $(CFLAGS) -o perft.out 

queens:
	$(CC) queens.c $(BDDFILES) $(CFLAGS) $(QFLAGS) -o queens.out

clean:
	rm connect4.out
	rm queens.out
