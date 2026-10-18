add_executable(kvstore src/main.cpp src/Store.cpp src/Command.cpp src/Reply.cpp src/Resp.cpp src/Server.cpp src/Fd.cpp)
target_link_libraries(kvstore PRIVATE shared)
