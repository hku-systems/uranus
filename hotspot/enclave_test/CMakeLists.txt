cmake_minimum_required(VERSION 3.0)
project(hotspot)

# set(CMAKE_CXX_STANDARD 98)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
find_library(SGX_URTS sgx_urts ../lib/)

include_directories(../include/)
include_directories(../enclave/panoply/include/)

file(GLOB_RECURSE ocall_src ../src/share/vm/panoply/*)

add_executable(enclave_init_test main.cpp securecompiler_u.c em_ocall.cpp ${ocall_src})
target_link_libraries(enclave_init_test ${SGX_URTS} dl pthread)
