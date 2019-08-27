
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

struct tm *gmtime_r(const time_t *timer, struct tm *tp)
{
	struct tm * retval;
	ocall_gmtime_r(&retval, timer, tp);
	return tp;
}

struct tm *localtime_r(const time_t *timer, struct tm *tp)
{
	struct tm * retval;
	ocall_localtime_r(&retval, timer, tp);
	return tp;
}

time_t mktime(struct tm *tp)	
{
	time_t retval;
	return ocall_mktime(&retval, tp);
}

clock_t clock()	{
	clock_t c;
	ocall_clock(&c);
	return c;
}
