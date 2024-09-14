CC = gcc
CFLAGS = -ggdb -pedantic -Wno-strict-prototypes -Wno-newline-eof -Wno-ignored-qualifiers
LDFLAGS = -lpthread

SRC = tinydb_hashmap.c tinydb_database_entry_destructor.c tinydb_log.c tinydb_memory_pool.c
TEST_SRC = test/tests.c

TARGET = tinydb
TEST_TARGET = test/tests

build: $(SRC)
	$(CC) $(CFLAGS) *.c -o $(TARGET) $(LDFLAGS)

test: $(TEST_SRC) $(SRC)
	$(CC) -std=c99 -o $(TEST_TARGET) $(TEST_SRC) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TEST_TARGET)
