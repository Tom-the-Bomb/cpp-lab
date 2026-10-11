cmake_minimum_required(VERSION 3.20)
project(cpp_lab CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -g -fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)

add_subdirectory(kvstore)