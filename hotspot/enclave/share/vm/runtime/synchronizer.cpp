/*
 * Copyright (c) 1998, 2014, Oracle and/or its affiliates. All rights reserved.
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

#include "synchronizer.hpp"
#include "precompiled.hpp"
#include "classfile/vmSymbols.hpp"
#include "memory/resourceArea.hpp"
#include "oops/markOop.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/synchronizer.hpp"

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

#if defined(__GNUC__) && !defined(PPC64)
  // Need to inhibit inlining for older versions of GCC to avoid build-time failures
  #define ATTR __attribute__((noinline))
#else
  #define ATTR
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// The "core" versions of monitor enter and exit reside in this file.
// The interpreter and compilers contain specialized transliterated
// variants of the enter-exit fast-path operations.  See i486.ad fast_lock(),
// for instance.  If you make changes here, make sure to modify the
// interpreter, and both C1 and C2 fast-path inline locking code emission.
//
//
// -----------------------------------------------------------------------------

#ifdef DTRACE_ENABLED

// Only bother with this argument setup if dtrace is available
// TODO-FIXME: probes should not fire when caller is _blocked.  assert() accordingly.

#define DTRACE_MONITOR_PROBE_COMMON(obj, thread)                           \
  char* bytes = NULL;                                                      \
  int len = 0;                                                             \
  jlong jtid = SharedRuntime::get_java_tid(thread);                        \
  Symbol* klassname = ((oop)(obj))->klass()->name();                       \
  if (klassname != NULL) {                                                 \
    bytes = (char*)klassname->bytes();                                     \
    len = klassname->utf8_length();                                        \
  }

#ifndef USDT2
HS_DTRACE_PROBE_DECL5(hotspot, monitor__wait,
  jlong, uintptr_t, char*, int, long);
HS_DTRACE_PROBE_DECL4(hotspot, monitor__waited,
  jlong, uintptr_t, char*, int);

#define DTRACE_MONITOR_WAIT_PROBE(monitor, obj, thread, millis)            \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HS_DTRACE_PROBE5(hotspot, monitor__wait, jtid,                       \
                       (monitor), bytes, len, (millis));                   \
    }                                                                      \
  }

#define DTRACE_MONITOR_PROBE(probe, monitor, obj, thread)                  \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HS_DTRACE_PROBE4(hotspot, monitor__##probe, jtid,                    \
                       (uintptr_t)(monitor), bytes, len);                  \
    }                                                                      \
  }

#else /* USDT2 */

#define DTRACE_MONITOR_WAIT_PROBE(monitor, obj, thread, millis)            \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HOTSPOT_MONITOR_WAIT(jtid,                                           \
                           (uintptr_t)(monitor), bytes, len, (millis));  \
    }                                                                      \
  }

#define HOTSPOT_MONITOR_PROBE_waited HOTSPOT_MONITOR_WAITED

#define DTRACE_MONITOR_PROBE(probe, monitor, obj, thread)                  \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HOTSPOT_MONITOR_PROBE_##probe(jtid, /* probe = waited */             \
                       (uintptr_t)(monitor), bytes, len);                  \
    }                                                                      \
  }

#endif /* USDT2 */
#else //  ndef DTRACE_ENABLED

#define DTRACE_MONITOR_WAIT_PROBE(obj, thread, millis, mon)    {;}
#define DTRACE_MONITOR_PROBE(probe, obj, thread, mon)          {;}

#endif // ndef DTRACE_ENABLED

// This exists only as a workaround of dtrace bug 6254741
int dtrace_waited_probe(ObjectMonitor* monitor, Handle obj, Thread* thr) {
  DTRACE_MONITOR_PROBE(waited, monitor, obj(), thr);
  return 0;
}

#define NINFLATIONLOCKS 256
static volatile intptr_t InflationLocks [NINFLATIONLOCKS] ;

ObjectMonitor * ObjectSynchronizer::gBlockList = NULL ;
ObjectMonitor * volatile ObjectSynchronizer::gFreeList  = NULL ;
ObjectMonitor * volatile ObjectSynchronizer::gOmInUseList  = NULL ;
int ObjectSynchronizer::gOmInUseCount = 0;
static volatile intptr_t ListLock = 0 ;      // protects global monitor free-list cache
static volatile int MonitorFreeCount  = 0 ;      // # on gFreeList
static volatile int MonitorPopulation = 0 ;      // # Extant -- in circulation
#define CHAINMARKER (cast_to_oop<intptr_t>(-1))

// -----------------------------------------------------------------------------
//  Fast Monitor Enter/Exit
// This the fast monitor enter. The interpreter and compiler use
// some assembly copies of this code. Make sure update those code
// if the following function is changed. The implementation is
// extremely sensitive to race condition. Be careful.

void ObjectSynchronizer::fast_enter(Handle obj, BasicLock* lock, bool attempt_rebias, TRAPS) {
 slow_enter (obj, lock, THREAD) ;
}

void ObjectSynchronizer::fast_exit(oop object, BasicLock* lock, TRAPS) {
  D_WARN_Unimplement;
}

// -----------------------------------------------------------------------------
// Interpreter/Compiler Slow Case
// This routine is used to handle interpreter/compiler slow case
// We don't need to use fast path here, because it must have been
// failed in the interpreter/compiler code.
void ObjectSynchronizer::slow_enter(Handle obj, BasicLock* lock, TRAPS) {
D_WARN_Unimplement;
}

// This routine is used to handle interpreter/compiler slow case
// We don't need to use fast path here, because it must have
// failed in the interpreter/compiler code. Simply use the heavy
// weight monitor should be ok, unless someone find otherwise.
void ObjectSynchronizer::slow_exit(oop object, BasicLock* lock, TRAPS) {
  fast_exit (object, lock, THREAD) ;
}

// -----------------------------------------------------------------------------
// Class Loader  support to workaround deadlocks on the class loader lock objects
// Also used by GC
// complete_exit()/reenter() are used to wait on a nested lock
// i.e. to give up an outer lock completely and then re-enter
// Used when holding nested locks - lock acquisition order: lock1 then lock2
//  1) complete_exit lock1 - saving recursion count
//  2) wait on lock2
//  3) when notified on lock2, unlock lock2
//  4) reenter lock1 with original recursion count
//  5) lock lock2
// NOTE: must use heavy weight monitor to handle complete_exit/reenter()
intptr_t ObjectSynchronizer::complete_exit(Handle obj, TRAPS) {
  D_WARN_Unimplement;
}

// NOTE: must use heavy weight monitor to handle complete_exit/reenter()
void ObjectSynchronizer::reenter(Handle obj, intptr_t recursion, TRAPS) {
D_WARN_Unimplement;
}
// -----------------------------------------------------------------------------
// JNI locks on java objects
// NOTE: must use heavy weight monitor to handle jni monitor enter
void ObjectSynchronizer::jni_enter(Handle obj, TRAPS) { // possible entry from jni enter
  D_WARN_Unimplement;
}

// NOTE: must use heavy weight monitor to handle jni monitor enter
bool ObjectSynchronizer::jni_try_enter(Handle obj, Thread* THREAD) {
  D_WARN_Unimplement;
}


// NOTE: must use heavy weight monitor to handle jni monitor exit
void ObjectSynchronizer::jni_exit(oop obj, Thread* THREAD) {
  D_WARN_Unimplement;
}

// -----------------------------------------------------------------------------
// Internal VM locks on java objects
// standard constructor, allows locking failures
ObjectLocker::ObjectLocker(Handle obj, Thread* thread, bool doLock) {
  D_WARN_Unimplement;
}

ObjectLocker::~ObjectLocker() {
    D_WARN_Unimplement;
}


// -----------------------------------------------------------------------------
//  Wait/Notify/NotifyAll
// NOTE: must use heavy weight monitor to handle wait()
void ObjectSynchronizer::wait(Handle obj, jlong millis, TRAPS) {
  D_WARN_Unimplement;
}

void ObjectSynchronizer::waitUninterruptibly (Handle obj, jlong millis, TRAPS) {
  D_WARN_Unimplement;
}

void ObjectSynchronizer::notify(Handle obj, TRAPS) {
  D_WARN_Unimplement;
}

// NOTE: see comment of notify()
void ObjectSynchronizer::notifyall(Handle obj, TRAPS) {
  D_WARN_Unimplement;
}

// -----------------------------------------------------------------------------
// Hash Code handling
//
// Performance concern:
// OrderAccess::storestore() calls release() which at one time stored 0
// into the global volatile OrderAccess::dummy variable. This store was
// unnecessary for correctness. Many threads storing into a common location
// causes considerable cache migration or "sloshing" on large SMP systems.
// As such, I avoided using OrderAccess::storestore(). In some cases
// OrderAccess::fence() -- which incurs local latency on the executing
// processor -- is a better choice as it scales on SMP systems.
//
// See http://blogs.oracle.com/dave/entry/biased_locking_in_hotspot for
// a discussion of coherency costs. Note that all our current reference
// platforms provide strong ST-ST order, so the issue is moot on IA32,
// x64, and SPARC.
//
// As a general policy we use "volatile" to control compiler-based reordering
// and explicit fences (barriers) to control for architectural reordering
// performed by the CPU(s) or platform.

struct SharedGlobals {
    // These are highly shared mostly-read variables.
    // To avoid false-sharing they need to be the sole occupants of a $ line.
    double padPrefix [8];
    volatile int stwRandom ;
    volatile int stwCycle ;

    // Hot RW variables -- Sequester to avoid false-sharing
    double padSuffix [16];
    volatile int hcSequence ;
    double padFinal [8] ;
} ;

static SharedGlobals GVars ;
static int MonitorScavengeThreshold = 1000000 ;
static volatile int ForceMonitorScavenge = 0 ; // Scavenge required and pending

static markOop ReadStableMark (oop obj) {
  D_WARN_Unimplement;
}

// hashCode() generation :
//
// Possibilities:
// * MD5Digest of {obj,stwRandom}
// * CRC32 of {obj,stwRandom} or any linear-feedback shift register function.
// * A DES- or AES-style SBox[] mechanism
// * One of the Phi-based schemes, such as:
//   2654435761 = 2^32 * Phi (golden ratio)
//   HashCodeValue = ((uintptr_t(obj) >> 3) * 2654435761) ^ GVars.stwRandom ;
// * A variation of Marsaglia's shift-xor RNG scheme.
// * (obj ^ stwRandom) is appealing, but can result
//   in undesirable regularity in the hashCode values of adjacent objects
//   (objects allocated back-to-back, in particular).  This could potentially
//   result in hashtable collisions and reduced hashtable efficiency.
//   There are simple ways to "diffuse" the middle address bits over the
//   generated hashCode values:
//

static inline intptr_t get_next_hash(Thread * Self, oop obj) {
//  printf(D_INFO("HashCode")" generatating hashcode using %d\n", hashCode);
  intptr_t value = 0 ;
  if (hashCode == 0) {
     // This form uses an unguarded global Park-Miller RNG,
     // so it's possible for two threads to race and generate the same RNG.
     // On MP system we'll have lots of RW access to a global, so the
     // mechanism induces lots of coherency traffic.
     value = os::random() ;
  } else
  if (hashCode == 1) {
     // This variation has the property of being stable (idempotent)
     // between STW operations.  This can be useful in some of the 1-0
     // synchronization schemes.
     intptr_t addrBits = cast_from_oop<intptr_t>(obj) >> 3 ;
     value = addrBits ^ (addrBits >> 5) ^ GVars.stwRandom ;
  } else
  if (hashCode == 2) {
     value = 1 ;            // for sensitivity testing
  } else
  if (hashCode == 3) {
     value = ++GVars.hcSequence ;
  } else
  if (hashCode == 4) {
     value = cast_from_oop<intptr_t>(obj) ;
  } else {
     // Marsaglia's xor-shift scheme with thread-specific state
     // This is probably the best overall implementation -- we'll
     // likely make this the default in future releases.
     ShouldNotReachHere();
  }

  value &= markOopDesc::hash_mask;
  if (value == 0) value = 0xBAD ;
  assert (value != markOopDesc::no_hash, "invariant") ;
  return value;
}
//
intptr_t ObjectSynchronizer::FastHashCode (Thread * Self, oop obj) {
    intptr_t hash;
    markOop temp, test;
    markOop mark = obj->mark();
    if (!mark->is_neutral()) {
//        printf(D_ERROR("Native") "multi-threading %s:%d\n", __FUNCTION__, __LINE__);
    }
    hash = mark->hash();              // this is a normal header
    if (hash) {                       // if it has hash, just return it
//        printf(D_INFO("Native") "hash %lx %s:%d\n", hash, __FUNCTION__, __LINE__);
        return hash;
    }
    hash = get_next_hash(Self, obj);  // allocate a new hash code
    temp = mark->copy_set_hash(hash); // merge the hash code into header
    // use (machine word version) atomic operation to install the hash
    test = (markOop) Atomic::cmpxchg_ptr(temp, obj->mark_addr(), mark);
    if (test == mark) {
//        printf(D_INFO("Native") "hash %lx %s:%d\n", hash, __FUNCTION__, __LINE__);
        return hash;
    } else {
//        printf(D_ERROR("Native") "multi-threading %s:%d\n", __FUNCTION__, __LINE__);
        return hash;
    }
    ShouldNotReachHere();
}

// Deprecated -- use FastHashCode() instead.

intptr_t ObjectSynchronizer::identity_hash_value_for(Handle obj) {
  return FastHashCode (Thread::current(), obj()) ;
}


bool ObjectSynchronizer::current_thread_holds_lock(JavaThread* thread,
                                                   Handle h_obj) {
  D_WARN_Unimplement;
}

// Be aware of this method could revoke bias of the lock object.
// This method querys the ownership of the lock handle specified by 'h_obj'.
// If the current thread owns the lock, it returns owner_self. If no
// thread owns the lock, it returns owner_none. Otherwise, it will return
// ower_other.
ObjectSynchronizer::LockOwnership ObjectSynchronizer::query_lock_ownership
(JavaThread *self, Handle h_obj) {
  D_WARN_Unimplement;
}

// FIXME: jvmti should call this
JavaThread* ObjectSynchronizer::get_lock_owner(Handle h_obj, bool doLock) {
 D_WARN_Unimplement;
}
// Visitors ...

void ObjectSynchronizer::monitors_iterate(MonitorClosure* closure) {
  D_WARN_Unimplement;
}

// Get the next block in the block list.
static inline ObjectMonitor* next(ObjectMonitor* block) {
  D_WARN_Unimplement;
}


void ObjectSynchronizer::oops_do(OopClosure* f) {
  D_WARN_Unimplement;
}


// -----------------------------------------------------------------------------
// ObjectMonitor Lifecycle
// -----------------------
// Inflation unlinks monitors from the global gFreeList and
// associates them with objects.  Deflation -- which occurs at
// STW-time -- disassociates idle monitors from objects.  Such
// scavenged monitors are returned to the gFreeList.
//
// The global list is protected by ListLock.  All the critical sections
// are short and operate in constant-time.
//
// ObjectMonitors reside in type-stable memory (TSM) and are immortal.
//
// Lifecycle:
// --   unassigned and on the global free list
// --   unassigned and on a thread's private omFreeList
// --   assigned to an object.  The object is inflated and the mark refers
//      to the objectmonitor.
//


// Constraining monitor pool growth via MonitorBound ...
//
// The monitor pool is grow-only.  We scavenge at STW safepoint-time, but the
// the rate of scavenging is driven primarily by GC.  As such,  we can find
// an inordinate number of monitors in circulation.
// To avoid that scenario we can artificially induce a STW safepoint
// if the pool appears to be growing past some reasonable bound.
// Generally we favor time in space-time tradeoffs, but as there's no
// natural back-pressure on the # of extant monitors we need to impose some
// type of limit.  Beware that if MonitorBound is set to too low a value
// we could just loop. In addition, if MonitorBound is set to a low value
// we'll incur more safepoints, which are harmful to performance.
// See also: GuaranteedSafepointInterval
//
// The current implementation uses asynchronous VM operations.
//

static void InduceScavenge (Thread * Self, const char * Whence) {
  D_WARN_Unimplement;
}
/* Too slow for general assert or debug
void ObjectSynchronizer::verifyInUse (Thread *Self) {
   ObjectMonitor* mid;
   int inusetally = 0;
   for (mid = Self->omInUseList; mid != NULL; mid = mid->FreeNext) {
     inusetally ++;
   }
   assert(inusetally == Self->omInUseCount, "inuse count off");

   int freetally = 0;
   for (mid = Self->omFreeList; mid != NULL; mid = mid->FreeNext) {
     freetally ++;
   }
   assert(freetally == Self->omFreeCount, "free count off");
}
*/
ObjectMonitor * ATTR ObjectSynchronizer::omAlloc (Thread * Self) {
  D_WARN_Unimplement;
}

// Place "m" on the caller's private per-thread omFreeList.
// In practice there's no need to clamp or limit the number of
// monitors on a thread's omFreeList as the only time we'll call
// omRelease is to return a monitor to the free list after a CAS
// attempt failed.  This doesn't allow unbounded #s of monitors to
// accumulate on a thread's free list.
//

void ObjectSynchronizer::omRelease (Thread * Self, ObjectMonitor * m, bool fromPerThreadAlloc) {
  D_WARN_Unimplement;
}

// Return the monitors of a moribund thread's local free list to
// the global free list.  Typically a thread calls omFlush() when
// it's dying.  We could also consider having the VM thread steal
// monitors from threads that have not run java code over a few
// consecutive STW safepoints.  Relatedly, we might decay
// omFreeProvision at STW safepoints.
//
// Also return the monitors of a moribund thread"s omInUseList to
// a global gOmInUseList under the global list lock so these
// will continue to be scanned.
//
// We currently call omFlush() from the Thread:: dtor _after the thread
// has been excised from the thread list and is no longer a mutator.
// That means that omFlush() can run concurrently with a safepoint and
// the scavenge operator.  Calling omFlush() from JavaThread::exit() might
// be a better choice as we could safely reason that that the JVM is
// not at a safepoint at the time of the call, and thus there could
// be not inopportune interleavings between omFlush() and the scavenge
// operator.

void ObjectSynchronizer::omFlush (Thread * Self) {
  D_WARN_Unimplement;
}

// Fast path code shared by multiple functions
ObjectMonitor* ObjectSynchronizer::inflate_helper(oop obj) {
  markOop mark = obj->mark();
  if (mark->has_monitor()) {
    assert(ObjectSynchronizer::verify_objmon_isinpool(mark->monitor()), "monitor is invalid");
    assert(mark->monitor()->header()->is_neutral(), "monitor must record a good object header");
    return mark->monitor();
  }
  return ObjectSynchronizer::inflate(Thread::current(), obj);
}


// Note that we could encounter some performance loss through false-sharing as
// multiple locks occupy the same $ line.  Padding might be appropriate.


ObjectMonitor * ATTR ObjectSynchronizer::inflate (Thread * Self, oop object) {
  D_WARN_Unimplement;
}

// Note that we could encounter some performance loss through false-sharing as
// multiple locks occupy the same $ line.  Padding might be appropriate.


// Deflate_idle_monitors() is called at all safepoints, immediately
// after all mutators are stopped, but before any objects have moved.
// It traverses the list of known monitors, deflating where possible.
// The scavenged monitor are returned to the monitor free list.
//
// Beware that we scavenge at *every* stop-the-world point.
// Having a large number of monitors in-circulation negatively
// impacts the performance of some applications (e.g., PointBase).
// Broadly, we want to minimize the # of monitors in circulation.
//
// We have added a flag, MonitorInUseLists, which creates a list
// of active monitors for each thread. deflate_idle_monitors()
// only scans the per-thread inuse lists. omAlloc() puts all
// assigned monitors on the per-thread list. deflate_idle_monitors()
// returns the non-busy monitors to the global free list.
// When a thread dies, omFlush() adds the list of active monitors for
// that thread to a global gOmInUseList acquiring the
// global list lock. deflate_idle_monitors() acquires the global
// list lock to scan for non-busy monitors to the global free list.
// An alternative could have used a single global inuse list. The
// downside would have been the additional cost of acquiring the global list lock
// for every omAlloc().
//
// Perversely, the heap size -- and thus the STW safepoint rate --
// typically drives the scavenge rate.  Large heaps can mean infrequent GC,
// which in turn can mean large(r) numbers of objectmonitors in circulation.
// This is an unfortunate aspect of this design.
//

enum ManifestConstants {
    ClearResponsibleAtSTW   = 0,
    MaximumRecheckInterval  = 1000
} ;

// Deflate a single monitor if not in use
// Return true if deflated, false if in use
bool ObjectSynchronizer::deflate_monitor(ObjectMonitor* mid, oop obj,
                                         ObjectMonitor** FreeHeadp, ObjectMonitor** FreeTailp) {
  D_WARN_Unimplement;
}

// Caller acquires ListLock
int ObjectSynchronizer::walk_monitor_list(ObjectMonitor** listheadp,
                                          ObjectMonitor** FreeHeadp, ObjectMonitor** FreeTailp) {
  D_WARN_Unimplement;
}

void ObjectSynchronizer::deflate_idle_monitors() {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at safepoint");
  D_WARN_Unimplement;
}

// Monitor cleanup on JavaThread::exit

// Iterate through monitor cache and attempt to release thread's monitors
// Gives up on a particular monitor if an exception occurs, but continues
// the overall iteration, swallowing the exception.
class ReleaseJavaMonitorsClosure: public MonitorClosure {
private:
  TRAPS;

public:
  ReleaseJavaMonitorsClosure(Thread* thread) : THREAD(thread) {}
  void do_monitor(ObjectMonitor* mid) {
    D_WARN_Unimplement;
  }
};

// Release all inflated monitors owned by THREAD.  Lightweight monitors are
// ignored.  This is meant to be called during JNI thread detach which assumes
// all remaining monitors are heavyweight.  All exceptions are swallowed.
// Scanning the extant monitor list can be time consuming.
// A simple optimization is to add a per-thread flag that indicates a thread
// called jni_monitorenter() during its lifetime.
//
// Instead of No_Savepoint_Verifier it might be cheaper to
// use an idiom of the form:
//   auto int tmp = SafepointSynchronize::_safepoint_counter ;
//   <code that must not run at safepoint>
//   guarantee (((tmp ^ _safepoint_counter) | (tmp & 1)) == 0) ;
// Since the tests are extremely cheap we could leave them enabled
// for normal product builds.

void ObjectSynchronizer::release_monitors_owned_by_thread(TRAPS) {
  D_WARN_Unimplement;
}

//------------------------------------------------------------------------------
// Debugging code

void ObjectSynchronizer::sanity_checks(const bool verbose,
                                       const uint cache_line_size,
                                       int *error_cnt_ptr,
                                       int *warning_cnt_ptr) {
  u_char *addr_begin      = (u_char*)&GVars;
  u_char *addr_stwRandom  = (u_char*)&GVars.stwRandom;
  u_char *addr_hcSequence = (u_char*)&GVars.hcSequence;

  if (verbose) {
    tty->print_cr("INFO: sizeof(SharedGlobals)=" SIZE_FORMAT,
                  sizeof(SharedGlobals));
  }

  uint offset_stwRandom = (uint)(addr_stwRandom - addr_begin);
  if (verbose) tty->print_cr("INFO: offset(stwRandom)=%u", offset_stwRandom);

  uint offset_hcSequence = (uint)(addr_hcSequence - addr_begin);
  if (verbose) {
    tty->print_cr("INFO: offset(_hcSequence)=%u", offset_hcSequence);
  }

  if (cache_line_size != 0) {
    // We were able to determine the L1 data cache line size so
    // do some cache line specific sanity checks

    if (offset_stwRandom < cache_line_size) {
      tty->print_cr("WARNING: the SharedGlobals.stwRandom field is closer "
                    "to the struct beginning than a cache line which permits "
                    "false sharing.");
      (*warning_cnt_ptr)++;
    }

    if ((offset_hcSequence - offset_stwRandom) < cache_line_size) {
      tty->print_cr("WARNING: the SharedGlobals.stwRandom and "
                    "SharedGlobals.hcSequence fields are closer than a cache "
                    "line which permits false sharing.");
      (*warning_cnt_ptr)++;
    }

    if ((sizeof(SharedGlobals) - offset_hcSequence) < cache_line_size) {
      tty->print_cr("WARNING: the SharedGlobals.hcSequence field is closer "
                    "to the struct end than a cache line which permits false "
                    "sharing.");
      (*warning_cnt_ptr)++;
    }
  }
}

#ifndef PRODUCT

// Verify all monitors in the monitor cache, the verification is weak.
void ObjectSynchronizer::verify() {
  D_WARN_Unimplement;
}

// Check if monitor belongs to the monitor cache
// The list is grow-only so it's *relatively* safe to traverse
// the list of extant blocks without taking a lock.

int ObjectSynchronizer::verify_objmon_isinpool(ObjectMonitor *monitor) {
  D_WARN_Unimplement;
}

#endif
