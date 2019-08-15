//
// Created by Maxxie Jiang on 14/8/2019.
//

#ifndef HOTSPOT_BYTESWAP_H
#define HOTSPOT_BYTESWAP_H

// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)

#endif //HOTSPOT_BYTESWAP_H
