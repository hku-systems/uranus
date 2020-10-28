/*
 * Copyright (c) 1999, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "utilities/exceptions.hpp"
#include "oops/oop.hpp"
#include "enclave/EnclaveMemory.h"
#include "enclave/EnclaveDebug.h"
#include "oops/instanceKlass.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "jvm_linux.h"
#include "memory/allocation.inline.hpp"
#include "oops/oop.inline.hpp"
#include "os_share_linux.hpp"
#include "prims/jvm.h"
#include "runtime/extendedPC.hpp"
#include "runtime/globals.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/growableArray.hpp"
#include "runtime/threadLocalStorage.hpp"

#include "sgx_trts.h"

// used by os::exception_name()
extern bool signal_name(int signo, char* buf, size_t len) {
    D_WARN_Unimplement;
    return false;
}

void syscall_invoke_count(const char* ss) {
    printf("call syscall: %s\n", ss);
//    assert(false, "call sys call, error");
}

void TestReserveMemorySpecial_test() {
    syscall_invoke_count(__func__);
}

intx os::current_thread_id() { return (intx)pthread_self(); }

void os::breakpoint() {
}

int os::current_process_id() {

    // Under the old linux thread library, linux gives each thread
    // its own process id. Because of this each thread will return
    // a different pid if this method were to return the result
    // of getpid(2). Linux provides no api that returns the pid
    // of the launcher thread for the vm. This implementation
    // returns a unique pid, the pid of the launcher thread
    // that starts the vm 'process'.

    // Under the NPTL, getpid() returns the same pid as the
    // launcher thread rather than a unique pid per thread.
    // Use gettid() if you want the old pre NPTL behaviour.

    // if you are looking for the result of a call to getpid() that
    // returns a unique pid for the calling thread, then look at the
    // OSThread::thread_id() method in osThread_linux.hpp file

    return (int)(getpid());
}

void os::print_memory_info(outputStream* st) { syscall_invoke_count(__func__); }

void os::die() { syscall_invoke_count(__func__); }

void os::abort(bool dump_core) { syscall_invoke_count(__func__); }

int os::fork_and_exec(char* cmd) { syscall_invoke_count(__func__); }

int os::vm_allocation_granularity() { syscall_invoke_count(__func__); }

jlong os::javaTimeNanos() { syscall_invoke_count(__func__); }

jlong os::elapsed_counter() { syscall_invoke_count(__func__); }

int os::vm_page_size() { return 4096; }

bool os::create_thread(Thread* thread, ThreadType thr_type, size_t stack_size) { syscall_invoke_count(__func__); }

void os::yield() { syscall_invoke_count(__func__); }

char *os::scan_pages(char *start, char* end, page_info* page_expected, page_info* page_found) { syscall_invoke_count(__func__); }

bool os::numa_topology_changed()   { return false; }

size_t os::numa_get_groups_num() { syscall_invoke_count(__func__); }

size_t os::numa_get_leaf_groups(int *ids, size_t size) { syscall_invoke_count(__func__); }

size_t os::large_page_size() { syscall_invoke_count(__func__); }

size_t os::read(int fd, void *buf, unsigned int nBytes) { syscall_invoke_count(__func__); }

void os::infinite_sleep() { syscall_invoke_count(__func__); }

void os::print_siginfo(outputStream* st, void* siginfo) { syscall_invoke_count(__func__); }

void os::print_dll_info(outputStream *st) { syscall_invoke_count(__func__); }

void os::print_os_info(outputStream* st) { syscall_invoke_count(__func__); }

void os::print_signal_handlers(outputStream* st, char* buf, size_t buflen) { syscall_invoke_count(__func__); }

bool os::dll_address_to_function_name(address addr, char *buf,
                                      int buflen, int *offset) { syscall_invoke_count(__func__); }

extern "C" void breakpoint() {

}

const char* os::get_temp_directory() { return "/tmp"; }

int os::sleep(Thread* thread, jlong millis, bool interruptible) { syscall_invoke_count(__func__); }

int os::java_to_os_priority[CriticalPriority + 1] = {
        19,              // 0 Entry should never be used

        4,              // 1 MinPriority
        3,              // 2
        2,              // 3

        1,              // 4
        0,              // 5 NormPriority
        -1,              // 6

        -2,              // 7
        -3,              // 8
        -4,              // 9 NearMaxPriority

        -5,              // 10 MaxPriority

        -5               // 11 CriticalPriority
};

bool os::is_thread_cpu_time_supported() {
    return true;
}

jlong os::elapsed_frequency() {
    return NANOSECS_PER_SEC; // nanosecond resolution
}

jlong os::thread_cpu_time(Thread* thread) { syscall_invoke_count(__func__); }

os::YieldResult os::NakedYield() { syscall_invoke_count(__func__); }

bool os::getenv(const char* name, char* buf, int len) {
    D_WARN_Unimplement;
}

bool os::release_memory_special(char* base, size_t bytes) { syscall_invoke_count(__func__); }


bool os::getTimesSecs(double* process_real_time,
                      double* process_user_time,
                      double* process_system_time) { syscall_invoke_count(__func__); }

jlong os::javaTimeMillis() { syscall_invoke_count(__func__); }

jlong os::current_file_offset(int fd) { syscall_invoke_count(__func__); }

bool os::guard_memory(char* addr, size_t size) { syscall_invoke_count(__func__); }

void os::naked_short_sleep(jlong ms) { syscall_invoke_count(__func__); }

OSReturn os::get_native_priority(const Thread* const thread, int *priority_ptr) { syscall_invoke_count(__func__); }

void os::init(void) { syscall_invoke_count(__func__); }

void os::pause() { syscall_invoke_count(__func__); }




jint os::init_2(void){ syscall_invoke_count(__func__); }

char * os::local_time_string(char *buf, size_t buflen) { syscall_invoke_count(__func__); }

void os::hint_no_preempt() { syscall_invoke_count(__func__); }

int os::create_binary_file(const char* path, bool rewrite_existing) { syscall_invoke_count(__func__); }

jlong os::seek_to_file_offset(int fd, jlong offset) { syscall_invoke_count(__func__); }

julong os::physical_memory() { syscall_invoke_count(__func__); }

jlong os::current_thread_cpu_time() { syscall_invoke_count(__func__); }

OSReturn os::set_native_priority(Thread* thread, int newpri) { syscall_invoke_count(__func__); }

extern bool signal_name(int signo, char* buf, size_t len);

const char* os::exception_name(int exception_code, char* buf, size_t size) {
    return "null";
}

bool os::is_interrupted(Thread* thread, bool clear_interrupted) { syscall_invoke_count(__func__); }

void os::interrupt(Thread* thread) { syscall_invoke_count(__func__); }

void os::set_native_thread_name(const char *name) { syscall_invoke_count(__func__); }

bool os::create_main_thread(JavaThread* thread) { syscall_invoke_count(__func__); }

bool os::unguard_memory(char* addr, size_t size) { syscall_invoke_count(__func__); }

void * os::dll_load(const char *filename, char *ebuf, int ebuflen) { syscall_invoke_count(__func__); }

bool os::remove_stack_guard_pages(char* addr, size_t size) { syscall_invoke_count(__func__); }

void os::free_thread(OSThread* osthread) { syscall_invoke_count(__func__); }

double os::elapsedTime() { syscall_invoke_count(__func__); }

void os::make_polling_page_readable(void) { syscall_invoke_count(__func__); }

void os::yield_all(int attempts) { syscall_invoke_count(__func__); }

void os::make_polling_page_unreadable(void) { syscall_invoke_count(__func__); }

void os::pd_realign_memory(char *addr, size_t bytes, size_t alignment_hint) { syscall_invoke_count(__func__); }

bool os::pd_create_stack_guard_pages(char* addr, size_t size) { syscall_invoke_count(__func__); }

void os::pd_print_cpu_info(outputStream* st) { syscall_invoke_count(__func__); }

void os::pd_start_thread(Thread* thread) { syscall_invoke_count(__func__); }

void os::pd_commit_memory_or_exit(char* addr, size_t size, bool exec,
                                  const char* mesg) { syscall_invoke_count(__func__); }

void os::pd_commit_memory_or_exit(char* addr, size_t size,
                                  size_t alignment_hint, bool exec,
                                  const char* mesg) { syscall_invoke_count(__func__); }

char* os::pd_attempt_reserve_memory_at(size_t bytes, char* requested_addr) { syscall_invoke_count(__func__); }

char* os::pd_reserve_memory(size_t bytes, char* requested_addr,
                            size_t alignment_hint) { syscall_invoke_count(__func__); }

int os::active_processor_count() { syscall_invoke_count(__func__); }

bool os::protect_memory(char* addr, size_t bytes, ProtType prot,
                        bool is_committed) { syscall_invoke_count(__func__); }

bool os::find(address addr, outputStream* st) { syscall_invoke_count(__func__); }

void* os::get_default_process_handle() { syscall_invoke_count(__func__); }

void* os::dll_lookup(void* handle, const char* name) {
  address addr = sgx_get_func(name);
  printf("look for %s %lx\n", name, addr);
  return addr;
}

bool os::dll_build_name(char* buffer, size_t buflen,
                        const char* pname, const char* fname) { syscall_invoke_count(__func__); }

int os::sigexitnum_pd() { syscall_invoke_count(__func__); }

void* os::user_handler() { syscall_invoke_count(__func__); }

void* os::signal(int signal_number, void* handler) { syscall_invoke_count(__func__); }

void os::signal_init_pd() { syscall_invoke_count(__func__); }

struct tm* os::localtime_pd(const time_t* clock, struct tm*  res) { syscall_invoke_count(__func__); }

int os::signal_wait() { syscall_invoke_count(__func__); }

jlong os::thread_cpu_time(Thread *thread, bool user_sys_cpu_time) { syscall_invoke_count(__func__); }

jlong os::current_thread_cpu_time(bool user_sys_cpu_time) { syscall_invoke_count(__func__); }

char* os::reserve_memory_special(size_t bytes, size_t alignment, char* req_addr, bool exec) { syscall_invoke_count(__func__); }

bool os::can_commit_large_page_memory() { syscall_invoke_count(__func__); }

void os::pd_free_memory(char *addr, size_t bytes, size_t alignment_hint) { syscall_invoke_count(__func__); }

void os::signal_notify(int sig) { syscall_invoke_count(__func__); }

void os::large_page_init() { syscall_invoke_count(__func__); }

bool os::pd_release_memory(char* addr, size_t size) { syscall_invoke_count(__func__); }

bool os::pd_uncommit_memory(char* addr, size_t size) { syscall_invoke_count(__func__); }

bool os::pd_commit_memory(char* addr, size_t size, bool exec) { syscall_invoke_count(__func__); }

void
os::os_exception_wrapper(java_call_t f, JavaValue* value, methodHandle* method,
                         JavaCallArguments* args, Thread* thread) { f(value, method, args, thread); }

bool os::pd_commit_memory(char* addr, size_t size, size_t alignment_hint,
                          bool exec) { syscall_invoke_count(__func__); }

char* os::pd_remap_memory(int fd, const char* file_name, size_t file_offset,
                          char *addr, size_t bytes, bool read_only,
                          bool allow_exec) { syscall_invoke_count(__func__); }

void os::shutdown() { syscall_invoke_count(__func__); }

int os::allocate_thread_local_storage() { syscall_invoke_count(__func__); }

void os::SuspendedThreadTask::internal_do_task() { syscall_invoke_count(__func__); }

int os::stat(const char *path, struct stat *sbuf) { syscall_invoke_count(__func__); }

int os::open(const char *path, int oflag, int mode) { syscall_invoke_count(__func__); }

bool os::dir_is_empty(const char* path) { syscall_invoke_count(__func__); }

const char* os::dll_file_extension() { return ".so"; }

bool os::bind_to_processor(uint processor_id) { syscall_invoke_count(__func__); }

double os::elapsedVTime() { syscall_invoke_count(__func__); }

bool os::supports_vtime() { return true; }

bool os::enable_vtime()   { return false; }

bool os::vtime_enabled()  { return false; }

bool os::distribute_processes(uint length, uint* distribution) { syscall_invoke_count(__func__); }

int os::numa_get_group_id() { syscall_invoke_count(__func__); }

void os::numa_make_local(char *addr, size_t bytes, int lgrp_hint) { syscall_invoke_count(__func__); }

bool os::get_page_info(char *start, page_info* info) {
    return false;
}


void os::numa_make_global(char *addr, size_t bytes) { syscall_invoke_count(__func__); }

void linux_wrap_code(char* base, size_t size) { syscall_invoke_count(__func__); }

size_t os::lasterror(char *buf, size_t len) { syscall_invoke_count(__func__); }

bool os::create_attached_thread(JavaThread* thread) { syscall_invoke_count(__func__); }

bool os::dont_yield() { syscall_invoke_count(__func__); }

int os::available(int fd, jlong *bytes) { syscall_invoke_count(__func__); }

int os::loadavg(double loadavg[], int nelem) { syscall_invoke_count(__func__); }

void os::print_jni_name_suffix_on(outputStream* st, int args_size) {
  // no suffix is required
}

bool os::dll_address_to_library_name(address addr, char* buf,
                                     int buflen, int* offset)  { syscall_invoke_count(__func__); }

julong os::available_memory() { syscall_invoke_count(__func__); }

int os::socket_available(int fd, jint *pbytes) { syscall_invoke_count(__func__); }

bool os::can_execute_large_page_memory() { syscall_invoke_count(__func__); }

void os::jvm_path(char *buf, jint buflen) { syscall_invoke_count(__func__); }

bool os::Linux::manually_expand_stack(JavaThread * t, address addr) { syscall_invoke_count(__func__); }

bool os::Linux::signal_handlers_are_installed = false;

int os::message_box(const char* title, const char* message) { syscall_invoke_count(__func__); }

void os::signal_raise(int signal_number) { syscall_invoke_count(__func__); }

bool os::Linux::is_sig_ignored(int sig) { syscall_invoke_count(__func__); }

extern "C" Thread* get_thread() {
    return ThreadLocalStorage::thread();
}

bool os::pd_unmap_memory(char* addr, size_t bytes) { syscall_invoke_count(__func__); }

char* os::pd_map_memory(int fd, const char* file_name, size_t file_offset,
                        char *addr, size_t bytes, bool read_only,
                        bool allow_exec) { syscall_invoke_count(__func__); }

bool os::address_is_in_vm(address addr) { syscall_invoke_count(__func__); }

ExtendedPC os::get_thread_pc(Thread* thread) { syscall_invoke_count(__func__); }

void os::print_jni_name_prefix_on(outputStream* st, int args_size) {}
bool os::is_headless_jre() { syscall_invoke_count(__func__); }

bool os::have_special_privileges() { syscall_invoke_count(__func__); }

bool os::check_heap(bool force) { syscall_invoke_count(__func__); }

void os::run_periodic_checks() { syscall_invoke_count(__func__); }

void os::init_system_properties_values() { syscall_invoke_count(__func__); }

void os::print_statistics() {
}

uintptr_t os::Linux::_initial_thread_stack_size = 0;

address   os::Linux::_initial_thread_stack_bottom = NULL;

bool os::Linux::is_initial_thread(void) {
    return true;
}

void os::thread_local_storage_at_put(int index, void* value) {
    _thread_data_cache[index] = (JavaThread*)value;
}

int os::Linux::_page_size = -1;
