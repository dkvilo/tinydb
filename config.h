#ifndef __TINY_DB_CONFIG
#define __TINY_DB_CONFIG

// port that TCP connection will be open
#define PORT 8079

// host that TCP connection will be open
#define HOST (u_int32_t)0x00000000

// total command message length in bytes
#define COMMAND_BUFFER_SIZE 4096

// total connections that server can queue
#define CONN_QUEUE_SIZE 128

// snapshot name that db will look for on startup
#define DEFAULT_SNAPSHOT_NAME "snapshot.bin"

// snapshot that will be created every time program will terminate
#define DEFAULT_EXIT_SNAPSHOT_NAME "on_exit_snapshot.bin"

// number of initial databases to be initalized by default on startup
#define NUM_INITAL_DATABASES 1

// this must be a power of 2 (e.g., 2, 4, 8, 16, 32 ...)
#define NUM_SHARDS 16

// max size of string buffer size in the list (1 MiB)
#define MAX_STRING_LENGTH 1000000

#endif // __TINY_DB_CONFIG