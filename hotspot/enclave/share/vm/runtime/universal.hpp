
#ifndef UNIVERSAL_HPP
#define UNIVERSAL_HPP

#include "enclave/EnclaveMemory.h"

class CollectedHeap {
    public:
    inline void* top_addr() {
        return EnclaveMemory::top_addr();
    }
    inline void* end_addr() {
        return EnclaveMemory::bottom_addr();
    }
    inline bool supports_inline_contig_alloc() {
        return false;
    }
};

#endif