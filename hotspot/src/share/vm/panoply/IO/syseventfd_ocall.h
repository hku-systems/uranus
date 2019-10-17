#ifndef PANOPLY_SYSEVENTFD_H
#define PANOPLY_SYSEVENTFD_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_eventfd(unsigned int initval, int flags);

#if defined(__cplusplus)
}
#endif
#endif
