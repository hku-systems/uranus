#include <sgx_pthread_util.h>

int sgx_wrapper_pthread_create(pthread_t *newthread,
			   SGX_WRAPPER_PTHREAD_ATTRIBUTE attr,
			   void *(*start_routine) (void *),
			   void *arg)	
{
	// printf("sgx_wrapper_pthread_create \n");
	if (!initiated_self_id)
	{
		/* dangerous situation when enclave id is not initiated before using pthread wrapper  */
		/* need to halt the programme since we cannot make an ecall without eid */
//		fprintf(stderr, "The enclave self_id is not set. ");
		abort();
		return -1;
	}

	pthread_job_t new_job = {start_routine, arg};
	unsigned long int job_id = put_job(new_job);
	int retval; 
	ocall_pthread_create(&retval, newthread, attr, job_id, enclave_self_id);
	// printf("done sgx_wrapper_pthread_create \n");
	return retval;
}

pthread_t sgx_wrapper_pthread_self(void)
{
	pthread_t retval;
	ocall_pthread_self(&retval);
	return retval;
}

int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {

}

int pthread_attr_getstack(pthread_attr_t *attr,
						  void **stackaddr, size_t *stacksize) {

}

int sgx_wrapper_pthread_join(pthread_t pt, void **thread_result)
{
    int retval;
    ocall_pthread_join(&retval, pt, thread_result);
    return retval;
}

int sgx_wrapper_pthread_detach(pthread_t pt)
{
	int retval;
	ocall_pthread_detach(&retval, pt);
    return retval;
}

int sgx_wrapper_pthread_equal(pthread_t pt1, pthread_t pt2)
{
	int retval;
	ocall_pthread_equal(&retval, pt1, pt2);
    return retval;
}

void sgx_wrapper_pthread_exit(void *retval)
{
    ocall_pthread_exit(retval);
}

int sgx_wrapper_pthread_cancel (pthread_t th)
{
	int retval;
    ocall_pthread_cancel(&retval, th);
    return retval;
}

void sgx_wrapper_pthread_testcancel (void)
{
    ocall_pthread_testcancel();
}

int sgx_wrapper_pthread_attr_init(SGX_WRAPPER_PTHREAD_ATTRIBUTE *__attr)
{
	int retval;
    ocall_pthread_attr_init(&retval, __attr);
    return retval;
}

int sgx_wrapper_pthread_attr_destroy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr)
{
	int retval;
    ocall_pthread_attr_destroy(&retval, __attr);
    return retval;
}

int sgx_wrapper_pthread_attr_getdetachstate(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int *__detachstate)
{
	int retval;
    ocall_pthread_attr_getdetachstate(&retval, __attr, __detachstate);
    return retval;
}

int sgx_wrapper_pthread_attr_setdetachstate(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __detachstate)
{
	int retval;
    ocall_pthread_attr_setdetachstate(&retval, __attr, __detachstate);
    return retval;
}

int sgx_wrapper_pthread_attr_getguardsize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t *__guardsize)
{
	int retval;
    ocall_pthread_attr_getguardsize(&retval, __attr, __guardsize);
    return retval;
}

int sgx_wrapper_pthread_attr_setguardsize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __guardsize)
{
	int retval;
    ocall_pthread_attr_setguardsize(&retval, __attr, __guardsize);
    return retval;
}

int sgx_wrapper_pthread_attr_getschedpolicy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int *__policy)
{
	int retval;
    ocall_pthread_attr_getschedpolicy(&retval, __attr, __policy);
    return retval;
}

int sgx_wrapper_pthread_attr_setschedpolicy(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __policy)
{
	int retval;
    ocall_pthread_attr_setschedpolicy(&retval, __attr, __policy);
    return retval;
}

int sgx_wrapper_pthread_attr_getstacksize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t *__stacksize)
{
	int retval;
    ocall_pthread_attr_getstacksize(&retval, __attr, __stacksize);
    return retval;
}

int sgx_wrapper_pthread_attr_setstacksize(SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __stacksize)
{
	int retval;
    ocall_pthread_attr_setstacksize(&retval, __attr, __stacksize);
    return retval;
}


int sgx_wrapper_pthread_setspecific(pthread_key_t key, const void *value)
{
	int retval;
    ocall_pthread_setspecific(&retval, key, value);
    return retval;
}

void *pthread_getspecific(pthread_key_t key)
{
	void *retval;
	ocall_pthread_getspecific(&retval, key);
	return retval;
	// ocall_pthread_getspecific(key);
}

