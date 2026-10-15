# KV Store

## Run

Build first (see [root README](../README.md)).

```sh
./build/kvstore/kvstore                         # REPL
./build/kvstore/kvstore --server                # server on 127.0.0.1:6380
./build/kvstore/kvstore --server --maxkeys 1000 # turn on LRU eviction
nc 127.0.0.1 6380                               # client
./build-tsan/kvstore/kvstore --server           # server under TSan
```

TSan needs several clients sending at once to catch races.

## Leaks (macOS)

```sh
printf 'SET a 1\nGET a\n' | leaks --atExit -- ./build-plain/kvstore/kvstore
```
