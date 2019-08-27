#ifndef SGX_TIME_UTIL_H
#define SGX_TIME_UTIL_H

#include <sgx_stdio_util.h>
#include "struct/sgx_time_struct.h"
#include "proxy/sgx_time_t.h"

extern long int timezone;

typedef __clockid_t clockid_t;

char *ctime (const time_t *__timer);

time_t time(time_t * tim);

int gettimeofday(void * tv, void * tz);

struct tm *gmtime_r(const time_t *timer, struct tm *tp);

struct tm *localtime_r(const time_t *timer, struct tm *tp);

time_t mktime(struct tm *tp);
clock_t clock();

#endif
