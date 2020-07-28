//
// Created by Maxxie Jiang on 22/11/2018.
//

#ifndef HOTSPOT_NATIVECOMPILETASK_HPP
#define HOTSPOT_NATIVECOMPILETASK_HPP

#ifdef HAVE_NATIVE_COMPILE_TASK
#include "NormalCompileTask.h"
#include "CompileTask.hpp"

class NativeCompileTask: public CompileTask {
public:
    NativeCompileTask(Method* _method): CompileTask(_method) {}
    int compile(int size);
    void entry();
};

#endif
#endif //HOTSPOT_NATIVECOMPILETASK_HPP

