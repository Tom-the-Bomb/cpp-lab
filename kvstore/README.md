# KV Store

## Run

Build first (see [root README](../README.md)).

```sh
./build/kvstore/kvstore                # REPL
./build/kvstore/kvstore --server       # server on 127.0.0.1:6380
nc 127.0.0.1 6380                      # client
./build-tsan/kvstore/kvstore --server  # server under TSan
```

TSan needs several clients sending at once to catch races.

## Leaks (macOS)

```sh
printf 'SET a 1\nGET a\n' | leaks --atExit -- ./build-plain/kvstore/kvstore
```
