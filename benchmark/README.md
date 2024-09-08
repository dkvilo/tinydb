## System Information:

- **Machine**: M1 MacBook Air (8GB RAM, macOS)
- **OS Version**: macOS
- **Processor**: Apple M1

## Version

- **Redis Server**: v=7.2.5 sha=00000000:0 malloc=libc bits=64 build=bd81cd1340e80580
- **Tiny DB**: v 0.0.1 (In Dev.)

## Stress Test Results Comparison

**Operation Type**: SET  
**Num Operations**: 1 Million  
**Num Clients**: 50

| **System**                | **Time to Complete** | **Ops per Second** | **Memory Policy** | **Persistence**   | **Max Memory** | **Max Clients** | **Optimizations**           | **Benchmark File Path**                        |
|---------------------------|----------------------|--------------------|-------------------|-------------------|----------------|----------------|-----------------------------|------------------------------------------------|
| **Redis (with `redis-py`)**| 26.06 seconds        | 38,372 ops/sec      | `allkeys-lru`     | Disabled (AOF: no) | 6 GB           | 1000           | Default                      | `redis_benchmark.py`                           |
| **Redis (without `redis-py`)** | 17.82 seconds      | 56,118 ops/sec      | `allkeys-lru`     | Disabled (AOF: no) | 6 GB           | 1000           | Raw Socket (RESP Encoding)   | `redis_raw_socket.py`                          |
| **Tiny DB**               | 14.85 seconds        | 67,340 ops/sec      | Num Shards 16      | N/A               | N/A            | N/A            | Raw Socket                   | `tinydb_benchmark.py`                          |


## Percentage Differences

- **Redis without `redis-py` vs Redis with `redis-py`**:
  - **Time**: 31.61% faster
  - **Ops/sec**: 46.27% more ops/sec

- **Tiny DB vs Redis with `redis-py`**:
  - **Time**: 43.03% faster
  - **Ops/sec**: 75.48% more ops/sec

- **Tiny DB vs Redis without `redis-py`**:
  - **Time**: 16.67% faster
  - **Ops/sec**: 20.04% more ops/sec
