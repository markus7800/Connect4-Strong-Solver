CC = clang

ENTER_TO_CONTINUE = 0 # should require to press enter to start?

CFLAGS = -O3 -flto -Wall -O3

BDDFILES = ../bdd/caches.c ../bdd/counts.c ../bdd/memorypool.c ../bdd/node.c ../bdd/ops.c ../bdd/uniquetable.c ../bdd/save.c ../bdd/nodeindexmap.c

QFLAGS = \
-DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE)

queens:
	$(CC) queens.c $(BDDFILES) $(CFLAGS) $(QFLAGS) -o queens.out

