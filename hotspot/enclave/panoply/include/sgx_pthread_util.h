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

#define PTHREAD_MUTEX_INITIALIZER SGX_THREAD_MUTEX_INITIALIZER
#define pthread_mutexattr_t sgx_thread_mutexattr_t
#define pthread_mutex_t sgx_thread_mutex_t

#define pthread_mutex_lock(A) sgx_thread_mutex_lock(A)
#define pthread_mutex_trylock(A) sgx_thread_mutex_trylock(A)
#define pthread_mutex_unlock(A) sgx_thread_mutex_unlock(A)
#define pthread_mutex_init(A, B) sgx_thread_mutex_init(A, B)
#define pthread_mutex_destroy(A) sgx_thread_mutex_destroy(A)

#define PTHREAD_COND_INITIALIZER SGX_THREAD_COND_INITIALIZER
#define pthread_cond_t sgx_thread_cond_t
#define pthread_condattr_t sgx_thread_condattr_t

#define pthread_cond_init(A, B) sgx_thread_cond_init(A, B)
#define pthread_cond_destroy(A) sgx_thread_cond_destroy(A)
#define pthread_cond_signal(A) sgx_thread_cond_signal(A)
#define pthread_cond_broadcast(A) sgx_thread_cond_broadcast(A)
#define pthread_cond_wait(A, B) sgx_thread_cond_wait(A, B)

#define pthread_create(A, B, C, D) sgx_wrapper_pthread_create(A, B, C, D)
#define pthread_self() sgx_wrapper_pthread_self()
#define pthread_join(A,B) sgx_wrapper_pthread_join(A, B)
#define pthread_equal(A, B) sgx_wrapper_pthread_equal(A, B)
#define pthread_detach(A) sgx_wrapper_pthread_detach(A)
#define pthread_exit(A) sgx_wrapper_pthread_exit(A)

#define pthread_cancel(A) sgx_wrapper_pthread_cancel(A)
#define pthread_testcancel(A) sgx_wrapper_pthread_testcancel(A)
#define pthread_attr_init(A) sgx_wrapper_pthread_attr_init(A)
#define pthread_attr_destroy(A) sgx_wrapper_pthread_attr_destroy(A)
#define pthread_attr_getdetachstate(A, B) sgx_wrapper_pthread_attr_getdetachstate(A, B)
#define pthread_attr_setdetachstate(A, B) sgx_wrapper_pthread_attr_setdetachstate(A, B)
#define pthread_attr_getguardsize(A, B) sgx_wrapper_pthread_attr_getguardsize(A, B)
#define pthread_attr_setguardsize(A, B) sgx_wrapper_pthread_attr_setguardsize(A, B)
#define pthread_attr_getschedpolicy(A, B) sgx_wrapper_pthread_attr_getschedpolicy(A, B)
#define pthread_attr_setschedpolicy(A, B) sgx_wrapper_pthread_attr_setschedpolicy(A, B)
#define pthread_attr_getstacksize(A, B) sgx_wrapper_pthread_attr_getstacksize(A, B)
#define pthread_attr_setstacksize(A, B) sgx_wrapper_pthread_attr_setstacksize(A, B)
//#define pthread_getattr_np(A, B) sgx_wrapper_pthread_getattr_np(A, B)

#define pthread_setspecific(A, B) sgx_wrapper_pthread_setspecific(A, B)
#define pthread_key_create(A, B) sgx_wrapper_pthread_key_create(A, B)

#endif