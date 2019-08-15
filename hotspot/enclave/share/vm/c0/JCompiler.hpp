//
// Created by jianyu on 11/11/18.
//

#ifndef HOTSPOT_JCOMPILER_H
#define HOTSPOT_JCOMPILER_H

#include "oops/method.hpp"

#define NMethodSizeLimit (64*K)*wordSize

class CodeBuffer;

class JCompiler {
public:
    enum { call_stub_size = NOT_LP64(15) LP64_ONLY(28),
        exception_handler_size = DEBUG_ONLY(1*K) NOT_DEBUG(175),
        deopt_handler_size = NOT_LP64(10) LP64_ONLY(17)
    };
    static JCompiler* compiler;
    static int desired_max_constant_size() { return NMethodSizeLimit / 10;}
    void init() {}
    static JCompiler* create_compiler() { compiler = new JCompiler(); return compiler; }
    void compile_method(Method* method);
    bool static setup_code_buffer(CodeBuffer* code, int call_stub_estimate);
    bool static is_compile(Method* method);
};


#endif //HOTSPOT_JCOMPILER_H
