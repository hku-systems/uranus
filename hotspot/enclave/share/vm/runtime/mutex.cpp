
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

#include "precompiled.hpp"
#include "runtime/mutex.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/thread.inline.hpp"
#ifdef TARGET_OS_FAMILY_linux
//# include "mutex_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "mutex_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "mutex_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "mutex_bsd.inline.hpp"
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o
//
// Native Monitor-Mutex locking - theory of operations
//
// * Native Monitors are completely unrelated to Java-level monitors,
//   although the "back-end" slow-path implementations share a common lineage.
//   See objectMonitor:: in synchronizer.cpp.
//   Native Monitors do *not* support nesting or recursion but otherwise
//   they're basically Hoare-flavor monitors.
//
// * A thread acquires ownership of a Monitor/Mutex by CASing the LockByte
//   in the _LockWord from zero to non-zero.  Note that the _Owner field
//   is advisory and is used only to verify that the thread calling unlock()
//   is indeed the last thread to have acquired the lock.
//
// * Contending threads "push" themselves onto the front of the contention
//   queue -- called the cxq -- with CAS and then spin/park.
//   The _LockWord contains the LockByte as well as the pointer to the head
//   of the cxq.  Colocating the LockByte with the cxq precludes certain races.
//
// * Using a separately addressable LockByte allows for CAS:MEMBAR or CAS:0
//   idioms.  We currently use MEMBAR in the uncontended unlock() path, as
//   MEMBAR often has less latency than CAS.  If warranted, we could switch to
//   a CAS:0 mode, using timers to close the resultant race, as is done
//   with Java Monitors in synchronizer.cpp.
//
//   See the following for a discussion of the relative cost of atomics (CAS)
//   MEMBAR, and ways to eliminate such instructions from the common-case paths:
//   -- http://blogs.sun.com/dave/entry/biased_locking_in_hotspot
//   -- http://blogs.sun.com/dave/resource/MustangSync.pdf
//   -- http://blogs.sun.com/dave/resource/synchronization-public2.pdf
//   -- synchronizer.cpp
//
// * Overall goals - desiderata
//   1. Minimize context switching
//   2. Minimize lock migration
//   3. Minimize CPI -- affinity and locality
//   4. Minimize the execution of high-latency instructions such as CAS or MEMBAR
//   5. Minimize outer lock hold times
//   6. Behave gracefully on a loaded system
//
// * Thread flow and list residency:
//
//   Contention queue --> EntryList --> OnDeck --> Owner --> !Owner
//   [..resident on monitor list..]
//   [...........contending..................]
//
//   -- The contention queue (cxq) contains recently-arrived threads (RATs).
//      Threads on the cxq eventually drain into the EntryList.
//   -- Invariant: a thread appears on at most one list -- cxq, EntryList
//      or WaitSet -- at any one time.
//   -- For a given monitor there can be at most one "OnDeck" thread at any
//      given time but if needbe this particular invariant could be relaxed.
//
// * The WaitSet and EntryList linked lists are composed of ParkEvents.
//   I use ParkEvent instead of threads as ParkEvents are immortal and
//   type-stable, meaning we can safely unpark() a possibly stale
//   list element in the unlock()-path.  (That's benign).
//
// * Succession policy - providing for progress:
//
//   As necessary, the unlock()ing thread identifies, unlinks, and unparks
//   an "heir presumptive" tentative successor thread from the EntryList.
//   This becomes the so-called "OnDeck" thread, of which there can be only
//   one at any given time for a given monitor.  The wakee will recontend
//   for ownership of monitor.
//
//   Succession is provided for by a policy of competitive handoff.
//   The exiting thread does _not_ grant or pass ownership to the
//   successor thread.  (This is also referred to as "handoff" succession").
//   Instead the exiting thread releases ownership and possibly wakes
//   a successor, so the successor can (re)compete for ownership of the lock.
//
//   Competitive handoff provides excellent overall throughput at the expense
//   of short-term fairness.  If fairness is a concern then one remedy might
//   be to add an AcquireCounter field to the monitor.  After a thread acquires
//   the lock it will decrement the AcquireCounter field.  When the count
//   reaches 0 the thread would reset the AcquireCounter variable, abdicate
//   the lock directly to some thread on the EntryList, and then move itself to the
//   tail of the EntryList.
//
//   But in practice most threads engage or otherwise participate in resource
//   bounded producer-consumer relationships, so lock domination is not usually
//   a practical concern.  Recall too, that in general it's easier to construct
//   a fair lock from a fast lock, but not vice-versa.
//
// * The cxq can have multiple concurrent "pushers" but only one concurrent
//   detaching thread.  This mechanism is immune from the ABA corruption.
//   More precisely, the CAS-based "push" onto cxq is ABA-oblivious.
//   We use OnDeck as a pseudo-lock to enforce the at-most-one detaching
//   thread constraint.
//
// * Taken together, the cxq and the EntryList constitute or form a
//   single logical queue of threads stalled trying to acquire the lock.
//   We use two distinct lists to reduce heat on the list ends.
//   Threads in lock() enqueue onto cxq while threads in unlock() will
//   dequeue from the EntryList.  (c.f. Michael Scott's "2Q" algorithm).
//   A key desideratum is to minimize queue & monitor metadata manipulation
//   that occurs while holding the "outer" monitor lock -- that is, we want to
//   minimize monitor lock holds times.
//
//   The EntryList is ordered by the prevailing queue discipline and
//   can be organized in any convenient fashion, such as a doubly-linked list or
//   a circular doubly-linked list.  If we need a priority queue then something akin
//   to Solaris' sleepq would work nicely.  Viz.,
//   -- http://agg.eng/ws/on10_nightly/source/usr/src/uts/common/os/sleepq.c.
//   -- http://cvs.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/uts/common/os/sleepq.c
//   Queue discipline is enforced at ::unlock() time, when the unlocking thread
//   drains the cxq into the EntryList, and orders or reorders the threads on the
//   EntryList accordingly.
//
//   Barring "lock barging", this mechanism provides fair cyclic ordering,
//   somewhat similar to an elevator-scan.
//
// * OnDeck
//   --  For a given monitor there can be at most one OnDeck thread at any given
//       instant.  The OnDeck thread is contending for the lock, but has been
//       unlinked from the EntryList and cxq by some previous unlock() operations.
//       Once a thread has been designated the OnDeck thread it will remain so
//       until it manages to acquire the lock -- being OnDeck is a stable property.
//   --  Threads on the EntryList or cxq are _not allowed to attempt lock acquisition.
//   --  OnDeck also serves as an "inner lock" as follows.  Threads in unlock() will, after
//       having cleared the LockByte and dropped the outer lock,  attempt to "trylock"
//       OnDeck by CASing the field from null to non-null.  If successful, that thread
//       is then responsible for progress and succession and can use CAS to detach and
//       drain the cxq into the EntryList.  By convention, only this thread, the holder of
//       the OnDeck inner lock, can manipulate the EntryList or detach and drain the
//       RATs on the cxq into the EntryList.  This avoids ABA corruption on the cxq as
//       we allow multiple concurrent "push" operations but restrict detach concurrency
//       to at most one thread.  Having selected and detached a successor, the thread then
//       changes the OnDeck to refer to that successor, and then unparks the successor.
//       That successor will eventually acquire the lock and clear OnDeck.  Beware
//       that the OnDeck usage as a lock is asymmetric.  A thread in unlock() transiently
//       "acquires" OnDeck, performs queue manipulations, passes OnDeck to some successor,
//       and then the successor eventually "drops" OnDeck.  Note that there's never
//       any sense of contention on the inner lock, however.  Threads never contend
//       or wait for the inner lock.
//   --  OnDeck provides for futile wakeup throttling a described in section 3.3 of
//       See http://www.usenix.org/events/jvm01/full_papers/dice/dice.pdf
//       In a sense, OnDeck subsumes the ObjectMonitor _Succ and ObjectWaiter
//       TState fields found in Java-level objectMonitors.  (See synchronizer.cpp).
//
// * Waiting threads reside on the WaitSet list -- wait() puts
//   the caller onto the WaitSet.  Notify() or notifyAll() simply
//   transfers threads from the WaitSet to either the EntryList or cxq.
//   Subsequent unlock() operations will eventually unpark the notifyee.
//   Unparking a notifee in notify() proper is inefficient - if we were to do so
//   it's likely the notifyee would simply impale itself on the lock held
//   by the notifier.
//
// * The mechanism is obstruction-free in that if the holder of the transient
//   OnDeck lock in unlock() is preempted or otherwise stalls, other threads
//   can still acquire and release the outer lock and continue to make progress.
//   At worst, waking of already blocked contending threads may be delayed,
//   but nothing worse.  (We only use "trylock" operations on the inner OnDeck
//   lock).
//
// * Note that thread-local storage must be initialized before a thread
//   uses Native monitors or mutexes.  The native monitor-mutex subsystem
//   depends on Thread::current().
//
// * The monitor synchronization subsystem avoids the use of native
//   synchronization primitives except for the narrow platform-specific
//   park-unpark abstraction.  See the comments in os_solaris.cpp regarding
//   the semantics of park-unpark.  Put another way, this monitor implementation
//   depends only on atomic operations and park-unpark.  The monitor subsystem
//   manages all RUNNING->BLOCKED and BLOCKED->READY transitions while the
//   underlying OS manages the READY<->RUN transitions.
//
// * The memory consistency model provide by lock()-unlock() is at least as
//   strong or stronger than the Java Memory model defined by JSR-133.
//   That is, we guarantee at least entry consistency, if not stronger.
//   See http://g.oswego.edu/dl/jmm/cookbook.html.
//
// * Thread:: currently contains a set of purpose-specific ParkEvents:
//   _MutexEvent, _ParkEvent, etc.  A better approach might be to do away with
//   the purpose-specific ParkEvents and instead implement a general per-thread
//   stack of available ParkEvents which we could provision on-demand.  The
//   stack acts as a local cache to avoid excessive calls to ParkEvent::Allocate()
//   and ::Release().  A thread would simply pop an element from the local stack before it
//   enqueued or park()ed.  When the contention was over the thread would
//   push the no-longer-needed ParkEvent back onto its stack.
//
// * A slightly reduced form of ILock() and IUnlock() have been partially
//   model-checked (Murphi) for safety and progress at T=1,2,3 and 4.
//   It'd be interesting to see if TLA/TLC could be useful as well.
//
// * Mutex-Monitor is a low-level "leaf" subsystem.  That is, the monitor
//   code should never call other code in the JVM that might itself need to
//   acquire monitors or mutexes.  That's true *except* in the case of the
//   ThreadBlockInVM state transition wrappers.  The ThreadBlockInVM DTOR handles
//   mutator reentry (ingress) by checking for a pending safepoint in which case it will
//   call SafepointSynchronize::block(), which in turn may call Safepoint_lock->lock(), etc.
//   In that particular case a call to lock() for a given Monitor can end up recursively
//   calling lock() on another monitor.   While distasteful, this is largely benign
//   as the calls come from jacket that wraps lock(), and not from deep within lock() itself.
//
//   It's unfortunate that native mutexes and thread state transitions were convolved.
//   They're really separate concerns and should have remained that way.  Melding
//   them together was facile -- a bit too facile.   The current implementation badly
//   conflates the two concerns.
//
// * TODO-FIXME:
//
//   -- Add DTRACE probes for contended acquire, contended acquired, contended unlock
//      We should also add DTRACE probes in the ParkEvent subsystem for
//      Park-entry, Park-exit, and Unpark.
//
//   -- We have an excess of mutex-like constructs in the JVM, namely:
//      1. objectMonitors for Java-level synchronization (synchronizer.cpp)
//      2. low-level muxAcquire and muxRelease
//      3. low-level spinAcquire and spinRelease
//      4. native Mutex:: and Monitor::
//      5. jvm_raw_lock() and _unlock()
//      6. JVMTI raw monitors -- distinct from (5) despite having a confusingly
//         similar name.
//
// o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o


// CASPTR() uses the canonical argument order that dominates in the literature.
// Our internal cmpxchg_ptr() uses a bastardized ordering to accommodate Sun .il templates.

#define CASPTR(a,c,s) intptr_t(Atomic::cmpxchg_ptr ((void *)(s),(void *)(a),(void *)(c)))
#define UNS(x) (uintptr_t(x))
#define TRACE(m) { static volatile int ctr = 0 ; int x = ++ctr ; if ((x & (x-1))==0) { ::printf ("%d:%s\n", x, #m); ::fflush(stdout); }}

// Simplistic low-quality Marsaglia SHIFT-XOR RNG.
// Bijective except for the trailing mask operation.
// Useful for spin loops as the compiler can't optimize it away.

static inline jint MarsagliaXORV (jint x) {
  if (x == 0) x = 1|os::random() ;
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ;
  return x & 0x7FFFFFFF ;
}

static inline jint MarsagliaXOR (jint * const a) {
  jint x = *a ;
  if (x == 0) x = UNS(a)|1 ;
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ;
  *a = x ;
  return x & 0x7FFFFFFF ;
}

static int Stall (int its) {
  static volatile jint rv = 1 ;
  volatile int OnFrame = 0 ;
  jint v = rv ^ UNS(OnFrame) ;
  while (--its >= 0) {
    v = MarsagliaXORV (v) ;
  }
  // Make this impossible for the compiler to optimize away,
  // but (mostly) avoid W coherency sharing on MP systems.
  if (v == 0x12345) rv = v ;
  return v ;
}

int Monitor::TryLock () {
  intptr_t v = _LockWord.FullWord ;
  for (;;) {
    if ((v & _LBIT) != 0) return 0 ;
    const intptr_t u = CASPTR (&_LockWord, v, v|_LBIT) ;
    if (v == u) return 1 ;
    v = u ;
  }
}

int Monitor::TryFast () {
  // Optimistic fast-path form ...
  // Fast-path attempt for the common uncontended case.
  // Avoid RTS->RTO $ coherence upgrade on typical SMP systems.
  intptr_t v = CASPTR (&_LockWord, 0, _LBIT) ;  // agro ...
  if (v == 0) return 1 ;

  for (;;) {
    if ((v & _LBIT) != 0) return 0 ;
    const intptr_t u = CASPTR (&_LockWord, v, v|_LBIT) ;
    if (v == u) return 1 ;
    v = u ;
  }
}

int Monitor::ILocked () {
  const intptr_t w = _LockWord.FullWord & 0xFF ;
  assert (w == 0 || w == _LBIT, "invariant") ;
  return w == _LBIT ;
}

// Polite TATAS spinlock with exponential backoff - bounded spin.
// Ideally we'd use processor cycles, time or vtime to control
// the loop, but we currently use iterations.
// All the constants within were derived empirically but work over
// over the spectrum of J2SE reference platforms.
// On Niagara-class systems the back-off is unnecessary but
// is relatively harmless.  (At worst it'll slightly retard
// acquisition times).  The back-off is critical for older SMP systems
// where constant fetching of the LockWord would otherwise impair
// scalability.
//
// Clamp spinning at approximately 1/2 of a context-switch round-trip.
// See synchronizer.cpp for details and rationale.

int Monitor::TrySpin (Thread * const Self) {
  D_WARN_Unimplement;
}

static int ParkCommon (ParkEvent * ev, jlong timo) {
  D_WARN_Unimplement;
}

inline int Monitor::AcquireOrPush (ParkEvent * ESelf) {
  D_WARN_Unimplement;
}

// ILock and IWait are the lowest level primitive internal blocking
// synchronization functions.  The callers of IWait and ILock must have
// performed any needed state transitions beforehand.
// IWait and ILock may directly call park() without any concern for thread state.
// Note that ILock and IWait do *not* access _owner.
// _owner is a higher-level logical concept.

void Monitor::ILock (Thread * Self) {
D_WARN_Unimplement;
}

void Monitor::IUnlock (bool RelaxAssert) {
D_WARN_Unimplement;
}

bool Monitor::notify() {
D_WARN_Unimplement;
}

// Currently notifyAll() transfers the waiters one-at-a-time from the waitset
// to the cxq.  This could be done more efficiently with a single bulk en-mass transfer,
// but in practice notifyAll() for large #s of threads is rare and not time-critical.
// Beware too, that we invert the order of the waiters.  Lets say that the
// waitset is "ABCD" and the cxq is "XYZ".  After a notifyAll() the waitset
// will be empty and the cxq will be "DCBAXYZ".  This is benign, of course.

bool Monitor::notify_all() {
  assert (_owner == Thread::current(), "invariant") ;
  assert (ILocked(), "invariant") ;
  while (_WaitSet != NULL) notify() ;
  return true ;
}

int Monitor::IWait (Thread * Self, jlong timo) {
D_WARN_Unimplement;
}


// ON THE VMTHREAD SNEAKING PAST HELD LOCKS:
// In particular, there are certain types of global lock that may be held
// by a Java thread while it is blocked at a safepoint but before it has
// written the _owner field. These locks may be sneakily acquired by the
// VM thread during a safepoint to avoid deadlocks. Alternatively, one should
// identify all such locks, and ensure that Java threads never block at
// safepoints while holding them (_no_safepoint_check_flag). While it
// seems as though this could increase the time to reach a safepoint
// (or at least increase the mean, if not the variance), the latter
// approach might make for a cleaner, more maintainable JVM design.
//
// Sneaking is vile and reprehensible and should be excised at the 1st
// opportunity.  It's possible that the need for sneaking could be obviated
// as follows.  Currently, a thread might (a) while TBIVM, call pthread_mutex_lock
// or ILock() thus acquiring the "physical" lock underlying Monitor/Mutex.
// (b) stall at the TBIVM exit point as a safepoint is in effect.  Critically,
// it'll stall at the TBIVM reentry state transition after having acquired the
// underlying lock, but before having set _owner and having entered the actual
// critical section.  The lock-sneaking facility leverages that fact and allowed the
// VM thread to logically acquire locks that had already be physically locked by mutators
// but where mutators were known blocked by the reentry thread state transition.
//
// If we were to modify the Monitor-Mutex so that TBIVM state transitions tightly
// wrapped calls to park(), then we could likely do away with sneaking.  We'd
// decouple lock acquisition and parking.  The critical invariant  to eliminating
// sneaking is to ensure that we never "physically" acquire the lock while TBIVM.
// An easy way to accomplish this is to wrap the park calls in a narrow TBIVM jacket.
// One difficulty with this approach is that the TBIVM wrapper could recurse and
// call lock() deep from within a lock() call, while the MutexEvent was already enqueued.
// Using a stack (N=2 at minimum) of ParkEvents would take care of that problem.
//
// But of course the proper ultimate approach is to avoid schemes that require explicit
// sneaking or dependence on any any clever invariants or subtle implementation properties
// of Mutex-Monitor and instead directly address the underlying design flaw.

void Monitor::lock (Thread * Self) {
D_WARN_Unimplement;
}

void Monitor::lock() {
  this->lock(Thread::current());
}

// Lock without safepoint check - a degenerate variant of lock().
// Should ONLY be used by safepoint code and other code
// that is guaranteed not to block while running inside the VM. If this is called with
// thread state set to be in VM, the safepoint synchronization code will deadlock!

void Monitor::lock_without_safepoint_check (Thread * Self) {
  assert (_owner != Self, "invariant") ;
  ILock (Self) ;
  assert (_owner == NULL, "invariant");
  set_owner (Self);
}

void Monitor::lock_without_safepoint_check () {
  lock_without_safepoint_check (Thread::current()) ;
}


// Returns true if thread succeceed [sic] in grabbing the lock, otherwise false.

bool Monitor::try_lock() {
  D_WARN_Unimplement;
}

void Monitor::unlock() {
  assert (_owner  == Thread::current(), "invariant") ;
  assert (_OnDeck != Thread::current()->_MutexEvent , "invariant") ;
  set_owner (NULL) ;
  if (_snuck) {
    assert(SafepointSynchronize::is_at_safepoint() && Thread::current()->is_VM_thread(), "sneak");
    _snuck = false;
    return ;
  }
  IUnlock (false) ;
}

// Yet another degenerate version of Monitor::lock() or lock_without_safepoint_check()
// jvm_raw_lock() and _unlock() can be called by non-Java threads via JVM_RawMonitorEnter.
//
// There's no expectation that JVM_RawMonitors will interoperate properly with the native
// Mutex-Monitor constructs.  We happen to implement JVM_RawMonitors in terms of
// native Mutex-Monitors simply as a matter of convenience.  A simple abstraction layer
// over a pthread_mutex_t would work equally as well, but require more platform-specific
// code -- a "PlatformMutex".  Alternatively, a simply layer over muxAcquire-muxRelease
// would work too.
//
// Since the caller might be a foreign thread, we don't necessarily have a Thread.MutexEvent
// instance available.  Instead, we transiently allocate a ParkEvent on-demand if
// we encounter contention.  That ParkEvent remains associated with the thread
// until it manages to acquire the lock, at which time we return the ParkEvent
// to the global ParkEvent free list.  This is correct and suffices for our purposes.
//
// Beware that the original jvm_raw_unlock() had a "_snuck" test but that
// jvm_raw_lock() didn't have the corresponding test.  I suspect that's an
// oversight, but I've replicated the original suspect logic in the new code ...

void Monitor::jvm_raw_lock() {
  D_WARN_Unimplement;
}

void Monitor::jvm_raw_unlock() {
  // Nearly the same as Monitor::unlock() ...
  // directly set _owner instead of using set_owner(null)
  _owner = NULL ;
  if (_snuck) {         // ???
    assert(SafepointSynchronize::is_at_safepoint() && Thread::current()->is_VM_thread(), "sneak");
    _snuck = false;
    return ;
  }
  IUnlock(false) ;
}

bool Monitor::wait(bool no_safepoint_check, long timeout, bool as_suspend_equivalent) {
  D_WARN_Unimplement;
}

Monitor::~Monitor() {
  assert ((UNS(_owner)|UNS(_LockWord.FullWord)|UNS(_EntryList)|UNS(_WaitSet)|UNS(_OnDeck)) == 0, "") ;
}

void Monitor::ClearMonitor (Monitor * m, const char *name) {
  m->_owner             = NULL ;
  m->_snuck             = false ;
  if (name == NULL) {
    strcpy(m->_name, "UNKNOWN") ;
  } else {
    strncpy(m->_name, name, MONITOR_NAME_LEN - 1);
    m->_name[MONITOR_NAME_LEN - 1] = '\0';
  }
  m->_LockWord.FullWord = 0 ;
  m->_EntryList         = NULL ;
  m->_OnDeck            = NULL ;
  m->_WaitSet           = NULL ;
  m->_WaitLock[0]       = 0 ;
}

Monitor::Monitor() { ClearMonitor(this); }

Monitor::Monitor (int Rank, const char * name, bool allow_vm_block) {
  ClearMonitor (this, name) ;
#ifdef ASSERT
  _allow_vm_block  = allow_vm_block;
  _rank            = Rank ;
#endif
}

Mutex::~Mutex() {
  assert ((UNS(_owner)|UNS(_LockWord.FullWord)|UNS(_EntryList)|UNS(_WaitSet)|UNS(_OnDeck)) == 0, "") ;
}

Mutex::Mutex (int Rank, const char * name, bool allow_vm_block) {
  ClearMonitor ((Monitor *) this, name) ;
#ifdef ASSERT
 _allow_vm_block   = allow_vm_block;
 _rank             = Rank ;
#endif
}

bool Monitor::owned_by_self() const {
  bool ret = _owner == Thread::current();
  assert (!ret || _LockWord.Bytes[_LSBINDEX] != 0, "invariant") ;
  return ret;
}

void Monitor::print_on_error(outputStream* st) const {
  st->print("[" PTR_FORMAT, this);
  st->print("] %s", _name);
  st->print(" - owner thread: " PTR_FORMAT, _owner);
}




// ----------------------------------------------------------------------------------
// Non-product code

#ifndef PRODUCT
void Monitor::print_on(outputStream* st) const {
  st->print_cr("Mutex: [0x%lx/0x%lx] %s - owner: 0x%lx", this, _LockWord.FullWord, _name, _owner);
}
#endif

#ifndef PRODUCT
#ifdef ASSERT
Monitor * Monitor::get_least_ranked_lock(Monitor * locks) {
  Monitor *res, *tmp;
  for (res = tmp = locks; tmp != NULL; tmp = tmp->next()) {
    if (tmp->rank() < res->rank()) {
      res = tmp;
    }
  }
  if (!SafepointSynchronize::is_at_safepoint()) {
    // In this case, we expect the held locks to be
    // in increasing rank order (modulo any native ranks)
    for (tmp = locks; tmp != NULL; tmp = tmp->next()) {
      if (tmp->next() != NULL) {
        assert(tmp->rank() == Mutex::native ||
               tmp->rank() <= tmp->next()->rank(), "mutex rank anomaly?");
      }
    }
  }
  return res;
}

Monitor* Monitor::get_least_ranked_lock_besides_this(Monitor* locks) {
  Monitor *res, *tmp;
  for (res = NULL, tmp = locks; tmp != NULL; tmp = tmp->next()) {
    if (tmp != this && (res == NULL || tmp->rank() < res->rank())) {
      res = tmp;
    }
  }
  if (!SafepointSynchronize::is_at_safepoint()) {
    // In this case, we expect the held locks to be
    // in increasing rank order (modulo any native ranks)
    for (tmp = locks; tmp != NULL; tmp = tmp->next()) {
      if (tmp->next() != NULL) {
        assert(tmp->rank() == Mutex::native ||
               tmp->rank() <= tmp->next()->rank(), "mutex rank anomaly?");
      }
    }
  }
  return res;
}


bool Monitor::contains(Monitor* locks, Monitor * lock) {
  for (; locks != NULL; locks = locks->next()) {
    if (locks == lock)
      return true;
  }
  return false;
}
#endif

// Called immediately after lock acquisition or release as a diagnostic
// to track the lock-set of the thread and test for rank violations that
// might indicate exposure to deadlock.
// Rather like an EventListener for _owner (:>).

void Monitor::set_owner_implementation(Thread *new_owner) {
  // This function is solely responsible for maintaining
  // and checking the invariant that threads and locks
  // are in a 1/N relation, with some some locks unowned.
  // It uses the Mutex::_owner, Mutex::_next, and
  // Thread::_owned_locks fields, and no other function
  // changes those fields.
  // It is illegal to set the mutex from one non-NULL
  // owner to another--it must be owned by NULL as an
  // intermediate state.

  if (new_owner != NULL) {
    // the thread is acquiring this lock

    assert(new_owner == Thread::current(), "Should I be doing this?");
    assert(_owner == NULL, "setting the owner thread of an already owned mutex");
    _owner = new_owner; // set the owner

    // link "this" into the owned locks list

    #ifdef ASSERT  // Thread::_owned_locks is under the same ifdef
      Monitor* locks = get_least_ranked_lock(new_owner->owned_locks());
                    // Mutex::set_owner_implementation is a friend of Thread

      assert(this->rank() >= 0, "bad lock rank");

      // Deadlock avoidance rules require us to acquire Mutexes only in
      // a global total order. For example m1 is the lowest ranked mutex
      // that the thread holds and m2 is the mutex the thread is trying
      // to acquire, then  deadlock avoidance rules require that the rank
      // of m2 be less  than the rank of m1.
      // The rank Mutex::native  is an exception in that it is not subject
      // to the verification rules.
      // Here are some further notes relating to mutex acquisition anomalies:
      // . under Solaris, the interrupt lock gets acquired when doing
      //   profiling, so any lock could be held.
      // . it is also ok to acquire Safepoint_lock at the very end while we
      //   already hold Terminator_lock - may happen because of periodic safepoints
      if (this->rank() != Mutex::native &&
          this->rank() != Mutex::suspend_resume &&
          locks != NULL && locks->rank() <= this->rank() &&
          !SafepointSynchronize::is_at_safepoint() &&
          this != Interrupt_lock && this != ProfileVM_lock &&
          !(this == Safepoint_lock && contains(locks, Terminator_lock) &&
            SafepointSynchronize::is_synchronizing())) {
        new_owner->print_owned_locks();
        fatal(err_msg("acquiring lock %s/%d out of order with lock %s/%d -- "
                      "possible deadlock", this->name(), this->rank(),
                      locks->name(), locks->rank()));
      }

      this->_next = new_owner->_owned_locks;
      new_owner->_owned_locks = this;
    #endif

  } else {
    // the thread is releasing this lock

    Thread* old_owner = _owner;
    debug_only(_last_owner = old_owner);

    assert(old_owner != NULL, "removing the owner thread of an unowned mutex");
    assert(old_owner == Thread::current(), "removing the owner thread of an unowned mutex");

    _owner = NULL; // set the owner

    #ifdef ASSERT
      Monitor *locks = old_owner->owned_locks();

      // remove "this" from the owned locks list

      Monitor *prev = NULL;
      bool found = false;
      for (; locks != NULL; prev = locks, locks = locks->next()) {
        if (locks == this) {
          found = true;
          break;
        }
      }
      assert(found, "Removing a lock not owned");
      if (prev == NULL) {
        old_owner->_owned_locks = _next;
      } else {
        prev->_next = _next;
      }
      _next = NULL;
    #endif
  }
}


// Factored out common sanity checks for locking mutex'es. Used by lock() and try_lock()
void Monitor::check_prelock_state(Thread *thread) {
  assert((!thread->is_Java_thread() || ((JavaThread *)thread)->thread_state() == _thread_in_vm)
         || rank() == Mutex::special, "wrong thread state for using locks");
  if (StrictSafepointChecks) {
    if (thread->is_VM_thread() && !allow_vm_block()) {
      fatal(err_msg("VM thread using lock %s (not allowed to block on)",
                    name()));
    }
    debug_only(if (rank() != Mutex::special) \
      thread->check_for_valid_safepoint_state(false);)
  }
  if (thread->is_Watcher_thread()) {
    assert(!WatcherThread::watcher_thread()->has_crash_protection(),
        "locking not allowed when crash protection is set");
  }
}

void Monitor::check_block_state(Thread *thread) {
  if (!_allow_vm_block && thread->is_VM_thread()) {
    warning("VM thread blocked on lock");
    print();
    BREAKPOINT;
  }
  assert(_owner != thread, "deadlock: blocking on monitor owned by current thread");
}

#endif // PRODUCT
