#include <struct/sgx_netif_struct.h>
#include <proxy/sgx_netif_t.h>

#include <sgx_netif_util.h>

unsigned int if_nametoindex(const char *ifname)
{
	unsigned int retval;
	ocall_if_nametoindex(&retval, ifname);
	return retval;
}

char *if_indextoname(unsigned int ifindex, char *ifname)
{
	char* retval;
	ocall_if_indextoname(&retval, ifindex, ifname);
	return retval;
}

struct if_nameindex *if_nameindex(void)
{
	struct if_nameindex *retval;
	ocall_if_nameindex(&retval);
	return retval;
}

void if_freenameindex(struct if_nameindex *ptr)
{
	ocall_if_freenameindex(ptr);
}
