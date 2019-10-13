//
// Created by max on 1/2/18.
//

#ifndef HOTSPOT_HEADERHELPER_H
#define HOTSPOT_HEADERHELPER_H

class HeaderHelper {

};

#ifdef ENCLAVE_UNIX
#define DEBUG_ENCLAVE(A) debug_enclave(A)
#else
#define DEBUG_ENCLAVE(A)
#endif

void debug_enclave(const char* c);
extern void codeCache_init();
extern void icache_init();
extern void VM_Version_init();
#endif //HOTSPOT_HEADERHELPER_H
