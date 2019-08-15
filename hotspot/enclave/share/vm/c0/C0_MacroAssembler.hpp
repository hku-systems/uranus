//
// Created by jianyu on 11/17/18.
//

#ifndef HOTSPOT_C0_MACROASSEMBLER_HPP
#define HOTSPOT_C0_MACROASSEMBLER_HPP

#include <enclave/sc/EnclaveDebug.h>
#include <oops/markOop.hpp>
#include <oops/oop.hpp>
#include <interp_masm_x86.hpp>
#include "C0MapOopEntry.hpp"
#include "C0MapOopSet.hpp"

class PatchingStub;

#define TOS_PUSH(tos, r, t)                                \
    if (start_expression) { current_entry->push(tos); } \
    InterpreterMacroAssembler::push##t(r);

#define TOS_POP(tos, r, t)  \
    if (start_expression) { current_entry->pop(tos); }  \
    InterpreterMacroAssembler::pop##t(r);


class C0_MacroAssembler: public InterpreterMacroAssembler {
public:
    C0MapOopEntry *current_entry;
    bool start_expression;
    C0_MacroAssembler(CodeBuffer* cb, C0MapOopEntry *entry): InterpreterMacroAssembler(cb) {
        current_entry = entry;
        start_expression = 0;
    }
    void pop_ptr(Register r = rax)      {   TOS_POP(atos, r, _ptr);     }
    void pop_i(Register r = rax)        {   TOS_POP(itos, r, _i);       }
    void pop_l(Register r = rax)        {   TOS_POP(ltos, r, _l);       }
    void pop_f(XMMRegister r = xmm0)    {   TOS_POP(ftos, r, _f);       }
    void pop_d(XMMRegister r = xmm0)    {   TOS_POP(dtos, r, _d);       }
    void push_ptr(Register r = rax)     {   TOS_PUSH(atos, r, _ptr);    }
    void push_i(Register r = rax)       {   TOS_PUSH(atos, r, _i);      }
    void push_l(Register r = rax)       {   TOS_PUSH(atos, r, _l);      }
    void push_f(XMMRegister r = xmm0)   {   TOS_PUSH(atos, r, _f);      }
    void push_d(XMMRegister r = xmm0)   {   TOS_PUSH(atos, r, _d);      }

    void pop(Register r )               {   TOS_POP(atos, r, );        }

    void push(Register r )              {   TOS_PUSH(atos, r, );        }
    void push(int32_t imm )             {   TOS_PUSH(atos, imm, );      }

    void pop(TosState state)            {   TOS_POP(state, state, );   }
    void push(TosState state)           {   TOS_PUSH(state, state, );   }

    void addptr(Register dst, int32_t imm32);

    void addptr(Register dst, Register src) { InterpreterMacroAssembler::addptr(dst, src); }

    void addptr(Address dst, Register src) { InterpreterMacroAssembler::addptr(dst, src); }

    void start_expression_stack() { start_expression = true; }
    void end_expression_stack() { start_expression = false; }
    void jump_to_compiled(Register method, address calladdr, bool force_compile, PatchingStub* &);
    void initialize_header(Register obj, Register klass, Register len, Register t1, Register t2);
    void initialize_body(Register obj, Register len_in_bytes, int hdr_size_in_bytes, Register t1);
    void initialize_object(Register obj, Register klass, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2);
    void sgx_bound_check(Register obj, Label& check_fail);
};


#endif //HOTSPOT_C0_MACROASSEMBLER_HPP
