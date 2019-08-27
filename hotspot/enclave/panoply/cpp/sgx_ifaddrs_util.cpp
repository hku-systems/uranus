#include <struct/sgx_ifaddrs_struct.h>
#include <proxy/sgx_ifaddrs_t.h>

#include <sgx_ifaddrs_util.h>

int getifaddrs(struct ifaddrs **ifap)
{
	int retval;
	ocall_getifaddrs(&retval, ifap);
	return retval;
}

void freeifaddrs(struct ifaddrs *ifa)
{
	ocall_freeifaddrs(ifa);
}

