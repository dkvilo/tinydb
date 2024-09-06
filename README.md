
# TINY DB

TINY DB is a fast, in-memory key-value store designed with support for sharded data storage, persistent state management, and multi-user access control. It offers thread-safe operations and user-defined access levels for managing stored data.

# Features

- Scalable (each database can be divided into multiple shards, allowing for high scalability)
- Thread-Safe (safe concurrent read/write operations)
- Data Types (strings, numbers, and objects)
- Multi-User Support with Custom Access Levels (read, write, and delete permissions)
- Built-in TCP server for handling client connections.
- Asynchronous Task Processing (client requests are handled asynchronously using a task queue and a thread pool)


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

By default, the server will bind to all available interfaces ```INADDR_ANY``` and listen on the specified port ```PORT```.

This project is in its early stages, so certain configurations that should be easily adjustable are currently hardcoded. Additionally, some functionality, such as user management, access levels, and object type handling, is not fully implemented yet.



