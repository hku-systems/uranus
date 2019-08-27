
#include <sgx_syslog_util.h>

void closelog (void)
{
	// fprintf(stderr, "Fix me: Call dummy closelog \n");
}

/* Open connection to system logger.

   This function is a possible cancellation point and therefore not
   marked with THROW.  */
void openlog (const char *ident, int option, int facility)
{
	// fprintf(stderr, "Fix me: Call dummy closelog \n");
}

/* Set the log mask level.  */
int setlogmask (int mask)
{
	// fprintf(stderr, "Fix me: Call dummy closelog \n");
	return -1;
}

/* Generate a log message using FMT string and option arguments.

   This function is a possible cancellation point and therefore not
   marked with THROW.  */
void syslog (int pri, const char *fmt, ...)
{
	// fprintf(stderr, "Fix me: Call dummy closelog \n");
}
