# RedisLiteX

RedisLiteX is a Redis-inspired in-memory key-value store written in C++.

It supports a small subset of Redis-style commands over TCP and can be tested using `redis-cli`.

The goal of this project was to understand how an in-memory database server handles networking, command parsing, concurrency, TTL-based expiration, and basic benchmarking.

---

## Features

- TCP server built using Linux socket programming
- RESP command parsing for basic `redis-cli` compatibility
- Thread-per-client concurrency model
- Thread-safe in-memory key-value store using `std::mutex`
- Redis-style commands:
  - `PING`
  - `SET`
  - `GET`
  - `DEL`
  - `EXISTS`
  - `INCR`
  - `DECR`
  - `EXPIRE`
  - `TTL`
- Lazy expiration of keys
- Background cleanup thread for expired keys
- Benchmark client for throughput and latency measurement
- Docker support

---

## Project Structure

```text
redislitex/
├── benchmark/
│   └── bench_client.cpp
├── include/
│   ├── protocol/
│   │   └── resp.hpp
│   ├── server/
│   │   └── tcp_server.hpp
│   └── store/
│       └── kv_store.hpp
├── src/
│   ├── main.cpp
│   ├── protocol/
│   │   └── resp.cpp
│   ├── server/
│   │   └── tcp_server.cpp
│   └── store/
│       └── kv_store.cpp
├── Dockerfile
├── docker-compose.yml
└── CMakeLists.txt
```

---

## Architecture

```text
redis-cli / client
        |
        v
TCP Server
        |
        v
Client Handler Thread
        |
        v
RESP Parser
        |
        v
Command Handler
        |
        v
Thread-safe KVStore
```

The server listens on port `6379`.

Each client connection is handled in a separate thread.

All client threads share the same `KVStore`, so access to the internal map is protected using a mutex.

---

## Build Locally

### Requirements

- Linux / WSL
- C++17 compiler
- CMake
- `redis-cli` for testing

Install dependencies on Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential cmake redis-tools -y
```

Build:

```bash
cmake -S . -B build
cmake --build build
```

Run server:

```bash
./build/redislitex
```

---

## Run with Docker

Build and run:

```bash
docker compose up --build
```

In another terminal:

```bash
redis-cli -p 6379 PING
```

Stop:

```bash
docker compose down
```

---

## Supported Commands

### PING

```bash
redis-cli -p 6379 PING
```

Output:

```text
PONG
```

### SET / GET

```bash
redis-cli -p 6379 SET name deepanshu
redis-cli -p 6379 GET name
```

Output:

```text
OK
"deepanshu"
```

### DEL

```bash
redis-cli -p 6379 DEL name
```

Output:

```text
(integer) 1
```

### EXISTS

```bash
redis-cli -p 6379 EXISTS name
```

Output:

```text
(integer) 1
```

### INCR / DECR

```bash
redis-cli -p 6379 SET count 10
redis-cli -p 6379 INCR count
redis-cli -p 6379 DECR count
```

Output:

```text
OK
(integer) 11
(integer) 10
```

### EXPIRE / TTL

```bash
redis-cli -p 6379 SET otp 1234
redis-cli -p 6379 EXPIRE otp 20
redis-cli -p 6379 TTL otp
```

Output:

```text
OK
(integer) 1
(integer) 19
```

---

## Expiration Design

RedisLiteX supports TTL-based key expiration.

Each key stores:

```text
value + optional expiry timestamp
```

There are two expiration mechanisms:

### 1. Lazy Expiration

When a key is accessed through commands like `GET`, `EXISTS`, `TTL`, `INCR`, or `DECR`, the server checks whether the key has expired.

If it has expired, the key is removed before continuing.

### 2. Background Cleanup

A background thread periodically scans the store and removes expired keys.

This avoids keeping expired keys in memory forever if they are never accessed again.

---

## Benchmarking

RedisLiteX includes a simple benchmark client.

Build first:

```bash
cmake -S . -B build
cmake --build build
```

Run the server:

```bash
./build/redislitex
```

Run benchmark:

```bash
./build/redislitex_bench 10 1000
./build/redislitex_bench 20 5000
```

Benchmark client behavior:

- creates multiple client threads
- opens TCP connections to the server
- sends RESP-formatted `SET` commands
- measures throughput and latency using `std::chrono`

### Sample Results

Tested locally on WSL:

| Clients | Requests | Throughput | p50 Latency | p95 Latency | p99 Latency |
|---:|---:|---:|---:|---:|---:|
| 10 | 1000 | 16005 ops/sec | 263 µs | 1805 µs | 3046 µs |
| 20 | 5000 | 11674 ops/sec | 204 µs | 2912 µs | 16412 µs |

The increase in p99 latency with more clients is expected because the current design uses a thread-per-client model and a single mutex-protected store.

---

## Design Decisions

### Thread-per-client model

The server creates one thread per connected client.

This was chosen because it is simple to implement and easy to understand for the first version.

A more scalable design could use:

- thread pool
- non-blocking sockets
- `epoll`

### Mutex-protected store

All client threads share the same in-memory store.

A mutex is used to prevent race conditions while reading or modifying the map.

The current design uses a single global lock inside `KVStore`.

This is simple but can reduce performance under high concurrency.

A better design could use:

- sharded maps
- per-shard locks
- lock-free structures for specific workloads

### RESP parser

The server supports a basic subset of RESP, enough for common `redis-cli` commands.

Current limitation:

- assumes a complete command is received in one `recv()` call

A production-grade implementation should maintain a per-client buffer and handle:

- partial reads
- pipelined commands
- malformed frames more robustly

---

## Limitations

This is a learning project, not a production database.

Current limitations:

- no persistence
- no replication
- no authentication
- no pipelining support
- RESP parser is minimal
- one thread per client
- single mutex-protected store
- supports only string values

---

## Future Improvements

Possible next improvements:

- append-only file persistence
- RDB-style snapshots
- pipelined RESP command handling
- sharded key-value store
- thread pool
- list/hash data types
- Pub/Sub support
- transactions using `MULTI`, `EXEC`, and `DISCARD`

---

## What I Learned

Through this project, I learned:

- how TCP servers accept and handle client connections
- how Redis-style RESP commands are structured
- how to build a simple in-memory key-value store
- how mutexes prevent race conditions in shared data
- how TTL and key expiration can be implemented
- how to benchmark throughput and latency under concurrent workloads