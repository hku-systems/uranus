/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/allocation.inline.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "runtime/atomic.hpp"
#include "runtime/os.hpp"
#include "runtime/threadCritical.hpp"
#include "utilities/ostream.hpp"

#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_aix
# include "os_aix.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

class NativeCallStack {};
NativeCallStack EMPTY_STACK;

void* StackObj::operator new(size_t size)     throw() { ShouldNotCallThis(); return 0; }
void  StackObj::operator delete(void* p)              { ShouldNotCallThis(); }
void* StackObj::operator new [](size_t size)  throw() { ShouldNotCallThis(); return 0; }
void  StackObj::operator delete [](void* p)           { ShouldNotCallThis(); }

void* _ValueObj::operator new(size_t size)    throw() { ShouldNotCallThis(); return 0; }
void  _ValueObj::operator delete(void* p)             { ShouldNotCallThis(); }
void* _ValueObj::operator new [](size_t size) throw() { ShouldNotCallThis(); return 0; }
void  _ValueObj::operator delete [](void* p)          { ShouldNotCallThis(); }

void* MetaspaceObj::operator new(size_t size, ClassLoaderData* loader_data,
                                 size_t word_size, bool read_only,
                                 MetaspaceObj::Type type, TRAPS) throw() {
  // Klass has it's own operator new
  return malloc(size * sizeof(char*));
}

bool MetaspaceObj::is_shared() const {
  D_WARN_Unimplement;
}

bool MetaspaceObj::is_metaspace_object() const {
  D_WARN_Unimplement;
}

void MetaspaceObj::print_address_on(outputStream* st) const {
  st->print(" {" INTPTR_FORMAT "}", p2i(this));
}

void* ResourceObj::operator new(size_t size, allocation_type type, MEMFLAGS flags) throw() {
  address res = NULL;
  switch (type) {
   case C_HEAP:
    res = (address)AllocateHeap(size, flags, CALLER_PC);
    DEBUG_ONLY(set_allocation_type(res, C_HEAP);)
    break;
   case RESOURCE_AREA:
    // new(size) sets allocation type RESOURCE_AREA.
    res = (address)operator new(size);
    break;
   default:
    ShouldNotReachHere();
  }
  return res;
}

void* ResourceObj::operator new [](size_t size, allocation_type type, MEMFLAGS flags) throw() {
  return (address) operator new(size, type, flags);
}

void* ResourceObj::operator new(size_t size, const std::nothrow_t&  nothrow_constant,
    allocation_type type, MEMFLAGS flags) throw() {
  // should only call this with std::nothrow, use other operator new() otherwise
  address res = NULL;
  switch (type) {
   case C_HEAP:
    res = (address)AllocateHeap(size, flags, CALLER_PC, AllocFailStrategy::RETURN_NULL);
    DEBUG_ONLY(if (res!= NULL) set_allocation_type(res, C_HEAP);)
    break;
   case RESOURCE_AREA:
    // new(size) sets allocation type RESOURCE_AREA.
    res = (address)operator new(size, std::nothrow);
    break;
   default:
    ShouldNotReachHere();
  }
  return res;
}

void* ResourceObj::operator new [](size_t size, const std::nothrow_t&  nothrow_constant,
    allocation_type type, MEMFLAGS flags) throw() {
  return (address)operator new(size, nothrow_constant, type, flags);
}

void ResourceObj::operator delete(void* p) {
  assert(((ResourceObj *)p)->allocated_on_C_heap(),
         "delete only allowed for C_HEAP objects");
  DEBUG_ONLY(((ResourceObj *)p)->_allocation_t[0] = (uintptr_t)badHeapOopVal;)
  FreeHeap(p);
}

void ResourceObj::operator delete [](void* p) {
  operator delete(p);
}

void trace_heap_malloc(size_t size, const char* name, void* p) {
  // A lock is not needed here - tty uses a lock internally
  tty->print_cr("Heap malloc " INTPTR_FORMAT " " SIZE_FORMAT " %s", p2i(p), size, name == NULL ? "" : name);
}


void trace_heap_free(void* p) {
  // A lock is not needed here - tty uses a lock internally
  tty->print_cr("Heap free   " INTPTR_FORMAT, p2i(p));
}

//--------------------------------------------------------------------------------------
// ChunkPool implementation

// MT-safe pool of chunks to reduce malloc/free thrashing
// NB: not using Mutex because pools are used before Threads are initialized
class ChunkPool: public CHeapObj<mtInternal> {
  Chunk*       _first;        // first cached Chunk; its first word points to next chunk
  size_t       _num_chunks;   // number of unused chunks in pool
  size_t       _num_used;     // number of chunks currently checked out
  const size_t _size;         // size of each chunk (must be uniform)

  // Our four static pools
  static ChunkPool* _large_pool;
  static ChunkPool* _medium_pool;
  static ChunkPool* _small_pool;
  static ChunkPool* _tiny_pool;

  // return first element or null
  void* get_first() {
    Chunk* c = _first;
    if (_first) {
      _first = _first->next();
      _num_chunks--;
    }
    return c;
  }

 public:
  // All chunks in a ChunkPool has the same size
   ChunkPool(size_t size) : _size(size) { _first = NULL; _num_chunks = _num_used = 0; }

  // Allocate a new chunk from the pool (might expand the pool)
  _NOINLINE_ void* allocate(size_t bytes, AllocFailType alloc_failmode) {
    assert(bytes == _size, "bad size");
    void* p = NULL;
    // No VM lock can be taken inside ThreadCritical lock, so os::malloc
    // should be done outside ThreadCritical lock due to NMT
    { ThreadCritical tc;
      _num_used++;
      p = get_first();
    }
    if (p == NULL) p = os::malloc(bytes, mtChunk, CURRENT_PC);
    if (p == NULL && alloc_failmode == AllocFailStrategy::EXIT_OOM) {
      vm_exit_out_of_memory(bytes, OOM_MALLOC_ERROR, "ChunkPool::allocate");
    }
    return p;
  }

  // Return a chunk to the pool
  void free(Chunk* chunk) {
    assert(chunk->length() + Chunk::aligned_overhead_size() == _size, "bad size");
    ThreadCritical tc;
    _num_used--;

    // Add chunk to list
    chunk->set_next(_first);
    _first = chunk;
    _num_chunks++;
  }

  // Prune the pool
  void free_all_but(size_t n) {
    Chunk* cur = NULL;
    Chunk* next;
    {
    // if we have more than n chunks, free all of them
    ThreadCritical tc;
    if (_num_chunks > n) {
      // free chunks at end of queue, for better locality
        cur = _first;
      for (size_t i = 0; i < (n - 1) && cur != NULL; i++) cur = cur->next();

      if (cur != NULL) {
          next = cur->next();
        cur->set_next(NULL);
        cur = next;

          _num_chunks = n;
        }
      }
    }

    // Free all remaining chunks, outside of ThreadCritical
    // to avoid deadlock with NMT
        while(cur != NULL) {
          next = cur->next();
      os::free(cur, mtChunk);
          cur = next;
        }
      }

  // Accessors to preallocated pool's
  static ChunkPool* large_pool()  { assert(_large_pool  != NULL, "must be initialized"); return _large_pool;  }
  static ChunkPool* medium_pool() { assert(_medium_pool != NULL, "must be initialized"); return _medium_pool; }
  static ChunkPool* small_pool()  { assert(_small_pool  != NULL, "must be initialized"); return _small_pool;  }
  static ChunkPool* tiny_pool()   { assert(_tiny_pool   != NULL, "must be initialized"); return _tiny_pool;   }

  static void initialize() {
    _large_pool  = new ChunkPool(Chunk::size        + Chunk::aligned_overhead_size());
    _medium_pool = new ChunkPool(Chunk::medium_size + Chunk::aligned_overhead_size());
    _small_pool  = new ChunkPool(Chunk::init_size   + Chunk::aligned_overhead_size());
    _tiny_pool   = new ChunkPool(Chunk::tiny_size   + Chunk::aligned_overhead_size());
  }

  static void clean() {
    enum { BlocksToKeep = 5 };
     _tiny_pool->free_all_but(BlocksToKeep);
     _small_pool->free_all_but(BlocksToKeep);
     _medium_pool->free_all_but(BlocksToKeep);
     _large_pool->free_all_but(BlocksToKeep);
  }
};

ChunkPool* ChunkPool::_large_pool  = NULL;
ChunkPool* ChunkPool::_medium_pool = NULL;
ChunkPool* ChunkPool::_small_pool  = NULL;
ChunkPool* ChunkPool::_tiny_pool   = NULL;

void chunkpool_init() {
  ChunkPool::initialize();
}

void
Chunk::clean_chunk_pool() {
  ChunkPool::clean();
}


//--------------------------------------------------------------------------------------
// Chunk implementation

void* Chunk::operator new (size_t requested_size, AllocFailType alloc_failmode, size_t length) throw() {
  // requested_size is equal to sizeof(Chunk) but in order for the arena
  // allocations to come out aligned as expected the size must be aligned
  // to expected arena alignment.
  // expect requested_size but if sizeof(Chunk) doesn't match isn't proper size we must align it.
  assert(ARENA_ALIGN(requested_size) == aligned_overhead_size(), "Bad alignment");
  size_t bytes = ARENA_ALIGN(requested_size) + length;
  switch (length) {
   case Chunk::size:        return ChunkPool::large_pool()->allocate(bytes, alloc_failmode);
   case Chunk::medium_size: return ChunkPool::medium_pool()->allocate(bytes, alloc_failmode);
   case Chunk::init_size:   return ChunkPool::small_pool()->allocate(bytes, alloc_failmode);
   case Chunk::tiny_size:   return ChunkPool::tiny_pool()->allocate(bytes, alloc_failmode);
   default: {
     void* p = os::malloc(bytes, mtChunk, CALLER_PC);
     if (p == NULL && alloc_failmode == AllocFailStrategy::EXIT_OOM) {
       vm_exit_out_of_memory(bytes, OOM_MALLOC_ERROR, "Chunk::new");
     }
     return p;
   }
  }
}

void Chunk::operator delete(void* p) {
  Chunk* c = (Chunk*)p;
  switch (c->length()) {
   case Chunk::size:        ChunkPool::large_pool()->free(c); break;
   case Chunk::medium_size: ChunkPool::medium_pool()->free(c); break;
   case Chunk::init_size:   ChunkPool::small_pool()->free(c); break;
   case Chunk::tiny_size:   ChunkPool::tiny_pool()->free(c); break;
   default:                 os::free(c, mtChunk);
  }
}

Chunk::Chunk(size_t length) : _len(length) {
  _next = NULL;         // Chain on the linked list
}


void Chunk::chop() {
  Chunk *k = this;
  while( k ) {
    Chunk *tmp = k->next();
    // clear out this chunk (to detect allocation bugs)
    if (ZapResourceArea) memset(k->bottom(), badResourceValue, k->length());
    delete k;                   // Free chunk (was malloc'd)
    k = tmp;
  }
}

void Chunk::next_chop() {
  _next->chop();
  _next = NULL;
}


void Chunk::start_chunk_pool_cleaner_task() {
  D_WARN_Unimplement;
}

//------------------------------Arena------------------------------------------
Arena::Arena(MEMFLAGS flag, size_t init_size) : _flags(flag), _size_in_bytes(0)  {
  size_t round_size = (sizeof (char *)) - 1;
  init_size = (init_size+round_size) & ~round_size;
  _first = _chunk = new (AllocFailStrategy::EXIT_OOM, init_size) Chunk(init_size);
  _hwm = _chunk->bottom();      // Save the cached hwm, max
  _max = _chunk->top();
  set_size_in_bytes(init_size);
}

Arena::Arena(MEMFLAGS flag) : _flags(flag), _size_in_bytes(0) {
  _first = _chunk = new (AllocFailStrategy::EXIT_OOM, Chunk::init_size) Chunk(Chunk::init_size);
  _hwm = _chunk->bottom();      // Save the cached hwm, max
  _max = _chunk->top();
  set_size_in_bytes(Chunk::init_size);
}

Arena *Arena::move_contents(Arena *copy) {
  copy->destruct_contents();
  copy->_chunk = _chunk;
  copy->_hwm   = _hwm;
  copy->_max   = _max;
  copy->_first = _first;

  // workaround rare racing condition, which could double count
  // the arena size by native memory tracking
  size_t size = size_in_bytes();
  set_size_in_bytes(0);
  copy->set_size_in_bytes(size);
  // Destroy original arena
  reset();
  return copy;            // Return Arena with contents
}

Arena::~Arena() {
  destruct_contents();
}

void* Arena::operator new(size_t size) throw() {
  assert(false, "Use dynamic memory type binding");
  return NULL;
}

void* Arena::operator new (size_t size, const std::nothrow_t&  nothrow_constant) throw() {
  assert(false, "Use dynamic memory type binding");
  return NULL;
}

  // dynamic memory type binding
void* Arena::operator new(size_t size, MEMFLAGS flags) throw() {
  return (void *) AllocateHeap(size, flags, CALLER_PC);
}

void* Arena::operator new(size_t size, const std::nothrow_t& nothrow_constant, MEMFLAGS flags) throw() {
  return os::malloc(size, flags, CALLER_PC);
}

void Arena::operator delete(void* p) {
  FreeHeap(p);
}

// Destroy this arenas contents and reset to empty
void Arena::destruct_contents() {
  if (UseMallocOnly && _first != NULL) {
    char* end = _first->next() ? _first->top() : _hwm;
    free_malloced_objects(_first, _first->bottom(), end, _hwm);
  }
  // reset size before chop to avoid a rare racing condition
  // that can have total arena memory exceed total chunk memory
  set_size_in_bytes(0);
  _first->chop();
  reset();
}

// This is high traffic method, but many calls actually don't
// change the size
void Arena::set_size_in_bytes(size_t size) {
  if (_size_in_bytes != size) {
    long delta = (long)(size - size_in_bytes());
    _size_in_bytes = size;
  }
}

// Total of all Chunks in arena
size_t Arena::used() const {
  size_t sum = _chunk->length() - (_max-_hwm); // Size leftover in this Chunk
  register Chunk *k = _first;
  while( k != _chunk) {         // Whilst have Chunks in a row
    sum += k->length();         // Total size of this Chunk
    k = k->next();              // Bump along to next Chunk
  }
  return sum;                   // Return total consumed space.
}

void Arena::signal_out_of_memory(size_t sz, const char* whence) const {
  vm_exit_out_of_memory(sz, OOM_MALLOC_ERROR, whence);
}

// Grow a new Chunk
void* Arena::grow(size_t x, AllocFailType alloc_failmode) {
  // Get minimal required size.  Either real big, or even bigger for giant objs
  size_t len = MAX2(x, (size_t) Chunk::size);

  Chunk *k = _chunk;            // Get filled-up chunk address
  _chunk = new (alloc_failmode, len) Chunk(len);

  if (_chunk == NULL) {
    _chunk = k;                 // restore the previous value of _chunk
    return NULL;
  }
  if (k) k->set_next(_chunk);   // Append new chunk to end of linked list
  else _first = _chunk;
  _hwm  = _chunk->bottom();     // Save the cached hwm, max
  _max =  _chunk->top();
  set_size_in_bytes(size_in_bytes() + len);
  void* result = _hwm;
  _hwm += x;
  return result;
}



// Reallocate storage in Arena.
void *Arena::Arealloc(void* old_ptr, size_t old_size, size_t new_size, AllocFailType alloc_failmode) {

  assert(new_size >= 0, "bad size");
  if (new_size == 0) return NULL;

  char *c_old = (char*)old_ptr; // Handy name
  // Stupid fast special case
  if( new_size <= old_size ) {  // Shrink in-place
    if( c_old+old_size == _hwm) // Attempt to free the excess bytes
      _hwm = c_old+new_size;    // Adjust hwm
    return c_old;
  }

  // make sure that new_size is legal
  size_t corrected_new_size = ARENA_ALIGN(new_size);

  // See if we can resize in-place
  if( (c_old+old_size == _hwm) &&       // Adjusting recent thing
      (c_old+corrected_new_size <= _max) ) {      // Still fits where it sits
    _hwm = c_old+corrected_new_size;      // Adjust hwm
    return c_old;               // Return old pointer
  }

  // Oops, got to relocate guts
  void *new_ptr = Amalloc(new_size, alloc_failmode);
  if (new_ptr == NULL) {
    return NULL;
  }
  memcpy( new_ptr, c_old, old_size );
  Afree(c_old,old_size);        // Mostly done to keep stats accurate
  return new_ptr;
}


// Determine if pointer belongs to this Arena or not.
bool Arena::contains( const void *ptr ) const {
  if( (void*)_chunk->bottom() <= ptr && ptr < (void*)_hwm )
    return true;                // Check for in this chunk
  for (Chunk *c = _first; c; c = c->next()) {
    if (c == _chunk) continue;  // current chunk has been processed
    if ((void*)c->bottom() <= ptr && ptr < (void*)c->top()) {
      return true;              // Check for every chunk in Arena
    }
  }
  return false;                 // Not in any Chunk, so not in Arena
}
