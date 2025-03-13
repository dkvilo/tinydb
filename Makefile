CC = gcc
CFLAGS = -ggdb -pedantic -Wno-strict-prototypes -Wno-newline-eof -Wno-ignored-qualifiers
LDFLAGS = -lpthread

SRC = main.c tinydb_context.c tinydb_database.c tinydb_atomic_proc.c tinydb_command_executor.c \
      tinydb_hash.c tinydb_hashmap.c tinydb_list.c tinydb_log.c tinydb_event_tcp_server.c \
      tinydb_event_client_handler.c tinydb_thread_pool.c tinydb_database_entry_destructor.c \
      tinydb_snapshot.c tinydb_pubsub.c tinydb_webhook.c tinydb_user_manager.c tinydb_ttl.c \
      tinydb_memory_pool.c tinydb_event_queue.c tinydb_query_parser.c tinydb_lex.c \
      tinydb_hashmap_iterator.c tinydb_object.c tinydb_utils.c tinydb_task_queue.c

OBJ = $(SRC:.c=.o)
EXEC = tinydb

CLIENT_SRC = tinydb_client.c
CLIENT_EXEC = tinydb-client
CLIENT_LDFLAGS = -lreadline

all: $(EXEC) client

$(EXEC): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(EXEC) $(LDFLAGS)

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_EXEC) $(CLIENT_LDFLAGS)

clean:
	rm -f $(OBJ) $(EXEC) $(CLIENT_EXEC)

.PHONY: all clean client
