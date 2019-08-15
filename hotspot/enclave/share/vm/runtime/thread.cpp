
#include <precompiled.hpp>
#include "oops/markOop.hpp"
#include "thread.hpp"
#include "code/codeBlob.hpp"

ciEnv* JavaThread::_ci_env = NULL;

void JavaThread::oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf) {
    ResourceMark rm;
    if (has_last_Java_frame()) {
        // Traverse the execution stack
        for(StackFrameStream fst(this); !fst.is_done(); fst.next()) {
            if (fst.current()->cb() == NULL) {
                break;
            }
#ifdef DB_GC
            printf(D_INFO("GC Frame")" %s %lx\n", fst.current()->cb()->name(), fst.current()->cb());
#endif
            fst.current()->oops_do(f, cld_f, cf, fst.register_map());
        }
    }
}

void JavaThread::initialize() {
    _thread_obj = (oop)EnclaveMemory::static_vm_new_obj(this, EnclaveMemory::wk_classes[SystemDictionary::Thread_klass_knum]);
}

Thread::Thread() {
    _resource = new (mtThread)ResourceArea();
    set_last_handle_mark(NULL);
    _handle_area = new (mtThread) HandleArea(NULL);
    new HandleMark(this);
}

Thread::~Thread() {
    delete resource_area();
    delete last_handle_mark();
    delete _handle_area;
}