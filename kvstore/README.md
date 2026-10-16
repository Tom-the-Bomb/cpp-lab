# KV Store

A Redis-style in-memory key-value store

- LRU eviction using self-implemented doubly-linked list
- TTL using a background thread and a self-implemented min-heap
- internal storage using a self-implemented hashmap (FNV-1a hashing)
- prefix matching using a self-implemented trie
- concurrency support using a mutex with 1 thread/client
- persistence using an append-only file that is replayed on startup
- raw TCP server on localhost

## Run

Build first (see [root README](../README.md)).

```sh
./build/kvstore/kvstore                            # REPL
./build/kvstore/kvstore --server                   # server on 127.0.0.1:6380
./build/kvstore/kvstore --server --maxkeys 1000    # turn on LRU eviction
./build/kvstore/kvstore --server --aof kvstore.aof # turn on persistent storage
nc 127.0.0.1 6380                                  # client
./build-tsan/kvstore/kvstore --server              # server under TSan
```

TSan needs several clients sending at once to catch races.

## Leaks (macOS)

```sh
printf 'SET a 1\nGET a\n' | leaks --atExit -- ./build-plain/kvstore/kvstore
```
