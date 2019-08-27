//
// Created by max on 1/1/18.
//

#ifndef HOTSPOT_HASH_BYTES_H
#define HOTSPOT_HASH_BYTES_H

#include "c++config.h"

namespace std
{
    _GLIBCXX_BEGIN_NAMESPACE_VERSION

    // Hash function implementation for the nontrivial specialization.
    // All of them are based on a primitive that hashes a pointer to a
    // byte array. The actual hash algorithm is not guaranteed to stay
    // the same from release to release -- it may be updated or tuned to
    // improve hash quality or speed.
    size_t
    _Hash_bytes(const void* __ptr, size_t __len, size_t __seed);

    // A similar hash primitive, using the FNV hash algorithm. This
    // algorithm is guaranteed to stay the same from release to release.
    // (although it might not produce the same values on different
    // machines.)
    size_t
    _Fnv_hash_bytes(const void* __ptr, size_t __len, size_t __seed);

    _GLIBCXX_END_NAMESPACE_VERSION
} // namespace


#endif //HOTSPOT_HASH_BYTES_H
