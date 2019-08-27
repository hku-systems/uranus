
#include <sgx_sysepoll_util.h>

int epoll_create(int __size)
{
	int ret;
	ocall_epoll_create(&ret, __size);
	return ret;
}

int epoll_create1(int __flags)
{
	int ret;
	ocall_epoll_create1(&ret, __flags);
	return ret;
}

int epoll_ctl(int __epfd, int __op, int __fd, struct epoll_event *__event)
{
	int ret;
	ocall_epoll_ctl(&ret, __epfd, __op, __fd, __event, sizeof(struct epoll_event));
	return ret;
}

int epoll_wait(int __epfd, struct epoll_event *__events,
       int __maxevents, int __timeout)
{
	int ret;
	ocall_epoll_wait(&ret, __epfd, __events, sizeof(struct epoll_event), __maxevents, __timeout);
	return ret;
}

int epoll_pwait(int __epfd,  struct epoll_event *__events, int __maxevents, int __timeout,
             const __sigset_t *__ss)
{
	int ret;
	// ocall_epoll_pwait(&ret, __epfd, __events, sizeof(struct epoll_event), __maxevents, __timeout, __ss, sizeof(__sigset_t));
	return ret;
}

