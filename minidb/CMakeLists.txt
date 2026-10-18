add_executable(minidb src/main.cpp src/Row.cpp src/Parser.cpp)
target_link_libraries(minidb PRIVATE shared)