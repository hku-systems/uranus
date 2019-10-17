#ifndef PANOPLY_STDIO_H
#define PANOPLY_STDIO_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_print_string(const char* s);

SGX_WRAPPER_FILE ocall_fopen(const char* filename, const char* mode);

SGX_WRAPPER_FILE ocall_fdopen(int fd, const char *modes);

FILE* getFile(int fd);

int ocall_fprint_string(SGX_WRAPPER_FILE FILESTREAM, const char* s);

int ocall_fileno(SGX_WRAPPER_FILE FILESTREAM);

int ocall_fclose(SGX_WRAPPER_FILE FILESTREAM);

int ocall_fputs(const char* str, SGX_WRAPPER_FILE FILESTREAM);

int ocall_feof(SGX_WRAPPER_FILE FILESTREAM);

void ocall_rewind(SGX_WRAPPER_FILE FILESTREAM);

int ocall_fflush(SGX_WRAPPER_FILE FILESTREAM);

size_t ocall_fread(void *ptr, size_t size, size_t nmemb, SGX_WRAPPER_FILE FILESTREAM);

size_t ocall_fwrite(const void * ptr, size_t size, size_t count, SGX_WRAPPER_FILE FILESTREAM);

int ocall_fprintf(SGX_WRAPPER_FILE FILESTREAM, const char* format, ...);

char* ocall_fgets(char * str, int num, SGX_WRAPPER_FILE FILESTREAM);

int ocall_getc_unlocked(SGX_WRAPPER_FILE FILESTREAM);

void ocall_flockfile(SGX_WRAPPER_FILE filehandle);

void ocall_funlockfile(SGX_WRAPPER_FILE filehandle);

off_t ocall_ftello(SGX_WRAPPER_FILE FILESTREAM);

int ocall_fseeko(SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence);

long ocall_ftell(SGX_WRAPPER_FILE FILESTREAM);

int ocall_fseek(SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence);

int ocall_ferror(SGX_WRAPPER_FILE FILESTREAM);

void ocall_perror(const char* s);

int ocall_getc(SGX_WRAPPER_FILE FILESTREAM);

int ocall_vfscanf(SGX_WRAPPER_FILE FILESTREAM, const char *format, void* ap);

int ocall_vscanf(const char *format, void* ap);

int ocall_putchar(int c);

int ocall_putc(int c, SGX_WRAPPER_FILE FILESTREAM);

int ocall_fputc(int c, SGX_WRAPPER_FILE FILESTREAM);

int ocall_rename(const char* _old, const char* _new);

#if defined(__cplusplus)
}
#endif
#endif

