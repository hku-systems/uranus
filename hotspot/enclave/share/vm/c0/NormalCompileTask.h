//
// Created by jianyu on 11/11/18.
//

#ifndef HOTSPOT_COMPILETASK_H
#define HOTSPOT_COMPILETASK_H

#include <asm/macroAssembler.hpp>
#include "interpreter/interpMasm.hpp"
#include "bytecodeStream.hpp"
#include "c0_CodeStubs.hpp"
#include "CompileTask.hpp"
#include "C0MapOopSet.hpp"
#include <map>
#include <queue>

using std::map;

class NormalCompileTask: public CompileTask {
public:
    enum Operation { add, sub, mul, div, rem, _and, _or, _xor, shl, shr, ushr };
    enum Condition {
        equal,
        not_equal,
        less      ,
        less_equal ,
        greater    ,
        greater_equal
    };
    enum CacheByte { f1_byte = 1, f2_byte = 2 };  // byte_no codes

    BytecodeStream *bs;
    TosState tos;
    TosState ret_tos;
    C0MapOopSet *oopSet;
    Label ret_now;
    Label no_such_interface;
    Label out_of_bound;
    bool has_interface;
    bool has_bound_check;

    // we use this to indicate if a function / file etc. should be patch or resolve now
    bool will_run;

    // this is only used in gdb
    static int break_bci;
    static char* break_method;
    static char* break_klass;
    

    // check if all jump target is vtos
    std::map<int, TosState> jmp_target;
    std::map<int, TosState> bci_tos;

    std::map<int, address> bci_ptr_map;
    std::vector<std::pair<int, address> > patch_address;
    std::vector<std::pair<int, address> > patch_dispatch;
    std::deque<CodeStub*> slow_cases;

    NormalCompileTask(Method* _method): CompileTask(_method) {
        bs = new BytecodeStream(methodHandle(method));
        oopSet = new C0MapOopSet();
        tos = vtos;
        ret_tos = udtos;
        jmp_target = std::map<int, TosState>();
        will_run = true;
        has_interface = false;
        has_bound_check = false;
    }

    ~NormalCompileTask() {
        delete bs;
    }

    void do_oop_store(InterpreterMacroAssembler* _masm,
                      Address obj,
                      Register val,
                      BarrierSet::Name barrier,
                      bool precise);

    void checkcast_state(TosState tos, TosState intos);

    //Comment out because it is in x86 format
    //int fast_compile();

    int compile(int size);

    void invokevirtual_helper(Register index, Register recv, Register flags);

    // compile template
    void nop();

    void aconst_null();
    void iconst(int value);
    void lconst(int value);
    void fconst(int value);
    void dconst(int value);

    void bipush();
    void sipush();
    void ldc(bool wide);
    void ldc2_w();
    void fast_aldc(bool wide);

    void locals_index(Register reg, int offset = 1);
    void iload();
    void fast_iload();
    void fast_iload2();
    void fast_icaload();
    void lload();
    void fload();
    void dload();
    void aload();

    void locals_index_wide(Register reg);
    void wide_iload();
    void wide_lload();
    void wide_fload();
    void wide_dload();
    void wide_aload();

    void iaload();
    void laload();
    void faload();
    void daload();
    void aaload();
    void baload();
    void caload();
    void saload();

    void iload(int n);
    void lload(int n);
    void fload(int n);
    void dload(int n);
    void aload(int n);
    void aload_0();

    void istore();
    void lstore();
    void fstore();
    void dstore();
    void astore();

    void wide_istore();
    void wide_lstore();
    void wide_fstore();
    void wide_dstore();
    void wide_astore();

    void iastore();
    void lastore();
    void fastore();
    void dastore();
    void aastore();
    void bastore();
    void castore();
    void sastore();

    void istore(int n);
    void lstore(int n);
    void fstore(int n);
    void dstore(int n);
    void astore(int n);

    void pop();
    void pop2();
    void dup();
    void dup_x1();
    void dup_x2();
    void dup2();
    void dup2_x1();
    void dup2_x2();
    void swap();

    void iop2(Operation op);
    void lop2(Operation op);
    void fop2(Operation op);
    void dop2(Operation op);

    void idiv();
    void irem();

    void lmul();
    void ldiv();
    void lrem();
    void lshl();
    void lshr();
    void lushr();

    void ineg();
    void lneg();
    void fneg();
    void dneg();

    void iinc();
    void wide_iinc();
    void convert();
    void lcmp();

    void float_cmp (bool is_float, int unordered_result);
    void float_cmp (int unordered_result);
    void double_cmp(int unordered_result);

    void count_calls(Register method, Register temp);
    void branch(bool is_jsr, bool is_wide);
    void if_0cmp   (Condition cc);
    void if_icmp   (Condition cc);
    void if_nullcmp(Condition cc);
    void if_acmp   (Condition cc);
    void jump_target(int bci, Condition c);

    void _goto();
    void jsr();
    void ret();
    void wide_ret();

    void goto_w();
    void jsr_w();

    void tableswitch();
    void lookupswitch();
    void fast_linearswitch();
    void fast_binaryswitch();

    void narrow(Register r, TosState tos);
    void _return(TosState state);
    void _jmp_return();
    void adjust_tos();
    void athrow();

    void remove_activation(TosState state,
                           Register ret_addr,
                           bool throw_monitor_exception,
                           bool install_monitor_exception,
                           bool notify_jvmdi);

    void entry();

    void return_entry(TosState state, int index_size);

    Address at_bcp(int offset) {
        assert(_desc->uses_bcp(), "inconsistent uses_bcp information");
        return Address(rbcp, offset);
    }

    // patch the real jmp address
    void patch_jmp(address inst_addr, address jmp_addr);

    void index_check(Register array, Register index);

    void _new();
    void newarray();
    void anewarray();
    void arraylength();
    void multianewarray();
/*
    void monitorenter();
    void monitorexit();
*/
    void checkcast();
    void instanceof();

    void getfield_or_static(int byte_no, bool is_static);
    void getfield(int byte_no);
    void getstatic(int byte_no);

    void putfield_or_static(int byte_no, bool is_static);
    void putfield(int byte_no);
    void putstatic(int byte_no);

    void invokevirtual(int byte_no);
    void invokestatic(int byte_no);
    void invokeinterface(int byte_no);
    void invokedynamic(int byte_no);
    void invokespecial(int byte_no);
    void invoke(int byte_no, Register m, Register index, Register recv, Register flags);
    void prepare_invoke(int byte_no,
                                       Register method, // linked method (or i-klass)
                                       Register index,  // itable index, MethodType, etc.
                                       Register recv,   // if caller wants to see it
                                       Register flags   // if caller wants to test it
    );

    void gc_point();

    void patch_bytecode(Bytecodes::Code bc, Register bc_reg,
                                           Register temp_reg, bool load_bc_into_bc_reg/*=true*/,
                                           int byte_no);

    PatchingStub* resolve_cache_and_index(int byte_no,
                                          Register Rcache,
                                          Register index,
                                          size_t index_size);
    void load_field_cp_cache_entry(Register obj,
                                                      Register cache,
                                                      Register index,
                                                      Register off,
                                                      Register flags,
                                                      bool is_static);
    void jvmti_post_field_access(Register cache, Register index, bool is_static, bool has_tos);
    void pop_and_check_object(Register r);

    int getfield_index() {
        return bs->get_index_u2_cpcache();
    }
    int get_method_index() {
        if (bs->has_index_u4())
            return bs->get_index_u4();  // invokedynamic
        return bs->get_index_u2_cpcache();
    }

    void append_stub(CodeStub *stub) { slow_cases.push_back(stub); }
};


#endif //HOTSPOT_COMPILETASK_H
