cmake_minimum_required(VERSION 3.20)
project(cpp_lab CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SANITIZERS "address,undefined" CACHE STRING "value passed to -fsanitize=")

add_compile_options(-Wall -Wextra -Wpedantic -g -fsanitize=${SANITIZERS})
add_link_options(-fsanitize=${SANITIZERS})

add_subdirectory(shared)
add_subdirectory(kvstore)
add_subdirectory(minidb)