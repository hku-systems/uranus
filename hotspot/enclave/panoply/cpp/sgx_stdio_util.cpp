
#include <sgx_stdio_util.h>


int printf(const char *fmt, ...)
{
	char buf[WRAPBUFSIZ] = {'\0'};
	int result;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, WRAPBUFSIZ, fmt, ap);
	va_end(ap);
	ocall_print_string(&result, buf);
	// printf("Return to wrapper \n ");
	return result;
}

static inline int sgx_wrapper_fprintf(int FILESTREAM, const char* fmt, ...)
{
	char buf[WRAPBUFSIZ] = {'\0'};
	int result = 0;
	va_list ap;
	va_start(ap, fmt);
//	vsnprintf(buf, WRAPBUFSIZ, fmt, ap);
	va_end(ap);
	ocall_fprint_string(&result, FILESTREAM, buf);
	return result;
}

int fprintf(FILE* fp, const char* fmt, ...) {
    return sgx_wrapper_fprintf(0, fmt);
}

int rename (const char *__old, const char *__new) {

}

// Fix me: Proposed wrapper of asprintf
int asprintf(char **s, const char* format, ...)
{
//	char buf[WRAPBUFSIZ] = {'\0'};
//	int res = 0;
//	va_list ap;
//	va_start(ap, format);
//	res += vsnprintf(buf, WRAPBUFSIZ, format, ap);
//	va_end(ap);
//	*s = (char*)malloc(strlen(buf)+1);
//	strncpy(*s,buf,strlen(buf)+1);
//	return res;
}

// Fix me: Every sprintf shoud change to snprintf due to context
// static inline int sgx_wrapper_sprintf(char* string, const char* fmt, ...)
// {
//     // char buf[BUFSIZ] = {'\0'};
//     int size = sizeof(string);
//     // sgx_wrapper_printf("Call sprintf with string size: %d", size);
//     int result;
//     va_list ap;
//     va_start(ap, fmt);
//     int ret = 0;
//     ret = vsnprintf(string, size, fmt, ap);
//     result += ret;
//     va_end(ap);
//     // ocall_fprint_string(&result, buf);
//     return result;
// }


// static inline int sgx_wrapper_printf(const char* format, ...)
// {
// 	int result = 0;
// 	va_list ap;
// 	va_start(ap, format);
// 	int ret;
// 	result += ret;
//     ocall_vprintf(&ret, format, &ap);
//     va_end(ap);
//     return result;
// }

// static inline int sgx_wrapper_fprintf(int FILESTREAM, const char* format, ...)
// {
// 	sgx_wrapper_printf("sgx_wrapper_fprintf \n");
// 	int result = 0;
// 	va_list ap;
// 	va_start(ap, format);
// 	int ret;
//     ocall_vfprintf(&ret, FILESTREAM, format, &ap);
//     result += ret;
//     va_end(ap);
//     sgx_wrapper_printf("sgx_wrapper_fprintf \n");
//     return result;
// }

// static inline int sgx_wrapper_sprintf(char* string, const char* format, ...)
// {
// 	sgx_wrapper_printf("sgx_wrapper_sprintf \n");
// 	int result = 0;
// 	va_list ap;
// 	va_start(ap, format);
// 	int ret;
//     ocall_vsprintf(&ret, string, format, &ap);
//     result += ret;
//     va_end(ap);
//     sgx_wrapper_printf("sgx_wrapper_sprintf \n");
//     return result;
// }

static inline SGX_WRAPPER_FILE sgx_wrapper_fdopen(int fd, const char* mode)
{
	SGX_WRAPPER_FILE f = 0;
	ocall_fdopen(&f, fd, mode);
	return f;
}

FILE* fdopen(int fd, const char* mode) {
    sgx_wrapper_fdopen(fd, mode);
    return NULL;
}

static inline int sgx_wrapper_fileno(SGX_WRAPPER_FILE stream)
{
	int retval;
	ocall_fileno(&retval, stream);
	return retval;
}

int fileno(FILE* fp) {
    return 0;
}

static inline int sgx_wrapper_fclose(SGX_WRAPPER_FILE file)
{
	int ret = 0;
	ocall_fclose(&ret, file);
	return ret;
}

int fclose(FILE *fp) {
    return sgx_wrapper_fclose(0);
}

static inline int sgx_wrapper_fputs(const char* str, SGX_WRAPPER_FILE file)
{
	int ret = 0;
	ocall_fputs(&ret, str, file);
	return ret;
}

int fputs(const char *str, FILE* fp) {
    return sgx_wrapper_fputs(str, 0);
}

static inline char* sgx_wrapper_fgets(char* str, int num, SGX_WRAPPER_FILE FILESTREAM)
{
	char* ret;
	ocall_fgets(&ret, str, num, FILESTREAM);
	return ret;
}

char* fgets(char* str, int num, FILE* fp) {
    return sgx_wrapper_fgets(str, num, 1);
}

static inline int sgx_wrapper_feof(SGX_WRAPPER_FILE file)
{
	int ret = 0;
	ocall_feof(&ret, file);
	return ret;
}

int feof(FILE* file)
{
	return sgx_wrapper_feof(0);
}
static inline size_t sgx_wrapper_fread(void* ptr, size_t size, size_t nmemb, SGX_WRAPPER_FILE FILESTREAM)
{
	size_t ret;
	ocall_fread(&ret, ptr, size, nmemb, FILESTREAM);
	return ret;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE *fp)
{
	return sgx_wrapper_fread(ptr, size, nmemb, 0);
}

static inline size_t sgx_wrapper_fwrite(const void* ptr, size_t size, size_t count, SGX_WRAPPER_FILE FILESTREAM)
{
	size_t ret;
	ocall_fwrite(&ret, ptr, size, count, FILESTREAM);
	return ret;
}


size_t fwrite(const void* ptr, size_t size, size_t count, FILE *fp)
{
	sgx_wrapper_fwrite(ptr, size, count, 0);
}

static inline int sgx_wrapper_fseeko(SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence)
{
	int ret;
	ocall_fseeko(&ret, FILESTREAM, offset, whence);
	return ret;
}

int fseeko(FILE* fp, off_t offset, int whence) {
    return sgx_wrapper_fseeko(0, offset, whence);
}

static inline off_t sgx_wrapper_ftello(SGX_WRAPPER_FILE FILESTREAM)
{
	off_t ret;
	ocall_ftello(&ret, FILESTREAM);
	return ret;
}

off_t  ftello(FILE *fp) {
    return sgx_wrapper_ftello(0);
}

static inline int sgx_wrapper_fseek(SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence)
{
	int ret;
	ocall_fseek(&ret, FILESTREAM, offset, whence);
	return ret;
}

int fseek(FILE *fp, off_t offset, int whence) {
    return sgx_wrapper_fseek(0, offset, whence);
}

static inline off_t sgx_wrapper_ftell(SGX_WRAPPER_FILE FILESTREAM)
{
	off_t ret;
	ocall_ftell(&ret, FILESTREAM);
	return ret;
}

off_t  ftell(FILE *fp) {
    return sgx_wrapper_ftell(0);
}

static inline int sgx_wrapper_ferror(SGX_WRAPPER_FILE FILESTREAM)
{
	int ret;
	ocall_ferror(&ret, FILESTREAM);
	return ret;
}

int ferror(FILE *fp) {
    return sgx_wrapper_ferror(0);
}

static inline void sgx_wrapper_perror(const char* s)
{
	ocall_perror(s);
}

static inline int sgx_wrapper_fflush(SGX_WRAPPER_FILE FILESTREAM)
{
	int ret;
	ocall_fflush(&ret, FILESTREAM);
	return ret;
}

int fflush(FILE *fp) {
    return sgx_wrapper_fflush(0);
}

static inline int sgx_wrapper_vfprintf(SGX_WRAPPER_FILE FILESTREAM, const char* format, void* val)
{
	int ret;
	ocall_vfprintf(&ret, FILESTREAM, format, val);
	return ret;
}

int vfprintf(FILE *fp, const char* format, void *val) {
    return sgx_wrapper_vfprintf(0, format, val);
}

int vprintf(const char* format, void* val)
{
	int ret;
	ocall_vprintf(&ret, format, val);
	return ret;
}

int vsprintf(char* string, const char* format, void* val)
{
	int ret;
	ocall_vsprintf(&ret, string, format, val);
	return ret;
}
int vasprintf(char** string, const char* format, void* val)
{
	int ret;
	ocall_vasprintf(&ret, string, format, val);
	return ret;
}
static inline int sgx_wrapper_getc(SGX_WRAPPER_FILE FILESTREAM)
{
	int ret;
	ocall_getc(&ret, FILESTREAM);
	return ret;
}

int getc(FILE *fp) {
    return sgx_wrapper_getc(1);
}

static inline int sgx_wrapper_getchar()
{
	return sgx_wrapper_getc(SGX_STDIN);
}


static inline int sgx_wrapper_vfscanf(SGX_WRAPPER_FILE FILESTREAM, const char *format, void* ap)
{
	int retval;
	ocall_vfscanf(&retval, FILESTREAM, format, ap);
	return retval;
}

int vfscanf(FILE *fp, const char *format, void *ap) {
    return sgx_wrapper_vfscanf(1, format, ap);
}

int vscanf(const char *format, void* ap)
{
	int retval;
	ocall_vscanf(&retval, format, ap);
	return retval;
}

int vsscanf(const char* s, const char *format, void* ap)
{
	int retval;
	ocall_vsscanf(&retval, s, format, ap);
	return retval;
}

int putchar(int c)
{
	int retval;
	ocall_putchar(&retval, c);
	return retval;
}

static inline int sgx_wrapper_putc(int c, SGX_WRAPPER_FILE FILESTREAM)
{
	int retval;
	ocall_putc(&retval, c, FILESTREAM);
	return retval;
}

int putc(int c, FILE *fp) {
    return sgx_wrapper_putc(c, 0);
}


static inline int sgx_wrapper_fputc(int c, SGX_WRAPPER_FILE FILESTREAM)
{
	int retval;
	ocall_fputc(&retval, c, FILESTREAM);
	return retval;
}

int fputc(int c, FILE *fp) {
    return sgx_wrapper_fputc(c, 0);
}

// #define fprintf(A, B, ...) ocall_fprintf(A, B, ##__VA_ARGS__)
// static inline int sgx_wrapper_scanf(const char* format, ...)
// {
// 	sgx_wrapper_printf("sgx_wrapper_scanf \n");
// 	int result = 0;
// 	va_list ap;
// 	va_start(ap, format);
// 	int ret;
// 	result += ret;
//     ocall_vscanf(&ret, format, &ap);
//     va_end(ap);
//     sgx_wrapper_printf("done sgx_wrapper_scanf \n");
//     return result;
// }

static inline int sgx_wrapper_fscanf(SGX_WRAPPER_FILE FILESTREAM, const char* format, ...)
{
	printf("sgx_wrapper_fscanf \n");
	int result = 0;
	va_list ap;
	va_start(ap, format);
	int ret;
	result += ret;
	ocall_vfscanf(&ret, FILESTREAM, format, &ap);
	va_end(ap);
	printf("done sgx_wrapper_fscanf \n");
	return result;
}

int fscanf(FILE *fp, const char* format, ...) {
    return sgx_wrapper_fscanf(0, format);
}

static inline SGX_WRAPPER_FILE sgx_wrapper_fopen(const char* filename, const char* mode)
{
	SGX_WRAPPER_FILE f = 0;
	// sgx_wrapper_printf("Call ocall_fopen: %s \n", filename);
	int err = ocall_fopen(&f, filename, mode);
	// sgx_wrapper_printf("ocall_fopen error code: %x \n", err);
	return f;
}

FILE* fopen(const char* filename, const char* mode) {
    return NULL;
}

// static inline int sgx_wrapper_sscanf(const char* s, const char* format, ...)
// {
// 	sgx_wrapper_printf("sgx_wrapper_sscanf \n");
// 	int result = 0;
// 	va_list ap;
// 	va_start(ap, format);
// 	int ret;
//     int status = ocall_vsscanf(&ret, s, format, &ap);
//     result += ret;
//     // sgx_wrapper_printf("\n n The ocall return status code ocall_vsscanf is: %x \n", status);
//     va_end(ap);
//     // sgx_wrapper_printf("sgx_wrapper_sscanf return result is: %d\n", result);
//     sgx_wrapper_printf("sgx_wrapper_sscanf \n");
//     return result;
// }

// static inline int sgx_wrapper_asprintf(char** string, const char* format, ...)
// {
// 	sgx_wrapper_printf("sgx_wrapper_asprintf \n");
// 	int result = 0;
// 	va_list ap;
// 	va_start(ap, format);
// 	int ret;
//     ocall_vasprintf(&ret, string, format, &ap);
//     result += ret;
//     va_end(ap);
//     sgx_wrapper_printf("sgx_wrapper_asprintf \n");
//     return result;
// }

static inline void sgx_wrapper_rewind(SGX_WRAPPER_FILE FILESTREAM) {
	ocall_rewind(FILESTREAM);
}

void rewind(FILE *fp) {
	ocall_rewind(0);
}

int sprintf (char *__restrict __s,
					const char *__restrict __format, ...) {

}

//int snprintf (char *__restrict __s, size_t __maxlen,
//					 const char *__restrict __format, ...) {
//
//}

int sscanf (const char *__restrict __s,
            const char *__restrict __format, ...) {

}

int remove (const char *__filename) {

}
