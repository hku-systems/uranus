//
// Created by jianyu on 11/22/18.
//

#ifndef HOTSPOT_OCALLCOMPILETASK_HPP
#define HOTSPOT_OCALLCOMPILETASK_HPP


#include "CompileTask.hpp"

class OCallCompileTask: public CompileTask {
public:
    OCallCompileTask(Method* _method): CompileTask(_method) {}
    void entry();
    int compile(int size);
    void print_disassembly();
};


#endif //HOTSPOT_OCALLCOMPILETASK_HPP
