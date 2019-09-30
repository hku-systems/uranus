
#include "securecompiler_u.h"
#include <sgx_urts.h>
// #include <iostream>

int __ENCLAVE_HEAP__ = 0x10000000;

int main() {
    sgx_enclave_id_t eid;
    if (sgx_create_enclave("/usr/lib/libenclave.so", 1, NULL, NULL, &eid, NULL) != SGX_SUCCESS) {
        // std::cout << "error\n";
        return -1;
    }
    char* ret;
    char buf[10];
    if (c1_initialize(eid, (void**)&ret, NULL, (void**)&buf, (void**)&buf, (void**)&buf, 0) != SGX_SUCCESS) {
        // std::cout << "init error\n";
    }
    return 0;
}