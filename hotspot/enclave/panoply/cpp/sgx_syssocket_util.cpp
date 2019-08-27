
#include <sgx_syssocket_util.h>

int shutdown(int fd, int how)
{
	int retval;
	ocall_shutdown(&retval, fd, how);
	return retval;
}
ssize_t recv(int fd, void *buf, size_t len, int flags)
{
	ssize_t retval;
	ocall_recv(&retval, fd, buf, len, flags);
	return retval;
}
ssize_t send(int fd, const void *buf, size_t len, int flags)
{
	ssize_t retval;
	ocall_send(&retval, fd, buf, len, flags);
	return retval;
}  

int connect(int soc, const struct sockaddr *address, socklen_t address_len)
{
	// printf("Call connect: %d \n", soc);
	int retval = -1;
	sgx_status_t status;

	if (!sgx_is_within_enclave(address, sizeof(struct sockaddr)))	{
		printf("\n WARNING: Call ocall_connect with sockaddr pointer outside enclave \n");
		struct sockaddr *dup = (struct sockaddr *)malloc(sizeof(struct sockaddr));
		memcpy(dup, address, sizeof(struct sockaddr));
		status = ocall_connect(&retval, soc, dup, address_len);
	    free(dup);
	} else	{
		status = ocall_connect(&retval, soc, address, address_len);
	}

    if (status != SGX_SUCCESS) {
        printf("Ret error code: %x\n", status);
    }
	
	// printf("Return connect: %d \n", retval);
	return retval;
}

int socket(int domain, int type, int protocol)
{
	// printf("Call socket: %d \n", domain);
	int retval;
	ocall_socket(&retval, domain, type, protocol);
	return retval;
}

int accept(int sockfd, struct sockaddr *address, socklen_t *addrlen)
{
	// printf("Call accept \n");
	int retval;
	sgx_status_t status;

	if (address!=NULL)	{
		// printf("\n WARNING: Call ocall_connect with sockaddr pointer outside enclave \n");
		struct sockaddr *dup = (struct sockaddr *)malloc(sizeof(struct sockaddr));
		memcpy(dup, address, sizeof(struct sockaddr));
		status = ocall_accept(&retval, sockfd, dup, addrlen);
		memcpy(address, dup, sizeof(struct sockaddr));
	    free(dup);
	} else	{
		// printf("right before ocall_accept\n");
		status = ocall_accept(&retval, sockfd, address, addrlen);
	}

    if (status != SGX_SUCCESS) {
        printf("Ret error code: %x\n", status);
    }

	// printf("Return accept: %d \n", retval);
	return retval;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const void *dest_addr_cast, unsigned int addrlen)
{
	ssize_t retval;
	ocall_sendto(&retval, sockfd, buf, len, flags, dest_addr_cast, addrlen);
	return retval;
}

int socketpair(int domain, int type, int protocol, int sv[2])
{
	int retval;
	ocall_socketpair(&retval, domain, type, protocol, sv);
	return retval;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, unsigned int optlen)
{
	int retval;
	ocall_setsockopt(&retval, sockfd, level, optname, optval, optlen);
	return retval;
}

int getsockopt(int sockfd, int level, int optname, void *optval, unsigned int *optlen)
{
	int retval;
	ocall_getsockopt(&retval, sockfd, level, optname, optval, optlen);
	return retval;
}

int bind(int sockfd, const struct sockaddr * address, socklen_t addrlen)
{
	// printf("Call bind \n");
	int retval = -1;
	sgx_status_t status;

	if (!sgx_is_within_enclave(address, sizeof(struct sockaddr)))	{
		printf("\n WARNING: Call ocall_bind with sockaddr pointer outside enclave \n");
		struct sockaddr *dup = (struct sockaddr *)malloc(sizeof(struct sockaddr));
		memcpy(dup, address, sizeof(struct sockaddr));
		status = ocall_bind(&retval, sockfd, dup, addrlen);
	    free(dup);
	} else	{
		// printf("Address before ocall: %p", address);
		status = ocall_bind(&retval, sockfd, address, addrlen);
	}

    if (status != SGX_SUCCESS) {
        printf("Ret error code: %x\n", status);
    }

	// printf("Return bind: %d \n", retval);
	return retval;
}

int listen(int fd, int n)
{
	int retval;
	ocall_listen(&retval, fd, n);
	return retval;
}

int getsockname(int fd, struct sockaddr * addr, socklen_t *len)
{
	printf("\n WARNING: getsockname \n");
	int retval;
	ocall_getsockname(&retval, fd, addr, len);
	return retval;
}

int getpeername(int fd, struct sockaddr * addr, socklen_t *len)
{
	printf("\n WARNING: getpeername \n");
	int retval;
	ocall_getpeername(&retval, fd, addr, len);
	return retval;
}

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr * addr, socklen_t *addr_len)
{
	ssize_t retval;
	ocall_recvfrom(&retval, fd, buf, n, flags, addr, addr_len);
	return retval;
}

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
	ssize_t retval;
	ocall_sendmsg(&retval, fd, message, flags);
	return retval;
}

ssize_t recvmsg(int fd, struct msghdr *message, int flags)
{
	ssize_t retval;
	ocall_recvmsg(&retval, fd, message, flags);
	return retval;
}


