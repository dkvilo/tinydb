
# TINY DB

TINY DB is a fast, in-memory key-value store designed with support for sharded data storage, persistent state management, and multi-user access control. It offers thread-safe operations and user-defined access levels for managing stored data.

# Features

- Scalable (each database can be divided into multiple shards)
- Thread-Safe (safe concurrent read/write operations)
- Data Types (strings, numbers, and objects)
- Multi-User Support with Custom Access Levels (read, write, and delete permissions)
- Built-in TCP server for handling client connections.
- Asynchronous Task Processing (client requests are handled asynchronously using a task queue and a thread pool)

## Performance Comparison: Redis vs Tiny DB

### System Information
- **Machine**: M1 MacBook Air (8GB RAM, macOS)
- **Redis Version**: v7.2.5
- **Tiny DB**: v0.0.1 (In Development)

### Benchmark Overview
The following table highlights the results of a stress test involving **1 million SET operations** distributed across **50 clients**. 

| **System**                | **Time to Complete** | **Ops per Second** | **Memory Policy** | **Max Memory** | **Optimizations** |
|---------------------------|----------------------|--------------------|-------------------|----------------|-------------------|
| **Redis (with `redis-py`)**| 26.06 seconds        | 38,372 ops/sec      | `allkeys-lru`     | 6 GB           | Default           |
| **Redis (Raw Socket)**     | 17.82 seconds        | 56,118 ops/sec      | `allkeys-lru`     | 6 GB           | Raw Socket        |
| **Tiny DB (Sharded)**      | 14.85 seconds        | 67,340 ops/sec      | Sharding (16 Buckets) | N/A         | Raw Socket        |

# Usage

### Build from source

```
make
```

### Starting the TCP Server

To start the server:

```sh
./tinydb
```

To connect the server:

```sh
nc host port
```

Or you can use libs/libtinydb.js simple client implementation for NodeJs

## Commands

| **Command**            | 
|------------------------|
| `SET <key> <value>`     |
| `GET <key>`             |
| `APPEND <key> <value>`  |
| `STRLEN <key>`          |
| `EXPORT snapshot.bin`   |


By default, the server will bind to all available interfaces ```INADDR_ANY``` and listen on the specified port ```PORT``` (config.h).

This project is in its early stages, so certain configurations that should be easily adjustable are currently hardcoded. Additionally, some functionality, such as user management, access levels, and object type handling, is not fully implemented.



