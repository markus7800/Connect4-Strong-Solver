
WRITE_TO_FILE = 1
ENTER_TO_CONTINUE = 0
DISABLE_AFTER_OP = 1
FULLBDD = 0

connect4:
	clang connect4.c caches.c counts.c memorypool.c node.c ops.c uniquetable.c -O3 -flto -Wall -O3 -DDISABLE_AFTER_OP=$(DISABLE_AFTER_OP) -DFULLBDD=$(FULLBDD) -DENTER_TO_CONTINUE=$(ENTER_TO_CONTINUE) -DWRITE_TO_FILE=$(WRITE_TO_FILE) -o connect4.out

queens:
	clang queens.c caches.c counts.c memorypool.c node.c ops.c uniquetable.c -O3 -flto -Wall -O3 -o queens.out

clean:
	rm connect4.out
	rm queens.out
