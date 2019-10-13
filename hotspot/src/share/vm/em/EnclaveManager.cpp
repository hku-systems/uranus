//
// Created by max on 12/29/17.
//

#include <dlfcn.h>
#include "EnclaveManager.h"
#include <stdio.h>
#include <sgx_urts.h>
#include <errno.h>

bool EnclaveManager::init = false;
sgx_enclave_id_t EnclaveManager::global_id = 0;

#ifdef SGX_HW
#define SGX_PSW_UAE_SO "libsgx_uae_service.so"
#define SGX_PSW_URT_SO "libsgx_urts.so"
#else
#define SGX_PSW_UAE_SO "libsgx_uae_service_sim.so"
#define SGX_PSW_URT_SO "libsgx_urts_sim.so"
#endif
int EnclaveManager::psw_installed_check() {
    if (init) {
        return 1;
    }
    void *handle_urts = NULL;
    void  *handle_uae_service = NULL;
    handle_uae_service = dlopen(SGX_PSW_UAE_SO , RTLD_NOW);
    if (!handle_uae_service) {
        printf("dlopen: uae_service not available %d\n", errno);
        return 0;
    }

    handle_urts = dlopen(SGX_PSW_URT_SO, RTLD_NOW);
    if (!handle_urts) {
        printf("dlopen: sgx_urts.so not avaialble\n");
        dlclose(handle_uae_service);
        handle_uae_service = NULL;
        return 0;
    }
    init = true;
    return 1;
}

sgx_enclave_id_t EnclaveManager::new_enclave(std::string enclave_path) {
    sgx_status_t ret = SGX_SUCCESS;
    sgx_launch_token_t token = {0};
    int updated = 0;
    if (!(psw_installed_check())) {
        printf(" PSW Not installed\n");
        return 0;
    }
    sgx_enclave_id_t current = global_id;
    global_id += 1;
    ret = sgx_create_enclave(enclave_path.c_str(), SGX_DEBUG_FLAG, &token, &updated, &current, NULL);
    if (ret != SGX_SUCCESS) {
        printf("\nApp: error %#x, failed to create enclave.\n", ret);
    }
    return current;
}