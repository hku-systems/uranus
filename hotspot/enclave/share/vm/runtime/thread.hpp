/*
 * Copyright (c) 1997, 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_RUNTIME_THREAD_HPP
#define SHARE_VM_RUNTIME_THREAD_HPP

#include "memory"
#include "memory/allocation.hpp"
#include "oops/oop.hpp"
#include "prims/jni.h"
#include "runtime/frame.hpp"
#include "runtime/javaFrameAnchor.hpp"
#include "runtime/os.hpp"
#include "utilities/exceptions.hpp"
#include "utilities/macros.hpp"
#include "utilities/top.hpp"
#include "threadLocalStorage.hpp"

class ciEnv;

#ifdef _LP64
typedef uint64_t sys_word_t;
#else
typedef uint32_t sys_word_t;
#endif
typedef struct _thread_data_t
{
    sys_word_t  self_addr;
    sys_word_t  last_sp;            /* set by urts, relative to TCS */
    sys_word_t  stack_base_addr;    /* set by urts, relative to TCS */
    sys_word_t  stack_limit_addr;   /* set by urts, relative to TCS */
    sys_word_t  first_ssa_gpr;      /* set by urts, relative to TCS */
    sys_word_t  stack_guard;        /* GCC expects start_guard at 0x14 on x86 and 0x28 on x64 */

    sys_word_t reserved;
    sys_word_t  ssa_frame_size;     /* set by urts, in pages (se_ptrace.c needs to know its offset). */
    sys_word_t  last_error;         /* init to be 0. Used by trts. */

#ifdef TD_SUPPORT_MULTI_PLATFORM
    sys_word_t  m_next;             /* next TD used by trusted thread library (of type "struct _thread_data *") */
#else
    struct _thread_data_t *m_next;
#endif
    sys_word_t  tls_addr;           /* points to TLS pages */
    sys_word_t  tls_array;          /* points to TD.tls_addr relative to TCS */
#ifdef TD_SUPPORT_MULTI_PLATFORM
    sys_word_t  exception_flag;     /* mark how many exceptions are being handled */
#else
    intptr_t    exception_flag;
#endif
    sys_word_t  cxx_thread_info[6];
} thread_data_t;

template <class T, MEMFLAGS F> class ChunkedList;
typedef ChunkedList<Metadata*, mtInternal> MetadataOnStackBuffer;

DEBUG_ONLY(class ResourceMark;)

class WorkerThread;

// Class hierarchy
// - Thread
//   - NamedThread
//     - VMThread
//     - ConcurrentGCThread
//     - WorkerThread
//       - GangWorker
//       - GCTaskThread
//   - JavaThread
//   - WatcherThread

class Thread: public ThreadShadow {
 private:
  // Exception handling
  // (Note: _pending_exception and friends are in ThreadShadow)
  //oop       _pending_exception;                // pending exception for current thread
  // const char* _exception_file;                   // file information for exception (debugging only)
  // int         _exception_line;                   // line information for exception (debugging only)
  address _stack_base;
  // address for the non-enclave thread;
  address _normal_thread;
  ResourceArea *_resource;
  HandleArea *_handle_area;
  HandleMark* _last_handle_mark;

  size_t _stack_size;
 public:

  Thread();

  ~Thread();

    // initializtion
  void initialize_thread_local_storage() {
    // init tcs?
    thread_data_t* data =  (thread_data_t*)sgx_thread_self();
    intptr_t *base = (intptr_t*)(data->stack_base_addr);
    intptr_t *limit_addr = (intptr_t*)(data->stack_limit_addr);
    set_stack_base((address)(*base));
    set_stack_size(abs(*limit_addr - *base));
  }
   static void* allocate() {
      D_WARN_Unimplement;
   }

  ResourceArea*  resource_area() { return _resource; }
  HandleArea*    handle_area()   { return _handle_area; }
  void           set_last_handle_mark(HandleMark *hm) { _last_handle_mark = hm; }
  HandleMark*    last_handle_mark() { return _last_handle_mark; }
  void    set_stack_base(address base) { _stack_base = base; }
  size_t  stack_size() const           { return _stack_size; }
  void    set_stack_size(size_t size)  { _stack_size = size; }

  static ByteSize stack_base_offset()            { return byte_offset_of(Thread, _stack_base ); }
  static ByteSize stack_size_offset()            { return byte_offset_of(Thread, _stack_size ); }
  static ByteSize normal_thread_offset()          { return byte_offset_of(Thread, _normal_thread ); }

  // Testers
  virtual bool is_VM_thread()       const            { return false; }
  virtual bool is_Java_thread()     const            { return false; }

  // Returns the current thread
  static inline Thread* current();
};

// Inline implementation of Thread::current()
// Thread::current is "hot" it's called > 128K times in the 1st 500 msecs of
// startup.
// ThreadLocalStorage::thread is warm -- it's called > 16K times in the same
// period.   This is inlined in thread_<os_family>.inline.hpp.

inline Thread* Thread::current() {
  return ThreadLocalStorage::thread();
}

class JavaThread: public Thread {
  friend class VMStructs;
 private:
  JavaThread*    _next;                          // The next thread in the Threads list
//  oop            _threadObj;                     // The Java level thread object

  int            _thread_state;
  // order is different to the one outside enclave
  JavaFrameAnchor _normal_anchor;                       // Encapsulation of current java frame and it state

  JavaFrameAnchor _anchor;

  bool do_not_unlock_if_synchronized;


//  ThreadFunction _entry_point;
  JNIEnv        _jni_environment;

  // Used to pass back results to the interpreter or generated code running Java code.
  oop           _vm_result;    // oop result is GC-preserved
  Metadata*     _vm_result_2;  // non-oop result
  Method*       _compiling_method;

  oop           _thread_obj;

  static ciEnv*        _ci_env;

  Method*       _callee_target;
  bool          _has_unhandle_exception;    // exception oop for compiled frame
  oop           _exception_oop;    // exception oop for compiled frame
  address       _exception_pc;    // exception pc for compiled frame
  address _exception_handler_pc;

 public:void initialize();                             // Initialized the instance variables

 public:
  // Constructor
  JavaThread() {
      _has_unhandle_exception = false;
  } // for main thread and JNI attached threads

  // Testers
  virtual bool is_Java_thread() const            { return true;  }

  // Thread chain operations
  JavaThread* next() const                       { return _next; }
  void set_next(JavaThread* p)                   { _next = p; }

  oop threadObj()                                { return _thread_obj; }

  // Last frame anchor routines

  JavaFrameAnchor* frame_anchor(void)            { return &_anchor; }

  // last_Java_sp
  bool has_last_Java_frame() const               { return _anchor.has_last_Java_frame(); }
  intptr_t* last_Java_sp() const                 { return _anchor.last_Java_sp(); }

  // last_Java_pc

  address last_Java_pc(void)                     { return _anchor.last_Java_pc(); }

  // Oop results of vm runtime calls
  oop  vm_result() const                         { return _vm_result; }
  void set_vm_result  (oop x)                    { _vm_result   = x; }
  void set_vm_result_2  (Metadata* x)            { _vm_result_2   = x; }
  Method*    callee_target() const               { return _callee_target; }
  void set_callee_target  (Method* x)            { _callee_target   = x; }
  Method*    compiled_method()                   { return _compiling_method; }
  static ByteSize do_not_unlock_if_synchronized_offset() { return byte_offset_of(JavaThread, do_not_unlock_if_synchronized); }
  static ByteSize thread_state_offset()          { return byte_offset_of(JavaThread, _thread_state); }
  static ByteSize jni_environment_offset()       { return byte_offset_of(JavaThread, _jni_environment     ); }
  static ByteSize last_Java_sp_offset()          {
    return byte_offset_of(JavaThread, _anchor) + JavaFrameAnchor::last_Java_sp_offset();
  }
  static ByteSize compiled_method_offset()          { return byte_offset_of(JavaThread, _compiling_method ); }
  static ByteSize last_Java_pc_offset()          {
    return byte_offset_of(JavaThread, _anchor) + JavaFrameAnchor::last_Java_pc_offset();
  }
  static ByteSize frame_anchor_offset()          {
    return byte_offset_of(JavaThread, _anchor);
  }
  static ByteSize vm_result_offset()             { return byte_offset_of(JavaThread, _vm_result           ); }
  static ByteSize vm_result_2_offset()           { return byte_offset_of(JavaThread, _vm_result_2         ); }
  static ByteSize exception_oop_offset()           { return byte_offset_of(JavaThread, _exception_oop         ); }
  static ByteSize exception_pc_offset()           { return byte_offset_of(JavaThread, _exception_pc         ); }
  static ByteSize callee_target_offset()           { return byte_offset_of(JavaThread, _callee_target         ); }

    // Exception handling for compiled methods
    bool     has_unhandle_exception() const        { return _has_unhandle_exception; }
    void     set_unhandle_exception()              { _has_unhandle_exception = true; }
    oop      exception_oop() const                 { return _exception_oop; }
    address  exception_pc() const                  { return _exception_pc; }
    address  exception_handler_pc() const          { return _exception_handler_pc; }

    void set_exception_oop(oop o)                  { (void)const_cast<oop&>(_exception_oop = o); }
    void set_exception_pc(address a)               { _exception_pc = a; }
    void set_exception_handler_pc(address a)       { _exception_handler_pc = a; }

    void clear_exception_oop_and_pc() {
      set_exception_oop(NULL);
      set_exception_pc(NULL);
    }

  // Returns the jni environment for this thread
  JNIEnv* jni_environment()                      { return &_jni_environment; }
  void set_env(ciEnv *ci)                      { _ci_env = ci; }
  ciEnv* env()                      { return _ci_env; }

  static JavaThread* thread_from_jni_environment(JNIEnv* env) {
    JavaThread *thread_from_jni_env = (JavaThread*)((intptr_t)env - in_bytes(jni_environment_offset()));
    // Only return NULL if thread is off the thread list; starting to
    // exit should not return NULL.
    return thread_from_jni_env;
  }
  // Accessing frames
  frame last_frame() {
    _anchor.make_walkable(this);
    return pd_last_frame();
  }
//  javaVFrame* last_java_vframe(RegisterMap* reg_map);

 public:
  // Returns the running thread as a JavaThread
  static inline JavaThread* current();

  void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);

  // Returns the active Java thread.  Do not use this if you know you are calling
  // from a JavaThread, as it's slower than JavaThread::current.  If called from
  // the VMThread, it also returns the JavaThread that instigated the VMThread's
  // operation.  You may not want that either.

 public:

  // Machine dependent stuff
#ifdef TARGET_OS_ARCH_linux_x86
# include "thread_linux_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_sparc
# include "thread_linux_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_zero
# include "thread_linux_zero.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_x86
# include "thread_solaris_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_sparc
# include "thread_solaris_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_windows_x86
# include "thread_windows_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_arm
# include "thread_linux_arm.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_ppc
# include "thread_linux_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_aix_ppc
# include "thread_aix_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_x86
# include "thread_bsd_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_zero
# include "thread_bsd_zero.hpp"
#endif

};

// Inline implementation of JavaThread::current
inline JavaThread* JavaThread::current() {
  return (JavaThread*)Thread::current();
}

#endif // SHARE_VM_RUNTIME_THREAD_HPP
