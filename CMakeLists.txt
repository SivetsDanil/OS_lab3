cmake_minimum_required(VERSION 3.20)
project(OS_lab3 C)

set(CMAKE_C_STANDARD 11)

find_package(Threads REQUIRED)

add_executable(parent parent.c)
add_executable(child child.c)
target_link_libraries(parent Threads::Threads)
target_link_libraries(child Threads::Threads)
