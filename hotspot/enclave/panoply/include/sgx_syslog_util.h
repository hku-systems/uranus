#ifndef SGX_SYSLOG_UTIL_H
#define SGX_SYSLOG_UTIL_H

#include <struct/sgx_syslog_struct.h>
#include <sgx_stdio_util.h>

void closelog (void);

void openlog (const char *ident, int option, int facility);

int setlogmask (int mask);

void syslog (int pri, const char *fmt, ...);


#endif