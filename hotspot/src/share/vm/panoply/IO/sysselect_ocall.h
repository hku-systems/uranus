#ifndef PANOPLY_SYSSELECT_H
#define PANOPLY_SYSSELECT_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_select (int __nfds,  fd_set *__readfds,
                  fd_set *__writefds,
                  fd_set *__exceptfds,
                  struct timeval *__timeout);

#if defined(__cplusplus)
}
#endif
#endif
