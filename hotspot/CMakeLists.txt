cmake_minimum_required(VERSION 3.0)
 project(hotspot)

set(CMAKE_CXX_STANDARD 98)

set(TOP_DIR ${CMAKE_SOURCE_DIR})

#set(CMAKE_C_FLAGS "-O2 ${DEBUG_FLAG} -nostdinc")
#set(CMAKE_CPP_FLAGS "-O2 ${DEBUG_FLAG} -nostdinc++")
# set(ENCLAVE_LINK_FLAGS "-Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles")

#include_directories(${TOP_DIR}/include/tlibc)
#include_directories(${TOP_DIR}/include/stdc++)
#include_directories(${TOP_DIR}/include/tlibcxx)
include_directories(${TOP_DIR}/include)
include_directories(${TOP_DIR}/enclave/share/vm/precompiled/)
include_directories(${TOP_DIR}/enclave/share/vm/)
include_directories(${TOP_DIR}/enclave/cpu/x86/vm/)
include_directories(${TOP_DIR}/enclave/os/linux/vm/)
include_directories(${TOP_DIR}/enclave/os/posix/vm/)
include_directories(${TOP_DIR}/enclave/os_cpu/linux_x86/vm/)
#include_directories(${TOP_DIR}/enclave/share/PanoplyEnclave/syscall_include)
#include_directories(${TOP_DIR}/enclave/share/PanoplyEnclave/include)
#include_directories(${TOP_DIR}/include)

add_definitions(-DLINUX -D_GNU_SOURCE -DAMD64 -DPRODUCT -DHOTSPOT_RELEASE_VERSION="25.71-b00"
        -DHOTSPOT_BUILD_TARGET="product" -DHOTSPOT_BUILD_USER="jianyu" -DHOTSPOT_LIB_ARCH="amd64"
        -DHOTSPOT_VM_DISTRO="OpenJDK"  -DTARGET_OS_FAMILY_linux -DTARGET_ARCH_x86 -DTARGET_ARCH_MODEL_x86_64
        -DTARGET_OS_ARCH_linux_x86 -DTARGET_OS_ARCH_MODEL_linux_x86_64
        -DTARGET_COMPILER_gcc -D_REENTRANT -DVM_LITTLE_ENDIAN -D_LP64=1
        -DJRE_RELEASE_VERSION="1.8.0-internal-jianyu_2018_04_23_00_03-b00"
        -DENCLAVE_UNIX -D__OpenBSD__)

find_library(SGX_TRTS_SIM sgx_trts_sim ${TOP_DIR}/lib)

file(GLOB_RECURSE src_app enclave/share/*.c*
        enclave/cpu/x86/*.c*
        enclave/os_cpu/*.c*
        enclave/os/linux/*.c*)

add_library(enclave SHARED ${src_app})

# set_target_properties(enclave PROPERTIES LINK_FLAGS ${ENCLAVE_LINK_FLAGS})

set_target_properties(enclave PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(enclave ${SGX_TRTS_SIM})

install (TARGETS enclave DESTINATION ${TOP_DIR}/lib)