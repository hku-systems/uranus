/*
 * Copyright (c) 1999, 2014, Oracle and/or its affiliates. All rights reserved.
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

// no precompiled headers
#include "enclave/EnclaveMemory.h"
#include "enclave/EnclaveDebug.h"
#include "asm/macroAssembler.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "jvm_linux.h"
#include "memory/allocation.inline.hpp"
#include "os_share_linux.hpp"
#include "prims/jvm.h"
#include "runtime/extendedPC.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"

// put OS-includes here
# include <sys/types.h>
# include <sys/mman.h>
# include <pthread.h>
# include <signal.h>
# include <errno.h>
# include <dlfcn.h>
# include <stdlib.h>
# include <stdio.h>
# include <unistd.h>
# include <sys/resource.h>
# include <pthread.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/utsname.h>
# include <sys/socket.h>
# include <sys/wait.h>
# include <pwd.h>
# include <poll.h>

#if !defined(__APPLE__)
# include <ucontext.h>
# include <fpu_control.h>
#endif

#ifdef AMD64
#define REG_SP REG_RSP
#define REG_PC REG_RIP
#define REG_FP REG_RBP
#define SPELL_REG_SP "rsp"
#define SPELL_REG_FP "rbp"
#else
#define REG_SP REG_UESP
#define REG_PC REG_EIP
#define REG_FP REG_EBP
#define SPELL_REG_SP "esp"
#define SPELL_REG_FP "ebp"
#endif // AMD64

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

address os::current_stack_pointer() {
#ifdef SPARC_WORKS
  register void *esp;
  __asm__("mov %%"SPELL_REG_SP", %0":"=r"(esp));
  return (address) ((char*)esp + sizeof(long)*2);
#elif defined(__clang__)
  intptr_t* esp;
  __asm__ __volatile__ ("mov %%"SPELL_REG_SP", %0":"=r"(esp):);
  return (address) esp;
#else
  register void *esp __asm__ (SPELL_REG_SP);
  return (address) esp;
#endif
}

char* os::non_memory_address_word() {
  // Must never look like an address returned by reserve_memory,
  // even in its subfields (as defined by the CPU immediate fields,
  // if the CPU splits constants across multiple instructions).

  return (char*) -1;
}

void os::initialize_thread(Thread* thr) {
// Nothing to do.
}

address os::Linux::ucontext_get_pc(ucontext_t * uc) {
  ShouldNotReachHere();
}

intptr_t* os::Linux::ucontext_get_sp(ucontext_t * uc) {
  ShouldNotReachHere();
}

intptr_t* os::Linux::ucontext_get_fp(ucontext_t * uc) {
  ShouldNotReachHere();
}

// For Forte Analyzer AsyncGetCallTrace profiling support - thread
// is currently interrupted by SIGPROF.
// os::Solaris::fetch_frame_from_ucontext() tries to skip nested signal
// frames. Currently we don't do that on Linux, so it's the same as
// os::fetch_frame_from_context().
ExtendedPC os::Linux::fetch_frame_from_ucontext(Thread* thread,
  ucontext_t* uc, intptr_t** ret_sp, intptr_t** ret_fp) {

  assert(thread != NULL, "just checking");
  assert(ret_sp != NULL, "just checking");
  assert(ret_fp != NULL, "just checking");

  return os::fetch_frame_from_context(uc, ret_sp, ret_fp);
}

ExtendedPC os::fetch_frame_from_context(void* ucVoid,
                    intptr_t** ret_sp, intptr_t** ret_fp) {

  ExtendedPC  epc;
  ucontext_t* uc = (ucontext_t*)ucVoid;

  if (uc != NULL) {
    epc = ExtendedPC(os::Linux::ucontext_get_pc(uc));
    if (ret_sp) *ret_sp = os::Linux::ucontext_get_sp(uc);
    if (ret_fp) *ret_fp = os::Linux::ucontext_get_fp(uc);
  } else {
    // construct empty ExtendedPC for return value checking
    epc = ExtendedPC(NULL);
    if (ret_sp) *ret_sp = (intptr_t *)NULL;
    if (ret_fp) *ret_fp = (intptr_t *)NULL;
  }

  return epc;
}

frame os::fetch_frame_from_context(void* ucVoid) {
  intptr_t* sp;
  intptr_t* fp;
  ExtendedPC epc = fetch_frame_from_context(ucVoid, &sp, &fp);
  return frame(sp, fp, epc.pc());
}

// By default, gcc always save frame pointer (%ebp/%rbp) on stack. It may get
// turned off by -fomit-frame-pointer,
frame os::get_sender_for_C_frame(frame* fr) {
  return frame(fr->sender_sp(), fr->link(), fr->sender_pc());
}

intptr_t* _get_previous_fp() {
#ifdef SPARC_WORKS
  register intptr_t **ebp;
  __asm__("mov %%"SPELL_REG_FP", %0":"=r"(ebp));
#elif defined(__clang__)
  intptr_t **ebp;
  __asm__ __volatile__ ("mov %%"SPELL_REG_FP", %0":"=r"(ebp):);
#else
  register intptr_t **ebp __asm__ (SPELL_REG_FP);
#endif
  return (intptr_t*) *ebp;   // we want what it points to.
}


frame os::current_frame() {
  intptr_t* fp = _get_previous_fp();
  frame myframe((intptr_t*)os::current_stack_pointer(),
                (intptr_t*)fp,
                CAST_FROM_FN_PTR(address, os::current_frame));
  if (os::is_first_C_frame(&myframe)) {
    // stack is not walkable
    return frame();
  } else {
    return os::get_sender_for_C_frame(&myframe);
  }
}

// Utility functions

// From IA32 System Programming Guide
enum {
  trap_page_fault = 0xE
};

extern "C" JNIEXPORT int
JVM_handle_linux_signal(int sig,
                        siginfo_t* info,
                        void* ucVoid,
                        int abort_if_unrecognized) {
  D_WARN_Unimplement;
}

void os::Linux::init_thread_fpu_state(void) {
#ifndef AMD64
  // set fpu to 53 bit precision
  set_fpu_control_word(0x27f);
#endif // !AMD64
}

int os::Linux::get_fpu_control_word(void) {
#ifdef AMD64
  return 0;
#else
  int fpu_control;
  _FPU_GETCW(fpu_control);
  return fpu_control & 0xffff;
#endif // AMD64
}

bool os::supports_sse() {
  return true;
}

void os::Linux::set_fpu_control_word(int fpu_control) {
#ifndef AMD64
  _FPU_SETCW(fpu_control);
#endif // !AMD64
}
////////////////////////////////////////////////////////////////////////////////
// thread stack

#ifdef AMD64
size_t os::Linux::min_stack_allowed  = 64 * K;

// amd64: pthread on amd64 is always in floating stack mode
bool os::Linux::supports_variable_stack_size() {  return true; }
#else
size_t os::Linux::min_stack_allowed  =  (48 DEBUG_ONLY(+4))*K;

#ifdef __GNUC__
#define GET_GS() ({int gs; __asm__ volatile("movw %%gs, %w0":"=q"(gs)); gs&0xffff;})
#endif

// Test if pthread library can support variable thread stack size. LinuxThreads
// in fixed stack mode allocates 2M fixed slot for each thread. LinuxThreads
// in floating stack mode and NPTL support variable stack size.
bool os::Linux::supports_variable_stack_size() {
  if (os::Linux::is_NPTL()) {
     // NPTL, yes
     return true;

  } else {
    // Note: We can't control default stack size when creating a thread.
    // If we use non-default stack size (pthread_attr_setstacksize), both
    // floating stack and non-floating stack LinuxThreads will return the
    // same value. This makes it impossible to implement this function by
    // detecting thread stack size directly.
    //
    // An alternative approach is to check %gs. Fixed-stack LinuxThreads
    // do not use %gs, so its value is 0. Floating-stack LinuxThreads use
    // %gs (either as LDT selector or GDT selector, depending on kernel)
    // to access thread specific data.
    //
    // Note that %gs is a reserved glibc register since early 2001, so
    // applications are not allowed to change its value (Ulrich Drepper from
    // Redhat confirmed that all known offenders have been modified to use
    // either %fs or TSD). In the worst case scenario, when VM is embedded in
    // a native application that plays with %gs, we might see non-zero %gs
    // even LinuxThreads is running in fixed stack mode. As the result, we'll
    // return true and skip _thread_safety_check(), so we may not be able to
    // detect stack-heap collisions. But otherwise it's harmless.
    //
#ifdef __GNUC__
    return (GET_GS() != 0);
#else
    return false;
#endif
  }
}
#endif // AMD64

// return default stack size for thr_type
size_t os::Linux::default_stack_size(os::ThreadType thr_type) {
  // default stack size (compiler thread needs larger stack)
#ifdef AMD64
  size_t s = (thr_type == os::compiler_thread ? 4 * M : 1 * M);
#else
  size_t s = (thr_type == os::compiler_thread ? 2 * M : 512 * K);
#endif // AMD64
  return s;
}

size_t os::Linux::default_guard_size(os::ThreadType thr_type) {
  // Creating guard page is very expensive. Java thread has HotSpot
  // guard page, only enable glibc guard page for non-Java threads.
  return (thr_type == java_thread ? 0 : page_size());
}

// Java thread:
//
//   Low memory addresses
//    +------------------------+
//    |                        |\  JavaThread created by VM does not have glibc
//    |    glibc guard page    | - guard, attached Java thread usually has
//    |                        |/  1 page glibc guard.
// P1 +------------------------+ Thread::stack_base() - Thread::stack_size()
//    |                        |\
//    |  HotSpot Guard Pages   | - red and yellow pages
//    |                        |/
//    +------------------------+ JavaThread::stack_yellow_zone_base()
//    |                        |\
//    |      Normal Stack      | -
//    |                        |/
// P2 +------------------------+ Thread::stack_base()
//
// Non-Java thread:
//
//   Low memory addresses
//    +------------------------+
//    |                        |\
//    |  glibc guard page      | - usually 1 page
//    |                        |/
// P1 +------------------------+ Thread::stack_base() - Thread::stack_size()
//    |                        |\
//    |      Normal Stack      | -
//    |                        |/
// P2 +------------------------+ Thread::stack_base()
//
// ** P1 (aka bottom) and size ( P2 = P1 - size) are the address and stack size returned from
//    pthread_attr_getstack()

static void current_stack_region(address * bottom, size_t * size) {
  D_WARN_Unimplement;
}

address os::current_stack_base() {
  address bottom;
  size_t size;
  current_stack_region(&bottom, &size);
  return (bottom + size);
}

size_t os::current_stack_size() {
  // stack size includes normal stack and HotSpot guard pages
  address bottom;
  size_t size;
  current_stack_region(&bottom, &size);
  return size;
}

/////////////////////////////////////////////////////////////////////////////
// helper functions for fatal error handler

void os::print_context(outputStream *st, void *context) {

}

void os::setup_fpu() {
#ifndef AMD64
  address fpu_cntrl = StubRoutines::addr_fpu_cntrl_wrd_std();
  __asm__ volatile (  "fldcw (%0)" :
                      : "r" (fpu_cntrl) : "memory");
#endif // !AMD64
}

#ifndef PRODUCT
void os::verify_stack_alignment() {
#ifdef AMD64
  assert(((intptr_t)os::current_stack_pointer() & (StackAlignmentInBytes-1)) == 0, "incorrect stack alignment");
#endif
}
#endif


/*
 * IA32 only: execute code at a high address in case buggy NX emulation is present. I.e. avoid CS limit
 * updates (JDK-8023956).
 */
void os::workaround_expand_exec_shield_cs_limit() {
#if defined(IA32)
  size_t page_size = os::vm_page_size();
  /*
   * Take the highest VA the OS will give us and exec
   *
   * Although using -(pagesz) as mmap hint works on newer kernel as you would
   * think, older variants affected by this work-around don't (search forward only).
   *
   * On the affected distributions, we understand the memory layout to be:
   *
   *   TASK_LIMIT= 3G, main stack base close to TASK_LIMT.
   *
   * A few pages south main stack will do it.
   *
   * If we are embedded in an app other than launcher (initial != main stack),
   * we don't have much control or understanding of the address space, just let it slide.
   */
  char* hint = (char*) (Linux::initial_thread_stack_bottom() -
                        ((StackYellowPages + StackRedPages + 1) * page_size));
  char* codebuf = os::attempt_reserve_memory_at(page_size, hint);
  if ( (codebuf == NULL) || (!os::commit_memory(codebuf, page_size, true)) ) {
    return; // No matter, we tried, best effort.
  }
  if (PrintMiscellaneous && (Verbose || WizardMode)) {
     tty->print_cr("[CS limit NX emulation work-around, exec code at: %p]", codebuf);
  }

  // Some code to exec: the 'ret' instruction
  codebuf[0] = 0xC3;

  // Call the code in the codebuf
  __asm__ volatile("call *%0" : : "r"(codebuf));

  // keep the page mapped so CS limit isn't reduced.
#endif
}
