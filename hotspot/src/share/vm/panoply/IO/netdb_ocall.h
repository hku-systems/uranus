#ifndef PANOPLY_NETDB_H
#define PANOPLY_NETDB_H



#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

static inline void printSockAddrInfo(const struct sockaddr* addr);

static inline void printAddrInfo(struct addrinfo* addr);

void ocall_freeaddrinfo(void * res_cast);

int ocall_getaddrinfo(const char *node, const char *service, const void *hints_cast, void **res_cast);

int ocall_getaddrinfo1(const char *node, const char *service, const void *hints_cast, void *res_cast);

void ocall_sethostent (int stay_open);

void ocall_endhostent (void);

struct hostent *ocall_gethostent (void);

struct hostent *ocall_gethostbyaddr (const void *addr, socklen_t len, int type);

struct hostent *ocall_gethostbyname (const char *name);

void ocall_setnetent (int stay_open);

void ocall_endnetent (void);

struct netent *ocall_getnetent (void);

struct netent *ocall_getnetbyaddr (uint32_t net, int type);

struct netent *ocall_getnetbyname (const char *name);

void ocall_setservent (int stay_open);

void ocall_endservent (void);

struct servent *ocall_getservent (void);

struct servent *ocall_getservbyname ( const char *name, const char *proto);

struct servent *ocall_getservbyport (int port, const char *proto);

void ocall_setprotoent (int stay_open);

void ocall_endprotoent (void);

struct protoent *ocall_getprotoent (void);

struct protoent *ocall_getprotobyname (const char *name);

struct protoent *ocall_getprotobynumber (int proto);

char *ocall_gai_strerror (int ecode);

int ocall_getnameinfo (const struct sockaddr *sa,
					   socklen_t salen, char *host,
					   socklen_t hostlen, char *serv,
					   socklen_t servlen, int flags);

#if defined(__cplusplus)
}
#endif
#endif
