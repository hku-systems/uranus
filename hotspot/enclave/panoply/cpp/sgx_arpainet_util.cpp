
#include <sgx_arpainet_util.h>

in_addr_t inet_addr (const char *cp)
{
	in_addr_t retval;
	ocall_inet_addr(&retval, cp);
	return retval;
}

in_addr_t inet_lnaof (struct in_addr in)
{
	in_addr_t retval;
	ocall_inet_lnaof(&retval, in);
	return retval;
}

struct in_addr inet_makeaddr (in_addr_t net, in_addr_t host)
{
	struct in_addr retval;
	ocall_inet_makeaddr(&retval, net, host);
	return retval;
}

in_addr_t inet_netof (struct in_addr in)
{
	in_addr_t retval;
	ocall_inet_netof(&retval, in);
	return retval;
}

in_addr_t inet_network (const char *cp)
{
	in_addr_t retval;
	ocall_inet_network(&retval, cp);
	return retval;
}

char *inet_ntoa (struct in_addr in)
{
	char *retval;
	ocall_inet_ntoa(&retval, in);
	return retval;
}

int inet_pton (int af, const char *cp, void *buf)
{
	int retval;
	ocall_inet_pton(&retval, af, cp, buf);
	return retval;
}

char *inet_ntop (int af,  const void *cp, char *buf, socklen_t len)
{
	char *retval;
	ocall_inet_ntop(&retval, af, cp, buf, len);
	return retval;
}

