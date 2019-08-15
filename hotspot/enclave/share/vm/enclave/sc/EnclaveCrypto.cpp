//
// Created by max on 3/20/18.
//

#include "EnclaveCrypto.h"

sgx_aes_gcm_128bit_key_t EnclaveCrypto::aes_key[16] = { 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b',
                                                    'c', 'd' };

sgx_cmac_128bit_key_t EnclaveCrypto::cmac_key[16] = { 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b',
                                                        'c', 'd' };

uint8_t EnclaveCrypto::aes_iv[12] = { 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd' };

u_char* EnclaveCrypto::sgx_encrypt(JavaThread *__the_thread__, char *plain, size_t *ret_size, allocate_t allocate) {
    size_t size = *ret_size;

    if (size <= 0) {
        ENCLAVE_THROW_0(EnclaveException::java_lang_NegativeArraySizeException);
    }

    uint8_t* cipher = (uint8_t*) allocate(size + MACLEN);

    uint8_t macbuffer[MACLEN];
    sgx_status_t status = sgx_rijndael128GCM_encrypt(aes_key, (u_char*)plain, size, cipher, aes_iv, IVLEN, NULL, 0, &macbuffer);

    if (status != SGX_SUCCESS) {
        printf("encryption error: %d %lx %d, %lx %lx\n", status, (intptr_t)plain, size, (intptr_t)cipher);
    }

    memcpy(cipher + size, macbuffer, MACLEN);
    *ret_size = size + MACLEN;

    return cipher;
}

u_char* EnclaveCrypto::sgx_decrypt(JavaThread *__the_thread__, char *cipher, size_t *ret_size, allocate_t allocate) {
    size_t size = *ret_size;

    if (size <= 0) {
        ENCLAVE_THROW_0(EnclaveException::java_lang_NegativeArraySizeException);
    }

    uint8_t macbuffer[MACLEN];
    char* mac_pos = cipher + size - MACLEN;
    memcpy(macbuffer, mac_pos, MACLEN);
    size -= MACLEN;

    uint8_t* plain = (uint8_t*) allocate(size);

    sgx_status_t status = sgx_rijndael128GCM_decrypt(aes_key, (u_char*)cipher, size, plain, aes_iv, IVLEN, NULL, 0,
                                                     (const sgx_aes_gcm_128bit_tag_t*) &macbuffer);

    if (status != SGX_SUCCESS) {
        printf("decryption error %d\n", status);
    }

    *ret_size = size;

    return plain;
}

u_char* EnclaveCrypto::sgx_hash(JavaThread *__the_thread__, char *src, size_t size, allocate_t allocate) {
    sgx_cmac_128bit_tag_t *tag = (sgx_cmac_128bit_tag_t*)heap_allocate(sizeof(sgx_cmac_128bit_tag_t));
    sgx_rijndael128_cmac_msg(cmac_key, (const uint8_t*)src, size, tag);
    return (u_char*)tag;
}

bool EnclaveCrypto::sgx_verify(JavaThread *__the_thread__, char *src, size_t size, char *hash) {
    sgx_cmac_128bit_tag_t *tag = (sgx_cmac_128bit_tag_t*)heap_allocate(sizeof(sgx_cmac_128bit_tag_t));
    sgx_rijndael128_cmac_msg(cmac_key, (u_char*)src, size, tag);
    return (strncmp(hash, (char*)tag, sizeof(sgx_cmac_128bit_tag_t)) == 0);
}