add_executable(echo_server server.cpp)
target_compile_options(echo_server PRIVATE -fcoroutines)

add_executable(echo_client client.cpp)
target_compile_options(echo_client PRIVATE -fcoroutines)

add_executable(x x.cpp)
target_compile_options(x PRIVATE -fcoroutines)
