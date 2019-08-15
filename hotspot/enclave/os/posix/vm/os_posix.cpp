/*
* Copyright (c) 1999, 2015, Oracle and/or its affiliates. All rights reserved.
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

#include <sys/resource.h>
#include <unistd.h>
extern void syscall_invoke_count(const char* ss);

void os::check_or_create_dump(void* exceptionRecord, void* contextRecord, char* buffer, size_t bufferSize) { syscall_invoke_count(__func__); }

int os::get_native_stack(address* stack, int frames, int toSkip) { syscall_invoke_count(__func__); }

bool os::unsetenv(const char* name) {
  assert(name != NULL, "Null pointer");
  return (::unsetenv(name) == 0);
}

int os::get_last_error() {
  return errno;
}

bool os::is_debugger_attached() {
  // not implemented
  return false;
}

void os::wait_for_keypress_at_exit(void) {
  // don't do anything on posix platforms
  return;
}

// Multiple threads can race in this code, and can remap over each other with MAP_FIXED,
// so on posix, unmap the section at the start and at the end of the chunk that we mapped
// rather than unmapping and remapping the whole chunk to get requested alignment.
char* os::reserve_memory_aligned(size_t size, size_t alignment) { syscall_invoke_count(__func__); }

void os::Posix::print_load_average(outputStream* st) { syscall_invoke_count(__func__); }

void os::Posix::print_rlimit_info(outputStream* st) { syscall_invoke_count(__func__); }

void os::Posix::print_uname_info(outputStream* st) { syscall_invoke_count(__func__); }

bool os::has_allocatable_memory_limit(julong* limit) {
  struct rlimit rlim;
  int getrlimit_res = getrlimit(RLIMIT_AS, &rlim);
  // if there was an error when calling getrlimit, assume that there is no limitation
  // on virtual memory.
  bool result;
  if ((getrlimit_res != 0) || (rlim.rlim_cur == RLIM_INFINITY)) {
    result = false;
  } else {
    *limit = (julong)rlim.rlim_cur;
    result = true;
  }
#ifdef _LP64
  return result;
#else
  // arbitrary virtual space limit for 32 bit Unices found by testing. If
  // getrlimit above returned a limit, bound it with this limit. Otherwise
  // directly use it.
  const julong max_virtual_limit = (julong)3800*M;
  if (result) {
    *limit = MIN2(*limit, max_virtual_limit);
  } else {
    *limit = max_virtual_limit;
  }

  // bound by actually allocatable memory. The algorithm uses two bounds, an
  // upper and a lower limit. The upper limit is the current highest amount of
  // memory that could not be allocated, the lower limit is the current highest
  // amount of memory that could be allocated.
  // The algorithm iteratively refines the result by halving the difference
  // between these limits, updating either the upper limit (if that value could
  // not be allocated) or the lower limit (if the that value could be allocated)
  // until the difference between these limits is "small".

  // the minimum amount of memory we care about allocating.
  const julong min_allocation_size = M;

  julong upper_limit = *limit;

  // first check a few trivial cases
  if (is_allocatable(upper_limit) || (upper_limit <= min_allocation_size)) {
    *limit = upper_limit;
  } else if (!is_allocatable(min_allocation_size)) {
    // we found that not even min_allocation_size is allocatable. Return it
    // anyway. There is no point to search for a better value any more.
    *limit = min_allocation_size;
  } else {
    // perform the binary search.
    julong lower_limit = min_allocation_size;
    while ((upper_limit - lower_limit) > min_allocation_size) {
      julong temp_limit = ((upper_limit - lower_limit) / 2) + lower_limit;
      temp_limit = align_size_down_(temp_limit, min_allocation_size);
      if (is_allocatable(temp_limit)) {
        lower_limit = temp_limit;
      } else {
        upper_limit = temp_limit;
      }
    }
    *limit = lower_limit;
  }
  return true;
#endif
}

const char* os::get_current_directory(char *buf, size_t buflen) {
  return getcwd(buf, buflen);
}

FILE* os::open(int fd, const char* mode) { syscall_invoke_count(__func__); }

// Builds a platform dependent Agent_OnLoad_<lib_name> function name
// which is used to find statically linked in agents.
// Parameters:
//            sym_name: Symbol in library we are looking for
//            lib_name: Name of library to look in, NULL for shared libs.
//            is_absolute_path == true if lib_name is absolute path to agent
//                                     such as "/a/b/libL.so"
//            == false if only the base name of the library is passed in
//               such as "L"
char* os::build_agent_function_name(const char *sym_name, const char *lib_name,
                                    bool is_absolute_path) { syscall_invoke_count(__func__); }

// Returned string is a constant. For unknown signals "UNKNOWN" is returned.
const char* os::Posix::get_signal_name(int sig, char* out, size_t outlen) { syscall_invoke_count(__func__); }

// Returns true if signal number is valid.
bool os::Posix::is_valid_signal(int sig) { syscall_invoke_count(__func__); }

#define NUM_IMPORTANT_SIGS 32
// Returns one-line short description of a signal set in a user provided buffer.
const char* os::Posix::describe_signal_set_short(const sigset_t* set, char* buffer, size_t buf_size) { syscall_invoke_count(__func__); }

// Prints one-line description of a signal set.
void os::Posix::print_signal_set_short(outputStream* st, const sigset_t* set) { syscall_invoke_count(__func__); }

// Writes one-line description of a combination of sigaction.sa_flags into a user
// provided buffer. Returns that buffer.
const char* os::Posix::describe_sa_flags(int flags, char* buffer, size_t size) { syscall_invoke_count(__func__); }

// Prints one-line description of a combination of sigaction.sa_flags.
void os::Posix::print_sa_flags(outputStream* st, int flags) { syscall_invoke_count(__func__); }

// Helper function for os::Posix::print_siginfo_...():
// return a textual description for signal code.
struct enum_sigcode_desc_t {
    const char* s_name;
    const char* s_desc;
};

static bool get_signal_code_description(const siginfo_t* si, enum_sigcode_desc_t* out) { syscall_invoke_count(__func__); }

// A POSIX conform, platform-independend siginfo print routine.
// Short print out on one line.
void os::Posix::print_siginfo_brief(outputStream* os, const siginfo_t* si) { syscall_invoke_count(__func__); }

os::WatcherThreadCrashProtection::WatcherThreadCrashProtection() { syscall_invoke_count(__func__); }

/*
 * See the caveats for this class in os_posix.hpp
 * Protects the callback call so that SIGSEGV / SIGBUS jumps back into this
 * method and returns false. If none of the signals are raised, returns true.
 * The callback is supposed to provide the method that should be protected.
 */
bool os::WatcherThreadCrashProtection::call(os::CrashProtectionCallback& cb) { syscall_invoke_count(__func__); }

void os::WatcherThreadCrashProtection::restore() { syscall_invoke_count(__func__); }

void os::WatcherThreadCrashProtection::check_crash_protection(int sig,
                                                              Thread* thread) { syscall_invoke_count(__func__); }
