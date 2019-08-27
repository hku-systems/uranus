
#include <sgx_netdb_util.h>

void freeaddrinfo(void * res_cast)
{
	ocall_freeaddrinfo(res_cast);
}

void printAddrInfo(struct addrinfo* addr)
{
	// printf("Addrinfo: %d %d %d %d %d \n", addr->ai_flags, addr->ai_family, addr->ai_socktype, addr->ai_protocol, addr->ai_addrlen); 
	printf("The return address is: %p", addr);
}

int getaddrinfo(const char *node, const char *service, const void *hints_cast, void **res_cast)
{
	int retval;
	struct addrinfo* my_addrinfo = (struct addrinfo*)malloc(sizeof(struct addrinfo));
	int err = ocall_getaddrinfo(&retval, node, service, hints_cast, res_cast);
	// int err = ocall_getaddrinfo1(&retval, node, service, hints_cast, my_addrinfo);
	// print_int(sizeof(struct addrinfo));
	// printAddrInfo(*res_cast);
	// *res_cast = my_addrinfo;
	//printAddrInfo(*res_cast);
	return retval;
}

void sethostent (int stay_open)
{
	ocall_sethostent(stay_open);
}

void endhostent (void)
{
	ocall_endhostent();
}

struct hostent *gethostent (void)
{
	struct hostent *retval;
	ocall_gethostent(&retval);
	return retval;
}

struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type)
{
	struct hostent *retval;
	ocall_gethostbyaddr(&retval, addr, len, type);
	return retval;
}

struct hostent *gethostbyname (const char *name)
{
	struct hostent *retval;
	ocall_gethostbyname(&retval, name);
	return retval;
}

void setnetent (int stay_open)
{
	ocall_setnetent(stay_open);
}

void endnetent (void)
{
	ocall_endnetent();
}

struct netent *getnetent (void)
{
	struct netent *retval;
	ocall_getnetent(&retval);
	return retval;
}

struct netent *getnetbyaddr (uint32_t net, int type)
{
	struct netent *retval;
	ocall_getnetbyaddr(&retval, net, type);
	return retval;
}

struct netent *getnetbyname (const char *name)
{
	struct netent *retval;
	ocall_getnetbyname(&retval, name);
	return retval;
}

void setservent (int stay_open)
{
	ocall_setservent(stay_open);
}

void endservent (void)
{
	ocall_endservent();
}

struct servent *getservent (void)
{
	struct servent* retval;
	ocall_getservent(&retval);
	return retval;
}

struct servent *getservbyname ( const char *name, const char *proto)
{
	struct servent* retval;
	ocall_getservbyname(&retval, name, proto);
	return retval;
}

struct servent *getservbyport (int port, const char *proto)
{
	struct servent* retval;
	ocall_getservbyport(&retval, port, proto);
	return retval;
}

void setprotoent (int stay_open)
{
	ocall_setprotoent(stay_open);
}

void endprotoent (void)
{
	ocall_endprotoent();
}

struct protoent *getprotoent (void)
{
	struct protoent* retval;
	ocall_getprotoent(&retval);
	return retval;
}

struct protoent *getprotobyname (const char *name)
{
	struct protoent* retval;
	ocall_getprotobyname(&retval, name);
	return retval;
}

struct protoent *getprotobynumber (int proto)
{
	struct protoent* retval;
	ocall_getprotobynumber(&retval, proto);
	return retval;
}

char *gai_strerror (int ecode)
{
	char* retval;
	ocall_gai_strerror(&retval, ecode);
	return retval;
}

int getnameinfo (const struct sockaddr *sa,
	socklen_t salen, char *host,
	socklen_t hostlen, char *serv,
	socklen_t servlen, int flags)
{
	int retval;
	ocall_getnameinfo(&retval, sa, salen, host, hostlen, serv, servlen, flags);
	return retval;
}
