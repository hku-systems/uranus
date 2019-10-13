/* Determine the wordsize from the preprocessor defines.  */

#if defined __x86_64__ && !defined __ILP32__
# define __WORDSIZE	64
#elif defined __aarch64__
# define __WORDSIZE	64
#else
# define __WORDSIZE	32
#endif

#if defined __x86_64__ || defined __aarch64__
# define __WORDSIZE_TIME64_COMPAT32	1
/* Both x86-64 and x32 use the 64-bit system call interface.  */
# define __SYSCALL_WORDSIZE		64
#endif
