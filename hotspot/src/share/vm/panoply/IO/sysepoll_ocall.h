#ifndef PANOPLY_SYSEPOLL_H
#define PANOPLY_SYSEPOLL_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_epoll_create(int __size);

int ocall_epoll_create1 (int __flags);

int ocall_epoll_ctl (int __epfd, int __op, int __fd, void *__event, int event_size);

int ocall_epoll_wait (int __epfd, void *__events, int event_size,
					  int __maxevents, int __timeout);

int ocall_epoll_pwait (int __epfd, void *__events, int event_size ,int __maxevents, int __timeout,
					   void *__ss, int sigset_size);

#if defined(__cplusplus)
}
#endif
#endif

