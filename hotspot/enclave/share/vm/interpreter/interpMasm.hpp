
#ifndef SHARE_VM_INTERPRETER_INTERPMASM_HPP
#define SHARE_VM_INTERPRETER_INTERPMASM_HPP

#ifdef TARGET_ARCH_x86
#include <interp_masm_x86.hpp>
#elif TARGET_ARCH_aarch64
#include <interp_masm_aarch64.hpp>
#endif

#endif