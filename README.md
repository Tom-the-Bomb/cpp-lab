# CPP Lab

A collection of C++ mini projects

## Build

Requires CMake 3.20+ and a C++20 compiler

```sh
# ASan + UBSan (default)
cmake -S . -B build && cmake --build build

# TSan
cmake -S . -B build-tsan -DSANITIZERS=thread && cmake --build build-tsan

# no sanitizers
cmake -S . -B build-plain -DSANITIZERS= && cmake --build build-plain
```

- Executables: `<build folder>/<project>/<executable>`
- `SANITIZERS` is passed to `-fsanitize=` and cached per build folder
- ASan and TSan can't be combined but UBSan works with either

## Leaks (macOS)

ASan leak detection isn't supported on macOS so use a `build-plain` binary

```sh
leaks --atExit -- ./build-plain/<project>/<executable>
```
