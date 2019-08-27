//
// Created by max on 12/31/17.
//

#ifndef HOTSPOT_CXXABI_TWEAKS_H
#define HOTSPOT_CXXABI_TWEAKS_H

#ifdef __cplusplus
namespace __cxxabiv1
{
    extern "C"
    {
#endif

    // The generic ABI uses the first byte of a 64-bit guard variable.
#define _GLIBCXX_GUARD_TEST(x) (*(char *) (x) != 0)
#define _GLIBCXX_GUARD_SET(x) *(char *) (x) = 1
#define _GLIBCXX_GUARD_BIT __guard_test_bit (0, 1)
#define _GLIBCXX_GUARD_PENDING_BIT __guard_test_bit (1, 1)
#define _GLIBCXX_GUARD_WAITING_BIT __guard_test_bit (2, 1)
    __extension__ typedef int __guard __attribute__((mode (__DI__)));

    // __cxa_vec_ctor has void return type.
    typedef void __cxa_vec_ctor_return_type;
#define _GLIBCXX_CXA_VEC_CTOR_RETURN(x) return
    // Constructors and destructors do not return a value.
    typedef void __cxa_cdtor_return_type;

#ifdef __cplusplus
    }
} // namespace __cxxabiv1
#endif

#endif //HOTSPOT_CXXABI_TWEAKS_H
