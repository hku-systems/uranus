//
// Created by max on 12/31/17.
//

#ifndef HOTSPOT_ALLOCA_H
#define HOTSPOT_ALLOCA_H

#include "sgx_stdio_util.h"

// do not allow allocation of stack memory
void *alloca (size_t __size) {
    return 0;
}


#endif //HOTSPOT_ALLOCA_H
