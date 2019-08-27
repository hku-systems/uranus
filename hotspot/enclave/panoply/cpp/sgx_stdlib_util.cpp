
#include <sgx_stdlib_util.h>

inline char *strdup (const char *s) {
	if (s == NULL) return NULL;
	int len = strlen (s) + 1;
	char *d = (char*)malloc (len);   // Space for length plus nul
	if (d == NULL) return NULL;          // No memory
	strncpy (d,s,len);                        // Copy the characters
	return d;                            // Return the new string
}

int atexit (void (*__func) (void)) {

}

char *getenv(const char *name)
{
	char* retval;
	ocall_getenv(&retval, name);
	return retval;
}

int putenv(char *string)
{
	int retval;
	ocall_putenv(&retval, string);
	return retval;
}

int clearenv(void)
{
	int retval;
	ocall_clearenv(&retval);
	return retval;
}

void exit(int status)
{
//	printf("Dummy exit call \n");
//	abort();
} 


int setenv(const char *name, const char *value, int replace)
{
	int retval;
	ocall_setenv(&retval, name, value, replace);
	return retval;
}

int unsetenv (const char *name)
{
	int retval;
	ocall_unsetenv(&retval, name);
	return retval;
}

inline int random()
{
	int ran;
	sgx_read_rand((unsigned char*)&ran, sizeof(int));
	return ran;
} 

void srandom (unsigned int seed)
{
//	fprintf(stdout, "Dummy srandom call \n");
}


int mkstemp(char *string)
{
	int retval;
	ocall_mkstemp(&retval, string);
	return retval;
}

char *mkdtemp(char *temp)
{
	char* retval;
	ocall_mkdtemp(&retval, temp);
	return retval;
}

