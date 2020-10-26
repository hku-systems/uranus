
#include <sgx_stdlib_util.h>
#include "proxy/sgx_stdlib_t.h"

static inline char *strdup (const char *s) {
	if (s == NULL) return NULL;
	int len = strlen (s) + 1;
	char *d = (char*)malloc (len);   // Space for length plus nul
	if (d == NULL) return NULL;          // No memory
	strncpy (d,s,len);                        // Copy the characters
	return d;                            // Return the new string
}
