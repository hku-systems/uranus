#ifndef PANOPLY_SYSSOCKET_H
#define PANOPLY_SYSSOCKET_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

static inline void printSockAddrInfo(struct sockaddr* addr);

static inline void printSockAddrUnInfo(const struct sockaddr* addr);

int ocall_shutdown(int sockfd, int how);

ssize_t ocall_recv(int sockfd, void *buf, size_t len, int flags);

ssize_t ocall_send(int sockfd, const void *buf, size_t len, int flags);

int ocall_connect(int soc, const struct sockaddr *address, socklen_t address_len);

int ocall_socket(int domain, int type, int protocol);

int ocall_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

ssize_t ocall_sendto(int sockfd, const void *buf, size_t len, int flags, const void *dest_addr_cast, unsigned int addrlen);

int ocall_socketpair(int domain, int type, int protocol, int sv[2]);

int ocall_setsockopt(int sockfd, int level, int optname, const void *optval, unsigned int optlen);

int ocall_getsockopt(int sockfd, int level, int optname, void *optval, unsigned int *optlen);

int ocall_bind(int fd, const struct sockaddr * addr, socklen_t len);

int ocall_listen(int fd, int n);

int ocall_getsockname(int fd, struct sockaddr * addr, socklen_t *len);

int ocall_getpeername(int fd, struct sockaddr * addr, socklen_t *len);

ssize_t ocall_recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr * addr, socklen_t *addr_len);

ssize_t ocall_sendmsg(int fd, const struct msghdr *message, int flags);

ssize_t ocall_recvmsg(int fd, struct msghdr *message, int flags);

#if defined(__cplusplus)
}
#endif
#endif