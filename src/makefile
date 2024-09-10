
PRINT_TO_FILE = 1
ENTER_TO_CONTINUE = 0
DISABLE_AFTER_OP = 1
FULLBDD = 0

queens:
	clang queens.c caches.c counts.c memorypool.c node.c ops.c uniquetable.c -O3 -flto -Wall -O3 -o queens.out

connect4:
	clang connect4.c caches.c counts.c memorypool.c node.c ops.c uniquetable.c -O3 -flto -Wall -O3 -DDISABLE_AFTER_OP=$(DISABLE_AFTER_OP) -DFULLBDD=$(FULLBDD) -DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) -DPRINT_TO_FILE=$(PRINT_TO_FILE) -o connect4.out

clean:
	rm connect4.out
	rm queens.out
