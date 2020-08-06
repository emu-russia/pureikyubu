cmake_minimum_required (VERSION 2.8.11)
project (DolwinPlayground)

add_definitions (-D_LINUX)

add_subdirectory (SRC/Common/Scripts)
add_subdirectory (SRC/Backends/VideoNull/Scripts)
add_subdirectory (SRC/UI/Playground/Scripts)
