/*
 * Copyright (c) 1997, 2016, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "memory/allocation.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/allocation.inline.hpp"


#ifdef ASSERT
#include "memory/guardedMemory.hpp"
#endif
#include "oops/oop.inline.hpp"
#include "prims/jvm.h"
#include "runtime/frame.inline.hpp"
#include "runtime/os.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

# include <signal.h>

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

OSThread*         os::_starting_thread    = NULL;
address           os::_polling_page       = NULL;
volatile int32_t* os::_mem_serialize_page = NULL;
uintptr_t         os::_serialize_page_mask = 0;
Thread*           os::_thread_data_cache[SGX_TCS_NUM];
long              os::_rand_seed          = 1;
int               os::_processor_count    = 0;
int               os::_initial_active_processor_count = 0;
size_t            os::_page_sizes[os::page_sizes_max];

#ifndef PRODUCT
julong os::num_mallocs = 0;         // # of calls to malloc/realloc
julong os::alloc_bytes = 0;         // # of bytes allocated
julong os::num_frees = 0;           // # of calls to free
julong os::free_bytes = 0;          // # of bytes freed
#endif

static juint cur_malloc_words = 0;  // current size for MallocMaxTestWords

void os_init_globals() {
  // Called from init_globals().
  // See Threads::create_vm() in thread.cpp, and init.cpp.
  os::init_globals();
}

// Fill in buffer with current local time as an ISO-8601 string.
// E.g., yyyy-mm-ddThh:mm:ss-zzzz.
// Returns buffer, or NULL if it failed.
// This would mostly be a call to
//     strftime(...., "%Y-%m-%d" "T" "%H:%M:%S" "%z", ....)
// except that on Windows the %z behaves badly, so we do it ourselves.
// Also, people wanted milliseconds on there,
// and strftime doesn't do milliseconds.
char* os::iso8601_time(char* buffer, size_t buffer_length) {
  // Output will be of the form "YYYY-MM-DDThh:mm:ss.mmm+zzzz\0"
  //                                      1         2
  //                             12345678901234567890123456789
  static const char* iso8601_format =
    "%04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d%02d";
  static const size_t needed_buffer = 29;

  // Sanity check the arguments
  if (buffer == NULL) {
    assert(false, "NULL buffer");
    return NULL;
  }
  if (buffer_length < needed_buffer) {
    assert(false, "buffer_length too small");
    return NULL;
  }
  // Get the current time
  jlong milliseconds_since_19700101 = javaTimeMillis();
  const int milliseconds_per_microsecond = 1000;
  const time_t seconds_since_19700101 =
    milliseconds_since_19700101 / milliseconds_per_microsecond;
  const int milliseconds_after_second =
    milliseconds_since_19700101 % milliseconds_per_microsecond;
  // Convert the time value to a tm and timezone variable
  struct tm time_struct;
  if (localtime_pd(&seconds_since_19700101, &time_struct) == NULL) {
    assert(false, "Failed localtime_pd");
    return NULL;
  }
#if defined(_ALLBSD_SOURCE)
  const time_t zone = (time_t) time_struct.tm_gmtoff;
#else
  const time_t zone = timezone;
#endif

  // If daylight savings time is in effect,
  // we are 1 hour East of our time zone
  const time_t seconds_per_minute = 60;
  const time_t minutes_per_hour = 60;
  const time_t seconds_per_hour = seconds_per_minute * minutes_per_hour;
  time_t UTC_to_local = zone;
  if (time_struct.tm_isdst > 0) {
    UTC_to_local = UTC_to_local - seconds_per_hour;
  }
  // Compute the time zone offset.
  //    localtime_pd() sets timezone to the difference (in seconds)
  //    between UTC and and local time.
  //    ISO 8601 says we need the difference between local time and UTC,
  //    we change the sign of the localtime_pd() result.
  const time_t local_to_UTC = -(UTC_to_local);
  // Then we have to figure out if if we are ahead (+) or behind (-) UTC.
  char sign_local_to_UTC = '+';
  time_t abs_local_to_UTC = local_to_UTC;
  if (local_to_UTC < 0) {
    sign_local_to_UTC = '-';
    abs_local_to_UTC = -(abs_local_to_UTC);
  }
  // Convert time zone offset seconds to hours and minutes.
  const time_t zone_hours = (abs_local_to_UTC / seconds_per_hour);
  const time_t zone_min =
    ((abs_local_to_UTC % seconds_per_hour) / seconds_per_minute);

  // Print an ISO 8601 date and time stamp into the buffer
  const int year = 1900 + time_struct.tm_year;
  const int month = 1 + time_struct.tm_mon;
  const int printed = jio_snprintf(buffer, buffer_length, iso8601_format,
                                   year,
                                   month,
                                   time_struct.tm_mday,
                                   time_struct.tm_hour,
                                   time_struct.tm_min,
                                   time_struct.tm_sec,
                                   milliseconds_after_second,
                                   sign_local_to_UTC,
                                   zone_hours,
                                   zone_min);
  if (printed == 0) {
    assert(false, "Failed jio_printf");
    return NULL;
  }
  return buffer;
}

OSReturn os::set_priority(Thread* thread, ThreadPriority p) {
#ifdef ASSERT
  if (!(!thread->is_Java_thread() ||
         Thread::current() == thread  ||
         Threads_lock->owned_by_self()
         || thread->is_Compiler_thread()
        )) {
    assert(false, "possibility of dangling Thread pointer");
  }
#endif

  if (p >= MinPriority && p <= MaxPriority) {
    int priority = java_to_os_priority[p];
    return set_native_priority(thread, priority);
  } else {
    assert(false, "Should not happen");
    return OS_ERR;
  }
}

// The mapping from OS priority back to Java priority may be inexact because
// Java priorities can map M:1 with native priorities. If you want the definite
// Java priority then use JavaThread::java_priority()
OSReturn os::get_priority(const Thread* const thread, ThreadPriority& priority) {
  int p;
  int os_prio;
  OSReturn ret = get_native_priority(thread, &os_prio);
  if (ret != OS_OK) return ret;

  if (java_to_os_priority[MaxPriority] > java_to_os_priority[MinPriority]) {
    for (p = MaxPriority; p > MinPriority && java_to_os_priority[p] > os_prio; p--) ;
  } else {
    // niceness values are in reverse order
    for (p = MaxPriority; p > MinPriority && java_to_os_priority[p] < os_prio; p--) ;
  }
  priority = (ThreadPriority)p;
  return OS_OK;
}


// --------------------- sun.misc.Signal (optional) ---------------------


// SIGBREAK is sent by the keyboard to query the VM state
#ifndef SIGBREAK
#define SIGBREAK SIGQUIT
#endif

// sigexitnum_pd is a platform-specific special signal used for terminating the Signal thread.

void os::init_before_ergo() {
  initialize_initial_active_processor_count();
  // We need to initialize large page support here because ergonomics takes some
  // decisions depending on large page support and the calculated large page size.
  large_page_init();

  // VM version initialization identifies some characteristics of the
  // the platform that are used during ergonomic decisions.
//  VM_Version::init_before_ergo();
}

void os::signal_init() {

}


void os::terminate_signal_thread() {
  if (!ReduceSignalUsage)
    signal_notify(sigexitnum_pd());
}


// --------------------- loading libraries ---------------------

extern struct JavaVM_ main_vm;

static void* _native_java_library = NULL;


// --------------------- heap allocation utilities ---------------------

char *os::strdup(const char *str, MEMFLAGS flags) {
  size_t size = strlen(str);
  char *dup_str = (char *)malloc(size + 1, flags);
  if (dup_str == NULL) return NULL;
  strcpy(dup_str, str);
  return dup_str;
}



#define paranoid                 0  /* only set to 1 if you suspect checking code has bug */

#ifdef ASSERT
static void verify_memory(void* ptr) {
  GuardedMemory guarded(ptr);
  if (!guarded.verify_guards()) {
    tty->print_cr("## nof_mallocs = " UINT64_FORMAT ", nof_frees = " UINT64_FORMAT, os::num_mallocs, os::num_frees);
    tty->print_cr("## memory stomp:");
    guarded.print_on(tty);
    fatal("memory stomping error");
  }
}
#endif

//
// This function supports testing of the malloc out of memory
// condition without really running the system out of memory.
//
static u_char* testMalloc(size_t alloc_size) {
  assert(MallocMaxTestWords > 0, "sanity check");

  if ((cur_malloc_words + (alloc_size / BytesPerWord)) > MallocMaxTestWords) {
    return NULL;
  }

  u_char* ptr = (u_char*)::malloc(alloc_size);

  if (ptr != NULL) {
    Atomic::add(((jint) (alloc_size / BytesPerWord)),
                (volatile jint *) &cur_malloc_words);
  }
  return ptr;
}

void* os::malloc(size_t size, MEMFLAGS flags) {
  return ::malloc(size);
}

void* os::malloc(size_t size, MEMFLAGS memflags, const NativeCallStack& stack) {
  return ::malloc(size);
}

void* os::realloc(void *memblock, size_t size, MEMFLAGS flags) {
  return ::realloc(memblock, size);
}

void* os::realloc(void *memblock, size_t size, MEMFLAGS memflags, const NativeCallStack& stack) {
  ::realloc(memblock, size);
}


void  os::free(void *memblock, MEMFLAGS memflags) {
  ::free(memblock);
}

void os::init_random(long initval) {
  _rand_seed = initval;
}


long os::random() {
  /* standard, well-known linear congruential random generator with
   * next_rand = (16807*seed) mod (2**31-1)
   * see
   * (1) "Random Number Generators: Good Ones Are Hard to Find",
   *      S.K. Park and K.W. Miller, Communications of the ACM 31:10 (Oct 1988),
   * (2) "Two Fast Implementations of the 'Minimal Standard' Random
   *     Number Generator", David G. Carta, Comm. ACM 33, 1 (Jan 1990), pp. 87-88.
  */
  const long a = 16807;
  const unsigned long m = 2147483647;
  const long q = m / a;        assert(q == 127773, "weird math");
  const long r = m % a;        assert(r == 2836, "weird math");

  // compute az=2^31p+q
  unsigned long lo = a * (long)(_rand_seed & 0xFFFF);
  unsigned long hi = a * (long)((unsigned long)_rand_seed >> 16);
  lo += (hi & 0x7FFF) << 16;

  // if q overflowed, ignore the overflow and increment q
  if (lo > m) {
    lo &= m;
    ++lo;
  }
  lo += hi >> 15;

  // if (p+q) overflowed, ignore the overflow and increment (p+q)
  if (lo > m) {
    lo &= m;
    ++lo;
  }
  return (_rand_seed = lo);
}

// The INITIALIZED state is distinguished from the SUSPENDED state because the
// conditions in which a thread is first started are different from those in which
// a suspension is resumed.  These differences make it hard for us to apply the
// tougher checks when starting threads that we want to do when resuming them.
// However, when start_thread is called as a result of Thread.start, on a Java
// thread, the operation is synchronized on the Java Thread object.  So there
// cannot be a race to start the thread and hence for the thread to exit while
// we are working on it.  Non-Java threads that start Java threads either have
// to do so in a context in which races are impossible, or should do appropriate
// locking.

//---------------------------------------------------------------------------
// Helper functions for fatal error handler

void os::print_hex_dump(outputStream* st, address start, address end, int unitsize) {
  assert(unitsize == 1 || unitsize == 2 || unitsize == 4 || unitsize == 8, "just checking");

  int cols = 0;
  int cols_per_line = 0;
  switch (unitsize) {
    case 1: cols_per_line = 16; break;
    case 2: cols_per_line = 8;  break;
    case 4: cols_per_line = 4;  break;
    case 8: cols_per_line = 2;  break;
    default: return;
  }

  address p = start;
  st->print(PTR_FORMAT ":   ", start);
  while (p < end) {
    switch (unitsize) {
      case 1: st->print("%02x", *(u1*)p); break;
      case 2: st->print("%04x", *(u2*)p); break;
      case 4: st->print("%08x", *(u4*)p); break;
      case 8: st->print("%016" FORMAT64_MODIFIER "x", *(u8*)p); break;
    }
    p += unitsize;
    cols++;
    if (cols >= cols_per_line && p < end) {
       cols = 0;
       st->cr();
       st->print(PTR_FORMAT ":   ", p);
    } else {
       st->print(" ");
    }
  }
  st->cr();
}

void os::print_environment_variables(outputStream* st, const char** env_list,
                                     char* buffer, int len) {
  if (env_list) {
    st->print_cr("Environment Variables:");

    for (int i = 0; env_list[i] != NULL; i++) {
      if (getenv(env_list[i], buffer, len)) {
        st->print("%s", env_list[i]);
        st->print("=");
        st->print_cr("%s", buffer);
      }
    }
  }
}

void os::print_cpu_info(outputStream* st) {
  // cpu
  st->print("CPU:");
  st->print("total %d", os::processor_count());
  // It's not safe to query number of active processors after crash
  // st->print("(active %d)", os::active_processor_count()); but we can
  // print the initial number of active processors.
  // We access the raw value here because the assert in the accessor will
  // fail if the crash occurs before initialization of this value.
  st->print(" (initial active %d)", _initial_active_processor_count);
  st->print(" %s", VM_Version::cpu_features());
  st->cr();
  pd_print_cpu_info(st);
}

void os::print_date_and_time(outputStream *st) {
  const int secs_per_day  = 86400;
  const int secs_per_hour = 3600;
  const int secs_per_min  = 60;

  time_t tloc;
  (void)time(&tloc);
  st->print("time: %s", ctime(&tloc));  // ctime adds newline.

  double t = os::elapsedTime();
  // NOTE: It tends to crash after a SEGV if we want to printf("%f",...) in
  //       Linux. Must be a bug in glibc ? Workaround is to round "t" to int
  //       before printf. We lost some precision, but who cares?
  int eltime = (int)t;  // elapsed time in seconds

  // print elapsed time in a human-readable format:
  int eldays = eltime / secs_per_day;
  int day_secs = eldays * secs_per_day;
  int elhours = (eltime - day_secs) / secs_per_hour;
  int hour_secs = elhours * secs_per_hour;
  int elmins = (eltime - day_secs - hour_secs) / secs_per_min;
  int minute_secs = elmins * secs_per_min;
  int elsecs = (eltime - day_secs - hour_secs - minute_secs);
  st->print_cr("elapsed time: %d seconds (%dd %dh %dm %ds)", eltime, eldays, elhours, elmins, elsecs);
}

// moved from debug.cpp (used to be find()) but still called from there
// The verbose parameter is only set by the debug code in one case
void os::print_location(outputStream* st, intptr_t x, bool verbose) {
//
}

// Looks like all platforms except IA64 can use the same function to check
// if C stack is walkable beyond current frame. The check for fp() is not
// necessary on Sparc, but it's harmless.
bool os::is_first_C_frame(frame* fr) {
#if (defined(IA64) && !defined(AIX)) && !defined(_WIN32)
  // On IA64 we have to check if the callers bsp is still valid
  // (i.e. within the register stack bounds).
  // Notice: this only works for threads created by the VM and only if
  // we walk the current stack!!! If we want to be able to walk
  // arbitrary other threads, we'll have to somehow store the thread
  // object in the frame.
  Thread *thread = Thread::current();
  if ((address)fr->fp() <=
      thread->register_stack_base() HPUX_ONLY(+ 0x0) LINUX_ONLY(+ 0x50)) {
    // This check is a little hacky, because on Linux the first C
    // frame's ('start_thread') register stack frame starts at
    // "register_stack_base + 0x48" while on HPUX, the first C frame's
    // ('__pthread_bound_body') register stack frame seems to really
    // start at "register_stack_base".
    return true;
  } else {
    return false;
  }
#elif defined(IA64) && defined(_WIN32)
  return true;
#else
  // Load up sp, fp, sender sp and sender fp, check for reasonable values.
  // Check usp first, because if that's bad the other accessors may fault
  // on some architectures.  Ditto ufp second, etc.
  uintptr_t fp_align_mask = (uintptr_t)(sizeof(address)-1);
  // sp on amd can be 32 bit aligned.
  uintptr_t sp_align_mask = (uintptr_t)(sizeof(int)-1);

  uintptr_t usp    = (uintptr_t)fr->sp();
  if ((usp & sp_align_mask) != 0) return true;

  uintptr_t ufp    = (uintptr_t)fr->fp();
  if ((ufp & fp_align_mask) != 0) return true;

  uintptr_t old_sp = (uintptr_t)fr->sender_sp();
  if ((old_sp & sp_align_mask) != 0) return true;
  if (old_sp == 0 || old_sp == (uintptr_t)-1) return true;

  uintptr_t old_fp = (uintptr_t)fr->link();
  if ((old_fp & fp_align_mask) != 0) return true;
  if (old_fp == 0 || old_fp == (uintptr_t)-1 || old_fp == ufp) return true;

  // stack grows downwards; if old_fp is below current fp or if the stack
  // frame is too large, either the stack is corrupted or fp is not saved
  // on stack (i.e. on x86, ebp may be used as general register). The stack
  // is not walkable beyond current frame.
  if (old_fp < ufp) return true;
  if (old_fp - ufp > 64 * K) return true;

  return false;
#endif
}

#ifdef ASSERT
extern "C" void test_random() {
  const double m = 2147483647;
  double mean = 0.0, variance = 0.0, t;
  long reps = 10000;
  unsigned long seed = 1;

  tty->print_cr("seed %ld for %ld repeats...", seed, reps);
  os::init_random(seed);
  long num;
  for (int k = 0; k < reps; k++) {
    num = os::random();
    double u = (double)num / m;
    assert(u >= 0.0 && u <= 1.0, "bad random number!");

    // calculate mean and variance of the random sequence
    mean += u;
    variance += (u*u);
  }
  mean /= reps;
  variance /= (reps - 1);

  assert(num == 1043618065, "bad seed");
  tty->print_cr("mean of the 1st 10000 numbers: %f", mean);
  tty->print_cr("variance of the 1st 10000 numbers: %f", variance);
  const double eps = 0.0001;
  t = fabsd(mean - 0.5018);
  assert(t < eps, "bad mean");
  t = (variance - 0.3355) < 0.0 ? -(variance - 0.3355) : variance - 0.3355;
  assert(t < eps, "bad variance");
}
#endif


// Set up the boot classpath.

char* os::format_boot_path(const char* format_string,
                           const char* home,
                           int home_len,
                           char fileSep,
                           char pathSep) {
    assert((fileSep == '/' && pathSep == ':') ||
           (fileSep == '\\' && pathSep == ';'), "unexpected seperator chars");

    // Scan the format string to determine the length of the actual
    // boot classpath, and handle platform dependencies as well.
    int formatted_path_len = 0;
    const char* p;
    for (p = format_string; *p != 0; ++p) {
        if (*p == '%') formatted_path_len += home_len - 1;
        ++formatted_path_len;
    }

    char* formatted_path = NEW_C_HEAP_ARRAY(char, formatted_path_len + 1, mtInternal);
    if (formatted_path == NULL) {
        return NULL;
    }

    // Create boot classpath from format, substituting separator chars and
    // java home directory.
    char* q = formatted_path;
    for (p = format_string; *p != 0; ++p) {
        switch (*p) {
        case '%':
            strcpy(q, home);
            q += home_len;
            break;
        case '/':
            *q++ = fileSep;
            break;
        case ':':
            *q++ = pathSep;
            break;
        default:
            *q++ = *p;
        }
    }
    *q = '\0';

    assert((q - formatted_path) == formatted_path_len, "formatted_path size botched");
    return formatted_path;
}


bool os::set_boot_path(char fileSep, char pathSep) {
  D_WARN_Unimplement;
}

/*
 * Splits a path, based on its separator, the number of
 * elements is returned back in n.
 * It is the callers responsibility to:
 *   a> check the value of n, and n may be 0.
 *   b> ignore any empty path elements
 *   c> free up the data.
 */
char** os::split_path(const char* path, int* n) {
  *n = 0;
  if (path == NULL || strlen(path) == 0) {
    return NULL;
  }
  const char psepchar = *os::path_separator();
  char* inpath = (char*)NEW_C_HEAP_ARRAY(char, strlen(path) + 1, mtInternal);
  if (inpath == NULL) {
    return NULL;
  }
  strcpy(inpath, path);
  int count = 1;
  char* p = strchr(inpath, psepchar);
  // Get a count of elements to allocate memory
  while (p != NULL) {
    count++;
    p++;
    p = strchr(p, psepchar);
  }
  char** opath = (char**) NEW_C_HEAP_ARRAY(char*, count, mtInternal);
  if (opath == NULL) {
    return NULL;
  }

  // do the actual splitting
  p = inpath;
  for (int i = 0 ; i < count ; i++) {
    size_t len = strcspn(p, os::path_separator());
    if (len > JVM_MAXPATHLEN) {
      return NULL;
    }
    // allocate the string and add terminator storage
    char* s  = (char*)NEW_C_HEAP_ARRAY(char, len + 1, mtInternal);
    if (s == NULL) {
      return NULL;
    }
    strncpy(s, p, len);
    s[len] = '\0';
    opath[i] = s;
    p += len + 1;
  }
  FREE_C_HEAP_ARRAY(char, inpath, mtInternal);
  *n = count;
  return opath;
}

void os::set_memory_serialize_page(address page) {
  int count = log2_intptr(sizeof(class JavaThread)) - log2_intptr(64);
  _mem_serialize_page = (volatile int32_t *)page;
  // We initialize the serialization page shift count here
  // We assume a cache line size of 64 bytes
  assert(SerializePageShiftCount == count,
         "thread size changed, fix SerializePageShiftCount constant");
  set_serialize_page_mask((uintptr_t)(vm_page_size() - sizeof(int32_t)));
}

static volatile intptr_t SerializePageLock = 0;

// This method is called from signal handler when SIGSEGV occurs while the current
// thread tries to store to the "read-only" memory serialize page during state
// transition.
void os::block_on_serialize_page_trap() {
  D_WARN_Unimplement;
}

// Serialize all thread state variables
void os::serialize_thread_states() {
  D_WARN_Unimplement;
}

// Returns true if the current stack pointer is above the stack shadow
// pages, false otherwise.

bool os::stack_shadow_pages_available(Thread *thread, methodHandle method) {
  D_WARN_Unimplement;
}

size_t os::page_size_for_region(size_t region_size, size_t min_pages, bool must_be_aligned) {
  assert(min_pages > 0, "sanity");
  if (UseLargePages) {
    const size_t max_page_size = region_size / min_pages;

    for (size_t i = 0; _page_sizes[i] != 0; ++i) {
      const size_t page_size = _page_sizes[i];
      if (page_size <= max_page_size) {
        if (!must_be_aligned || is_size_aligned(region_size, page_size)) {
          return page_size;
        }
      }
    }
  }

  return vm_page_size();
}

size_t os::page_size_for_region_aligned(size_t region_size, size_t min_pages) {
  return page_size_for_region(region_size, min_pages, true);
}

size_t os::page_size_for_region_unaligned(size_t region_size, size_t min_pages) {
  return page_size_for_region(region_size, min_pages, false);
}

#ifndef PRODUCT
void os::trace_page_sizes(const char* str, const size_t* page_sizes, int count)
{
  if (TracePageSizes) {
    tty->print("%s: ", str);
    for (int i = 0; i < count; ++i) {
      tty->print(" " SIZE_FORMAT, page_sizes[i]);
    }
    tty->cr();
  }
}

void os::trace_page_sizes(const char* str, const size_t region_min_size,
                          const size_t region_max_size, const size_t page_size,
                          const char* base, const size_t size)
{
  if (TracePageSizes) {
    tty->print_cr("%s:  min=" SIZE_FORMAT " max=" SIZE_FORMAT
                  " pg_sz=" SIZE_FORMAT " base=" PTR_FORMAT
                  " size=" SIZE_FORMAT,
                  str, region_min_size, region_max_size,
                  page_size, base, size);
  }
}
#endif  // #ifndef PRODUCT

// This is the working definition of a server class machine:
// >= 2 physical CPU's and >=2GB of memory, with some fuzz
// because the graphics memory (?) sometimes masks physical memory.
// If you want to change the definition of a server class machine
// on some OS or platform, e.g., >=4GB on Windohs platforms,
// then you'll have to parameterize this method based on that state,
// as was done for logical processors here, or replicate and
// specialize this method for each platform.  (Or fix os to have
// some inheritance structure and use subclassing.  Sigh.)
// If you want some platform to always or never behave as a server
// class machine, change the setting of AlwaysActAsServerClassMachine
// and NeverActAsServerClassMachine in globals*.hpp.
bool os::is_server_class_machine() {
  D_WARN_Unimplement;
}

void os::initialize_initial_active_processor_count() {
  assert(_initial_active_processor_count == 0, "Initial active processor count already set.");
  _initial_active_processor_count = active_processor_count();
}

void os::SuspendedThreadTask::run() {
  assert(Threads_lock->owned_by_self() || (_thread == VMThread::vm_thread()), "must have threads lock to call this");
  internal_do_task();
  _done = true;
}

bool os::create_stack_guard_pages(char* addr, size_t bytes) {
  return os::pd_create_stack_guard_pages(addr, bytes);
}

char* os::reserve_memory(size_t bytes, char* addr, size_t alignment_hint) {
  D_WARN_Unimplement;
}

char* os::reserve_memory(size_t bytes, char* addr, size_t alignment_hint,
   MEMFLAGS flags) {
  D_WARN_Unimplement;
}

char* os::attempt_reserve_memory_at(size_t bytes, char* addr) {
  D_WARN_Unimplement;
}

void os::split_reserved_memory(char *base, size_t size,
                                 size_t split, bool realloc) {
  pd_split_reserved_memory(base, size, split, realloc);
}

bool os::commit_memory(char* addr, size_t bytes, bool executable) {
  D_WARN_Unimplement;
}

bool os::commit_memory(char* addr, size_t size, size_t alignment_hint,
                              bool executable) {
  D_WARN_Unimplement;
}

void os::commit_memory_or_exit(char* addr, size_t bytes, bool executable,
                               const char* mesg) {
  D_WARN_Unimplement;
}

void os::commit_memory_or_exit(char* addr, size_t size, size_t alignment_hint,
                               bool executable, const char* mesg) {
  D_WARN_Unimplement;
}

bool os::uncommit_memory(char* addr, size_t bytes) {
  D_WARN_Unimplement;
}

bool os::release_memory(char* addr, size_t bytes) {
  D_WARN_Unimplement;
}

void os::pretouch_memory(char* start, char* end) {
  for (volatile char *p = start; p < end; p += os::vm_page_size()) {
    *p = 0;
  }
}

char* os::map_memory(int fd, const char* file_name, size_t file_offset,
                           char *addr, size_t bytes, bool read_only,
                           bool allow_exec) {
  D_WARN_Unimplement;
}

char* os::remap_memory(int fd, const char* file_name, size_t file_offset,
                             char *addr, size_t bytes, bool read_only,
                             bool allow_exec) {
  return pd_remap_memory(fd, file_name, file_offset, addr, bytes,
                    read_only, allow_exec);
}

bool os::unmap_memory(char *addr, size_t bytes) {
  D_WARN_Unimplement;
}

void os::free_memory(char *addr, size_t bytes, size_t alignment_hint) {
  pd_free_memory(addr, bytes, alignment_hint);
}

void os::realign_memory(char *addr, size_t bytes, size_t alignment_hint) {
  pd_realign_memory(addr, bytes, alignment_hint);
}
