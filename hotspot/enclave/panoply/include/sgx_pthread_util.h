#ifndef SGX_PTHREAD_UTIL_H
#define SGX_PTHREAD_UTIL_H 1

#include <sgx_thread.h>
#include <pthread.h>
#include "struct/sgx_pthread_struct.h"
#include "proxy/Pthread_t.h"
#include "sgx_stdio_util.h"

extern sgx_enclave_id_t enclave_self_id;
extern bool initiated_self_id;

int sgx_wrapper_pthread_create(pthread_t *newthread,
											 SGX_WRAPPER_PTHREAD_ATTRIBUTE attr,
											 void *(*start_routine) (void *),
											 void *arg);

pthread_t sgx_wrapper_pthread_self(void);

int sgx_wrapper_pthread_join(pthread_t pt, void **thread_result);

int sgx_wrapper_pthread_detach(pthread_t pt);

int sgx_wrapper_pthread_equal(pthread_t pt1, pthread_t pt2);

void sgx_wrapper_pthread_exit(void *retval);

int sgx_wrapper_pthread_cancel (pthread_t th);

void sgx_wrapper_pthread_testcancel (void);

int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr);

int pthread_attr_getstack(pthread_attr_t *attr,
						  void **stackaddr, size_t *stacksize);

int sgx_wrapper_pthread_attr_init(SGX_WRAPPER_PTHREAD_ATTRIBUTE *__attr);

int sgx_wrapper_pthread_attr_destroy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr);

int sgx_wrapper_pthread_attr_getdetachstate(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int *__detachstate);

int sgx_wrapper_pthread_attr_setdetachstate(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __detachstate);

int sgx_wrapper_pthread_attr_getguardsize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t *__guardsize);

int sgx_wrapper_pthread_attr_setguardsize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __guardsize);

int sgx_wrapper_pthread_attr_getschedpolicy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int *__policy);

int sgx_wrapper_pthread_attr_setschedpolicy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __policy);

int sgx_wrapper_pthread_attr_getstacksize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t *__stacksize);

int sgx_wrapper_pthread_attr_setstacksize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __stacksize);

int sgx_wrapper_pthread_setspecific(pthread_key_t key, const void *value);

void *pthread_getspecific(pthread_key_t key);

#define pthread_self() sgx_thread_self()

#endif