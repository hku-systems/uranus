cmake_minimum_required(VERSION 3.0)
 project(hotspot)

set(CMAKE_CXX_STANDARD 98)


set(BUILD_ARCH "aarch64")
set(BUILD_64 "")
set(BUILD_ARCH_xx "aarch64")

# set(BUILD_ARCH "x86")
# set(BUILD_64 "_64")

# set(BUILD_ARCH "x86")
# set(BUILD_64 "_x64")

if(NOT DEFINED ENV{TEE_SDK})
 set(ENV{TEE_SDK} /opt/intel/sgxsdk/)
endif()

if(NOT DEFINED ENV{TEE_SDK})
 message(FATAL_ERROR "$TEE_SDK environment variable must be set.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
 set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
 set(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH} $ENV{TEE_SDK}/lib")
 set(SGX_ENCLAVE_SIGNER "$ENV{TEE_SDK}/bin/x86/sgx_sign")
 set(SGX_EDGER8R "$ENV{TEE_SDK}/bin/x86/sgx_edger8r")
else()
 set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
 set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "$ENV{TEE_SDK}/lib64")
 set(SGX_ENCLAVE_SIGNER "$ENV{TEE_SDK}/bin/${BUILD_ARCH_xx}/sgx_sign")
 set(SGX_EDGER8R "$ENV{TEE_SDK}/bin/${BUILD_ARCH_xx}/sgx_edger8r")
endif()

set(TOP_DIR ${CMAKE_SOURCE_DIR})

set(CMAKE_C_FLAGS "-O0 -g -DENCLAVE ${DEBUG_FLAG} -nostdinc -fpie -fstack-protector")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -nostdinc++")
set(CMAKE_CXX_FLAGS "-O0 -g -DENCLAVE ${DEBUG_FLAG} -fPIC -fno-rtti -fno-exceptions -fcheck-new -fvisibility=hidden -pipe -fno-strict-aliasing  -fno-omit-frame-pointer -Wno-write-strings -fpermissive")
set(ENCLAVE_LINK_FLAGS "-Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined -Wl,-pie,-eenclave_entry -Wl,--export-dynamic -Wl,--defsym,__ImageBase=0")

include_directories(${TOP_DIR}/include)
include_directories(${TOP_DIR}/include/tlibc)
include_directories(${TOP_DIR}/include/stdc++)
include_directories(${TOP_DIR}/include/stlport)
include_directories(${TOP_DIR}/enclave/panoply)
include_directories(${TOP_DIR}/enclave/panoply/include)
include_directories(${TOP_DIR}/enclave/panoply/syscall_include)
include_directories(${TOP_DIR}/enclave/share/vm/precompiled/)
include_directories(${TOP_DIR}/enclave/share/vm/)
include_directories(${TOP_DIR}/enclave/cpu/${BUILD_ARCH}/vm/)
include_directories(${TOP_DIR}/enclave/os/linux/vm/)
include_directories(${TOP_DIR}/enclave/os/posix/vm/)
include_directories(${TOP_DIR}/enclave/os_cpu/linux_${BUILD_ARCH}/vm/)
include_directories(${TOP_DIR}/enclave/share/vm/prims/)
include_directories(${TOP_DIR}/enclave/share/vm/enclave/sc/native_libs/)
# include_directories(${TOP_DIR}/enclave/panoply/include)
#include_directories(${TOP_DIR}/include/tlibc)
#include_directories(${TOP_DIR}/include/stlc++)

add_definitions(-DLINUX -D_GNU_SOURCE -DAMD64 -DPRODUCT -DHOTSPOT_RELEASE_VERSION="25.71-b00"
        -DHOTSPOT_BUILD_TARGET="product" -DHOTSPOT_BUILD_USER="jianyu" -DHOTSPOT_LIB_ARCH="amd64"
        -DHOTSPOT_VM_DISTRO="OpenJDK"  -DTARGET_OS_FAMILY_linux -DTARGET_ARCH_${BUILD_ARCH} -DTARGET_ARCH_MODEL_${BUILD_ARCH}${BUILD_64}
        -DTARGET_OS_ARCH_linux_${BUILD_ARCH} -DTARGET_OS_ARCH_MODEL_linux_${BUILD_ARCH}${BUILD_64} -DJDK_MAJOR_VERSION="1.7" -DJDK_MINOR_VERSION=".1" -DRELEASE="major"
        -DTARGET_COMPILER_gcc -D_REENTRANT -DVM_LITTLE_ENDIAN -D_LP64=1
        -DJRE_RELEASE_VERSION="1.8.0-internal-jianyu_2018_04_23_00_03-b00"
        -DENCLAVE_UNIX)

find_library(SGX_TRTS_SIM sgx_trts ${TOP_DIR}/lib)
find_library(SGX_TRTS_TLIBC sgx_tstdc ${TOP_DIR}/lib)
find_library(SGX_TRTS_TCXX sgx_tcxx ${TOP_DIR}/lib)
find_library(SGX_TRTS_TSTDCXX sgx_tstdcxx ${TOP_DIR}/lib)
find_library(SGX_TCRYPTO sgx_tcrypto ${TOP_DIR}/lib)

file(GLOB_RECURSE src_app 
        enclave/share/vm/asm/*.c*
        enclave/share/vm/c0/*pp
        enclave/share/vm/classfile/*.c*
        enclave/share/vm/code/*.c*
        enclave/share/vm/enclave/*.c*
        # enclave/share/vm/mpx/*.c*
        enclave/share/vm/interpreter/*.c*
        enclave/share/vm/memory/*.c*
        enclave/share/vm/oops/*.c*
        enclave/share/vm/prims/*.c*
        enclave/share/vm/runtime/*.c*
        # enclave/share/vm/udis/*.c*
        enclave/share/vm/utilities/*.c*
        enclave/cpu/${BUILD_ARCH}/*.c*
        enclave/os_cpu/linux_${BUILD_ARCH}/*.c*
        enclave/os/linux/*.c*
        enclave/panoply/*.c*
        )

file(GLOB src_assembly
        "enclave/os_cpu/linux_${BUILD_ARCH}/vm/*.s")

set_property(SOURCE enclave/os_cpu/linux_${BUILD_ARCH}/vm/linux_${BUILD_ARCH}${BUILD_64}.s PROPERTY LANGUAGE C)

add_library(enclave SHARED ${src_app} ${src_assembly})

set_target_properties(enclave PROPERTIES LINK_FLAGS ${ENCLAVE_LINK_FLAGS})

set_target_properties(enclave PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(enclave -Wl,--whole-archive "${SGX_TRTS_SIM}" -Wl,--no-whole-archive -Wl,--start-group "${SGX_TRTS_TLIBC}" "${SGX_TRTS_TCXX}" -Wl,--hash-style=both "${SGX_TCRYPTO}" -Wl,--end-group)

add_custom_command(
        COMMAND ${SGX_EDGER8R} --trusted ${CMAKE_SOURCE_DIR}/enclave/share/vm/enclave/sc/securecompiler.edl --search-path ${CMAKE_SOURCE_DIR}/enclave/panoply/ --search-path ${CMAKE_SOURCE_DIR}/enclave/share/vm/ --search-path "$ENV{TEE_SDK}/include"
        DEPENDS ${CMAKE_SOURCE_DIR}/make/linux/makefiles/enclave/securecompiler.edl
        OUTPUT ${CMAKE_BINARY_DIR}/securecompiler_t.h ${CMAKE_BINARY_DIR}/securecompiler_t.c)

install (TARGETS enclave DESTINATION ${TOP_DIR}/lib)
