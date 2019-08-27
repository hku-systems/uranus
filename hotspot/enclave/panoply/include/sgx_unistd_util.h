#ifndef SGX_UNISTD_UTIL_H
#define SGX_UNISTD_UTIL_H

#include <cstring>
#include "struct/sgx_unistd_struct.h"
#include "proxy/sgx_unistd_t.h"

#ifdef __cplusplus
extern "C" {
#endif

int access (const char *__name, int __type);

long int syscall (long int __sysno, ...);

int gethostname(char *name, size_t len);

int sethostname(const char *name, size_t len);

int ftruncate64 (int __fd, __off64_t __length);
 
long lseek(int fd, long offset, int whence);

int fsync(int fd);

long read(int fd, void *buf, size_t count);

long write(int fd, const void *buf, size_t count);

int close(int fd);

pid_t getpid(void);

pid_t getppid(void);

ssize_t pread(int fd, void *buf, size_t nbytes, off_t offset);

ssize_t pwrite(int fd, const void *buf, size_t n, off_t offset);

char *get_current_dir_name(void);

unsigned int sleep(unsigned int seconds);

unsigned int usleep(unsigned int seconds);

int pipe(int pipedes[2]);

int pipe2(int pipedes[2], int flag);

int chown(const char *file, uid_t owner, gid_t group);

int fchown(int fd, uid_t owner, gid_t group);

int lchown(const char *file, uid_t owner, gid_t group);

int chdir(const char *path);

int fchdir(int fd);

int dup(int fd);

int dup2(int fd, int fd2);

int dup3(int fd, int fd2, int flags);
/* Get the real user ID of the calling process.  */
uid_t getuid(void);

/* Get the effective user ID of the calling process.  */
uid_t geteuid(void);

/* Get the real group ID of the calling process.  */
gid_t getgid(void);

/* Get the effective group ID of the calling process.  */
gid_t getegid(void);

int getpagesize(void);

char *getcwd(char *buf, size_t size);

int unlink(const char *name);

int rmdir(const char *name);

void _exit(int status);

long sysconf(int name);

int setuid(uid_t uid);

int setgid(gid_t gid);

char *strcpy (char *__restrict __dest, const char *__restrict __src);

char *strcat (char *__restrict __dest, const char *__restrict __src);

long lseek64(int fd, long offset, int whence);

#ifdef __cplusplus
}
#endif

/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */


#endif

