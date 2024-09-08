build:
	gcc -std=c99 -glldb -pedantic -Wno-strict-prototypes -Wno-newline-eof -Wno-ignored-qualifiers *.c -o tinydb -lpthread 

test:
	gcc -std=c99 -o test/tests test/tests.c tinydb_hashmap.c tinydb_log.c tinydb_memory_pool.c -lpthread