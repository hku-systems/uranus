#ifndef PANOPLY_SYSSENDFILE_H
#define PANOPLY_SYSSENDFILE_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

ssize_t ocall_sendfile (int out_fd, int in_fd, off_t *offset, size_t count);

#if defined(__cplusplus)
}
#endif
#endif