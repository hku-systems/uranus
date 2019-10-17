#ifndef PANOPLY_TIME_H
#define PANOPLY_TIME_H
#if defined(__cplusplus)
extern "C" {
#endif

time_t ocall_time(time_t *t);

int ocall_gettimeofday(void *tv_cast, int tv_size, void *tz_cast, int tz_size);

int ocall_gettimeofday2(void *tv_cast, int tv_size);

clock_t ocall_clock(void);

time_t ocall_mktime(struct tm *tp);

struct tm *ocall_gmtime_r(const time_t *timer, struct tm *tp);

struct tm *ocall_localtime_r(const time_t *timer, struct tm *tp);

extern void increase_ocall_count();

#if defined(__cplusplus)
}
#endif
#endif
