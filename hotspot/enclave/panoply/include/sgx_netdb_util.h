#ifndef SGX_NETDB_UTIL_H
#define SGX_NETDB_UTIL_H 1

#include <proxy/sgx_netdb_t.h>
// #define __OPTIMIZE__ 1
// #include <TorEnclave_t.h>
#include <sgx_stdio_util.h>

void freeaddrinfo(void * res_cast);

void printAddrInfo(struct addrinfo* addr);

int getaddrinfo(const char *node, const char *service, const void *hints_cast, void **res_cast);

void sethostent (int stay_open);
void endhostent (void);

struct hostent *gethostent (void);

struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type);

struct hostent *gethostbyname (const char *name);

void setnetent (int stay_open);
void endnetent (void);

struct netent *getnetent (void);

struct netent *getnetbyaddr (uint32_t net, int type);
struct netent *getnetbyname (const char *name);

void setservent (int stay_open);
void endservent (void);
struct servent *getservent (void);
struct servent *getservbyname ( const char *name, const char *proto);

struct servent *getservbyport (int port, const char *proto);
void setprotoent (int stay_open);
void endprotoent (void);
struct protoent *getprotoent (void);

struct protoent *getprotobyname (const char *name);

struct protoent *getprotobynumber (int proto);

char *gai_strerror (int ecode);

int getnameinfo (const struct sockaddr *sa,
	socklen_t salen, char *host,
	socklen_t hostlen, char *serv,
	socklen_t servlen, int flags);

#endif