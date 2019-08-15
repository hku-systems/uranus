//
// Created by max on 3/20/18.
//

#ifndef HOTSPOT_ENCLAVECRYPTO_H
#define HOTSPOT_ENCLAVECRYPTO_H


#include <sgx_tcrypto.h>
#include <cstdio>
#include "EnclaveMemory.h"
#include "EnclaveException.h"
#include "EnclaveOcall.h"

#define MACLEN 16
#define HASHLEN 32
#define IVLEN 12

typedef void* (allocate_t)(size_t);

#define ENCLAVE_HASH_SIZE SGX_CMAC_KEY_SIZE

class EnclaveCrypto {
public:
    static void* enclave_allocate(size_t size) {
        return EnclaveMemory::allocate(size, NULL);
    }

    static void* heap_allocate(size_t size) {
        return EnclaveMemory::allocate_heap(size, NULL);
    }

    static sgx_aes_gcm_128bit_key_t aes_key[16];

    static sgx_cmac_128bit_key_t cmac_key[16];

    static uint8_t aes_iv[12];

    static u_char * sgx_encrypt(JavaThread* __the_thread__, char* plain, size_t* ret_size,
                         allocate_t allocate = heap_allocate);

    static u_char * sgx_decrypt(JavaThread* __the_thread__,char* cipher, size_t* ret_size,
                         allocate_t allocate = enclave_allocate);

    static u_char * sgx_hash(JavaThread* __the_thread__, char* src, size_t size, allocate_t allocate = heap_allocate);

    static bool sgx_verify(JavaThread* __the_thread__, char* src, size_t size, char* hash);
};


#endif //HOTSPOT_ENCLAVECRYPTO_H
