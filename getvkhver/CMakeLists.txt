cmake_minimum_required(VERSION 3.10)
project(getvkhver CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(getvkhver getvkhver.cpp)
string(REPLACE "\"" "" VKINC "${INC}")
target_include_directories(getvkhver BEFORE PRIVATE ${VKINC})
#message(STATUS "%%%%%%%%%%% ${VKINC}")

