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
#include "code/codeBlob.hpp"
#include "code/relocInfo.hpp"
#include "interpreter/bytecode.hpp"
#include "memory/allocation.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#ifdef TARGET_ARCH_x86
# include "nativeInst_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "nativeInst_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "nativeInst_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "nativeInst_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "nativeInst_ppc.hpp"
#endif

class HeapBlock VALUE_OBJ_CLASS_SPEC {
    friend class VMStructs;

public:
    struct Header {
        size_t  _length;                             // the length in segments
        bool    _used;                               // Used bit
    };

protected:
    union {
        Header _header;
        int64_t _padding[ (sizeof(Header) + sizeof(int64_t)-1) / sizeof(int64_t) ];
        // pad to 0 mod 8
    };

public:
    // Initialization
    void initialize(size_t length)                 { _header._length = length; set_used(); }

    // Accessors
    void* allocated_space() const                  { return (void*)(this + 1); }
    size_t length() const                          { return _header._length; }

    // Used/free
    void set_used()                                { _header._used = true; }
    void set_free()                                { _header._used = false; }
    bool free()                                    { return !_header._used; }
};

size_t CodeHeap_header_size() {
  return sizeof(HeapBlock);
}

unsigned int align_code_offset(int offset) {
  // align the size to CodeEntryAlignment
  return
    ((offset + (int)CodeHeap_header_size() + (CodeEntryAlignment-1)) & ~(CodeEntryAlignment-1))
    - (int)CodeHeap_header_size();
}


// This must be consistent with the CodeBlob constructor's layout actions.
unsigned int CodeBlob::allocation_size(CodeBuffer* cb, int header_size) {
  unsigned int size = header_size;
  size += round_to(cb->total_relocation_size(), oopSize);
  // align the size to CodeEntryAlignment
  size = align_code_offset(size);
  size += round_to(cb->total_content_size(), oopSize);
  size += round_to(cb->total_oop_size(), oopSize);
  size += round_to(cb->total_metadata_size(), oopSize);
  return size;
}


// Creates a simple CodeBlob. Sets up the size of the different regions.
CodeBlob::CodeBlob(const char* name, int header_size, int size, int frame_complete, int locs_size) {
  assert(size        == round_to(size,        oopSize), "unaligned size");
  assert(locs_size   == round_to(locs_size,   oopSize), "unaligned size");
  assert(header_size == round_to(header_size, oopSize), "unaligned size");
  assert(!UseRelocIndex, "no space allocated for reloc index yet");

  // Note: If UseRelocIndex is enabled, there needs to be (at least) one
  //       extra word for the relocation information, containing the reloc
  //       index table length. Unfortunately, the reloc index table imple-
  //       mentation is not easily understandable and thus it is not clear
  //       what exactly the format is supposed to be. For now, we just turn
  //       off the use of this table (gri 7/6/2000).

  _name                  = name;
  _size                  = size;
  _frame_complete_offset = frame_complete;
  _header_size           = header_size;
  _relocation_size       = locs_size;
  _content_offset        = align_code_offset(header_size + _relocation_size);
  _code_offset           = _content_offset;
  _data_offset           = size;
  _frame_size            =  0;
  set_oop_maps(NULL);
}


// Creates a CodeBlob from a CodeBuffer. Sets up the size of the different regions,
// and copy code and relocation info.
CodeBlob::CodeBlob(
  const char* name,
  CodeBuffer* cb,
  int         header_size,
  int         size,
  int         frame_complete,
  int         frame_size,
  OopMapSet*  oop_maps
) {
  assert(size        == round_to(size,        oopSize), "unaligned size");
  assert(header_size == round_to(header_size, oopSize), "unaligned size");

  _name                  = name;
  _size                  = size;
  _frame_complete_offset = frame_complete;
  _header_size           = header_size;
  _relocation_size       = round_to(cb->total_relocation_size(), oopSize);
  _content_offset        = align_code_offset(header_size + _relocation_size);
  _code_offset           = _content_offset + cb->total_offset_of(cb->insts());
  _data_offset           = _content_offset + round_to(cb->total_content_size(), oopSize);
  assert(_data_offset <= size, "codeBlob is too small");

  cb->copy_code_and_locs_to(this);
  set_oop_maps(oop_maps);
  _frame_size = frame_size;
}


void CodeBlob::set_oop_maps(OopMapSet* p) {
//  D_WARN_Unimplement;
}


void CodeBlob::trace_new_stub(CodeBlob* stub, const char* name1, const char* name2) {
  D_WARN_Unimplement;
}


void CodeBlob::flush() {
  _strings.free();
}


OopMap* CodeBlob::oop_map_for_return_address(address return_address) {
  D_WARN_Unimplement;
}


//----------------------------------------------------------------------------------------------------
// Implementation of BufferBlob


BufferBlob::BufferBlob(const char* name, int size)
: CodeBlob(name, sizeof(BufferBlob), size, CodeOffsets::frame_never_safe, /*locs_size:*/ 0)
{}

BufferBlob* BufferBlob::create(const char* name, int buffer_size) {
  unsigned int size = sizeof(BufferBlob);
  size = align_code_offset(size);
  size += round_to(buffer_size, oopSize);
  BufferBlob *blob = NULL;
  blob = new (size) BufferBlob(name, size);
  return blob;
}


BufferBlob::BufferBlob(const char* name, int size, CodeBuffer* cb)
  : CodeBlob(name, cb, sizeof(BufferBlob), size, CodeOffsets::frame_never_safe, 0, NULL)
{}

BufferBlob* BufferBlob::create(const char* name, CodeBuffer* cb) {

  BufferBlob* blob = NULL;
  unsigned int size = allocation_size(cb, sizeof(BufferBlob));
  assert(name != NULL, "must provide a name");
  {
    blob = new (size) BufferBlob(name, size, cb);
  }
  // Track memory usage statistic after releasing CodeCache_lock

  return blob;
}


void* BufferBlob::operator new(size_t s, unsigned size, bool is_critical) throw() {
  void* p = malloc(size);
  return p;
}


void BufferBlob::free( BufferBlob *blob ) {
    ::free(blob);
}


//----------------------------------------------------------------------------------------------------
// Implementation of AdapterBlob

AdapterBlob::AdapterBlob(int size, CodeBuffer* cb) :
  BufferBlob("I2C/C2I adapters", size, cb) {
    D_WARN_Unimplement;
}

AdapterBlob* AdapterBlob::create(CodeBuffer* cb) {

  AdapterBlob* blob = NULL;
  unsigned int size = allocation_size(cb, sizeof(AdapterBlob));
  {
    // The parameter 'true' indicates a critical memory allocation.
    // This means that CodeCacheMinimumFreeSpace is used, if necessary
    const bool is_critical = true;
    blob = new (size, is_critical) AdapterBlob(size, cb);
  }

  return blob;
}


//----------------------------------------------------------------------------------------------------
// Implementation of MethodHandlesAdapterBlob

MethodHandlesAdapterBlob* MethodHandlesAdapterBlob::create(int buffer_size) {

  MethodHandlesAdapterBlob* blob = NULL;
  unsigned int size = sizeof(MethodHandlesAdapterBlob);
  // align the size to CodeEntryAlignment
  size = align_code_offset(size);
  size += round_to(buffer_size, oopSize);
  {
    // The parameter 'true' indicates a critical memory allocation.
    // This means that CodeCacheMinimumFreeSpace is used, if necessary
    const bool is_critical = true;
    blob = new (size, is_critical) MethodHandlesAdapterBlob(size);
  }
  // Track memory usage statistic after releasing CodeCache_lock

  return blob;
}


//----------------------------------------------------------------------------------------------------
// Implementation of RuntimeStub

RuntimeStub::RuntimeStub(
  const char* name,
  CodeBuffer* cb,
  int         size,
  int         frame_complete,
  int         frame_size,
  OopMapSet*  oop_maps,
  bool        caller_must_gc_arguments
)
: CodeBlob(name, cb, sizeof(RuntimeStub), size, frame_complete, frame_size, oop_maps)
{
  _caller_must_gc_arguments = caller_must_gc_arguments;
}


RuntimeStub* RuntimeStub::new_runtime_stub(const char* stub_name,
                                           CodeBuffer* cb,
                                           int frame_complete,
                                           int frame_size,
                                           OopMapSet* oop_maps,
                                           bool caller_must_gc_arguments)
{
  RuntimeStub* stub = NULL;
  {
    unsigned int size = allocation_size(cb, sizeof(RuntimeStub));
    stub = new (size) RuntimeStub(stub_name, cb, size, frame_complete, frame_size, oop_maps, caller_must_gc_arguments);
  }

  trace_new_stub(stub, "RuntimeStub - ", stub_name);

  return stub;
}


void* RuntimeStub::operator new(size_t s, unsigned size) throw() {
  return malloc(size * sizeof(HeapWord));
}

// operator new shared by all singletons:
void* SingletonBlob::operator new(size_t s, unsigned size) throw() {
    return malloc(size * sizeof(HeapWord));
}


//----------------------------------------------------------------------------------------------------
// Implementation of DeoptimizationBlob

//----------------------------------------------------------------------------------------------------
// Verification and printing

void CodeBlob::verify() {
  ShouldNotReachHere();
}

void CodeBlob::print_on(outputStream* st) const {
  st->print_cr("[CodeBlob (" INTPTR_FORMAT ")]", p2i(this));
  st->print_cr("Framesize: %d", _frame_size);
}

void CodeBlob::print_value_on(outputStream* st) const {
  st->print_cr("[CodeBlob]");
}

void BufferBlob::verify() {
  // unimplemented
}

void BufferBlob::print_on(outputStream* st) const {
  CodeBlob::print_on(st);
  print_value_on(st);
}

void BufferBlob::print_value_on(outputStream* st) const {
  st->print_cr("BufferBlob (" INTPTR_FORMAT  ") used for %s", p2i(this), name());
}

void RuntimeStub::verify() {
  // unimplemented
}

void RuntimeStub::print_on(outputStream* st) const {
  ttyLocker ttyl;
  CodeBlob::print_on(st);
  st->print("Runtime Stub (" INTPTR_FORMAT "): ", p2i(this));
  st->print_cr("%s", name());
}

void RuntimeStub::print_value_on(outputStream* st) const {
  st->print("RuntimeStub (" INTPTR_FORMAT "): ", p2i(this)); st->print("%s", name());
}

void SingletonBlob::verify() {
  // unimplemented
}

void SingletonBlob::print_on(outputStream* st) const {
  ttyLocker ttyl;
  CodeBlob::print_on(st);
  st->print_cr("%s", name());
}

void SingletonBlob::print_value_on(outputStream* st) const {
  st->print_cr("%s", name());
}