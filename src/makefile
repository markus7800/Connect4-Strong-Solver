CC = clang

WRITE_TO_FILE = 1      # write results to file?
ENTER_TO_CONTINUE = 1  # should require to press enter to start?
FULLBDD = 0            # compute a BDD encoding all positions (OR over all BDDs for single plies)

connect4:
	$(CC) connect4.c caches.c counts.c memorypool.c node.c ops.c uniquetable.c -O3 -flto -Wall -O3 -DFULLBDD=$(FULLBDD) -DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) -DWRITE_TO_FILE=$(WRITE_TO_FILE) -o connect4.out

queens:
	$(CC) queens.c caches.c counts.c memorypool.c node.c ops.c uniquetable.c -O3 -flto -Wall -O3 -DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) -o queens.out

clean:
	rm connect4.out
	rm queens.out
