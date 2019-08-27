//
// Created by max on 12/31/17.
//

#ifndef HOTSPOT_OS_DEFINES_H
#define HOTSPOT_OS_DEFINES_H

// System-specific #define, typedefs, corrections, etc, go here.  This
// file will come before all others.

// This keeps isanum, et al from being propagated as macros.
#define __NO_CTYPE 1

#include "../features.h"

// Provide a declaration for the possibly deprecated gets function, as
// glibc 2.15 and later does not declare gets for ISO C11 when
// __GNU_SOURCE is defined.
#if __GLIBC_PREREQ(2,15) && defined(_GNU_SOURCE)
# undef _GLIBCXX_HAVE_GETS
#endif

#endif //HOTSPOT_OS_DEFINES_H
