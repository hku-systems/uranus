
#include <sgx_time_util.h>

long int timezone;

char *ctime (const time_t *__timer) {
	return "";
}

time_t time(time_t * tim)
{
	time_t retval;
	ocall_time(&retval, tim);
	return retval;
}

int gettimeofday(void * tv, void * tz)
{
	int retval;
	if (tz!=NULL)
		ocall_gettimeofday(&retval, tv, sizeof(struct timeval), tz, sizeof(struct timezone));
	else	{
		// printf("Call the second get time of day 2 \n");
		int code = ocall_gettimeofday2(&retval, tv, sizeof(struct timeval));
		// printf("Return code is: %x", code);
	}
	return retval;
}

clock_t clock()	{
	clock_t c;
	ocall_clock(&c);
	return c;
}
