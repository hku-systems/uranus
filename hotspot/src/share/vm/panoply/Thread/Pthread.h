#ifndef PANOPLY_PTHREAD_H
#define PANOPLY_PTHREAD_H
#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sgx_eid.h>
#include <stdio.h>
#include <malloc.h>
#include "sgx_status.h"
#include "struct/sgx_pthread_struct.h"

int ocall_pthread_attr_init(SGX_WRAPPER_PTHREAD_ATTRIBUTE *__attr);

pthread_attr_t* getAttribute(SGX_WRAPPER_PTHREAD_ATTRIBUTE attr);

int ocall_pthread_attr_destroy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr);

pthread_t ocall_pthread_self();

int ocall_pthread_join(pthread_t pt, void **thread_result);

int ocall_pthread_detach(pthread_t pt);

int ocall_pthread_equal(pthread_t pt1, pthread_t pt2);

void ocall_pthread_exit(void *retval);

int ocall_pthread_cancel (pthread_t th);

void ocall_pthread_testcancel (void);

int ocall_pthread_attr_getdetachstate(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int *__detachstate);

int ocall_pthread_attr_setdetachstate(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __detachstate);

int ocall_pthread_attr_getguardsize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t *__guardsize);

int ocall_pthread_attr_setguardsize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __guardsize);

int ocall_pthread_attr_getschedpolicy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int *__policy);

int ocall_pthread_attr_setschedpolicy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __policy);

int ocall_pthread_attr_getstacksize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t *__stacksize);

int ocall_pthread_attr_setstacksize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __stacksize);

int ocall_pthread_setspecific(pthread_key_t key, const void *value);

void * ocall_pthread_getspecific(pthread_key_t key);

int ocall_pthread_key_create(pthread_key_t *key, void* destructor);

void *generic_ecall_routine(void* arguments);

int ocall_pthread_create(pthread_t *new_thread, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr,
                         unsigned long int job_id, sgx_enclave_id_t eid);
extern void increase_ocall_count();


#if defined(__cplusplus)
}
#endif
#endif