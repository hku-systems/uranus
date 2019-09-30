#ifndef SECURECOMPILER_U_H__
#define SECURECOMPILER_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_satus_t etc. */

#include "struct/sgx_pthread_struct.h"
#include "sgx_eid.h"
#include "time.h"
#include "struct/sgx_pwd_struct.h"
#include "struct/sgx_grp_struct.h"
#include "struct/sgx_utsname_struct.h"
#include "sgx/sys/types.h"
#include "stdarg.h"
#include "sgx/sys/types.h"
#include "struct/sgx_stdio_struct.h"
#include "struct/sgx_syseventfd_struct.h"
#include "sgx/sys/types.h"
#include "struct/sgx_syssocket_struct.h"
#include "sgx/sys/types.h"
#include "struct/sgx_netdb_struct.h"
#include "struct/sgx_netinetin_struct.h"
#include "sgx/sys/types.h"
#include "sgx/sys/types.h"
#include "struct/sgx_sysuio_struct.h"
#include "sgx/sys/types.h"
#include "struct/sgx_sysmman_struct.h"
#include "struct/sgx_poll_struct.h"
#include "struct/sgx_sysepoll_struct.h"
#include "struct/sgx_sysselect_struct.h"
#include "sgx/sys/types.h"
#include "struct/sgx_syswait_struct.h"
#include "sgx/sys/types.h"
#include "sgx/sys/stat.h"
#include "struct/sgx_dirent_struct.h"
#include "struct/sgx_sysresource_struct.h"
#include "struct/sgx_arpainet_struct.h"
#include "sgx/sys/types.h"
#include "struct/sgx_signal_struct.h"
#include "struct/sgx_ifaddrs_struct.h"
#include "struct/sgx_netif_struct.h"
#include "dlfcn.h"

#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_interpreter, (void* r14, int size, void* method, void* thread, void* sender));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_invoke, (int byte, void* mh, int bci, void* recv, int idx, void* bcp, void* recv_klass));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_method_C, (int bc, void* constant, int idx));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_invoke_C, (void* recv, void* constant, int bidx, int bc, void* recv_klass));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_invoke_handle, ());
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_invoke_dynamic, ());
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_ldc, (bool wide, void* p, int index));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_ldc, (void* p, int index, int byte));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_get_put, (int byte, void* mh, int idx, void* bcp));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_resolve_get_put_klass, (int byte, void* mh, int idx, void* bcp));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_klass_type_array, ());
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_klass_type, ());
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_obj_array_klass_get, (void* pool, int index));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_klass_resolve_or_fail, (const char* name));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_klass_find_or_null, (const char* name));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_klass_get, (void* pool, int index));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_klass_compute_oopmap, (void* klass, void* m, int bci));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_array_klass, (void* klass, int rank, int get_all));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_multi_array_klass_get, (void* pool, int index));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_pre_native, (void* method, int resolve));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_jvm_malloc, (int size));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_create, (pthread_t* new_thread, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, unsigned long int job_id, sgx_enclave_id_t eid));
pthread_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_self, ());
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_join, (pthread_t pt, void** thread_result));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_detach, (pthread_t pt));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_equal, (pthread_t pt1, pthread_t pt2));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_exit, (void* retval));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_cancel, (pthread_t th));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_testcancel, ());
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_init, (SGX_WRAPPER_PTHREAD_ATTRIBUTE* __attr));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_destroy, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_getdetachstate, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int* __detachstate));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_setdetachstate, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __detachstate));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_getguardsize, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t* __guardsize));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_setguardsize, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __guardsize));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_getschedpolicy, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int* __policy));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_setschedpolicy, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __policy));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_getstacksize, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t* __stacksize));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_attr_setstacksize, (SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __stacksize));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_setspecific, (pthread_key_t key, const void* value));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_getspecific, (pthread_key_t key));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pthread_key_create, (pthread_key_t* key, void* destructor));
time_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_time, (time_t* t));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gettimeofday, (void* tv, int tv_size, void* tz, int tz_size));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gettimeofday2, (void* tv, int tv_size));
clock_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_clock, ());
struct tm* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gmtime_r, (const time_t* timer, struct tm* tp));
struct tm* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_localtime_r, (const time_t* timer, struct tm* tp));
time_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mktime, (struct tm* tp));
struct passwd* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getpwuid, (uid_t uid));
struct passwd* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getpwnam, (const char* name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getpwnam_r, (const char* name, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result));
struct group* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getgrgid, (gid_t gid));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_initgroups, (const char* user, gid_t group));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_uname, (struct utsname* name));
int SGX_UBRIDGE(SGX_NOCONVENTION, wrapper_getopt, (int argc, char** argv, const char* optstring));
void SGX_UBRIDGE(SGX_NOCONVENTION, set_optind, (int oi));
void SGX_UBRIDGE(SGX_NOCONVENTION, set_opterr, (int oe));
void SGX_UBRIDGE(SGX_NOCONVENTION, set_optopt, (int oo));
void SGX_UBRIDGE(SGX_NOCONVENTION, set_optreset, (int ors));
char* SGX_UBRIDGE(SGX_NOCONVENTION, get_optarg, ());
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getenv, (const char* name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_putenv, (char* string));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_clearenv, ());
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setenv, (const char* name, const char* value, int replace));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_unsetenv, (const char* name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mkstemp, (char* temp));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mkdtemp, (char* temp));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_open1, (const char* pathname, int flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_open2, (const char* pathname, int flags, unsigned int mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_creat, (const char* pathname, unsigned int mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_openat1, (int dirfd, const char* pathname, int flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_openat2, (int dirfd, const char* pathname, int flags, unsigned int mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fcntl1, (int fd, int cmd));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fcntl2, (int fd, int cmd, long int arg));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fcntl3, (int fd, int cmd, void* arg, int size));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlopen, (const char* __file, int __mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlclose, (void* __handle));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlsym, (void* __handle, const char* __name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dladdr, (const void* __address, void* __info));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dladdr1, (const void* __address, void* __info, void** __extra_info, int __flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlinfo, (void* __handle, int __request, void* __arg));
long int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_syscall, (long int __sysno, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gethostname, (char* name, size_t len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sethostname, (const char* name, size_t len));
off_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_lseek, (int fd, off_t offset, int whence));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fsync, (int fd));
off_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_read, (int fd, void* buf, size_t count));
off_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_write, (int fd, const void* buf, size_t count));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_close, (int fd));
pid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getpid, ());
pid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getppid, ());
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pread, (int fd, void* buf, size_t nbytes, off_t offset));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pwrite, (int fd, const void* buf, size_t n, off_t offset));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pipe, (int pipedes[2]));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pipe2, (int pipedes[2], int flag));
unsigned int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sleep, (unsigned int seconds));
unsigned int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_usleep, (unsigned int seconds));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_chown, (const char* file, uid_t owner, gid_t group));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fchown, (int fd, uid_t owner, gid_t group));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_lchown, (const char* file, uid_t owner, gid_t group));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_chdir, (const char* path));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fchdir, (int fd));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_get_current_dir_name, ());
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dup, (int fd));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dup2, (int fd, int fd2));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dup3, (int fd, int fd2, int flags));
uid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getuid, ());
uid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_geteuid, ());
gid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getgid, ());
gid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getegid, ());
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getpagesize, ());
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getcwd, (char* buf, size_t size));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_unlink, (const char* name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_rmdir, (const char* name));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall__exit, (int stat));
long int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sysconf, (int name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setgid, (gid_t gid));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setuid, (uid_t uid));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_execvp, (const char* file, const char** argv));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ftruncate, (int fd, off_t len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ftruncate64, (int fd, __off64_t length));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_free, (void* p));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_geterrno, ());
SGX_WRAPPER_FILE SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fopen, (const char* filename, const char* mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fclose, (SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fputs, (const char* str, SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_feof, (SGX_WRAPPER_FILE FILESTREAM));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_rewind, (SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fflush, (SGX_WRAPPER_FILE FILESTREAM));
size_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fread, (void* ptr, size_t size, size_t nmemb, SGX_WRAPPER_FILE FILESTREAM));
size_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fwrite, (const void* ptr, size_t size, size_t count, SGX_WRAPPER_FILE FILESTREAM));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fgets, (char* str, int num, SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getc_unlocked, (SGX_WRAPPER_FILE FILESTREAM));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_flockfile, (SGX_WRAPPER_FILE filehandle));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_funlockfile, (SGX_WRAPPER_FILE filehandle));
off_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ftello, (SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fseeko, (SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence));
off_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ftell, (SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fseek, (SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ferror, (SGX_WRAPPER_FILE FILESTREAM));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_perror, (const char* s));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getc, (SGX_WRAPPER_FILE FILESTREAM));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_putchar, (int c));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_putc, (int c, SGX_WRAPPER_FILE stream));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fputc, (int c, SGX_WRAPPER_FILE stream));
SGX_WRAPPER_FILE SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fdopen, (int fd, const char* modes));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fileno, (SGX_WRAPPER_FILE stream));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_rename, (const char* _old, const char* _new));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_print_string, (const char* s));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fprint_string, (SGX_WRAPPER_FILE stream, const char* s));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vfscanf, (SGX_WRAPPER_FILE s, const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vscanf, (const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vsscanf, (const char* s, const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vsprintf, (char* string, const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vasprintf, (char** string, const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vfprintf, (SGX_WRAPPER_FILE FILESTREAM, const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_vprintf, (const char* format, void* val));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_eventfd, (unsigned int initval, int flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_socket, (int domain, int type, int protocol));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_accept, (int sockfd, struct sockaddr* addr, socklen_t* addrlen));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_connect, (int socket, const struct sockaddr* address, socklen_t address_len));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sendto, (int sockfd, const void* buf, size_t len, int flags, const void* dest_addr, unsigned int addrlen));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_recv, (int fd, void* buf, size_t len, int flags));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_send, (int fd, const void* buf, size_t len, int flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_socketpair, (int domain, int type, int protocol, int sv[2]));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setsockopt, (int sockfd, int level, int optname, const void* optval, unsigned int optlen));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getsockopt, (int sockfd, int level, int optname, void* optval, unsigned int* optlen));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_shutdown, (int fd, int how));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_bind, (int fd, const struct sockaddr* addr, socklen_t len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_listen, (int fd, int n));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getsockname, (int fd, struct sockaddr* addr, socklen_t* len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getpeername, (int fd, struct sockaddr* addr, socklen_t* len));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_recvfrom, (int fd, void* buf, size_t n, int flags, struct sockaddr* addr, socklen_t* addr_len));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sendmsg, (int fd, const struct msghdr* message, int flags));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_recvmsg, (int fd, struct msghdr* message, int flags));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_freeaddrinfo, (void* res));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getaddrinfo, (const char* node, const char* service, const void* hints, void** res));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getaddrinfo1, (const char* node, const char* service, const void* hints, void* res));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sethostent, (int stay_open));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_endhostent, ());
struct hostent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gethostent, ());
struct hostent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gethostbyaddr, (const void* addr, socklen_t len, int type));
struct hostent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gethostbyname, (const char* name));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setnetent, (int stay_open));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_endnetent, ());
struct netent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getnetent, ());
struct netent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getnetbyaddr, (uint32_t net, int type));
struct netent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getnetbyname, (const char* name));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setservent, (int stay_open));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_endservent, ());
struct servent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getservent, ());
struct servent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getservbyname, (const char* name, const char* proto));
struct servent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getservbyport, (int port, const char* proto));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setprotoent, (int stay_open));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_endprotoent, ());
struct protoent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getprotoent, ());
struct protoent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getprotobyname, (const char* name));
struct protoent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getprotobynumber, (int proto));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gai_strerror, (int ecode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getnameinfo, (const struct sockaddr* sa, socklen_t salen, char* host, socklen_t hostlen, char* serv, socklen_t servlen, int flags));
uint32_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ntohl, (uint32_t netlong));
uint16_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ntohs, (uint16_t netshort));
uint32_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_htonl, (uint32_t hostlong));
uint16_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_htons, (uint16_t hostshort));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_ioctl, (int fd, unsigned long int request, void* arguments));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_readv, (int __fd, const void* __iovec, int iovec_size, int __count));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_writev, (int __fd, int iovec_id, int iovec_size, int __count));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_init_multiple_iovec_outside, (const void* __iovec, int iovec_size, int __count));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_copy_base_to_outside, (int iovec_id, int i, const void* base, int len));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_free_iovec_outside, (int iovec_id, int iovec_size, int __count));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_process_vm_readv, (pid_t __pid, const struct iovec* __lvec, unsigned long int __liovcnt, const struct iovec* __rvec, unsigned long int __riovcnt, unsigned long int __flags));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_process_vm_writev, (pid_t __pid, const struct iovec* __lvec, unsigned long int __liovcnt, const struct iovec* __rvec, unsigned long int __riovcnt, unsigned long int __flags));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mmap, (void* __addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mmap64, (void* __addr, size_t __len, int __prot, int __flags, int __fd, __off64_t __offset));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_munmap, (void* __addr, size_t __len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mprotect, (void* __addr, size_t __len, int __prot));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_msync, (void* __addr, size_t __len, int __flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mlock, (const void* __addr, size_t __len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_munlock, (const void* __addr, size_t __len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mlockall, (int __flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_munlockall, ());
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mincore, (void* __start, size_t __len, unsigned char* __vec));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_shm_open, (const char* __name, int __oflag, mode_t __mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_shm_unlink, (const char* __name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_poll, (struct pollfd* __fds, nfds_t __nfds, int __timeout));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_epoll_create, (int __size));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_epoll_create1, (int __flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_epoll_ctl, (int __epfd, int __op, int __fd, void* __event, int event_size));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_epoll_wait, (int __epfd, void* __events, int event_size, int __maxevents, int __timeout));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_epoll_pwait, (int __epfd, void* __events, int event_size, int __maxevents, int __timeout, void* __ss, int sigset_size));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_select, (int __nfds, fd_set* __readfds, fd_set* __writefds, fd_set* __exceptfds, struct timeval* __timeout));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sendfile, (int out_fd, int in_fd, off_t* offset, size_t count));
__pid_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_waitpid, (__pid_t __pid, int* __stat_loc, int __options));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_waitid, (idtype_t __idtype, __id_t __id, siginfo_t* __infop, int __options));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_stat, (const char* path, struct stat* buf));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fstat, (int fd, struct stat* buf));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_lstat, (const char* path, struct stat* buf));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_chmod, (const char* file, mode_t mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fchmod, (int fd, mode_t mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fchmodat, (int fd, const char* file, mode_t mode, int flag));
mode_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_umask, (mode_t mask));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mkdir, (const char* path, mode_t mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mkdirat, (int fd, const char* path, mode_t mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mkfifo, (const char* path, mode_t mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_mkfifoat, (int fd, const char* path, mode_t mode));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_opendir, (const char* name));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_fdopendir, (int fd));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_closedir, (void* dirp));
struct dirent* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_readdir, (void* dirp));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_readdir_r, (void* dirp, struct dirent* entry, struct dirent** result));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_rewinddir, (void* dirp));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_seekdir, (void* dirp, long int pos));
long int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_telldir, (void* dirp));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dirfd, (void* dirp));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_alphasort, (const struct dirent** e1, const struct dirent** e2));
ssize_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getdirentries, (int fd, char* buf, size_t nbytes, off_t* basep));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_versionsort, (const struct dirent** e1, const struct dirent** e2));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_prlimit, (__pid_t pid, enum __rlimit_resource resource, const struct rlimit* new_limit, struct rlimit* old_limit));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getrlimit, (int resource, struct rlimit* rlim));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_setrlimit, (int resource, const struct rlimit* rlim));
in_addr_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_addr, (const char* cp));
in_addr_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_lnaof, (struct in_addr in));
struct in_addr SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_makeaddr, (in_addr_t net, in_addr_t host));
in_addr_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_netof, (struct in_addr in));
in_addr_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_network, (const char* cp));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_ntoa, (struct in_addr in));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_pton, (int af, const char* cp, void* buf));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_inet_ntop, (int af, const void* cp, char* buf, socklen_t len));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sysctl, (int* name, int nlen, void* oldval, size_t* oldlenp, void* newval, size_t newlen));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigemptyset, (sigset_t* set));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigfillset, (sigset_t* set));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigaddset, (sigset_t* set, int signo));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigdelset, (sigset_t* set, int signo));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigismember, (const sigset_t* set, int signo));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigsuspend, (const sigset_t* set));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigaction, (int sig, const struct sigaction* act, struct sigaction* oact));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigpending, (sigset_t* set));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_sigwait, (const sigset_t* set, int* sig));
__sighandler_t SGX_UBRIDGE(SGX_NOCONVENTION, ocall_signal, (int __sig, __sighandler_t __handler));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_raise, (int sig));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_getifaddrs, (struct ifaddrs** ifap));
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_freeifaddrs, (struct ifaddrs* ifa));
unsigned int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_if_nametoindex, (const char* ifname));
char* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_if_indextoname, (unsigned int ifindex, char* ifname));
struct if_nameindex* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_if_nameindex, ());
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_if_freenameindex, (struct if_nameindex* ptr));

sgx_status_t c1_initialize(sgx_enclave_id_t eid, void** retval, void* cpuid, void** top, void** bottom, void** klass_list, int debug);
sgx_status_t gc_scavenge(sgx_enclave_id_t eid, void* tasks, int n);
sgx_status_t within_enclave(sgx_enclave_id_t eid, int* retval, void* addr);
sgx_status_t interpreted_entry_zero_locals(sgx_enclave_id_t eid, void** retval, void* rbx, void* method, int* has_exception);
sgx_status_t ecall_execute_job(sgx_enclave_id_t eid, pthread_t pthread_self_id, unsigned long int job_id);
sgx_status_t ecall_set_enclave_id(sgx_enclave_id_t eid, sgx_enclave_id_t self_eid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
