#ifndef SGX_SYSSOCKET_UTIL_H
#define SGX_SYSSOCKET_UTIL_H

#include "struct/sgx_syssocket_struct.h"
#include "proxy/sgx_syssocket_t.h"
#include <sgx_stdio_util.h>
#include <string.h>
#include "sgx_trts.h"
#include "sgx_status.h"

int shutdown(int fd, int how);

ssize_t recv(int fd, void *buf, size_t len, int flags);

ssize_t send(int fd, const void *buf, size_t len, int flags);

int connect(int soc, const struct sockaddr *address, socklen_t address_len);

int socket(int domain, int type, int protocol);

int accept(int sockfd, struct sockaddr *address, socklen_t *addrlen);

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const void *dest_addr_cast, unsigned int addrlen);

int socketpair(int domain, int type, int protocol, int sv[2]);

int setsockopt(int sockfd, int level, int optname, const void *optval, unsigned int optlen);

int getsockopt(int sockfd, int level, int optname, void *optval, unsigned int *optlen);

int bind(int sockfd, const struct sockaddr * address, socklen_t addrlen);

int listen(int fd, int n);

int getsockname(int fd, struct sockaddr * addr, socklen_t *len);

int getpeername(int fd, struct sockaddr * addr, socklen_t *len);

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr * addr, socklen_t *addr_len);

ssize_t sendmsg(int fd, const struct msghdr *message, int flags);

ssize_t recvmsg(int fd, struct msghdr *message, int flags);


#endif

