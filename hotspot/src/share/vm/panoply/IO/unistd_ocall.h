#ifndef PANOPLY_UNISTD_H
#define PANOPLY_UNISTD_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

off_t ocall_read(int fd, void *buf, size_t count);

int ocall_geterrno();

off_t ocall_write(int fd, const void *buf, size_t count);

off_t ocall_lseek(int fd, off_t offset, int whence);

int ocall_gethostname(char *name, size_t len);

int ocall_sethostname(const char *name, size_t len);

int ocall_close(int sock);

pid_t ocall_getpid(void);

pid_t ocall_getppid(void);

ssize_t ocall_pread(int fd, void *buf, size_t nbytes, off_t offset);

ssize_t ocall_pwrite(int fd, const void *buf, size_t n, off_t offset);

int ocall_pipe(int pipedes[2]);

int ocall_pipe2(int pipedes[2], int flag);

unsigned int ocall_sleep(unsigned int seconds);

unsigned int ocall_usleep(unsigned int seconds);

int ocall_chown(const char *file, uid_t owner, gid_t group);

int ocall_fchown(int fd, uid_t owner, gid_t group);

int ocall_lchown(const char *file, uid_t owner, gid_t group);

int ocall_chdir(const char *path);

int ocall_fchdir(int fd);

char *ocall_get_current_dir_name(void);

int ocall_dup(int fd);

int ocall_dup2(int fd, int fd2);

int ocall_dup3(int fd, int fd2, int flags);
/* Get the real user ID of the calling process.  */
uid_t ocall_getuid(void);

/* Get the effective user ID of the calling process.  */
uid_t ocall_geteuid(void);

/* Get the real group ID of the calling process.  */
gid_t ocall_getgid(void);

/* Get the effective group ID of the calling process.  */
gid_t ocall_getegid(void);

int ocall_getpagesize(void);

char *ocall_getcwd(char *buf, size_t size);

int ocall_unlink(const char *name);

int ocall_rmdir(const char *name);

// Fork is not supported and should be remove from all the code. Call fork will bring enclave to power transition lost state
pid_t ocall_fork(void);

void ocall__exit(int status);

long ocall_sysconf(int name);

int ocall_setgid(gid_t gid);

int ocall_setuid(uid_t uid);

int ocall_execvp(const char *file, const char **argv);

int ocall_ftruncate(int fd, off_t len);

void ocall_free(void* p);

int ocall_fsync(int fd);

int ocall_ftruncate64 (int __fd, __off64_t __length);

long int ocall_syscall(long int __sysno, void* val);

#if defined(__cplusplus)
}
#endif
#endif

