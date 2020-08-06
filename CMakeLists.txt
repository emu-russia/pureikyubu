cmake_minimum_required (VERSION 2.8.11)
project (DolwinPlayground)

add_definitions (-D_LINUX)

add_subdirectory (SRC/Common/Scripts)

# Backends

add_subdirectory (SRC/Backends/VideoNull/Scripts)

# Hardware

add_subdirectory (SRC/DSP/Scripts)


add_subdirectory (SRC/UI/Playground/Scripts)
