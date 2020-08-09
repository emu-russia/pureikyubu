cmake_minimum_required (VERSION 2.8.11)
project (ThreadDemo)

add_definitions (-D_LINUX)

add_subdirectory (../../SRC/Common/Scripts Common)

add_executable (ThreadDemo 
	ThreadDemo.cpp 
)

target_link_libraries (ThreadDemo LINK_PUBLIC Common)

find_package (Threads)
target_link_libraries (ThreadDemo ${CMAKE_THREAD_LIBS_INIT})
