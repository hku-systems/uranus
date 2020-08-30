#include "securecompiler_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */

#include <errno.h>
#include <string.h> /* for memcpy etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)


typedef struct ms_c1_initialize_t {
	void* ms_retval;
	void* ms_cpuid;
	void** ms_top;
	void** ms_bottom;
	void** ms_klass_list;
	int ms_debug;
} ms_c1_initialize_t;

typedef struct ms_gc_scavenge_t {
	void* ms_tasks;
	int ms_n;
} ms_gc_scavenge_t;

typedef struct ms_within_enclave_t {
	int ms_retval;
	void* ms_addr;
} ms_within_enclave_t;

typedef struct ms_interpreted_entry_zero_locals_t {
	void* ms_retval;
	void* ms_rbx;
	void* ms_method;
	int* ms_has_exception;
} ms_interpreted_entry_zero_locals_t;

typedef struct ms_ecall_execute_job_t {
	pthread_t ms_pthread_self_id;
	unsigned long int ms_job_id;
} ms_ecall_execute_job_t;

typedef struct ms_ecall_set_enclave_id_t {
	sgx_enclave_id_t ms_self_eid;
} ms_ecall_set_enclave_id_t;

typedef struct ms_ocall_interpreter_t {
	void* ms_retval;
	void* ms_r14;
	int ms_size;
	void* ms_method;
	void* ms_thread;
	void* ms_sender;
} ms_ocall_interpreter_t;

typedef struct ms_ocall_jvm_resolve_invoke_t {
	int ms_byte;
	void* ms_mh;
	int ms_bci;
	void* ms_recv;
	int ms_idx;
	void* ms_bcp;
	void* ms_recv_klass;
} ms_ocall_jvm_resolve_invoke_t;

typedef struct ms_ocall_jvm_resolve_method_C_t {
	void* ms_retval;
	int ms_bc;
	void* ms_constant;
	int ms_idx;
} ms_ocall_jvm_resolve_method_C_t;

typedef struct ms_ocall_jvm_resolve_invoke_C_t {
	void* ms_retval;
	void* ms_recv;
	void* ms_constant;
	int ms_bidx;
	int ms_bc;
	void* ms_recv_klass;
} ms_ocall_jvm_resolve_invoke_C_t;



typedef struct ms_ocall_jvm_ldc_t {
	void* ms_retval;
	bool ms_wide;
	void* ms_p;
	int ms_index;
} ms_ocall_jvm_ldc_t;

typedef struct ms_ocall_jvm_resolve_ldc_t {
	void* ms_retval;
	void* ms_p;
	int ms_index;
	int ms_byte;
} ms_ocall_jvm_resolve_ldc_t;

typedef struct ms_ocall_jvm_resolve_get_put_t {
	int ms_retval;
	int ms_byte;
	void* ms_mh;
	int ms_idx;
	void* ms_bcp;
} ms_ocall_jvm_resolve_get_put_t;

typedef struct ms_ocall_jvm_resolve_get_put_klass_t {
	void* ms_retval;
	int ms_byte;
	void* ms_mh;
	int ms_idx;
	void* ms_bcp;
} ms_ocall_jvm_resolve_get_put_klass_t;

typedef struct ms_ocall_klass_type_array_t {
	void* ms_retval;
} ms_ocall_klass_type_array_t;

typedef struct ms_ocall_klass_type_t {
	void* ms_retval;
} ms_ocall_klass_type_t;

typedef struct ms_ocall_obj_array_klass_get_t {
	void* ms_retval;
	void* ms_pool;
	int ms_index;
} ms_ocall_obj_array_klass_get_t;

typedef struct ms_ocall_klass_resolve_or_fail_t {
	void* ms_retval;
	char* ms_name;
} ms_ocall_klass_resolve_or_fail_t;

typedef struct ms_ocall_klass_find_or_null_t {
	void* ms_retval;
	char* ms_name;
} ms_ocall_klass_find_or_null_t;

typedef struct ms_ocall_klass_get_t {
	void* ms_retval;
	void* ms_pool;
	int ms_index;
} ms_ocall_klass_get_t;

typedef struct ms_ocall_klass_compute_oopmap_t {
	void* ms_klass;
	void* ms_m;
	int ms_bci;
} ms_ocall_klass_compute_oopmap_t;

typedef struct ms_ocall_array_klass_t {
	void* ms_retval;
	void* ms_klass;
	int ms_rank;
	int ms_get_all;
} ms_ocall_array_klass_t;

typedef struct ms_ocall_multi_array_klass_get_t {
	void* ms_retval;
	void* ms_pool;
	int ms_index;
} ms_ocall_multi_array_klass_get_t;

typedef struct ms_ocall_jvm_pre_native_t {
	void* ms_method;
	int ms_resolve;
} ms_ocall_jvm_pre_native_t;

typedef struct ms_ocall_jvm_malloc_t {
	void* ms_retval;
	int ms_size;
} ms_ocall_jvm_malloc_t;

typedef struct ms_ocall_classfile_buffer_t {
	void* ms_retval;
	char* ms_name;
	int* ms_size;
} ms_ocall_classfile_buffer_t;

typedef struct ms_ocall_pthread_create_t {
	int ms_retval;
	pthread_t* ms_new_thread;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	unsigned long int ms_job_id;
	sgx_enclave_id_t ms_eid;
} ms_ocall_pthread_create_t;

typedef struct ms_ocall_pthread_self_t {
	pthread_t ms_retval;
} ms_ocall_pthread_self_t;

typedef struct ms_ocall_pthread_join_t {
	int ms_retval;
	pthread_t ms_pt;
	void** ms_thread_result;
} ms_ocall_pthread_join_t;

typedef struct ms_ocall_pthread_detach_t {
	int ms_retval;
	pthread_t ms_pt;
} ms_ocall_pthread_detach_t;

typedef struct ms_ocall_pthread_equal_t {
	int ms_retval;
	pthread_t ms_pt1;
	pthread_t ms_pt2;
} ms_ocall_pthread_equal_t;

typedef struct ms_ocall_pthread_exit_t {
	void* ms_retval;
} ms_ocall_pthread_exit_t;

typedef struct ms_ocall_pthread_cancel_t {
	int ms_retval;
	pthread_t ms_th;
} ms_ocall_pthread_cancel_t;


typedef struct ms_ocall_pthread_attr_init_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE* ms___attr;
} ms_ocall_pthread_attr_init_t;

typedef struct ms_ocall_pthread_attr_destroy_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
} ms_ocall_pthread_attr_destroy_t;

typedef struct ms_ocall_pthread_attr_getdetachstate_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	int* ms___detachstate;
} ms_ocall_pthread_attr_getdetachstate_t;

typedef struct ms_ocall_pthread_attr_setdetachstate_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	int ms___detachstate;
} ms_ocall_pthread_attr_setdetachstate_t;

typedef struct ms_ocall_pthread_attr_getguardsize_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	size_t* ms___guardsize;
} ms_ocall_pthread_attr_getguardsize_t;

typedef struct ms_ocall_pthread_attr_setguardsize_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	size_t ms___guardsize;
} ms_ocall_pthread_attr_setguardsize_t;

typedef struct ms_ocall_pthread_attr_getschedpolicy_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	int* ms___policy;
} ms_ocall_pthread_attr_getschedpolicy_t;

typedef struct ms_ocall_pthread_attr_setschedpolicy_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	int ms___policy;
} ms_ocall_pthread_attr_setschedpolicy_t;

typedef struct ms_ocall_pthread_attr_getstacksize_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	size_t* ms___stacksize;
} ms_ocall_pthread_attr_getstacksize_t;

typedef struct ms_ocall_pthread_attr_setstacksize_t {
	int ms_retval;
	SGX_WRAPPER_PTHREAD_ATTRIBUTE ms___attr;
	size_t ms___stacksize;
} ms_ocall_pthread_attr_setstacksize_t;

typedef struct ms_ocall_pthread_setspecific_t {
	int ms_retval;
	pthread_key_t ms_key;
	void* ms_value;
} ms_ocall_pthread_setspecific_t;

typedef struct ms_ocall_pthread_getspecific_t {
	void* ms_retval;
	pthread_key_t ms_key;
} ms_ocall_pthread_getspecific_t;

typedef struct ms_ocall_pthread_key_create_t {
	int ms_retval;
	pthread_key_t* ms_key;
	void* ms_destructor;
} ms_ocall_pthread_key_create_t;

typedef struct ms_ocall_time_t {
	time_t ms_retval;
	time_t* ms_t;
} ms_ocall_time_t;

typedef struct ms_ocall_gettimeofday_t {
	int ms_retval;
	void* ms_tv;
	int ms_tv_size;
	void* ms_tz;
	int ms_tz_size;
} ms_ocall_gettimeofday_t;

typedef struct ms_ocall_gettimeofday2_t {
	int ms_retval;
	void* ms_tv;
	int ms_tv_size;
} ms_ocall_gettimeofday2_t;

typedef struct ms_ocall_clock_t {
	clock_t ms_retval;
} ms_ocall_clock_t;

typedef struct ms_ocall_gmtime_r_t {
	struct tm* ms_retval;
	time_t* ms_timer;
	struct tm* ms_tp;
} ms_ocall_gmtime_r_t;

typedef struct ms_ocall_localtime_r_t {
	struct tm* ms_retval;
	time_t* ms_timer;
	struct tm* ms_tp;
} ms_ocall_localtime_r_t;

typedef struct ms_ocall_mktime_t {
	time_t ms_retval;
	struct tm* ms_tp;
} ms_ocall_mktime_t;

typedef struct ms_ocall_getpwuid_t {
	struct passwd* ms_retval;
	uid_t ms_uid;
} ms_ocall_getpwuid_t;

typedef struct ms_ocall_getpwnam_t {
	struct passwd* ms_retval;
	char* ms_name;
} ms_ocall_getpwnam_t;

typedef struct ms_ocall_getpwnam_r_t {
	int ms_retval;
	char* ms_name;
	struct passwd* ms_pwd;
	char* ms_buf;
	size_t ms_buflen;
	struct passwd** ms_result;
} ms_ocall_getpwnam_r_t;

typedef struct ms_ocall_getgrgid_t {
	struct group* ms_retval;
	gid_t ms_gid;
} ms_ocall_getgrgid_t;

typedef struct ms_ocall_initgroups_t {
	int ms_retval;
	char* ms_user;
	gid_t ms_group;
} ms_ocall_initgroups_t;

typedef struct ms_ocall_uname_t {
	int ms_retval;
	struct utsname* ms_name;
} ms_ocall_uname_t;

typedef struct ms_wrapper_getopt_t {
	int ms_retval;
	int ms_argc;
	char** ms_argv;
	char* ms_optstring;
} ms_wrapper_getopt_t;

typedef struct ms_set_optind_t {
	int ms_oi;
} ms_set_optind_t;

typedef struct ms_set_opterr_t {
	int ms_oe;
} ms_set_opterr_t;

typedef struct ms_set_optopt_t {
	int ms_oo;
} ms_set_optopt_t;

typedef struct ms_set_optreset_t {
	int ms_ors;
} ms_set_optreset_t;

typedef struct ms_get_optarg_t {
	char* ms_retval;
} ms_get_optarg_t;

typedef struct ms_ocall_getenv_t {
	char* ms_retval;
	char* ms_name;
} ms_ocall_getenv_t;

typedef struct ms_ocall_putenv_t {
	int ms_retval;
	char* ms_string;
} ms_ocall_putenv_t;

typedef struct ms_ocall_clearenv_t {
	int ms_retval;
} ms_ocall_clearenv_t;

typedef struct ms_ocall_setenv_t {
	int ms_retval;
	char* ms_name;
	char* ms_value;
	int ms_replace;
} ms_ocall_setenv_t;

typedef struct ms_ocall_unsetenv_t {
	int ms_retval;
	char* ms_name;
} ms_ocall_unsetenv_t;

typedef struct ms_ocall_mkstemp_t {
	int ms_retval;
	char* ms_temp;
} ms_ocall_mkstemp_t;

typedef struct ms_ocall_mkdtemp_t {
	char* ms_retval;
	char* ms_temp;
} ms_ocall_mkdtemp_t;

typedef struct ms_ocall_open1_t {
	int ms_retval;
	char* ms_pathname;
	int ms_flags;
} ms_ocall_open1_t;

typedef struct ms_ocall_open2_t {
	int ms_retval;
	char* ms_pathname;
	int ms_flags;
	unsigned int ms_mode;
} ms_ocall_open2_t;

typedef struct ms_ocall_creat_t {
	int ms_retval;
	char* ms_pathname;
	unsigned int ms_mode;
} ms_ocall_creat_t;

typedef struct ms_ocall_openat1_t {
	int ms_retval;
	int ms_dirfd;
	char* ms_pathname;
	int ms_flags;
} ms_ocall_openat1_t;

typedef struct ms_ocall_openat2_t {
	int ms_retval;
	int ms_dirfd;
	char* ms_pathname;
	int ms_flags;
	unsigned int ms_mode;
} ms_ocall_openat2_t;

typedef struct ms_ocall_fcntl1_t {
	int ms_retval;
	int ms_fd;
	int ms_cmd;
} ms_ocall_fcntl1_t;

typedef struct ms_ocall_fcntl2_t {
	int ms_retval;
	int ms_fd;
	int ms_cmd;
	long int ms_arg;
} ms_ocall_fcntl2_t;

typedef struct ms_ocall_fcntl3_t {
	int ms_retval;
	int ms_fd;
	int ms_cmd;
	void* ms_arg;
	int ms_size;
} ms_ocall_fcntl3_t;

typedef struct ms_ocall_dlopen_t {
	void* ms_retval;
	char* ms___file;
	int ms___mode;
} ms_ocall_dlopen_t;

typedef struct ms_ocall_dlclose_t {
	int ms_retval;
	void* ms___handle;
} ms_ocall_dlclose_t;

typedef struct ms_ocall_dlsym_t {
	void* ms_retval;
	void* ms___handle;
	char* ms___name;
} ms_ocall_dlsym_t;

typedef struct ms_ocall_dladdr_t {
	int ms_retval;
	void* ms___address;
	void* ms___info;
} ms_ocall_dladdr_t;

typedef struct ms_ocall_dladdr1_t {
	int ms_retval;
	void* ms___address;
	void* ms___info;
	void** ms___extra_info;
	int ms___flags;
} ms_ocall_dladdr1_t;

typedef struct ms_ocall_dlinfo_t {
	int ms_retval;
	void* ms___handle;
	int ms___request;
	void* ms___arg;
} ms_ocall_dlinfo_t;

typedef struct ms_ocall_syscall_t {
	long int ms_retval;
	long int ms___sysno;
	void* ms_val;
} ms_ocall_syscall_t;

typedef struct ms_ocall_gethostname_t {
	int ms_retval;
	char* ms_name;
	size_t ms_len;
} ms_ocall_gethostname_t;

typedef struct ms_ocall_sethostname_t {
	int ms_retval;
	char* ms_name;
	size_t ms_len;
} ms_ocall_sethostname_t;

typedef struct ms_ocall_lseek_t {
	off_t ms_retval;
	int ms_fd;
	off_t ms_offset;
	int ms_whence;
} ms_ocall_lseek_t;

typedef struct ms_ocall_fsync_t {
	int ms_retval;
	int ms_fd;
} ms_ocall_fsync_t;

typedef struct ms_ocall_read_t {
	off_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_count;
} ms_ocall_read_t;

typedef struct ms_ocall_write_t {
	off_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_count;
} ms_ocall_write_t;

typedef struct ms_ocall_close_t {
	int ms_retval;
	int ms_fd;
} ms_ocall_close_t;

typedef struct ms_ocall_getpid_t {
	pid_t ms_retval;
} ms_ocall_getpid_t;

typedef struct ms_ocall_getppid_t {
	pid_t ms_retval;
} ms_ocall_getppid_t;

typedef struct ms_ocall_pread_t {
	ssize_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_nbytes;
	off_t ms_offset;
} ms_ocall_pread_t;

typedef struct ms_ocall_pwrite_t {
	ssize_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_n;
	off_t ms_offset;
} ms_ocall_pwrite_t;

typedef struct ms_ocall_pipe_t {
	int ms_retval;
	int* ms_pipedes;
} ms_ocall_pipe_t;

typedef struct ms_ocall_pipe2_t {
	int ms_retval;
	int* ms_pipedes;
	int ms_flag;
} ms_ocall_pipe2_t;

typedef struct ms_ocall_sleep_t {
	unsigned int ms_retval;
	unsigned int ms_seconds;
} ms_ocall_sleep_t;

typedef struct ms_ocall_usleep_t {
	unsigned int ms_retval;
	unsigned int ms_seconds;
} ms_ocall_usleep_t;

typedef struct ms_ocall_chown_t {
	int ms_retval;
	char* ms_file;
	uid_t ms_owner;
	gid_t ms_group;
} ms_ocall_chown_t;

typedef struct ms_ocall_fchown_t {
	int ms_retval;
	int ms_fd;
	uid_t ms_owner;
	gid_t ms_group;
} ms_ocall_fchown_t;

typedef struct ms_ocall_lchown_t {
	int ms_retval;
	char* ms_file;
	uid_t ms_owner;
	gid_t ms_group;
} ms_ocall_lchown_t;

typedef struct ms_ocall_chdir_t {
	int ms_retval;
	char* ms_path;
} ms_ocall_chdir_t;

typedef struct ms_ocall_fchdir_t {
	int ms_retval;
	int ms_fd;
} ms_ocall_fchdir_t;

typedef struct ms_ocall_get_current_dir_name_t {
	char* ms_retval;
} ms_ocall_get_current_dir_name_t;

typedef struct ms_ocall_dup_t {
	int ms_retval;
	int ms_fd;
} ms_ocall_dup_t;

typedef struct ms_ocall_dup2_t {
	int ms_retval;
	int ms_fd;
	int ms_fd2;
} ms_ocall_dup2_t;

typedef struct ms_ocall_dup3_t {
	int ms_retval;
	int ms_fd;
	int ms_fd2;
	int ms_flags;
} ms_ocall_dup3_t;

typedef struct ms_ocall_getuid_t {
	uid_t ms_retval;
} ms_ocall_getuid_t;

typedef struct ms_ocall_geteuid_t {
	uid_t ms_retval;
} ms_ocall_geteuid_t;

typedef struct ms_ocall_getgid_t {
	gid_t ms_retval;
} ms_ocall_getgid_t;

typedef struct ms_ocall_getegid_t {
	gid_t ms_retval;
} ms_ocall_getegid_t;

typedef struct ms_ocall_getpagesize_t {
	int ms_retval;
} ms_ocall_getpagesize_t;

typedef struct ms_ocall_getcwd_t {
	char* ms_retval;
	char* ms_buf;
	size_t ms_size;
} ms_ocall_getcwd_t;

typedef struct ms_ocall_unlink_t {
	int ms_retval;
	char* ms_name;
} ms_ocall_unlink_t;

typedef struct ms_ocall_rmdir_t {
	int ms_retval;
	char* ms_name;
} ms_ocall_rmdir_t;

typedef struct ms_ocall__exit_t {
	int ms_stat;
} ms_ocall__exit_t;

typedef struct ms_ocall_sysconf_t {
	long int ms_retval;
	int ms_name;
} ms_ocall_sysconf_t;

typedef struct ms_ocall_setgid_t {
	int ms_retval;
	gid_t ms_gid;
} ms_ocall_setgid_t;

typedef struct ms_ocall_setuid_t {
	int ms_retval;
	uid_t ms_uid;
} ms_ocall_setuid_t;

typedef struct ms_ocall_execvp_t {
	int ms_retval;
	char* ms_file;
	char** ms_argv;
} ms_ocall_execvp_t;

typedef struct ms_ocall_ftruncate_t {
	int ms_retval;
	int ms_fd;
	off_t ms_len;
} ms_ocall_ftruncate_t;

typedef struct ms_ocall_ftruncate64_t {
	int ms_retval;
	int ms_fd;
	__off64_t ms_length;
} ms_ocall_ftruncate64_t;

typedef struct ms_ocall_free_t {
	void* ms_p;
} ms_ocall_free_t;

typedef struct ms_ocall_geterrno_t {
	int ms_retval;
} ms_ocall_geterrno_t;

typedef struct ms_ocall_fopen_t {
	SGX_WRAPPER_FILE ms_retval;
	char* ms_filename;
	char* ms_mode;
} ms_ocall_fopen_t;

typedef struct ms_ocall_fclose_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_fclose_t;

typedef struct ms_ocall_fputs_t {
	int ms_retval;
	char* ms_str;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_fputs_t;

typedef struct ms_ocall_feof_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_feof_t;

typedef struct ms_ocall_rewind_t {
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_rewind_t;

typedef struct ms_ocall_fflush_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_fflush_t;

typedef struct ms_ocall_fread_t {
	size_t ms_retval;
	void* ms_ptr;
	size_t ms_size;
	size_t ms_nmemb;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_fread_t;

typedef struct ms_ocall_fwrite_t {
	size_t ms_retval;
	void* ms_ptr;
	size_t ms_size;
	size_t ms_count;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_fwrite_t;

typedef struct ms_ocall_fgets_t {
	char* ms_retval;
	char* ms_str;
	int ms_num;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_fgets_t;

typedef struct ms_ocall_getc_unlocked_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_getc_unlocked_t;

typedef struct ms_ocall_flockfile_t {
	SGX_WRAPPER_FILE ms_filehandle;
} ms_ocall_flockfile_t;

typedef struct ms_ocall_funlockfile_t {
	SGX_WRAPPER_FILE ms_filehandle;
} ms_ocall_funlockfile_t;

typedef struct ms_ocall_ftello_t {
	off_t ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_ftello_t;

typedef struct ms_ocall_fseeko_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
	off_t ms_offset;
	int ms_whence;
} ms_ocall_fseeko_t;

typedef struct ms_ocall_ftell_t {
	off_t ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_ftell_t;

typedef struct ms_ocall_fseek_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
	off_t ms_offset;
	int ms_whence;
} ms_ocall_fseek_t;

typedef struct ms_ocall_ferror_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_ferror_t;

typedef struct ms_ocall_perror_t {
	char* ms_s;
} ms_ocall_perror_t;

typedef struct ms_ocall_getc_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
} ms_ocall_getc_t;

typedef struct ms_ocall_putchar_t {
	int ms_retval;
	int ms_c;
} ms_ocall_putchar_t;

typedef struct ms_ocall_putc_t {
	int ms_retval;
	int ms_c;
	SGX_WRAPPER_FILE ms_stream;
} ms_ocall_putc_t;

typedef struct ms_ocall_fputc_t {
	int ms_retval;
	int ms_c;
	SGX_WRAPPER_FILE ms_stream;
} ms_ocall_fputc_t;

typedef struct ms_ocall_fdopen_t {
	SGX_WRAPPER_FILE ms_retval;
	int ms_fd;
	char* ms_modes;
} ms_ocall_fdopen_t;

typedef struct ms_ocall_fileno_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_stream;
} ms_ocall_fileno_t;

typedef struct ms_ocall_rename_t {
	int ms_retval;
	char* ms__old;
	char* ms__new;
} ms_ocall_rename_t;

typedef struct ms_ocall_print_string_t {
	int ms_retval;
	char* ms_s;
} ms_ocall_print_string_t;

typedef struct ms_ocall_fprint_string_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_stream;
	char* ms_s;
} ms_ocall_fprint_string_t;

typedef struct ms_ocall_vfscanf_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_s;
	char* ms_format;
	void* ms_val;
} ms_ocall_vfscanf_t;

typedef struct ms_ocall_vscanf_t {
	int ms_retval;
	char* ms_format;
	void* ms_val;
} ms_ocall_vscanf_t;

typedef struct ms_ocall_vsscanf_t {
	int ms_retval;
	char* ms_s;
	char* ms_format;
	void* ms_val;
} ms_ocall_vsscanf_t;

typedef struct ms_ocall_vsprintf_t {
	int ms_retval;
	char* ms_string;
	char* ms_format;
	void* ms_val;
} ms_ocall_vsprintf_t;

typedef struct ms_ocall_vasprintf_t {
	int ms_retval;
	char** ms_string;
	char* ms_format;
	void* ms_val;
} ms_ocall_vasprintf_t;

typedef struct ms_ocall_vfprintf_t {
	int ms_retval;
	SGX_WRAPPER_FILE ms_FILESTREAM;
	char* ms_format;
	void* ms_val;
} ms_ocall_vfprintf_t;

typedef struct ms_ocall_vprintf_t {
	int ms_retval;
	char* ms_format;
	void* ms_val;
} ms_ocall_vprintf_t;

typedef struct ms_ocall_eventfd_t {
	int ms_retval;
	unsigned int ms_initval;
	int ms_flags;
} ms_ocall_eventfd_t;

typedef struct ms_ocall_socket_t {
	int ms_retval;
	int ms_domain;
	int ms_type;
	int ms_protocol;
} ms_ocall_socket_t;

typedef struct ms_ocall_accept_t {
	int ms_retval;
	int ms_sockfd;
	struct sockaddr* ms_addr;
	socklen_t* ms_addrlen;
} ms_ocall_accept_t;

typedef struct ms_ocall_connect_t {
	int ms_retval;
	int ms_socket;
	struct sockaddr* ms_address;
	socklen_t ms_address_len;
} ms_ocall_connect_t;

typedef struct ms_ocall_sendto_t {
	ssize_t ms_retval;
	int ms_sockfd;
	void* ms_buf;
	size_t ms_len;
	int ms_flags;
	void* ms_dest_addr;
	unsigned int ms_addrlen;
} ms_ocall_sendto_t;

typedef struct ms_ocall_recv_t {
	ssize_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_len;
	int ms_flags;
} ms_ocall_recv_t;

typedef struct ms_ocall_send_t {
	ssize_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_len;
	int ms_flags;
} ms_ocall_send_t;

typedef struct ms_ocall_socketpair_t {
	int ms_retval;
	int ms_domain;
	int ms_type;
	int ms_protocol;
	int* ms_sv;
} ms_ocall_socketpair_t;

typedef struct ms_ocall_setsockopt_t {
	int ms_retval;
	int ms_sockfd;
	int ms_level;
	int ms_optname;
	void* ms_optval;
	unsigned int ms_optlen;
} ms_ocall_setsockopt_t;

typedef struct ms_ocall_getsockopt_t {
	int ms_retval;
	int ms_sockfd;
	int ms_level;
	int ms_optname;
	void* ms_optval;
	unsigned int* ms_optlen;
} ms_ocall_getsockopt_t;

typedef struct ms_ocall_shutdown_t {
	int ms_retval;
	int ms_fd;
	int ms_how;
} ms_ocall_shutdown_t;

typedef struct ms_ocall_bind_t {
	int ms_retval;
	int ms_fd;
	struct sockaddr* ms_addr;
	socklen_t ms_len;
} ms_ocall_bind_t;

typedef struct ms_ocall_listen_t {
	int ms_retval;
	int ms_fd;
	int ms_n;
} ms_ocall_listen_t;

typedef struct ms_ocall_getsockname_t {
	int ms_retval;
	int ms_fd;
	struct sockaddr* ms_addr;
	socklen_t* ms_len;
} ms_ocall_getsockname_t;

typedef struct ms_ocall_getpeername_t {
	int ms_retval;
	int ms_fd;
	struct sockaddr* ms_addr;
	socklen_t* ms_len;
} ms_ocall_getpeername_t;

typedef struct ms_ocall_recvfrom_t {
	ssize_t ms_retval;
	int ms_fd;
	void* ms_buf;
	size_t ms_n;
	int ms_flags;
	struct sockaddr* ms_addr;
	socklen_t* ms_addr_len;
} ms_ocall_recvfrom_t;

typedef struct ms_ocall_sendmsg_t {
	ssize_t ms_retval;
	int ms_fd;
	struct msghdr* ms_message;
	int ms_flags;
} ms_ocall_sendmsg_t;

typedef struct ms_ocall_recvmsg_t {
	ssize_t ms_retval;
	int ms_fd;
	struct msghdr* ms_message;
	int ms_flags;
} ms_ocall_recvmsg_t;

typedef struct ms_ocall_freeaddrinfo_t {
	void* ms_res;
} ms_ocall_freeaddrinfo_t;

typedef struct ms_ocall_getaddrinfo_t {
	int ms_retval;
	char* ms_node;
	char* ms_service;
	void* ms_hints;
	void** ms_res;
} ms_ocall_getaddrinfo_t;

typedef struct ms_ocall_getaddrinfo1_t {
	int ms_retval;
	char* ms_node;
	char* ms_service;
	void* ms_hints;
	void* ms_res;
} ms_ocall_getaddrinfo1_t;

typedef struct ms_ocall_sethostent_t {
	int ms_stay_open;
} ms_ocall_sethostent_t;


typedef struct ms_ocall_gethostent_t {
	struct hostent* ms_retval;
} ms_ocall_gethostent_t;

typedef struct ms_ocall_gethostbyaddr_t {
	struct hostent* ms_retval;
	void* ms_addr;
	socklen_t ms_len;
	int ms_type;
} ms_ocall_gethostbyaddr_t;

typedef struct ms_ocall_gethostbyname_t {
	struct hostent* ms_retval;
	char* ms_name;
} ms_ocall_gethostbyname_t;

typedef struct ms_ocall_setnetent_t {
	int ms_stay_open;
} ms_ocall_setnetent_t;


typedef struct ms_ocall_getnetent_t {
	struct netent* ms_retval;
} ms_ocall_getnetent_t;

typedef struct ms_ocall_getnetbyaddr_t {
	struct netent* ms_retval;
	uint32_t ms_net;
	int ms_type;
} ms_ocall_getnetbyaddr_t;

typedef struct ms_ocall_getnetbyname_t {
	struct netent* ms_retval;
	char* ms_name;
} ms_ocall_getnetbyname_t;

typedef struct ms_ocall_setservent_t {
	int ms_stay_open;
} ms_ocall_setservent_t;


typedef struct ms_ocall_getservent_t {
	struct servent* ms_retval;
} ms_ocall_getservent_t;

typedef struct ms_ocall_getservbyname_t {
	struct servent* ms_retval;
	char* ms_name;
	char* ms_proto;
} ms_ocall_getservbyname_t;

typedef struct ms_ocall_getservbyport_t {
	struct servent* ms_retval;
	int ms_port;
	char* ms_proto;
} ms_ocall_getservbyport_t;

typedef struct ms_ocall_setprotoent_t {
	int ms_stay_open;
} ms_ocall_setprotoent_t;


typedef struct ms_ocall_getprotoent_t {
	struct protoent* ms_retval;
} ms_ocall_getprotoent_t;

typedef struct ms_ocall_getprotobyname_t {
	struct protoent* ms_retval;
	char* ms_name;
} ms_ocall_getprotobyname_t;

typedef struct ms_ocall_getprotobynumber_t {
	struct protoent* ms_retval;
	int ms_proto;
} ms_ocall_getprotobynumber_t;

typedef struct ms_ocall_gai_strerror_t {
	char* ms_retval;
	int ms_ecode;
} ms_ocall_gai_strerror_t;

typedef struct ms_ocall_getnameinfo_t {
	int ms_retval;
	struct sockaddr* ms_sa;
	socklen_t ms_salen;
	char* ms_host;
	socklen_t ms_hostlen;
	char* ms_serv;
	socklen_t ms_servlen;
	int ms_flags;
} ms_ocall_getnameinfo_t;

typedef struct ms_ocall_ntohl_t {
	uint32_t ms_retval;
	uint32_t ms_netlong;
} ms_ocall_ntohl_t;

typedef struct ms_ocall_ntohs_t {
	uint16_t ms_retval;
	uint16_t ms_netshort;
} ms_ocall_ntohs_t;

typedef struct ms_ocall_htonl_t {
	uint32_t ms_retval;
	uint32_t ms_hostlong;
} ms_ocall_htonl_t;

typedef struct ms_ocall_htons_t {
	uint16_t ms_retval;
	uint16_t ms_hostshort;
} ms_ocall_htons_t;

typedef struct ms_ocall_ioctl_t {
	int ms_retval;
	int ms_fd;
	unsigned long int ms_request;
	void* ms_arguments;
} ms_ocall_ioctl_t;

typedef struct ms_ocall_readv_t {
	ssize_t ms_retval;
	int ms___fd;
	void* ms___iovec;
	int ms_iovec_size;
	int ms___count;
} ms_ocall_readv_t;

typedef struct ms_ocall_writev_t {
	ssize_t ms_retval;
	int ms___fd;
	int ms_iovec_id;
	int ms_iovec_size;
	int ms___count;
} ms_ocall_writev_t;

typedef struct ms_ocall_init_multiple_iovec_outside_t {
	int ms_retval;
	void* ms___iovec;
	int ms_iovec_size;
	int ms___count;
} ms_ocall_init_multiple_iovec_outside_t;

typedef struct ms_ocall_copy_base_to_outside_t {
	int ms_iovec_id;
	int ms_i;
	void* ms_base;
	int ms_len;
} ms_ocall_copy_base_to_outside_t;

typedef struct ms_ocall_free_iovec_outside_t {
	int ms_iovec_id;
	int ms_iovec_size;
	int ms___count;
} ms_ocall_free_iovec_outside_t;

typedef struct ms_ocall_process_vm_readv_t {
	ssize_t ms_retval;
	pid_t ms___pid;
	struct iovec* ms___lvec;
	unsigned long int ms___liovcnt;
	struct iovec* ms___rvec;
	unsigned long int ms___riovcnt;
	unsigned long int ms___flags;
} ms_ocall_process_vm_readv_t;

typedef struct ms_ocall_process_vm_writev_t {
	ssize_t ms_retval;
	pid_t ms___pid;
	struct iovec* ms___lvec;
	unsigned long int ms___liovcnt;
	struct iovec* ms___rvec;
	unsigned long int ms___riovcnt;
	unsigned long int ms___flags;
} ms_ocall_process_vm_writev_t;

typedef struct ms_ocall_mmap_t {
	void* ms_retval;
	void* ms___addr;
	size_t ms___len;
	int ms___prot;
	int ms___flags;
	int ms___fd;
	__off_t ms___offset;
} ms_ocall_mmap_t;

typedef struct ms_ocall_mmap64_t {
	void* ms_retval;
	void* ms___addr;
	size_t ms___len;
	int ms___prot;
	int ms___flags;
	int ms___fd;
	__off64_t ms___offset;
} ms_ocall_mmap64_t;

typedef struct ms_ocall_munmap_t {
	int ms_retval;
	void* ms___addr;
	size_t ms___len;
} ms_ocall_munmap_t;

typedef struct ms_ocall_mprotect_t {
	int ms_retval;
	void* ms___addr;
	size_t ms___len;
	int ms___prot;
} ms_ocall_mprotect_t;

typedef struct ms_ocall_msync_t {
	int ms_retval;
	void* ms___addr;
	size_t ms___len;
	int ms___flags;
} ms_ocall_msync_t;

typedef struct ms_ocall_mlock_t {
	int ms_retval;
	void* ms___addr;
	size_t ms___len;
} ms_ocall_mlock_t;

typedef struct ms_ocall_munlock_t {
	int ms_retval;
	void* ms___addr;
	size_t ms___len;
} ms_ocall_munlock_t;

typedef struct ms_ocall_mlockall_t {
	int ms_retval;
	int ms___flags;
} ms_ocall_mlockall_t;

typedef struct ms_ocall_munlockall_t {
	int ms_retval;
} ms_ocall_munlockall_t;

typedef struct ms_ocall_mincore_t {
	int ms_retval;
	void* ms___start;
	size_t ms___len;
	unsigned char* ms___vec;
} ms_ocall_mincore_t;

typedef struct ms_ocall_shm_open_t {
	int ms_retval;
	char* ms___name;
	int ms___oflag;
	mode_t ms___mode;
} ms_ocall_shm_open_t;

typedef struct ms_ocall_shm_unlink_t {
	int ms_retval;
	char* ms___name;
} ms_ocall_shm_unlink_t;

typedef struct ms_ocall_poll_t {
	int ms_retval;
	struct pollfd* ms___fds;
	nfds_t ms___nfds;
	int ms___timeout;
} ms_ocall_poll_t;

typedef struct ms_ocall_epoll_create_t {
	int ms_retval;
	int ms___size;
} ms_ocall_epoll_create_t;

typedef struct ms_ocall_epoll_create1_t {
	int ms_retval;
	int ms___flags;
} ms_ocall_epoll_create1_t;

typedef struct ms_ocall_epoll_ctl_t {
	int ms_retval;
	int ms___epfd;
	int ms___op;
	int ms___fd;
	void* ms___event;
	int ms_event_size;
} ms_ocall_epoll_ctl_t;

typedef struct ms_ocall_epoll_wait_t {
	int ms_retval;
	int ms___epfd;
	void* ms___events;
	int ms_event_size;
	int ms___maxevents;
	int ms___timeout;
} ms_ocall_epoll_wait_t;

typedef struct ms_ocall_epoll_pwait_t {
	int ms_retval;
	int ms___epfd;
	void* ms___events;
	int ms_event_size;
	int ms___maxevents;
	int ms___timeout;
	void* ms___ss;
	int ms_sigset_size;
} ms_ocall_epoll_pwait_t;

typedef struct ms_ocall_select_t {
	int ms_retval;
	int ms___nfds;
	fd_set* ms___readfds;
	fd_set* ms___writefds;
	fd_set* ms___exceptfds;
	struct timeval* ms___timeout;
} ms_ocall_select_t;

typedef struct ms_ocall_sendfile_t {
	ssize_t ms_retval;
	int ms_out_fd;
	int ms_in_fd;
	off_t* ms_offset;
	size_t ms_count;
} ms_ocall_sendfile_t;

typedef struct ms_ocall_waitpid_t {
	__pid_t ms_retval;
	__pid_t ms___pid;
	int* ms___stat_loc;
	int ms___options;
} ms_ocall_waitpid_t;

typedef struct ms_ocall_waitid_t {
	int ms_retval;
	idtype_t ms___idtype;
	__id_t ms___id;
	siginfo_t* ms___infop;
	int ms___options;
} ms_ocall_waitid_t;

typedef struct ms_ocall_stat_t {
	int ms_retval;
	char* ms_path;
	struct stat* ms_buf;
} ms_ocall_stat_t;

typedef struct ms_ocall_fstat_t {
	int ms_retval;
	int ms_fd;
	struct stat* ms_buf;
} ms_ocall_fstat_t;

typedef struct ms_ocall_lstat_t {
	int ms_retval;
	char* ms_path;
	struct stat* ms_buf;
} ms_ocall_lstat_t;

typedef struct ms_ocall_chmod_t {
	int ms_retval;
	char* ms_file;
	mode_t ms_mode;
} ms_ocall_chmod_t;

typedef struct ms_ocall_fchmod_t {
	int ms_retval;
	int ms_fd;
	mode_t ms_mode;
} ms_ocall_fchmod_t;

typedef struct ms_ocall_fchmodat_t {
	int ms_retval;
	int ms_fd;
	char* ms_file;
	mode_t ms_mode;
	int ms_flag;
} ms_ocall_fchmodat_t;

typedef struct ms_ocall_umask_t {
	mode_t ms_retval;
	mode_t ms_mask;
} ms_ocall_umask_t;

typedef struct ms_ocall_mkdir_t {
	int ms_retval;
	char* ms_path;
	mode_t ms_mode;
} ms_ocall_mkdir_t;

typedef struct ms_ocall_mkdirat_t {
	int ms_retval;
	int ms_fd;
	char* ms_path;
	mode_t ms_mode;
} ms_ocall_mkdirat_t;

typedef struct ms_ocall_mkfifo_t {
	int ms_retval;
	char* ms_path;
	mode_t ms_mode;
} ms_ocall_mkfifo_t;

typedef struct ms_ocall_mkfifoat_t {
	int ms_retval;
	int ms_fd;
	char* ms_path;
	mode_t ms_mode;
} ms_ocall_mkfifoat_t;

typedef struct ms_ocall_opendir_t {
	void* ms_retval;
	char* ms_name;
} ms_ocall_opendir_t;

typedef struct ms_ocall_fdopendir_t {
	void* ms_retval;
	int ms_fd;
} ms_ocall_fdopendir_t;

typedef struct ms_ocall_closedir_t {
	int ms_retval;
	void* ms_dirp;
} ms_ocall_closedir_t;

typedef struct ms_ocall_readdir_t {
	struct dirent* ms_retval;
	void* ms_dirp;
} ms_ocall_readdir_t;

typedef struct ms_ocall_readdir_r_t {
	int ms_retval;
	void* ms_dirp;
	struct dirent* ms_entry;
	struct dirent** ms_result;
} ms_ocall_readdir_r_t;

typedef struct ms_ocall_rewinddir_t {
	void* ms_dirp;
} ms_ocall_rewinddir_t;

typedef struct ms_ocall_seekdir_t {
	void* ms_dirp;
	long int ms_pos;
} ms_ocall_seekdir_t;

typedef struct ms_ocall_telldir_t {
	long int ms_retval;
	void* ms_dirp;
} ms_ocall_telldir_t;

typedef struct ms_ocall_dirfd_t {
	int ms_retval;
	void* ms_dirp;
} ms_ocall_dirfd_t;

typedef struct ms_ocall_alphasort_t {
	int ms_retval;
	struct dirent** ms_e1;
	struct dirent** ms_e2;
} ms_ocall_alphasort_t;

typedef struct ms_ocall_getdirentries_t {
	ssize_t ms_retval;
	int ms_fd;
	char* ms_buf;
	size_t ms_nbytes;
	off_t* ms_basep;
} ms_ocall_getdirentries_t;

typedef struct ms_ocall_versionsort_t {
	int ms_retval;
	struct dirent** ms_e1;
	struct dirent** ms_e2;
} ms_ocall_versionsort_t;

typedef struct ms_ocall_prlimit_t {
	int ms_retval;
	__pid_t ms_pid;
	enum __rlimit_resource ms_resource;
	struct rlimit* ms_new_limit;
	struct rlimit* ms_old_limit;
} ms_ocall_prlimit_t;

typedef struct ms_ocall_getrlimit_t {
	int ms_retval;
	int ms_resource;
	struct rlimit* ms_rlim;
} ms_ocall_getrlimit_t;

typedef struct ms_ocall_setrlimit_t {
	int ms_retval;
	int ms_resource;
	struct rlimit* ms_rlim;
} ms_ocall_setrlimit_t;

typedef struct ms_ocall_inet_addr_t {
	in_addr_t ms_retval;
	char* ms_cp;
} ms_ocall_inet_addr_t;

typedef struct ms_ocall_inet_lnaof_t {
	in_addr_t ms_retval;
	struct in_addr ms_in;
} ms_ocall_inet_lnaof_t;

typedef struct ms_ocall_inet_makeaddr_t {
	struct in_addr ms_retval;
	in_addr_t ms_net;
	in_addr_t ms_host;
} ms_ocall_inet_makeaddr_t;

typedef struct ms_ocall_inet_netof_t {
	in_addr_t ms_retval;
	struct in_addr ms_in;
} ms_ocall_inet_netof_t;

typedef struct ms_ocall_inet_network_t {
	in_addr_t ms_retval;
	char* ms_cp;
} ms_ocall_inet_network_t;

typedef struct ms_ocall_inet_ntoa_t {
	char* ms_retval;
	struct in_addr ms_in;
} ms_ocall_inet_ntoa_t;

typedef struct ms_ocall_inet_pton_t {
	int ms_retval;
	int ms_af;
	char* ms_cp;
	void* ms_buf;
} ms_ocall_inet_pton_t;

typedef struct ms_ocall_inet_ntop_t {
	char* ms_retval;
	int ms_af;
	void* ms_cp;
	char* ms_buf;
	socklen_t ms_len;
} ms_ocall_inet_ntop_t;

typedef struct ms_ocall_sysctl_t {
	int ms_retval;
	int* ms_name;
	int ms_nlen;
	void* ms_oldval;
	size_t* ms_oldlenp;
	void* ms_newval;
	size_t ms_newlen;
} ms_ocall_sysctl_t;

typedef struct ms_ocall_sigemptyset_t {
	int ms_retval;
	sigset_t* ms_set;
} ms_ocall_sigemptyset_t;

typedef struct ms_ocall_sigfillset_t {
	int ms_retval;
	sigset_t* ms_set;
} ms_ocall_sigfillset_t;

typedef struct ms_ocall_sigaddset_t {
	int ms_retval;
	sigset_t* ms_set;
	int ms_signo;
} ms_ocall_sigaddset_t;

typedef struct ms_ocall_sigdelset_t {
	int ms_retval;
	sigset_t* ms_set;
	int ms_signo;
} ms_ocall_sigdelset_t;

typedef struct ms_ocall_sigismember_t {
	int ms_retval;
	sigset_t* ms_set;
	int ms_signo;
} ms_ocall_sigismember_t;

typedef struct ms_ocall_sigsuspend_t {
	int ms_retval;
	sigset_t* ms_set;
} ms_ocall_sigsuspend_t;

typedef struct ms_ocall_sigaction_t {
	int ms_retval;
	int ms_sig;
	struct sigaction* ms_act;
	struct sigaction* ms_oact;
} ms_ocall_sigaction_t;

typedef struct ms_ocall_sigpending_t {
	int ms_retval;
	sigset_t* ms_set;
} ms_ocall_sigpending_t;

typedef struct ms_ocall_sigwait_t {
	int ms_retval;
	sigset_t* ms_set;
	int* ms_sig;
} ms_ocall_sigwait_t;

typedef struct ms_ocall_signal_t {
	__sighandler_t ms_retval;
	int ms___sig;
	__sighandler_t ms___handler;
} ms_ocall_signal_t;

typedef struct ms_ocall_raise_t {
	int ms_retval;
	int ms_sig;
} ms_ocall_raise_t;

typedef struct ms_ocall_getifaddrs_t {
	int ms_retval;
	struct ifaddrs** ms_ifap;
} ms_ocall_getifaddrs_t;

typedef struct ms_ocall_freeifaddrs_t {
	struct ifaddrs* ms_ifa;
} ms_ocall_freeifaddrs_t;

typedef struct ms_ocall_if_nametoindex_t {
	unsigned int ms_retval;
	char* ms_ifname;
} ms_ocall_if_nametoindex_t;

typedef struct ms_ocall_if_indextoname_t {
	char* ms_retval;
	unsigned int ms_ifindex;
	char* ms_ifname;
} ms_ocall_if_indextoname_t;

typedef struct ms_ocall_if_nameindex_t {
	struct if_nameindex* ms_retval;
} ms_ocall_if_nameindex_t;

typedef struct ms_ocall_if_freenameindex_t {
	struct if_nameindex* ms_ptr;
} ms_ocall_if_freenameindex_t;

static sgx_status_t SGX_CDECL sgx_c1_initialize(void* pms)
{
	ms_c1_initialize_t* ms = SGX_CAST(ms_c1_initialize_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	void* _tmp_cpuid = ms->ms_cpuid;
	void** _tmp_top = ms->ms_top;
	void** _tmp_bottom = ms->ms_bottom;
	void** _tmp_klass_list = ms->ms_klass_list;


	ms->ms_retval = c1_initialize(_tmp_cpuid, _tmp_top, _tmp_bottom, _tmp_klass_list, ms->ms_debug);


	return status;
}

static sgx_status_t SGX_CDECL sgx_gc_scavenge(void* pms)
{
	ms_gc_scavenge_t* ms = SGX_CAST(ms_gc_scavenge_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	void* _tmp_tasks = ms->ms_tasks;


	gc_scavenge(_tmp_tasks, ms->ms_n);


	return status;
}

static sgx_status_t SGX_CDECL sgx_within_enclave(void* pms)
{
	ms_within_enclave_t* ms = SGX_CAST(ms_within_enclave_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	void* _tmp_addr = ms->ms_addr;


	ms->ms_retval = within_enclave(_tmp_addr);


	return status;
}

static sgx_status_t SGX_CDECL sgx_interpreted_entry_zero_locals(void* pms)
{
	ms_interpreted_entry_zero_locals_t* ms = SGX_CAST(ms_interpreted_entry_zero_locals_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	void* _tmp_rbx = ms->ms_rbx;
	void* _tmp_method = ms->ms_method;
	int* _tmp_has_exception = ms->ms_has_exception;


	ms->ms_retval = interpreted_entry_zero_locals(_tmp_rbx, _tmp_method, _tmp_has_exception);


	return status;
}

static sgx_status_t SGX_CDECL sgx_ecall_execute_job(void* pms)
{
	ms_ecall_execute_job_t* ms = SGX_CAST(ms_ecall_execute_job_t*, pms);
	sgx_status_t status = SGX_SUCCESS;


	ecall_execute_job(ms->ms_pthread_self_id, ms->ms_job_id);


	return status;
}

static sgx_status_t SGX_CDECL sgx_ecall_set_enclave_id(void* pms)
{
	ms_ecall_set_enclave_id_t* ms = SGX_CAST(ms_ecall_set_enclave_id_t*, pms);
	sgx_status_t status = SGX_SUCCESS;


	ecall_set_enclave_id(ms->ms_self_eid);


	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv;} ecall_table[6];
} g_ecall_table = {
	6,
	{
		{(void*)(uintptr_t)sgx_c1_initialize, 0},
		{(void*)(uintptr_t)sgx_gc_scavenge, 0},
		{(void*)(uintptr_t)sgx_within_enclave, 0},
		{(void*)(uintptr_t)sgx_interpreted_entry_zero_locals, 0},
		{(void*)(uintptr_t)sgx_ecall_execute_job, 0},
		{(void*)(uintptr_t)sgx_ecall_set_enclave_id, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[288][6];
} g_dyn_entry_table = {
	288,
	{
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 1, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
		{0, 0, 0, 0, 0, 0, },
	}
};


sgx_status_t SGX_CDECL ocall_interpreter(void** retval, void* r14, int size, void* method, void* thread, void* sender)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_interpreter_t ms_o;
	ms_ocall_interpreter_t* ms = &ms_o;
	ms->ms_r14 = SGX_CAST(void*, r14);
	ms->ms_size = size;
	ms->ms_method = SGX_CAST(void*, method);
	ms->ms_thread = SGX_CAST(void*, thread);
	ms->ms_sender = SGX_CAST(void*, sender);
	status = sgx_ocall(0, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke(int byte, void* mh, int bci, void* recv, int idx, void* bcp, void* recv_klass)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_resolve_invoke_t ms_o;
	ms_ocall_jvm_resolve_invoke_t* ms = &ms_o;
	ms->ms_byte = byte;
	ms->ms_mh = SGX_CAST(void*, mh);
	ms->ms_bci = bci;
	ms->ms_recv = SGX_CAST(void*, recv);
	ms->ms_idx = idx;
	ms->ms_bcp = SGX_CAST(void*, bcp);
	ms->ms_recv_klass = SGX_CAST(void*, recv_klass);
	status = sgx_ocall(1, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_method_C(void** retval, int bc, void* constant, int idx)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_resolve_method_C_t ms_o;
	ms_ocall_jvm_resolve_method_C_t* ms = &ms_o;
	ms->ms_bc = bc;
	ms->ms_constant = SGX_CAST(void*, constant);
	ms->ms_idx = idx;
	status = sgx_ocall(2, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_C(void** retval, void* recv, void* constant, int bidx, int bc, void* recv_klass)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_resolve_invoke_C_t ms_o;
	ms_ocall_jvm_resolve_invoke_C_t* ms = &ms_o;
	ms->ms_recv = SGX_CAST(void*, recv);
	ms->ms_constant = SGX_CAST(void*, constant);
	ms->ms_bidx = bidx;
	ms->ms_bc = bc;
	ms->ms_recv_klass = SGX_CAST(void*, recv_klass);
	status = sgx_ocall(3, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_handle()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(4, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_dynamic()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(5, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_ldc(void** retval, bool wide, void* p, int index)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_ldc_t ms_o;
	ms_ocall_jvm_ldc_t* ms = &ms_o;
	ms->ms_wide = wide;
	ms->ms_p = SGX_CAST(void*, p);
	ms->ms_index = index;
	status = sgx_ocall(6, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_ldc(void** retval, void* p, int index, int byte)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_resolve_ldc_t ms_o;
	ms_ocall_jvm_resolve_ldc_t* ms = &ms_o;
	ms->ms_p = SGX_CAST(void*, p);
	ms->ms_index = index;
	ms->ms_byte = byte;
	status = sgx_ocall(7, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_get_put(int* retval, int byte, void* mh, int idx, void* bcp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_resolve_get_put_t ms_o;
	ms_ocall_jvm_resolve_get_put_t* ms = &ms_o;
	ms->ms_byte = byte;
	ms->ms_mh = SGX_CAST(void*, mh);
	ms->ms_idx = idx;
	ms->ms_bcp = SGX_CAST(void*, bcp);
	status = sgx_ocall(8, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_resolve_get_put_klass(void** retval, int byte, void* mh, int idx, void* bcp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_resolve_get_put_klass_t ms_o;
	ms_ocall_jvm_resolve_get_put_klass_t* ms = &ms_o;
	ms->ms_byte = byte;
	ms->ms_mh = SGX_CAST(void*, mh);
	ms->ms_idx = idx;
	ms->ms_bcp = SGX_CAST(void*, bcp);
	status = sgx_ocall(9, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_klass_type_array(void** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_klass_type_array_t ms_o;
	ms_ocall_klass_type_array_t* ms = &ms_o;
	status = sgx_ocall(10, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_klass_type(void** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_klass_type_t ms_o;
	ms_ocall_klass_type_t* ms = &ms_o;
	status = sgx_ocall(11, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_obj_array_klass_get(void** retval, void* pool, int index)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_obj_array_klass_get_t ms_o;
	ms_ocall_obj_array_klass_get_t* ms = &ms_o;
	ms->ms_pool = SGX_CAST(void*, pool);
	ms->ms_index = index;
	status = sgx_ocall(12, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_klass_resolve_or_fail(void** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_klass_resolve_or_fail_t ms_o;
	ms_ocall_klass_resolve_or_fail_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(13, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_klass_find_or_null(void** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_klass_find_or_null_t ms_o;
	ms_ocall_klass_find_or_null_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(14, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_klass_get(void** retval, void* pool, int index)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_klass_get_t ms_o;
	ms_ocall_klass_get_t* ms = &ms_o;
	ms->ms_pool = SGX_CAST(void*, pool);
	ms->ms_index = index;
	status = sgx_ocall(15, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_klass_compute_oopmap(void* klass, void* m, int bci)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_klass_compute_oopmap_t ms_o;
	ms_ocall_klass_compute_oopmap_t* ms = &ms_o;
	ms->ms_klass = SGX_CAST(void*, klass);
	ms->ms_m = SGX_CAST(void*, m);
	ms->ms_bci = bci;
	status = sgx_ocall(16, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_array_klass(void** retval, void* klass, int rank, int get_all)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_array_klass_t ms_o;
	ms_ocall_array_klass_t* ms = &ms_o;
	ms->ms_klass = SGX_CAST(void*, klass);
	ms->ms_rank = rank;
	ms->ms_get_all = get_all;
	status = sgx_ocall(17, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_multi_array_klass_get(void** retval, void* pool, int index)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_multi_array_klass_get_t ms_o;
	ms_ocall_multi_array_klass_get_t* ms = &ms_o;
	ms->ms_pool = SGX_CAST(void*, pool);
	ms->ms_index = index;
	status = sgx_ocall(18, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_pre_native(void* method, int resolve)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_pre_native_t ms_o;
	ms_ocall_jvm_pre_native_t* ms = &ms_o;
	ms->ms_method = SGX_CAST(void*, method);
	ms->ms_resolve = resolve;
	status = sgx_ocall(19, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_jvm_malloc(void** retval, int size)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_jvm_malloc_t ms_o;
	ms_ocall_jvm_malloc_t* ms = &ms_o;
	ms->ms_size = size;
	status = sgx_ocall(20, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_classfile_buffer(void** retval, const char* name, int* size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;
	size_t _len_size = 4 * sizeof(*size);

	ms_ocall_classfile_buffer_t ms_o;
	ms_ocall_classfile_buffer_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	ms->ms_size = SGX_CAST(int*, size);
	status = sgx_ocall(21, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_create(int* retval, pthread_t* new_thread, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, unsigned long int job_id, sgx_enclave_id_t eid)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_create_t ms_o;
	ms_ocall_pthread_create_t* ms = &ms_o;
	ms->ms_new_thread = SGX_CAST(pthread_t*, new_thread);
	ms->ms___attr = __attr;
	ms->ms_job_id = job_id;
	ms->ms_eid = eid;
	status = sgx_ocall(22, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_self(pthread_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_self_t ms_o;
	ms_ocall_pthread_self_t* ms = &ms_o;
	status = sgx_ocall(23, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_join(int* retval, pthread_t pt, void** thread_result)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_join_t ms_o;
	ms_ocall_pthread_join_t* ms = &ms_o;
	ms->ms_pt = pt;
	ms->ms_thread_result = SGX_CAST(void**, thread_result);
	status = sgx_ocall(24, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_detach(int* retval, pthread_t pt)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_detach_t ms_o;
	ms_ocall_pthread_detach_t* ms = &ms_o;
	ms->ms_pt = pt;
	status = sgx_ocall(25, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_equal(int* retval, pthread_t pt1, pthread_t pt2)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_equal_t ms_o;
	ms_ocall_pthread_equal_t* ms = &ms_o;
	ms->ms_pt1 = pt1;
	ms->ms_pt2 = pt2;
	status = sgx_ocall(26, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_exit(void* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_exit_t ms_o;
	ms_ocall_pthread_exit_t* ms = &ms_o;
	ms->ms_retval = SGX_CAST(void*, retval);
	status = sgx_ocall(27, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_cancel(int* retval, pthread_t th)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_cancel_t ms_o;
	ms_ocall_pthread_cancel_t* ms = &ms_o;
	ms->ms_th = th;
	status = sgx_ocall(28, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_testcancel()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(29, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_init(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE* __attr)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___attr = 1;

	ms_ocall_pthread_attr_init_t ms_o;
	ms_ocall_pthread_attr_init_t* ms = &ms_o;
	ms->ms___attr = SGX_CAST(SGX_WRAPPER_PTHREAD_ATTRIBUTE*, __attr);
	status = sgx_ocall(30, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_destroy(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_attr_destroy_t ms_o;
	ms_ocall_pthread_attr_destroy_t* ms = &ms_o;
	ms->ms___attr = __attr;
	status = sgx_ocall(31, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_getdetachstate(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int* __detachstate)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___detachstate = 1;

	ms_ocall_pthread_attr_getdetachstate_t ms_o;
	ms_ocall_pthread_attr_getdetachstate_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___detachstate = SGX_CAST(int*, __detachstate);
	status = sgx_ocall(32, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_setdetachstate(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __detachstate)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_attr_setdetachstate_t ms_o;
	ms_ocall_pthread_attr_setdetachstate_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___detachstate = __detachstate;
	status = sgx_ocall(33, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_getguardsize(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t* __guardsize)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___guardsize = 1;

	ms_ocall_pthread_attr_getguardsize_t ms_o;
	ms_ocall_pthread_attr_getguardsize_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___guardsize = SGX_CAST(size_t*, __guardsize);
	status = sgx_ocall(34, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_setguardsize(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __guardsize)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_attr_setguardsize_t ms_o;
	ms_ocall_pthread_attr_setguardsize_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___guardsize = __guardsize;
	status = sgx_ocall(35, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_getschedpolicy(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int* __policy)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___policy = 1;

	ms_ocall_pthread_attr_getschedpolicy_t ms_o;
	ms_ocall_pthread_attr_getschedpolicy_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___policy = SGX_CAST(int*, __policy);
	status = sgx_ocall(36, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_setschedpolicy(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, int __policy)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_attr_setschedpolicy_t ms_o;
	ms_ocall_pthread_attr_setschedpolicy_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___policy = __policy;
	status = sgx_ocall(37, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_getstacksize(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t* __stacksize)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___stacksize = 1;

	ms_ocall_pthread_attr_getstacksize_t ms_o;
	ms_ocall_pthread_attr_getstacksize_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___stacksize = SGX_CAST(size_t*, __stacksize);
	status = sgx_ocall(38, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_attr_setstacksize(int* retval, SGX_WRAPPER_PTHREAD_ATTRIBUTE __attr, size_t __stacksize)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_attr_setstacksize_t ms_o;
	ms_ocall_pthread_attr_setstacksize_t* ms = &ms_o;
	ms->ms___attr = __attr;
	ms->ms___stacksize = __stacksize;
	status = sgx_ocall(39, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_setspecific(int* retval, pthread_key_t key, const void* value)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_setspecific_t ms_o;
	ms_ocall_pthread_setspecific_t* ms = &ms_o;
	ms->ms_key = key;
	ms->ms_value = SGX_CAST(void*, value);
	status = sgx_ocall(40, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_getspecific(void** retval, pthread_key_t key)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pthread_getspecific_t ms_o;
	ms_ocall_pthread_getspecific_t* ms = &ms_o;
	ms->ms_key = key;
	status = sgx_ocall(41, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pthread_key_create(int* retval, pthread_key_t* key, void* destructor)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_key = sizeof(*key);

	ms_ocall_pthread_key_create_t ms_o;
	ms_ocall_pthread_key_create_t* ms = &ms_o;
	ms->ms_key = SGX_CAST(pthread_key_t*, key);
	ms->ms_destructor = SGX_CAST(void*, destructor);
	status = sgx_ocall(42, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_time(time_t* retval, time_t* t)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_t = sizeof(*t);

	ms_ocall_time_t ms_o;
	ms_ocall_time_t* ms = &ms_o;
	ms->ms_t = SGX_CAST(time_t*, t);
	status = sgx_ocall(43, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gettimeofday(int* retval, void* tv, int tv_size, void* tz, int tz_size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_tv = tv_size;
	size_t _len_tz = tz_size;

	ms_ocall_gettimeofday_t ms_o;
	ms_ocall_gettimeofday_t* ms = &ms_o;
	ms->ms_tv = SGX_CAST(void*, tv);
	ms->ms_tv_size = tv_size;
	ms->ms_tz = SGX_CAST(void*, tz);
	ms->ms_tz_size = tz_size;
	status = sgx_ocall(44, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gettimeofday2(int* retval, void* tv, int tv_size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_tv = tv_size;

	ms_ocall_gettimeofday2_t ms_o;
	ms_ocall_gettimeofday2_t* ms = &ms_o;
	ms->ms_tv = SGX_CAST(void*, tv);
	ms->ms_tv_size = tv_size;
	status = sgx_ocall(45, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_clock(clock_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_clock_t ms_o;
	ms_ocall_clock_t* ms = &ms_o;
	status = sgx_ocall(46, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gmtime_r(struct tm** retval, const time_t* timer, struct tm* tp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_timer = sizeof(*timer);
	size_t _len_tp = sizeof(*tp);

	ms_ocall_gmtime_r_t ms_o;
	ms_ocall_gmtime_r_t* ms = &ms_o;
	ms->ms_timer = SGX_CAST(time_t*, timer);
	ms->ms_tp = SGX_CAST(struct tm*, tp);
	status = sgx_ocall(47, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_localtime_r(struct tm** retval, const time_t* timer, struct tm* tp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_timer = sizeof(*timer);
	size_t _len_tp = sizeof(*tp);

	ms_ocall_localtime_r_t ms_o;
	ms_ocall_localtime_r_t* ms = &ms_o;
	ms->ms_timer = SGX_CAST(time_t*, timer);
	ms->ms_tp = SGX_CAST(struct tm*, tp);
	status = sgx_ocall(48, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mktime(time_t* retval, struct tm* tp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_tp = sizeof(*tp);

	ms_ocall_mktime_t ms_o;
	ms_ocall_mktime_t* ms = &ms_o;
	ms->ms_tp = SGX_CAST(struct tm*, tp);
	status = sgx_ocall(49, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getpwuid(struct passwd** retval, uid_t uid)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getpwuid_t ms_o;
	ms_ocall_getpwuid_t* ms = &ms_o;
	ms->ms_uid = uid;
	status = sgx_ocall(50, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getpwnam(struct passwd** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_getpwnam_t ms_o;
	ms_ocall_getpwnam_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(51, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getpwnam_r(int* retval, const char* name, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;
	size_t _len_pwd = pwd ? strlen(pwd) + 1 : 0;
	size_t _len_buf = buflen;
	size_t _len_result = result ? strlen(result) + 1 : 0;

	ms_ocall_getpwnam_r_t ms_o;
	ms_ocall_getpwnam_r_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	ms->ms_pwd = SGX_CAST(struct passwd*, pwd);
	ms->ms_buf = SGX_CAST(char*, buf);
	ms->ms_buflen = buflen;
	ms->ms_result = SGX_CAST(struct passwd**, result);
	status = sgx_ocall(52, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getgrgid(struct group** retval, gid_t gid)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getgrgid_t ms_o;
	ms_ocall_getgrgid_t* ms = &ms_o;
	ms->ms_gid = gid;
	status = sgx_ocall(53, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_initgroups(int* retval, const char* user, gid_t group)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_user = user ? strlen(user) + 1 : 0;

	ms_ocall_initgroups_t ms_o;
	ms_ocall_initgroups_t* ms = &ms_o;
	ms->ms_user = SGX_CAST(char*, user);
	ms->ms_group = group;
	status = sgx_ocall(54, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_uname(int* retval, struct utsname* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = sizeof(*name);

	ms_ocall_uname_t ms_o;
	ms_ocall_uname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(struct utsname*, name);
	status = sgx_ocall(55, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL wrapper_getopt(int* retval, int argc, char** argv, const char* optstring)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_optstring = optstring ? strlen(optstring) + 1 : 0;

	ms_wrapper_getopt_t ms_o;
	ms_wrapper_getopt_t* ms = &ms_o;
	ms->ms_argc = argc;
	ms->ms_argv = SGX_CAST(char**, argv);
	ms->ms_optstring = SGX_CAST(char*, optstring);
	status = sgx_ocall(56, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL set_optind(int oi)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_set_optind_t ms_o;
	ms_set_optind_t* ms = &ms_o;
	ms->ms_oi = oi;
	status = sgx_ocall(57, ms);

	return status;
}
sgx_status_t SGX_CDECL set_opterr(int oe)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_set_opterr_t ms_o;
	ms_set_opterr_t* ms = &ms_o;
	ms->ms_oe = oe;
	status = sgx_ocall(58, ms);

	return status;
}
sgx_status_t SGX_CDECL set_optopt(int oo)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_set_optopt_t ms_o;
	ms_set_optopt_t* ms = &ms_o;
	ms->ms_oo = oo;
	status = sgx_ocall(59, ms);

	return status;
}
sgx_status_t SGX_CDECL set_optreset(int ors)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_set_optreset_t ms_o;
	ms_set_optreset_t* ms = &ms_o;
	ms->ms_ors = ors;
	status = sgx_ocall(60, ms);

	return status;
}
sgx_status_t SGX_CDECL get_optarg(char** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_get_optarg_t ms_o;
	ms_get_optarg_t* ms = &ms_o;
	status = sgx_ocall(61, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getenv(char** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_getenv_t ms_o;
	ms_ocall_getenv_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(62, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_putenv(int* retval, char* string)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_string = string ? strlen(string) + 1 : 0;

	ms_ocall_putenv_t ms_o;
	ms_ocall_putenv_t* ms = &ms_o;
	ms->ms_string = SGX_CAST(char*, string);
	status = sgx_ocall(63, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_clearenv(int* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_clearenv_t ms_o;
	ms_ocall_clearenv_t* ms = &ms_o;
	status = sgx_ocall(64, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setenv(int* retval, const char* name, const char* value, int replace)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;
	size_t _len_value = value ? strlen(value) + 1 : 0;

	ms_ocall_setenv_t ms_o;
	ms_ocall_setenv_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	ms->ms_value = SGX_CAST(char*, value);
	ms->ms_replace = replace;
	status = sgx_ocall(65, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_unsetenv(int* retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_unsetenv_t ms_o;
	ms_ocall_unsetenv_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(66, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mkstemp(int* retval, char* temp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_temp = temp ? strlen(temp) + 1 : 0;

	ms_ocall_mkstemp_t ms_o;
	ms_ocall_mkstemp_t* ms = &ms_o;
	ms->ms_temp = SGX_CAST(char*, temp);
	status = sgx_ocall(67, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mkdtemp(char** retval, char* temp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_temp = temp ? strlen(temp) + 1 : 0;

	ms_ocall_mkdtemp_t ms_o;
	ms_ocall_mkdtemp_t* ms = &ms_o;
	ms->ms_temp = SGX_CAST(char*, temp);
	status = sgx_ocall(68, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_open1(int* retval, const char* pathname, int flags)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_pathname = pathname ? strlen(pathname) + 1 : 0;

	ms_ocall_open1_t ms_o;
	ms_ocall_open1_t* ms = &ms_o;
	ms->ms_pathname = SGX_CAST(char*, pathname);
	ms->ms_flags = flags;
	status = sgx_ocall(69, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_open2(int* retval, const char* pathname, int flags, unsigned int mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_pathname = pathname ? strlen(pathname) + 1 : 0;

	ms_ocall_open2_t ms_o;
	ms_ocall_open2_t* ms = &ms_o;
	ms->ms_pathname = SGX_CAST(char*, pathname);
	ms->ms_flags = flags;
	ms->ms_mode = mode;
	status = sgx_ocall(70, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_creat(int* retval, const char* pathname, unsigned int mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_pathname = pathname ? strlen(pathname) + 1 : 0;

	ms_ocall_creat_t ms_o;
	ms_ocall_creat_t* ms = &ms_o;
	ms->ms_pathname = SGX_CAST(char*, pathname);
	ms->ms_mode = mode;
	status = sgx_ocall(71, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_openat1(int* retval, int dirfd, const char* pathname, int flags)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_pathname = pathname ? strlen(pathname) + 1 : 0;

	ms_ocall_openat1_t ms_o;
	ms_ocall_openat1_t* ms = &ms_o;
	ms->ms_dirfd = dirfd;
	ms->ms_pathname = SGX_CAST(char*, pathname);
	ms->ms_flags = flags;
	status = sgx_ocall(72, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_openat2(int* retval, int dirfd, const char* pathname, int flags, unsigned int mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_pathname = pathname ? strlen(pathname) + 1 : 0;

	ms_ocall_openat2_t ms_o;
	ms_ocall_openat2_t* ms = &ms_o;
	ms->ms_dirfd = dirfd;
	ms->ms_pathname = SGX_CAST(char*, pathname);
	ms->ms_flags = flags;
	ms->ms_mode = mode;
	status = sgx_ocall(73, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fcntl1(int* retval, int fd, int cmd)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fcntl1_t ms_o;
	ms_ocall_fcntl1_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_cmd = cmd;
	status = sgx_ocall(74, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fcntl2(int* retval, int fd, int cmd, long int arg)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fcntl2_t ms_o;
	ms_ocall_fcntl2_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_cmd = cmd;
	ms->ms_arg = arg;
	status = sgx_ocall(75, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fcntl3(int* retval, int fd, int cmd, void* arg, int size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_arg = size;

	ms_ocall_fcntl3_t ms_o;
	ms_ocall_fcntl3_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_cmd = cmd;
	ms->ms_arg = SGX_CAST(void*, arg);
	ms->ms_size = size;
	status = sgx_ocall(76, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dlopen(void** retval, const char* __file, int __mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___file = __file ? strlen(__file) + 1 : 0;

	ms_ocall_dlopen_t ms_o;
	ms_ocall_dlopen_t* ms = &ms_o;
	ms->ms___file = SGX_CAST(char*, __file);
	ms->ms___mode = __mode;
	status = sgx_ocall(77, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dlclose(int* retval, void* __handle)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dlclose_t ms_o;
	ms_ocall_dlclose_t* ms = &ms_o;
	ms->ms___handle = SGX_CAST(void*, __handle);
	status = sgx_ocall(78, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dlsym(void** retval, void* __handle, const char* __name)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dlsym_t ms_o;
	ms_ocall_dlsym_t* ms = &ms_o;
	ms->ms___handle = SGX_CAST(void*, __handle);
	ms->ms___name = SGX_CAST(char*, __name);
	status = sgx_ocall(79, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dladdr(int* retval, const void* __address, void* __info)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dladdr_t ms_o;
	ms_ocall_dladdr_t* ms = &ms_o;
	ms->ms___address = SGX_CAST(void*, __address);
	ms->ms___info = SGX_CAST(void*, __info);
	status = sgx_ocall(80, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dladdr1(int* retval, const void* __address, void* __info, void** __extra_info, int __flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dladdr1_t ms_o;
	ms_ocall_dladdr1_t* ms = &ms_o;
	ms->ms___address = SGX_CAST(void*, __address);
	ms->ms___info = SGX_CAST(void*, __info);
	ms->ms___extra_info = SGX_CAST(void**, __extra_info);
	ms->ms___flags = __flags;
	status = sgx_ocall(81, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dlinfo(int* retval, void* __handle, int __request, void* __arg)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dlinfo_t ms_o;
	ms_ocall_dlinfo_t* ms = &ms_o;
	ms->ms___handle = SGX_CAST(void*, __handle);
	ms->ms___request = __request;
	ms->ms___arg = SGX_CAST(void*, __arg);
	status = sgx_ocall(82, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_syscall(long int* retval, long int __sysno, void* val)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_syscall_t ms_o;
	ms_ocall_syscall_t* ms = &ms_o;
	ms->ms___sysno = __sysno;
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(83, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gethostname(int* retval, char* name, size_t len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = len;

	ms_ocall_gethostname_t ms_o;
	ms_ocall_gethostname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	ms->ms_len = len;
	status = sgx_ocall(84, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sethostname(int* retval, const char* name, size_t len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = len;

	ms_ocall_sethostname_t ms_o;
	ms_ocall_sethostname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	ms->ms_len = len;
	status = sgx_ocall(85, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_lseek(off_t* retval, int fd, off_t offset, int whence)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_lseek_t ms_o;
	ms_ocall_lseek_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_offset = offset;
	ms->ms_whence = whence;
	status = sgx_ocall(86, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fsync(int* retval, int fd)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fsync_t ms_o;
	ms_ocall_fsync_t* ms = &ms_o;
	ms->ms_fd = fd;
	status = sgx_ocall(87, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_read(off_t* retval, int fd, void* buf, size_t count)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = count;

	ms_ocall_read_t ms_o;
	ms_ocall_read_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_count = count;
	status = sgx_ocall(88, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_write(off_t* retval, int fd, const void* buf, size_t count)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = count;

	ms_ocall_write_t ms_o;
	ms_ocall_write_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_count = count;
	status = sgx_ocall(89, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_close(int* retval, int fd)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_close_t ms_o;
	ms_ocall_close_t* ms = &ms_o;
	ms->ms_fd = fd;
	status = sgx_ocall(90, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getpid(pid_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getpid_t ms_o;
	ms_ocall_getpid_t* ms = &ms_o;
	status = sgx_ocall(91, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getppid(pid_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getppid_t ms_o;
	ms_ocall_getppid_t* ms = &ms_o;
	status = sgx_ocall(92, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pread(ssize_t* retval, int fd, void* buf, size_t nbytes, off_t offset)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = nbytes;

	ms_ocall_pread_t ms_o;
	ms_ocall_pread_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_nbytes = nbytes;
	ms->ms_offset = offset;
	status = sgx_ocall(93, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pwrite(ssize_t* retval, int fd, const void* buf, size_t n, off_t offset)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = n;

	ms_ocall_pwrite_t ms_o;
	ms_ocall_pwrite_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_n = n;
	ms->ms_offset = offset;
	status = sgx_ocall(94, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pipe(int* retval, int pipedes[2])
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pipe_t ms_o;
	ms_ocall_pipe_t* ms = &ms_o;
	ms->ms_pipedes = SGX_CAST(int*, pipedes);
	status = sgx_ocall(95, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_pipe2(int* retval, int pipedes[2], int flag)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_pipe2_t ms_o;
	ms_ocall_pipe2_t* ms = &ms_o;
	ms->ms_pipedes = SGX_CAST(int*, pipedes);
	ms->ms_flag = flag;
	status = sgx_ocall(96, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sleep(unsigned int* retval, unsigned int seconds)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sleep_t ms_o;
	ms_ocall_sleep_t* ms = &ms_o;
	ms->ms_seconds = seconds;
	status = sgx_ocall(97, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_usleep(unsigned int* retval, unsigned int seconds)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_usleep_t ms_o;
	ms_ocall_usleep_t* ms = &ms_o;
	ms->ms_seconds = seconds;
	status = sgx_ocall(98, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_chown(int* retval, const char* file, uid_t owner, gid_t group)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_file = file ? strlen(file) + 1 : 0;

	ms_ocall_chown_t ms_o;
	ms_ocall_chown_t* ms = &ms_o;
	ms->ms_file = SGX_CAST(char*, file);
	ms->ms_owner = owner;
	ms->ms_group = group;
	status = sgx_ocall(99, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fchown(int* retval, int fd, uid_t owner, gid_t group)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fchown_t ms_o;
	ms_ocall_fchown_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_owner = owner;
	ms->ms_group = group;
	status = sgx_ocall(100, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_lchown(int* retval, const char* file, uid_t owner, gid_t group)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_file = file ? strlen(file) + 1 : 0;

	ms_ocall_lchown_t ms_o;
	ms_ocall_lchown_t* ms = &ms_o;
	ms->ms_file = SGX_CAST(char*, file);
	ms->ms_owner = owner;
	ms->ms_group = group;
	status = sgx_ocall(101, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_chdir(int* retval, const char* path)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;

	ms_ocall_chdir_t ms_o;
	ms_ocall_chdir_t* ms = &ms_o;
	ms->ms_path = SGX_CAST(char*, path);
	status = sgx_ocall(102, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fchdir(int* retval, int fd)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fchdir_t ms_o;
	ms_ocall_fchdir_t* ms = &ms_o;
	ms->ms_fd = fd;
	status = sgx_ocall(103, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_get_current_dir_name(char** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_get_current_dir_name_t ms_o;
	ms_ocall_get_current_dir_name_t* ms = &ms_o;
	status = sgx_ocall(104, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dup(int* retval, int fd)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dup_t ms_o;
	ms_ocall_dup_t* ms = &ms_o;
	ms->ms_fd = fd;
	status = sgx_ocall(105, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dup2(int* retval, int fd, int fd2)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dup2_t ms_o;
	ms_ocall_dup2_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_fd2 = fd2;
	status = sgx_ocall(106, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dup3(int* retval, int fd, int fd2, int flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dup3_t ms_o;
	ms_ocall_dup3_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_fd2 = fd2;
	ms->ms_flags = flags;
	status = sgx_ocall(107, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getuid(uid_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getuid_t ms_o;
	ms_ocall_getuid_t* ms = &ms_o;
	status = sgx_ocall(108, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_geteuid(uid_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_geteuid_t ms_o;
	ms_ocall_geteuid_t* ms = &ms_o;
	status = sgx_ocall(109, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getgid(gid_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getgid_t ms_o;
	ms_ocall_getgid_t* ms = &ms_o;
	status = sgx_ocall(110, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getegid(gid_t* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getegid_t ms_o;
	ms_ocall_getegid_t* ms = &ms_o;
	status = sgx_ocall(111, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getpagesize(int* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getpagesize_t ms_o;
	ms_ocall_getpagesize_t* ms = &ms_o;
	status = sgx_ocall(112, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getcwd(char** retval, char* buf, size_t size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = size;

	ms_ocall_getcwd_t ms_o;
	ms_ocall_getcwd_t* ms = &ms_o;
	ms->ms_buf = SGX_CAST(char*, buf);
	ms->ms_size = size;
	status = sgx_ocall(113, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_unlink(int* retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_unlink_t ms_o;
	ms_ocall_unlink_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(114, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_rmdir(int* retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_rmdir_t ms_o;
	ms_ocall_rmdir_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(115, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall__exit(int stat)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall__exit_t ms_o;
	ms_ocall__exit_t* ms = &ms_o;
	ms->ms_stat = stat;
	status = sgx_ocall(116, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_sysconf(long int* retval, int name)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sysconf_t ms_o;
	ms_ocall_sysconf_t* ms = &ms_o;
	ms->ms_name = name;
	status = sgx_ocall(117, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setgid(int* retval, gid_t gid)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_setgid_t ms_o;
	ms_ocall_setgid_t* ms = &ms_o;
	ms->ms_gid = gid;
	status = sgx_ocall(118, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setuid(int* retval, uid_t uid)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_setuid_t ms_o;
	ms_ocall_setuid_t* ms = &ms_o;
	ms->ms_uid = uid;
	status = sgx_ocall(119, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_execvp(int* retval, const char* file, const char** argv)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_file = file ? strlen(file) + 1 : 0;

	ms_ocall_execvp_t ms_o;
	ms_ocall_execvp_t* ms = &ms_o;
	ms->ms_file = SGX_CAST(char*, file);
	ms->ms_argv = SGX_CAST(char**, argv);
	status = sgx_ocall(120, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ftruncate(int* retval, int fd, off_t len)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ftruncate_t ms_o;
	ms_ocall_ftruncate_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_len = len;
	status = sgx_ocall(121, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ftruncate64(int* retval, int fd, __off64_t length)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ftruncate64_t ms_o;
	ms_ocall_ftruncate64_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_length = length;
	status = sgx_ocall(122, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_free(void* p)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_free_t ms_o;
	ms_ocall_free_t* ms = &ms_o;
	ms->ms_p = SGX_CAST(void*, p);
	status = sgx_ocall(123, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_geterrno(int* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_geterrno_t ms_o;
	ms_ocall_geterrno_t* ms = &ms_o;
	status = sgx_ocall(124, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fopen(SGX_WRAPPER_FILE* retval, const char* filename, const char* mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;
	size_t _len_mode = mode ? strlen(mode) + 1 : 0;

	ms_ocall_fopen_t ms_o;
	ms_ocall_fopen_t* ms = &ms_o;
	ms->ms_filename = SGX_CAST(char*, filename);
	ms->ms_mode = SGX_CAST(char*, mode);
	status = sgx_ocall(125, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fclose(int* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fclose_t ms_o;
	ms_ocall_fclose_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(126, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fputs(int* retval, const char* str, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = str ? strlen(str) + 1 : 0;

	ms_ocall_fputs_t ms_o;
	ms_ocall_fputs_t* ms = &ms_o;
	ms->ms_str = SGX_CAST(char*, str);
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(127, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_feof(int* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_feof_t ms_o;
	ms_ocall_feof_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(128, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_rewind(SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_rewind_t ms_o;
	ms_ocall_rewind_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(129, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_fflush(int* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fflush_t ms_o;
	ms_ocall_fflush_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(130, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fread(size_t* retval, void* ptr, size_t size, size_t nmemb, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_ptr = nmemb * size;

	ms_ocall_fread_t ms_o;
	ms_ocall_fread_t* ms = &ms_o;
	ms->ms_ptr = SGX_CAST(void*, ptr);
	ms->ms_size = size;
	ms->ms_nmemb = nmemb;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(131, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fwrite(size_t* retval, const void* ptr, size_t size, size_t count, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_ptr = count * size;

	ms_ocall_fwrite_t ms_o;
	ms_ocall_fwrite_t* ms = &ms_o;
	ms->ms_ptr = SGX_CAST(void*, ptr);
	ms->ms_size = size;
	ms->ms_count = count;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(132, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fgets(char** retval, char* str, int num, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = num;

	ms_ocall_fgets_t ms_o;
	ms_ocall_fgets_t* ms = &ms_o;
	ms->ms_str = SGX_CAST(char*, str);
	ms->ms_num = num;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(133, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getc_unlocked(int* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getc_unlocked_t ms_o;
	ms_ocall_getc_unlocked_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(134, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_flockfile(SGX_WRAPPER_FILE filehandle)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_flockfile_t ms_o;
	ms_ocall_flockfile_t* ms = &ms_o;
	ms->ms_filehandle = filehandle;
	status = sgx_ocall(135, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_funlockfile(SGX_WRAPPER_FILE filehandle)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_funlockfile_t ms_o;
	ms_ocall_funlockfile_t* ms = &ms_o;
	ms->ms_filehandle = filehandle;
	status = sgx_ocall(136, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_ftello(off_t* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ftello_t ms_o;
	ms_ocall_ftello_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(137, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fseeko(int* retval, SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fseeko_t ms_o;
	ms_ocall_fseeko_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	ms->ms_offset = offset;
	ms->ms_whence = whence;
	status = sgx_ocall(138, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ftell(off_t* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ftell_t ms_o;
	ms_ocall_ftell_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(139, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fseek(int* retval, SGX_WRAPPER_FILE FILESTREAM, off_t offset, int whence)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fseek_t ms_o;
	ms_ocall_fseek_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	ms->ms_offset = offset;
	ms->ms_whence = whence;
	status = sgx_ocall(140, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ferror(int* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ferror_t ms_o;
	ms_ocall_ferror_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(141, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_perror(const char* s)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_s = s ? strlen(s) + 1 : 0;

	ms_ocall_perror_t ms_o;
	ms_ocall_perror_t* ms = &ms_o;
	ms->ms_s = SGX_CAST(char*, s);
	status = sgx_ocall(142, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_getc(int* retval, SGX_WRAPPER_FILE FILESTREAM)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getc_t ms_o;
	ms_ocall_getc_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	status = sgx_ocall(143, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_putchar(int* retval, int c)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_putchar_t ms_o;
	ms_ocall_putchar_t* ms = &ms_o;
	ms->ms_c = c;
	status = sgx_ocall(144, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_putc(int* retval, int c, SGX_WRAPPER_FILE stream)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_putc_t ms_o;
	ms_ocall_putc_t* ms = &ms_o;
	ms->ms_c = c;
	ms->ms_stream = stream;
	status = sgx_ocall(145, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fputc(int* retval, int c, SGX_WRAPPER_FILE stream)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fputc_t ms_o;
	ms_ocall_fputc_t* ms = &ms_o;
	ms->ms_c = c;
	ms->ms_stream = stream;
	status = sgx_ocall(146, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fdopen(SGX_WRAPPER_FILE* retval, int fd, const char* modes)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_modes = modes ? strlen(modes) + 1 : 0;

	ms_ocall_fdopen_t ms_o;
	ms_ocall_fdopen_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_modes = SGX_CAST(char*, modes);
	status = sgx_ocall(147, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fileno(int* retval, SGX_WRAPPER_FILE stream)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fileno_t ms_o;
	ms_ocall_fileno_t* ms = &ms_o;
	ms->ms_stream = stream;
	status = sgx_ocall(148, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_rename(int* retval, const char* _old, const char* _new)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len__old = _old ? strlen(_old) + 1 : 0;
	size_t _len__new = _new ? strlen(_new) + 1 : 0;

	ms_ocall_rename_t ms_o;
	ms_ocall_rename_t* ms = &ms_o;
	ms->ms__old = SGX_CAST(char*, _old);
	ms->ms__new = SGX_CAST(char*, _new);
	status = sgx_ocall(149, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_print_string(int* retval, const char* s)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_s = s ? strlen(s) + 1 : 0;

	ms_ocall_print_string_t ms_o;
	ms_ocall_print_string_t* ms = &ms_o;
	ms->ms_s = SGX_CAST(char*, s);
	status = sgx_ocall(150, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fprint_string(int* retval, SGX_WRAPPER_FILE stream, const char* s)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_s = s ? strlen(s) + 1 : 0;

	ms_ocall_fprint_string_t ms_o;
	ms_ocall_fprint_string_t* ms = &ms_o;
	ms->ms_stream = stream;
	ms->ms_s = SGX_CAST(char*, s);
	status = sgx_ocall(151, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vfscanf(int* retval, SGX_WRAPPER_FILE s, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vfscanf_t ms_o;
	ms_ocall_vfscanf_t* ms = &ms_o;
	ms->ms_s = s;
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(152, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vscanf(int* retval, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vscanf_t ms_o;
	ms_ocall_vscanf_t* ms = &ms_o;
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(153, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vsscanf(int* retval, const char* s, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_s = s ? strlen(s) + 1 : 0;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vsscanf_t ms_o;
	ms_ocall_vsscanf_t* ms = &ms_o;
	ms->ms_s = SGX_CAST(char*, s);
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(154, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vsprintf(int* retval, char* string, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vsprintf_t ms_o;
	ms_ocall_vsprintf_t* ms = &ms_o;
	ms->ms_string = SGX_CAST(char*, string);
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(155, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vasprintf(int* retval, char** string, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vasprintf_t ms_o;
	ms_ocall_vasprintf_t* ms = &ms_o;
	ms->ms_string = SGX_CAST(char**, string);
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(156, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vfprintf(int* retval, SGX_WRAPPER_FILE FILESTREAM, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vfprintf_t ms_o;
	ms_ocall_vfprintf_t* ms = &ms_o;
	ms->ms_FILESTREAM = FILESTREAM;
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(157, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_vprintf(int* retval, const char* format, void* val)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_format = format ? strlen(format) + 1 : 0;

	ms_ocall_vprintf_t ms_o;
	ms_ocall_vprintf_t* ms = &ms_o;
	ms->ms_format = SGX_CAST(char*, format);
	ms->ms_val = SGX_CAST(void*, val);
	status = sgx_ocall(158, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_eventfd(int* retval, unsigned int initval, int flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_eventfd_t ms_o;
	ms_ocall_eventfd_t* ms = &ms_o;
	ms->ms_initval = initval;
	ms->ms_flags = flags;
	status = sgx_ocall(159, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_socket(int* retval, int domain, int type, int protocol)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_socket_t ms_o;
	ms_ocall_socket_t* ms = &ms_o;
	ms->ms_domain = domain;
	ms->ms_type = type;
	ms->ms_protocol = protocol;
	status = sgx_ocall(160, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_accept(int* retval, int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_addr = sizeof(*addr);
	size_t _len_addrlen = sizeof(*addrlen);

	ms_ocall_accept_t ms_o;
	ms_ocall_accept_t* ms = &ms_o;
	ms->ms_sockfd = sockfd;
	ms->ms_addr = SGX_CAST(struct sockaddr*, addr);
	ms->ms_addrlen = SGX_CAST(socklen_t*, addrlen);
	status = sgx_ocall(161, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_connect(int* retval, int socket, const struct sockaddr* address, socklen_t address_len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_address = sizeof(*address);

	ms_ocall_connect_t ms_o;
	ms_ocall_connect_t* ms = &ms_o;
	ms->ms_socket = socket;
	ms->ms_address = SGX_CAST(struct sockaddr*, address);
	ms->ms_address_len = address_len;
	status = sgx_ocall(162, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sendto(ssize_t* retval, int sockfd, const void* buf, size_t len, int flags, const void* dest_addr, unsigned int addrlen)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = len;

	ms_ocall_sendto_t ms_o;
	ms_ocall_sendto_t* ms = &ms_o;
	ms->ms_sockfd = sockfd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_len = len;
	ms->ms_flags = flags;
	ms->ms_dest_addr = SGX_CAST(void*, dest_addr);
	ms->ms_addrlen = addrlen;
	status = sgx_ocall(163, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_recv(ssize_t* retval, int fd, void* buf, size_t len, int flags)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = len;

	ms_ocall_recv_t ms_o;
	ms_ocall_recv_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_len = len;
	ms->ms_flags = flags;
	status = sgx_ocall(164, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_send(ssize_t* retval, int fd, const void* buf, size_t len, int flags)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = len;

	ms_ocall_send_t ms_o;
	ms_ocall_send_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_len = len;
	ms->ms_flags = flags;
	status = sgx_ocall(165, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_socketpair(int* retval, int domain, int type, int protocol, int sv[2])
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_sv = 2 * sizeof(*sv);

	ms_ocall_socketpair_t ms_o;
	ms_ocall_socketpair_t* ms = &ms_o;
	ms->ms_domain = domain;
	ms->ms_type = type;
	ms->ms_protocol = protocol;
	ms->ms_sv = SGX_CAST(int*, sv);
	status = sgx_ocall(166, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setsockopt(int* retval, int sockfd, int level, int optname, const void* optval, unsigned int optlen)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_optval = optlen;

	ms_ocall_setsockopt_t ms_o;
	ms_ocall_setsockopt_t* ms = &ms_o;
	ms->ms_sockfd = sockfd;
	ms->ms_level = level;
	ms->ms_optname = optname;
	ms->ms_optval = SGX_CAST(void*, optval);
	ms->ms_optlen = optlen;
	status = sgx_ocall(167, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getsockopt(int* retval, int sockfd, int level, int optname, void* optval, unsigned int* optlen)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_optval = 20;
	size_t _len_optlen = 4;

	ms_ocall_getsockopt_t ms_o;
	ms_ocall_getsockopt_t* ms = &ms_o;
	ms->ms_sockfd = sockfd;
	ms->ms_level = level;
	ms->ms_optname = optname;
	ms->ms_optval = SGX_CAST(void*, optval);
	ms->ms_optlen = SGX_CAST(unsigned int*, optlen);
	status = sgx_ocall(168, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_shutdown(int* retval, int fd, int how)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_shutdown_t ms_o;
	ms_ocall_shutdown_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_how = how;
	status = sgx_ocall(169, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_bind(int* retval, int fd, const struct sockaddr* addr, socklen_t len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_addr = len;

	ms_ocall_bind_t ms_o;
	ms_ocall_bind_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_addr = SGX_CAST(struct sockaddr*, addr);
	ms->ms_len = len;
	status = sgx_ocall(170, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_listen(int* retval, int fd, int n)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_listen_t ms_o;
	ms_ocall_listen_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_n = n;
	status = sgx_ocall(171, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getsockname(int* retval, int fd, struct sockaddr* addr, socklen_t* len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_addr = sizeof(*addr);
	size_t _len_len = sizeof(*len);

	ms_ocall_getsockname_t ms_o;
	ms_ocall_getsockname_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_addr = SGX_CAST(struct sockaddr*, addr);
	ms->ms_len = SGX_CAST(socklen_t*, len);
	status = sgx_ocall(172, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getpeername(int* retval, int fd, struct sockaddr* addr, socklen_t* len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_addr = sizeof(*addr);
	size_t _len_len = sizeof(*len);

	ms_ocall_getpeername_t ms_o;
	ms_ocall_getpeername_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_addr = SGX_CAST(struct sockaddr*, addr);
	ms->ms_len = SGX_CAST(socklen_t*, len);
	status = sgx_ocall(173, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_recvfrom(ssize_t* retval, int fd, void* buf, size_t n, int flags, struct sockaddr* addr, socklen_t* addr_len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = n;
	size_t _len_addr = sizeof(*addr);

	ms_ocall_recvfrom_t ms_o;
	ms_ocall_recvfrom_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(void*, buf);
	ms->ms_n = n;
	ms->ms_flags = flags;
	ms->ms_addr = SGX_CAST(struct sockaddr*, addr);
	ms->ms_addr_len = SGX_CAST(socklen_t*, addr_len);
	status = sgx_ocall(174, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sendmsg(ssize_t* retval, int fd, const struct msghdr* message, int flags)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_message = sizeof(*message);

	ms_ocall_sendmsg_t ms_o;
	ms_ocall_sendmsg_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_message = SGX_CAST(struct msghdr*, message);
	ms->ms_flags = flags;
	status = sgx_ocall(175, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_recvmsg(ssize_t* retval, int fd, struct msghdr* message, int flags)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_message = sizeof(*message);

	ms_ocall_recvmsg_t ms_o;
	ms_ocall_recvmsg_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_message = SGX_CAST(struct msghdr*, message);
	ms->ms_flags = flags;
	status = sgx_ocall(176, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_freeaddrinfo(void* res)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_freeaddrinfo_t ms_o;
	ms_ocall_freeaddrinfo_t* ms = &ms_o;
	ms->ms_res = SGX_CAST(void*, res);
	status = sgx_ocall(177, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_getaddrinfo(int* retval, const char* node, const char* service, const void* hints, void** res)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_node = node ? strlen(node) + 1 : 0;
	size_t _len_service = service ? strlen(service) + 1 : 0;
	size_t _len_hints = 48;
	size_t _len_res = sizeof(*res);

	ms_ocall_getaddrinfo_t ms_o;
	ms_ocall_getaddrinfo_t* ms = &ms_o;
	ms->ms_node = SGX_CAST(char*, node);
	ms->ms_service = SGX_CAST(char*, service);
	ms->ms_hints = SGX_CAST(void*, hints);
	ms->ms_res = SGX_CAST(void**, res);
	status = sgx_ocall(178, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getaddrinfo1(int* retval, const char* node, const char* service, const void* hints, void* res)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_node = node ? strlen(node) + 1 : 0;
	size_t _len_service = service ? strlen(service) + 1 : 0;

	ms_ocall_getaddrinfo1_t ms_o;
	ms_ocall_getaddrinfo1_t* ms = &ms_o;
	ms->ms_node = SGX_CAST(char*, node);
	ms->ms_service = SGX_CAST(char*, service);
	ms->ms_hints = SGX_CAST(void*, hints);
	ms->ms_res = SGX_CAST(void*, res);
	status = sgx_ocall(179, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sethostent(int stay_open)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sethostent_t ms_o;
	ms_ocall_sethostent_t* ms = &ms_o;
	ms->ms_stay_open = stay_open;
	status = sgx_ocall(180, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_endhostent()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(181, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_gethostent(struct hostent** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_gethostent_t ms_o;
	ms_ocall_gethostent_t* ms = &ms_o;
	status = sgx_ocall(182, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gethostbyaddr(struct hostent** retval, const void* addr, socklen_t len, int type)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_addr = len;

	ms_ocall_gethostbyaddr_t ms_o;
	ms_ocall_gethostbyaddr_t* ms = &ms_o;
	ms->ms_addr = SGX_CAST(void*, addr);
	ms->ms_len = len;
	ms->ms_type = type;
	status = sgx_ocall(183, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gethostbyname(struct hostent** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_gethostbyname_t ms_o;
	ms_ocall_gethostbyname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(184, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setnetent(int stay_open)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_setnetent_t ms_o;
	ms_ocall_setnetent_t* ms = &ms_o;
	ms->ms_stay_open = stay_open;
	status = sgx_ocall(185, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_endnetent()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(186, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_getnetent(struct netent** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getnetent_t ms_o;
	ms_ocall_getnetent_t* ms = &ms_o;
	status = sgx_ocall(187, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getnetbyaddr(struct netent** retval, uint32_t net, int type)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getnetbyaddr_t ms_o;
	ms_ocall_getnetbyaddr_t* ms = &ms_o;
	ms->ms_net = net;
	ms->ms_type = type;
	status = sgx_ocall(188, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getnetbyname(struct netent** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_getnetbyname_t ms_o;
	ms_ocall_getnetbyname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(189, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setservent(int stay_open)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_setservent_t ms_o;
	ms_ocall_setservent_t* ms = &ms_o;
	ms->ms_stay_open = stay_open;
	status = sgx_ocall(190, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_endservent()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(191, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_getservent(struct servent** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getservent_t ms_o;
	ms_ocall_getservent_t* ms = &ms_o;
	status = sgx_ocall(192, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getservbyname(struct servent** retval, const char* name, const char* proto)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;
	size_t _len_proto = proto ? strlen(proto) + 1 : 0;

	ms_ocall_getservbyname_t ms_o;
	ms_ocall_getservbyname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	ms->ms_proto = SGX_CAST(char*, proto);
	status = sgx_ocall(193, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getservbyport(struct servent** retval, int port, const char* proto)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_proto = proto ? strlen(proto) + 1 : 0;

	ms_ocall_getservbyport_t ms_o;
	ms_ocall_getservbyport_t* ms = &ms_o;
	ms->ms_port = port;
	ms->ms_proto = SGX_CAST(char*, proto);
	status = sgx_ocall(194, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setprotoent(int stay_open)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_setprotoent_t ms_o;
	ms_ocall_setprotoent_t* ms = &ms_o;
	ms->ms_stay_open = stay_open;
	status = sgx_ocall(195, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_endprotoent()
{
	sgx_status_t status = SGX_SUCCESS;
	status = sgx_ocall(196, NULL);
	return status;
}
sgx_status_t SGX_CDECL ocall_getprotoent(struct protoent** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getprotoent_t ms_o;
	ms_ocall_getprotoent_t* ms = &ms_o;
	status = sgx_ocall(197, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getprotobyname(struct protoent** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_getprotobyname_t ms_o;
	ms_ocall_getprotobyname_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(198, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getprotobynumber(struct protoent** retval, int proto)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getprotobynumber_t ms_o;
	ms_ocall_getprotobynumber_t* ms = &ms_o;
	ms->ms_proto = proto;
	status = sgx_ocall(199, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_gai_strerror(char** retval, int ecode)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_gai_strerror_t ms_o;
	ms_ocall_gai_strerror_t* ms = &ms_o;
	ms->ms_ecode = ecode;
	status = sgx_ocall(200, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getnameinfo(int* retval, const struct sockaddr* sa, socklen_t salen, char* host, socklen_t hostlen, char* serv, socklen_t servlen, int flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getnameinfo_t ms_o;
	ms_ocall_getnameinfo_t* ms = &ms_o;
	ms->ms_sa = SGX_CAST(struct sockaddr*, sa);
	ms->ms_salen = salen;
	ms->ms_host = SGX_CAST(char*, host);
	ms->ms_hostlen = hostlen;
	ms->ms_serv = SGX_CAST(char*, serv);
	ms->ms_servlen = servlen;
	ms->ms_flags = flags;
	status = sgx_ocall(201, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ntohl(uint32_t* retval, uint32_t netlong)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ntohl_t ms_o;
	ms_ocall_ntohl_t* ms = &ms_o;
	ms->ms_netlong = netlong;
	status = sgx_ocall(202, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ntohs(uint16_t* retval, uint16_t netshort)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ntohs_t ms_o;
	ms_ocall_ntohs_t* ms = &ms_o;
	ms->ms_netshort = netshort;
	status = sgx_ocall(203, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_htonl(uint32_t* retval, uint32_t hostlong)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_htonl_t ms_o;
	ms_ocall_htonl_t* ms = &ms_o;
	ms->ms_hostlong = hostlong;
	status = sgx_ocall(204, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_htons(uint16_t* retval, uint16_t hostshort)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_htons_t ms_o;
	ms_ocall_htons_t* ms = &ms_o;
	ms->ms_hostshort = hostshort;
	status = sgx_ocall(205, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_ioctl(int* retval, int fd, unsigned long int request, void* arguments)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_ioctl_t ms_o;
	ms_ocall_ioctl_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_request = request;
	ms->ms_arguments = SGX_CAST(void*, arguments);
	status = sgx_ocall(206, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_readv(ssize_t* retval, int __fd, const void* __iovec, int iovec_size, int __count)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___iovec = __count * iovec_size;

	ms_ocall_readv_t ms_o;
	ms_ocall_readv_t* ms = &ms_o;
	ms->ms___fd = __fd;
	ms->ms___iovec = SGX_CAST(void*, __iovec);
	ms->ms_iovec_size = iovec_size;
	ms->ms___count = __count;
	status = sgx_ocall(207, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_writev(ssize_t* retval, int __fd, int iovec_id, int iovec_size, int __count)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_writev_t ms_o;
	ms_ocall_writev_t* ms = &ms_o;
	ms->ms___fd = __fd;
	ms->ms_iovec_id = iovec_id;
	ms->ms_iovec_size = iovec_size;
	ms->ms___count = __count;
	status = sgx_ocall(208, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_init_multiple_iovec_outside(int* retval, const void* __iovec, int iovec_size, int __count)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___iovec = __count * iovec_size;

	ms_ocall_init_multiple_iovec_outside_t ms_o;
	ms_ocall_init_multiple_iovec_outside_t* ms = &ms_o;
	ms->ms___iovec = SGX_CAST(void*, __iovec);
	ms->ms_iovec_size = iovec_size;
	ms->ms___count = __count;
	status = sgx_ocall(209, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_copy_base_to_outside(int iovec_id, int i, const void* base, int len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_base = len;

	ms_ocall_copy_base_to_outside_t ms_o;
	ms_ocall_copy_base_to_outside_t* ms = &ms_o;
	ms->ms_iovec_id = iovec_id;
	ms->ms_i = i;
	ms->ms_base = SGX_CAST(void*, base);
	ms->ms_len = len;
	status = sgx_ocall(210, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_free_iovec_outside(int iovec_id, int iovec_size, int __count)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_free_iovec_outside_t ms_o;
	ms_ocall_free_iovec_outside_t* ms = &ms_o;
	ms->ms_iovec_id = iovec_id;
	ms->ms_iovec_size = iovec_size;
	ms->ms___count = __count;
	status = sgx_ocall(211, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_process_vm_readv(ssize_t* retval, pid_t __pid, const struct iovec* __lvec, unsigned long int __liovcnt, const struct iovec* __rvec, unsigned long int __riovcnt, unsigned long int __flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_process_vm_readv_t ms_o;
	ms_ocall_process_vm_readv_t* ms = &ms_o;
	ms->ms___pid = __pid;
	ms->ms___lvec = SGX_CAST(struct iovec*, __lvec);
	ms->ms___liovcnt = __liovcnt;
	ms->ms___rvec = SGX_CAST(struct iovec*, __rvec);
	ms->ms___riovcnt = __riovcnt;
	ms->ms___flags = __flags;
	status = sgx_ocall(212, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_process_vm_writev(ssize_t* retval, pid_t __pid, const struct iovec* __lvec, unsigned long int __liovcnt, const struct iovec* __rvec, unsigned long int __riovcnt, unsigned long int __flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_process_vm_writev_t ms_o;
	ms_ocall_process_vm_writev_t* ms = &ms_o;
	ms->ms___pid = __pid;
	ms->ms___lvec = SGX_CAST(struct iovec*, __lvec);
	ms->ms___liovcnt = __liovcnt;
	ms->ms___rvec = SGX_CAST(struct iovec*, __rvec);
	ms->ms___riovcnt = __riovcnt;
	ms->ms___flags = __flags;
	status = sgx_ocall(213, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mmap(void** retval, void* __addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_mmap_t ms_o;
	ms_ocall_mmap_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	ms->ms___prot = __prot;
	ms->ms___flags = __flags;
	ms->ms___fd = __fd;
	ms->ms___offset = __offset;
	status = sgx_ocall(214, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mmap64(void** retval, void* __addr, size_t __len, int __prot, int __flags, int __fd, __off64_t __offset)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_mmap64_t ms_o;
	ms_ocall_mmap64_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	ms->ms___prot = __prot;
	ms->ms___flags = __flags;
	ms->ms___fd = __fd;
	ms->ms___offset = __offset;
	status = sgx_ocall(215, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_munmap(int* retval, void* __addr, size_t __len)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_munmap_t ms_o;
	ms_ocall_munmap_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	status = sgx_ocall(216, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mprotect(int* retval, void* __addr, size_t __len, int __prot)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_mprotect_t ms_o;
	ms_ocall_mprotect_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	ms->ms___prot = __prot;
	status = sgx_ocall(217, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_msync(int* retval, void* __addr, size_t __len, int __flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_msync_t ms_o;
	ms_ocall_msync_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	ms->ms___flags = __flags;
	status = sgx_ocall(218, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mlock(int* retval, const void* __addr, size_t __len)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_mlock_t ms_o;
	ms_ocall_mlock_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	status = sgx_ocall(219, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_munlock(int* retval, const void* __addr, size_t __len)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_munlock_t ms_o;
	ms_ocall_munlock_t* ms = &ms_o;
	ms->ms___addr = SGX_CAST(void*, __addr);
	ms->ms___len = __len;
	status = sgx_ocall(220, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mlockall(int* retval, int __flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_mlockall_t ms_o;
	ms_ocall_mlockall_t* ms = &ms_o;
	ms->ms___flags = __flags;
	status = sgx_ocall(221, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_munlockall(int* retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_munlockall_t ms_o;
	ms_ocall_munlockall_t* ms = &ms_o;
	status = sgx_ocall(222, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mincore(int* retval, void* __start, size_t __len, unsigned char* __vec)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_mincore_t ms_o;
	ms_ocall_mincore_t* ms = &ms_o;
	ms->ms___start = SGX_CAST(void*, __start);
	ms->ms___len = __len;
	ms->ms___vec = SGX_CAST(unsigned char*, __vec);
	status = sgx_ocall(223, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_shm_open(int* retval, const char* __name, int __oflag, mode_t __mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___name = __name ? strlen(__name) + 1 : 0;

	ms_ocall_shm_open_t ms_o;
	ms_ocall_shm_open_t* ms = &ms_o;
	ms->ms___name = SGX_CAST(char*, __name);
	ms->ms___oflag = __oflag;
	ms->ms___mode = __mode;
	status = sgx_ocall(224, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_shm_unlink(int* retval, const char* __name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___name = __name ? strlen(__name) + 1 : 0;

	ms_ocall_shm_unlink_t ms_o;
	ms_ocall_shm_unlink_t* ms = &ms_o;
	ms->ms___name = SGX_CAST(char*, __name);
	status = sgx_ocall(225, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_poll(int* retval, struct pollfd* __fds, nfds_t __nfds, int __timeout)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___fds = __nfds * sizeof(*__fds);

	ms_ocall_poll_t ms_o;
	ms_ocall_poll_t* ms = &ms_o;
	ms->ms___fds = SGX_CAST(struct pollfd*, __fds);
	ms->ms___nfds = __nfds;
	ms->ms___timeout = __timeout;
	status = sgx_ocall(226, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_epoll_create(int* retval, int __size)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_epoll_create_t ms_o;
	ms_ocall_epoll_create_t* ms = &ms_o;
	ms->ms___size = __size;
	status = sgx_ocall(227, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_epoll_create1(int* retval, int __flags)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_epoll_create1_t ms_o;
	ms_ocall_epoll_create1_t* ms = &ms_o;
	ms->ms___flags = __flags;
	status = sgx_ocall(228, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_epoll_ctl(int* retval, int __epfd, int __op, int __fd, void* __event, int event_size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___event = event_size;

	ms_ocall_epoll_ctl_t ms_o;
	ms_ocall_epoll_ctl_t* ms = &ms_o;
	ms->ms___epfd = __epfd;
	ms->ms___op = __op;
	ms->ms___fd = __fd;
	ms->ms___event = SGX_CAST(void*, __event);
	ms->ms_event_size = event_size;
	status = sgx_ocall(229, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_epoll_wait(int* retval, int __epfd, void* __events, int event_size, int __maxevents, int __timeout)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___events = __maxevents * event_size;

	ms_ocall_epoll_wait_t ms_o;
	ms_ocall_epoll_wait_t* ms = &ms_o;
	ms->ms___epfd = __epfd;
	ms->ms___events = SGX_CAST(void*, __events);
	ms->ms_event_size = event_size;
	ms->ms___maxevents = __maxevents;
	ms->ms___timeout = __timeout;
	status = sgx_ocall(230, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_epoll_pwait(int* retval, int __epfd, void* __events, int event_size, int __maxevents, int __timeout, void* __ss, int sigset_size)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len___events = __maxevents * event_size;
	size_t _len___ss = sigset_size;

	ms_ocall_epoll_pwait_t ms_o;
	ms_ocall_epoll_pwait_t* ms = &ms_o;
	ms->ms___epfd = __epfd;
	ms->ms___events = SGX_CAST(void*, __events);
	ms->ms_event_size = event_size;
	ms->ms___maxevents = __maxevents;
	ms->ms___timeout = __timeout;
	ms->ms___ss = SGX_CAST(void*, __ss);
	ms->ms_sigset_size = sigset_size;
	status = sgx_ocall(231, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_select(int* retval, int __nfds, fd_set* __readfds, fd_set* __writefds, fd_set* __exceptfds, struct timeval* __timeout)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_select_t ms_o;
	ms_ocall_select_t* ms = &ms_o;
	ms->ms___nfds = __nfds;
	ms->ms___readfds = SGX_CAST(fd_set*, __readfds);
	ms->ms___writefds = SGX_CAST(fd_set*, __writefds);
	ms->ms___exceptfds = SGX_CAST(fd_set*, __exceptfds);
	ms->ms___timeout = SGX_CAST(struct timeval*, __timeout);
	status = sgx_ocall(232, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sendfile(ssize_t* retval, int out_fd, int in_fd, off_t* offset, size_t count)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_offset = count;

	ms_ocall_sendfile_t ms_o;
	ms_ocall_sendfile_t* ms = &ms_o;
	ms->ms_out_fd = out_fd;
	ms->ms_in_fd = in_fd;
	ms->ms_offset = SGX_CAST(off_t*, offset);
	ms->ms_count = count;
	status = sgx_ocall(233, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_waitpid(__pid_t* retval, __pid_t __pid, int* __stat_loc, int __options)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_waitpid_t ms_o;
	ms_ocall_waitpid_t* ms = &ms_o;
	ms->ms___pid = __pid;
	ms->ms___stat_loc = SGX_CAST(int*, __stat_loc);
	ms->ms___options = __options;
	status = sgx_ocall(234, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_waitid(int* retval, idtype_t __idtype, __id_t __id, siginfo_t* __infop, int __options)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_waitid_t ms_o;
	ms_ocall_waitid_t* ms = &ms_o;
	ms->ms___idtype = __idtype;
	ms->ms___id = __id;
	ms->ms___infop = SGX_CAST(siginfo_t*, __infop);
	ms->ms___options = __options;
	status = sgx_ocall(235, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_stat(int* retval, const char* path, struct stat* buf)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;
	size_t _len_buf = sizeof(*buf);

	ms_ocall_stat_t ms_o;
	ms_ocall_stat_t* ms = &ms_o;
	ms->ms_path = SGX_CAST(char*, path);
	ms->ms_buf = SGX_CAST(struct stat*, buf);
	status = sgx_ocall(236, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fstat(int* retval, int fd, struct stat* buf)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = sizeof(*buf);

	ms_ocall_fstat_t ms_o;
	ms_ocall_fstat_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(struct stat*, buf);
	status = sgx_ocall(237, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_lstat(int* retval, const char* path, struct stat* buf)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;
	size_t _len_buf = sizeof(*buf);

	ms_ocall_lstat_t ms_o;
	ms_ocall_lstat_t* ms = &ms_o;
	ms->ms_path = SGX_CAST(char*, path);
	ms->ms_buf = SGX_CAST(struct stat*, buf);
	status = sgx_ocall(238, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_chmod(int* retval, const char* file, mode_t mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_file = file ? strlen(file) + 1 : 0;

	ms_ocall_chmod_t ms_o;
	ms_ocall_chmod_t* ms = &ms_o;
	ms->ms_file = SGX_CAST(char*, file);
	ms->ms_mode = mode;
	status = sgx_ocall(239, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fchmod(int* retval, int fd, mode_t mode)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fchmod_t ms_o;
	ms_ocall_fchmod_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_mode = mode;
	status = sgx_ocall(240, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fchmodat(int* retval, int fd, const char* file, mode_t mode, int flag)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_file = file ? strlen(file) + 1 : 0;

	ms_ocall_fchmodat_t ms_o;
	ms_ocall_fchmodat_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_file = SGX_CAST(char*, file);
	ms->ms_mode = mode;
	ms->ms_flag = flag;
	status = sgx_ocall(241, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_umask(mode_t* retval, mode_t mask)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_umask_t ms_o;
	ms_ocall_umask_t* ms = &ms_o;
	ms->ms_mask = mask;
	status = sgx_ocall(242, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mkdir(int* retval, const char* path, mode_t mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;

	ms_ocall_mkdir_t ms_o;
	ms_ocall_mkdir_t* ms = &ms_o;
	ms->ms_path = SGX_CAST(char*, path);
	ms->ms_mode = mode;
	status = sgx_ocall(243, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mkdirat(int* retval, int fd, const char* path, mode_t mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;

	ms_ocall_mkdirat_t ms_o;
	ms_ocall_mkdirat_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_path = SGX_CAST(char*, path);
	ms->ms_mode = mode;
	status = sgx_ocall(244, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mkfifo(int* retval, const char* path, mode_t mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;

	ms_ocall_mkfifo_t ms_o;
	ms_ocall_mkfifo_t* ms = &ms_o;
	ms->ms_path = SGX_CAST(char*, path);
	ms->ms_mode = mode;
	status = sgx_ocall(245, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_mkfifoat(int* retval, int fd, const char* path, mode_t mode)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_path = path ? strlen(path) + 1 : 0;

	ms_ocall_mkfifoat_t ms_o;
	ms_ocall_mkfifoat_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_path = SGX_CAST(char*, path);
	ms->ms_mode = mode;
	status = sgx_ocall(246, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_opendir(void** retval, const char* name)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_opendir_t ms_o;
	ms_ocall_opendir_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(char*, name);
	status = sgx_ocall(247, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_fdopendir(void** retval, int fd)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_fdopendir_t ms_o;
	ms_ocall_fdopendir_t* ms = &ms_o;
	ms->ms_fd = fd;
	status = sgx_ocall(248, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_closedir(int* retval, void* dirp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_closedir_t ms_o;
	ms_ocall_closedir_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	status = sgx_ocall(249, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_readdir(struct dirent** retval, void* dirp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_readdir_t ms_o;
	ms_ocall_readdir_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	status = sgx_ocall(250, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_readdir_r(int* retval, void* dirp, struct dirent* entry, struct dirent** result)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_readdir_r_t ms_o;
	ms_ocall_readdir_r_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	ms->ms_entry = SGX_CAST(struct dirent*, entry);
	ms->ms_result = SGX_CAST(struct dirent**, result);
	status = sgx_ocall(251, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_rewinddir(void* dirp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_rewinddir_t ms_o;
	ms_ocall_rewinddir_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	status = sgx_ocall(252, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_seekdir(void* dirp, long int pos)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_seekdir_t ms_o;
	ms_ocall_seekdir_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	ms->ms_pos = pos;
	status = sgx_ocall(253, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_telldir(long int* retval, void* dirp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_telldir_t ms_o;
	ms_ocall_telldir_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	status = sgx_ocall(254, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_dirfd(int* retval, void* dirp)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_dirfd_t ms_o;
	ms_ocall_dirfd_t* ms = &ms_o;
	ms->ms_dirp = SGX_CAST(void*, dirp);
	status = sgx_ocall(255, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_alphasort(int* retval, const struct dirent** e1, const struct dirent** e2)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_alphasort_t ms_o;
	ms_ocall_alphasort_t* ms = &ms_o;
	ms->ms_e1 = SGX_CAST(struct dirent**, e1);
	ms->ms_e2 = SGX_CAST(struct dirent**, e2);
	status = sgx_ocall(256, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getdirentries(ssize_t* retval, int fd, char* buf, size_t nbytes, off_t* basep)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = nbytes;

	ms_ocall_getdirentries_t ms_o;
	ms_ocall_getdirentries_t* ms = &ms_o;
	ms->ms_fd = fd;
	ms->ms_buf = SGX_CAST(char*, buf);
	ms->ms_nbytes = nbytes;
	ms->ms_basep = SGX_CAST(off_t*, basep);
	status = sgx_ocall(257, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_versionsort(int* retval, const struct dirent** e1, const struct dirent** e2)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_versionsort_t ms_o;
	ms_ocall_versionsort_t* ms = &ms_o;
	ms->ms_e1 = SGX_CAST(struct dirent**, e1);
	ms->ms_e2 = SGX_CAST(struct dirent**, e2);
	status = sgx_ocall(258, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_prlimit(int* retval, __pid_t pid, enum __rlimit_resource resource, const struct rlimit* new_limit, struct rlimit* old_limit)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_new_limit = sizeof(*new_limit);
	size_t _len_old_limit = sizeof(*old_limit);

	ms_ocall_prlimit_t ms_o;
	ms_ocall_prlimit_t* ms = &ms_o;
	ms->ms_pid = pid;
	ms->ms_resource = resource;
	ms->ms_new_limit = SGX_CAST(struct rlimit*, new_limit);
	ms->ms_old_limit = SGX_CAST(struct rlimit*, old_limit);
	status = sgx_ocall(259, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getrlimit(int* retval, int resource, struct rlimit* rlim)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_rlim = sizeof(*rlim);

	ms_ocall_getrlimit_t ms_o;
	ms_ocall_getrlimit_t* ms = &ms_o;
	ms->ms_resource = resource;
	ms->ms_rlim = SGX_CAST(struct rlimit*, rlim);
	status = sgx_ocall(260, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_setrlimit(int* retval, int resource, const struct rlimit* rlim)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_rlim = sizeof(*rlim);

	ms_ocall_setrlimit_t ms_o;
	ms_ocall_setrlimit_t* ms = &ms_o;
	ms->ms_resource = resource;
	ms->ms_rlim = SGX_CAST(struct rlimit*, rlim);
	status = sgx_ocall(261, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_addr(in_addr_t* retval, const char* cp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_cp = cp ? strlen(cp) + 1 : 0;

	ms_ocall_inet_addr_t ms_o;
	ms_ocall_inet_addr_t* ms = &ms_o;
	ms->ms_cp = SGX_CAST(char*, cp);
	status = sgx_ocall(262, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_lnaof(in_addr_t* retval, struct in_addr in)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_inet_lnaof_t ms_o;
	ms_ocall_inet_lnaof_t* ms = &ms_o;
	ms->ms_in = in;
	status = sgx_ocall(263, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_makeaddr(struct in_addr* retval, in_addr_t net, in_addr_t host)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_inet_makeaddr_t ms_o;
	ms_ocall_inet_makeaddr_t* ms = &ms_o;
	ms->ms_net = net;
	ms->ms_host = host;
	status = sgx_ocall(264, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_netof(in_addr_t* retval, struct in_addr in)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_inet_netof_t ms_o;
	ms_ocall_inet_netof_t* ms = &ms_o;
	ms->ms_in = in;
	status = sgx_ocall(265, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_network(in_addr_t* retval, const char* cp)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_cp = cp ? strlen(cp) + 1 : 0;

	ms_ocall_inet_network_t ms_o;
	ms_ocall_inet_network_t* ms = &ms_o;
	ms->ms_cp = SGX_CAST(char*, cp);
	status = sgx_ocall(266, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_ntoa(char** retval, struct in_addr in)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_inet_ntoa_t ms_o;
	ms_ocall_inet_ntoa_t* ms = &ms_o;
	ms->ms_in = in;
	status = sgx_ocall(267, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_pton(int* retval, int af, const char* cp, void* buf)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_cp = cp ? strlen(cp) + 1 : 0;

	ms_ocall_inet_pton_t ms_o;
	ms_ocall_inet_pton_t* ms = &ms_o;
	ms->ms_af = af;
	ms->ms_cp = SGX_CAST(char*, cp);
	ms->ms_buf = SGX_CAST(void*, buf);
	status = sgx_ocall(268, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_inet_ntop(char** retval, int af, const void* cp, char* buf, socklen_t len)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_buf = len;

	ms_ocall_inet_ntop_t ms_o;
	ms_ocall_inet_ntop_t* ms = &ms_o;
	ms->ms_af = af;
	ms->ms_cp = SGX_CAST(void*, cp);
	ms->ms_buf = SGX_CAST(char*, buf);
	ms->ms_len = len;
	status = sgx_ocall(269, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sysctl(int* retval, int* name, int nlen, void* oldval, size_t* oldlenp, void* newval, size_t newlen)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = nlen;
	size_t _len_newval = newlen;

	ms_ocall_sysctl_t ms_o;
	ms_ocall_sysctl_t* ms = &ms_o;
	ms->ms_name = SGX_CAST(int*, name);
	ms->ms_nlen = nlen;
	ms->ms_oldval = SGX_CAST(void*, oldval);
	ms->ms_oldlenp = SGX_CAST(size_t*, oldlenp);
	ms->ms_newval = SGX_CAST(void*, newval);
	ms->ms_newlen = newlen;
	status = sgx_ocall(270, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigemptyset(int* retval, sigset_t* set)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigemptyset_t ms_o;
	ms_ocall_sigemptyset_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	status = sgx_ocall(271, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigfillset(int* retval, sigset_t* set)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigfillset_t ms_o;
	ms_ocall_sigfillset_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	status = sgx_ocall(272, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigaddset(int* retval, sigset_t* set, int signo)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigaddset_t ms_o;
	ms_ocall_sigaddset_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	ms->ms_signo = signo;
	status = sgx_ocall(273, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigdelset(int* retval, sigset_t* set, int signo)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigdelset_t ms_o;
	ms_ocall_sigdelset_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	ms->ms_signo = signo;
	status = sgx_ocall(274, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigismember(int* retval, const sigset_t* set, int signo)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigismember_t ms_o;
	ms_ocall_sigismember_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	ms->ms_signo = signo;
	status = sgx_ocall(275, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigsuspend(int* retval, const sigset_t* set)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigsuspend_t ms_o;
	ms_ocall_sigsuspend_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	status = sgx_ocall(276, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigaction(int* retval, int sig, const struct sigaction* act, struct sigaction* oact)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigaction_t ms_o;
	ms_ocall_sigaction_t* ms = &ms_o;
	ms->ms_sig = sig;
	ms->ms_act = SGX_CAST(struct sigaction*, act);
	ms->ms_oact = SGX_CAST(struct sigaction*, oact);
	status = sgx_ocall(277, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigpending(int* retval, sigset_t* set)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigpending_t ms_o;
	ms_ocall_sigpending_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	status = sgx_ocall(278, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_sigwait(int* retval, const sigset_t* set, int* sig)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_sigwait_t ms_o;
	ms_ocall_sigwait_t* ms = &ms_o;
	ms->ms_set = SGX_CAST(sigset_t*, set);
	ms->ms_sig = SGX_CAST(int*, sig);
	status = sgx_ocall(279, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_signal(__sighandler_t* retval, int __sig, __sighandler_t __handler)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_signal_t ms_o;
	ms_ocall_signal_t* ms = &ms_o;
	ms->ms___sig = __sig;
	ms->ms___handler = __handler;
	status = sgx_ocall(280, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_raise(int* retval, int sig)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_raise_t ms_o;
	ms_ocall_raise_t* ms = &ms_o;
	ms->ms_sig = sig;
	status = sgx_ocall(281, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_getifaddrs(int* retval, struct ifaddrs** ifap)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_getifaddrs_t ms_o;
	ms_ocall_getifaddrs_t* ms = &ms_o;
	ms->ms_ifap = SGX_CAST(struct ifaddrs**, ifap);
	status = sgx_ocall(282, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_freeifaddrs(struct ifaddrs* ifa)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_freeifaddrs_t ms_o;
	ms_ocall_freeifaddrs_t* ms = &ms_o;
	ms->ms_ifa = SGX_CAST(struct ifaddrs*, ifa);
	status = sgx_ocall(283, ms);

	return status;
}
sgx_status_t SGX_CDECL ocall_if_nametoindex(unsigned int* retval, const char* ifname)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_ifname = ifname ? strlen(ifname) + 1 : 0;

	ms_ocall_if_nametoindex_t ms_o;
	ms_ocall_if_nametoindex_t* ms = &ms_o;
	ms->ms_ifname = SGX_CAST(char*, ifname);
	status = sgx_ocall(284, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_if_indextoname(char** retval, unsigned int ifindex, char* ifname)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_ifname = ifname ? strlen(ifname) + 1 : 0;

	ms_ocall_if_indextoname_t ms_o;
	ms_ocall_if_indextoname_t* ms = &ms_o;
	ms->ms_ifindex = ifindex;
	ms->ms_ifname = SGX_CAST(char*, ifname);
	status = sgx_ocall(285, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_if_nameindex(struct if_nameindex** retval)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_if_nameindex_t ms_o;
	ms_ocall_if_nameindex_t* ms = &ms_o;
	status = sgx_ocall(286, ms);

	if (retval) *retval = ms->ms_retval;
	return status;
}
sgx_status_t SGX_CDECL ocall_if_freenameindex(struct if_nameindex* ptr)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_if_freenameindex_t ms_o;
	ms_ocall_if_freenameindex_t* ms = &ms_o;
	ms->ms_ptr = SGX_CAST(struct if_nameindex*, ptr);
	status = sgx_ocall(287, ms);

	return status;
}
