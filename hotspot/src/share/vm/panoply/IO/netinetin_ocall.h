#ifndef PANOPLY_NETINETIN_H
#define PANOPLY_NETINETIN_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

uint32_t ocall_ntohl(uint32_t netlong);

uint16_t ocall_ntohs(uint16_t netshort);

uint32_t ocall_htonl(uint32_t hostlong);

uint16_t ocall_htons(uint16_t hostshort);

#if defined(__cplusplus)
}
#endif
#endif

