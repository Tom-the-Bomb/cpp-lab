# CPP Lab

A collection of C++ mini projects

## Build

Requires CMake 3.20+ and a C++20 compiler.

```sh
cmake -S . -B build && cmake --build build                                # ASan + UBSan (default)
cmake -S . -B build-tsan -DSANITIZERS=thread && cmake --build build-tsan  # TSan
cmake -S . -B build-plain -DSANITIZERS= && cmake --build build-plain      # no sanitizers
```

- Executables: `/<build folder>/<project>/<executable>`
- `SANITIZERS` is passed to `-fsanitize=` and cached per build folder
- ASan and TSan can't be combined but UBSan works with either (`thread,undefined`)

## Leaks (macOS)

ASan leak detection isn't supported on macOS; use a `build-plain` binary:

```sh
leaks --atExit -- ./build-plain/<project>/<executable>
```
