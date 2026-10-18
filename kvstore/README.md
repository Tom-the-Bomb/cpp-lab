# KV Store

A Redis-style in-memory key-value store

- LRU eviction using self-implemented doubly-linked list
- TTL using self-implemented min-heap
  - lazy expiry in commands + active expiry using background thread
  - at most 1000 pops per sweep to avoid hogging mutex
- internal storage using a self-implemented hashmap
  - 64-bit FNV-1a hashing + power of 2 buckets (singly-linked lists)
- prefix matching using a self-implemented trie
- concurrency support using a mutex with 1 thread per client
- persistence using an append-only file that is replayed on startup
  - compaction of AOF file (manual + automatic via background thread)
  - durability using `fsync` (mimics Redis's `appendfsync everysec`)
- raw TCP server on localhost
- As close to redis behavior as possible for most features
  - the same input parsing specifications (`Tokenize.cpp`) and output formats
  - RESP support
- proper resource safety, size limits, validation, and error handling (i.e. corrupted writes get trimmed)

## Usage

Build first (see [root README](../README.md))

```
kvstore [-h | --help] [--server] [--maxkeys <count>]
        [--aof <path>] [--aof-resize <factor>]

  (no flags)              REPL on stdin
  -h, --help              this message
  --server                TCP server on 127.0.0.1:6380
                          RESP clients (redis-cli -p 6380) receive RESP,
                          inline clients (nc 127.0.0.1 6380) receive text
  --maxkeys <count>       evict least recently used key when over <count>
                          keys (0 = off)
  --aof <path>            persistent storage across sessions by logging
                          writes to <path> and replaying them on startup
  --aof-resize <factor>   rewrite the AOF once it is at least 64kB and
                          <factor> times its last size (0 = off)
```

## Commands

```
PING [message]                  PONG, or <message>
ECHO <message>                  <message>
GET <key>                       value, or (nil)
MGET <key>...                   value or (nil) for each key
STRLEN <key>                    length of value, 0 if no key
SET <key> <value>               OK, clears TTL
MSET <key> <value> ...          OK, sets all pairs at once
SETNX <key> <value>             1 if set, 0 if key exists
INCR <key>                      increments by 1
INCRBY <key> <delta>            increments by <delta>
DECR <key>                      decrements by 1
DECRBY <key> <delta>            decrements by <delta>
APPEND <key> <value>            new length, keeps TTL
DEL <key>...                    number of keys removed
EXISTS <key>...                 number of keys that exist
DBSIZE                          number of keys
KEYS <prefix>*                  keys starting with <prefix>, sorted
KEYS <key>                      <key> if it exists
FLUSHALL                        OK, deletes every key
EXPIRE <key> <seconds>          1 if TTL set, 0 if no key (<= 0 expires now)
EXPIREAT <key> <unix-seconds>   1 if TTL set, 0 if no key (past expires now)
TTL <key>                       seconds left, -1 no TTL, -2 no key
PERSIST <key>                   1 if TTL removed, 0 otherwise
REWRITEAOF                      compact the AOF, 1 on success
```

## Leaks (macOS)

```sh
printf 'SET a 1\nGET a\n' | leaks --atExit -- ./build-plain/kvstore/kvstore
```
