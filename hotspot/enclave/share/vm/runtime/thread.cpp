
#include <precompiled.hpp>
#include "oops/markOop.hpp"
#include "thread.hpp"
#include "code/codeBlob.hpp"
#include "classfile/javaClasses.hpp"
#include "runtime/javaCalls.hpp"

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

static void initialize_class(Symbol* class_name, TRAPS) {
  printf("thread class %s\n", class_name->as_C_string());
  Klass* klass = SystemDictionary::resolve_or_fail(class_name, true, CHECK);
  InstanceKlass::cast(klass)->initialize(CHECK);
  printf("thread class %s fin\n", class_name->as_C_string());
}

// Creates the initial ThreadGroup
static Handle create_initial_thread_group(TRAPS) {
  Klass* k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_ThreadGroup(), true, CHECK_NH);
  instanceKlassHandle klass (THREAD, k);

  printf("init\n");
  Handle system_instance = klass->allocate_instance_handle(CHECK_NH);
  {
    JavaValue result(T_VOID);
    JavaCalls::call_special(&result,
                            system_instance,
                            klass,
                            vmSymbols::object_initializer_name(),
                            vmSymbols::void_method_signature(),
    CHECK_NH);
  }
  Universe::set_system_thread_group(system_instance());

  printf("init\n");
  Handle main_instance = klass->allocate_instance_handle(CHECK_NH);
  {
    JavaValue result(T_VOID);
    Handle string = java_lang_String::create_from_str("main", CHECK_NH);
    JavaCalls::call_special(&result,
                            main_instance,
                            klass,
                            vmSymbols::object_initializer_name(),
                            vmSymbols::threadgroup_string_void_signature(),
                            system_instance,
                            string,
    CHECK_NH);
  }
  return main_instance;
}

// Creates the initial Thread
static oop create_initial_thread(Handle thread_group, JavaThread* thread, TRAPS) {
  Klass* k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_Thread(), true, CHECK_NULL);
  instanceKlassHandle klass (THREAD, k);
  instanceHandle thread_oop = klass->allocate_instance_handle(CHECK_NULL);

  java_lang_Thread::set_thread(thread_oop(), thread);
  java_lang_Thread::set_priority(thread_oop(), NormPriority);
  thread->set_threadObj(thread_oop());

  Handle string = java_lang_String::create_from_str("main", CHECK_NULL);

  JavaValue result(T_VOID);
  JavaCalls::call_special(&result, thread_oop,
                          klass,
                          vmSymbols::object_initializer_name(),
                          vmSymbols::threadgroup_string_void_signature(),
                          thread_group,
                          string,
  CHECK_NULL);
  return thread_oop();
}

static void call_initializeSystemClass(TRAPS) {
  Klass* k =  SystemDictionary::resolve_or_fail(vmSymbols::java_lang_System(), true, CHECK);
  instanceKlassHandle klass (THREAD, k);

  JavaValue result(T_VOID);
  JavaCalls::call_static(&result, klass, vmSymbols::initializeSystemClass_name(),
                         vmSymbols::void_method_signature(), CHECK);
}

bool is_thread_init = false;

void JavaThread::initialize() {
  if (is_thread_init) {
    return;
  }

  is_thread_init = true;

  JavaThread *THREAD = this;
  initialize_class(vmSymbols::java_lang_String(), CHECK);

  // Initialize java_lang.System (needed before creating the thread)
  initialize_class(vmSymbols::java_lang_System(), CHECK);
  initialize_class(vmSymbols::java_lang_ThreadGroup(), CHECK);
  Handle thread_group = create_initial_thread_group(CHECK);
  Universe::set_main_thread_group(thread_group());
  initialize_class(vmSymbols::java_lang_Thread(), CHECK);
  oop thread_object = create_initial_thread(thread_group, this, CHECK);
  // Set thread status to running since main thread has
  // been started and running.
//  java_lang_Thread::set_thread_status(thread_object,
//                                      java_lang_Thread::RUNNABLE);

  // The VM creates & returns objects of this class. Make sure it's initialized.
  initialize_class(vmSymbols::java_lang_Class(), CHECK);

  // The VM preresolves methods to these classes. Make sure that they get initialized
//  initialize_class(vmSymbols::java_lang_reflect_Method(), CHECK);
//  initialize_class(vmSymbols::java_lang_ref_Finalizer(),  CHECK);
  call_initializeSystemClass(CHECK);

  // an instance of OutOfMemory exception has been allocated earlier
  initialize_class(vmSymbols::java_lang_OutOfMemoryError(), CHECK);
  initialize_class(vmSymbols::java_lang_NullPointerException(), CHECK);
  initialize_class(vmSymbols::java_lang_ClassCastException(), CHECK);
  initialize_class(vmSymbols::java_lang_ArrayStoreException(), CHECK);
  initialize_class(vmSymbols::java_lang_ArithmeticException(), CHECK);
  initialize_class(vmSymbols::java_lang_StackOverflowError(), CHECK);
  initialize_class(vmSymbols::java_lang_IllegalMonitorStateException(), CHECK);
  initialize_class(vmSymbols::java_lang_IllegalArgumentException(), CHECK);
}

Thread::Thread() {
    _resource = new (mtThread)ResourceArea();
    set_free_handle_block(NULL);
    set_active_handles(NULL);
    set_active_handles(JNIHandleBlock::allocate_block(this));
    set_last_handle_mark(NULL);
    _handle_area = new (mtThread) HandleArea(NULL);
    new HandleMark(this);
    // thread-specific hashCode stream generator state - Marsaglia shift-xor form
    _hashStateX = os::random() ;
    _hashStateY = 842502087 ;
    _hashStateZ = 0x8767 ;    // (int)(3579807591LL & 0xffff) ;
    _hashStateW = 273326509 ;
}

Thread::~Thread() {
    delete resource_area();
    delete last_handle_mark();
    delete _handle_area;
    if (free_handle_block())
      JNIHandleBlock::release_block(free_handle_block(), this);
}