#ifndef SGX_SYSEPOLL_UTIL_H
#define SGX_SYSEPOLL_UTIL_H

#include <struct/sgx_sysepoll_struct.h>
#include <proxy/sgx_sysepoll_t.h>

int epoll_create(int __size);

int epoll_create1(int __flags);

int epoll_ctl(int __epfd, int __op, int __fd, struct epoll_event *__event);

int epoll_wait(int __epfd, struct epoll_event *__events,
       int __maxevents, int __timeout);

int epoll_pwait(int __epfd,  struct epoll_event *__events, int __maxevents, int __timeout,
             const __sigset_t *__ss);


#endif
