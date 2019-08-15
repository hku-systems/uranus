/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "oops/generateOopMap.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "runtime/handles.inline.hpp"
#include "utilities/bitMap.inline.hpp"

//
//
// Compute stack layouts for each instruction in method.
//
//  Problems:
//  - What to do about jsr with different types of local vars?
//  Need maps that are conditional on jsr path?
//  - Jsr and exceptions should be done more efficiently (the retAddr stuff)
//
//  Alternative:
//  - Could extend verifier to provide this information.
//    For: one fewer abstract interpreter to maintain. Against: the verifier
//    solves a bigger problem so slower (undesirable to force verification of
//    everything?).
//
//  Algorithm:
//    Partition bytecodes into basic blocks
//    For each basic block: store entry state (vars, stack). For instructions
//    inside basic blocks we do not store any state (instead we recompute it
//    from state produced by previous instruction).
//
//    Perform abstract interpretation of bytecodes over this lattice:
//
//                _--'#'--_
//               /  /  \   \
//             /   /     \   \
//            /    |     |     \
//          'r'   'v'   'p'   ' '
//           \     |     |     /
//            \    \     /    /
//              \   \   /    /
//                -- '@' --
//
//    '#'  top, result of conflict merge
//    'r'  reference type
//    'v'  value type
//    'p'  pc type for jsr/ret
//    ' '  uninitialized; never occurs on operand stack in Java
//    '@'  bottom/unexecuted; initial state each bytecode.
//
//    Basic block headers are the only merge points. We use this iteration to
//    compute the information:
//
//    find basic blocks;
//    initialize them with uninitialized state;
//    initialize first BB according to method signature;
//    mark first BB changed
//    while (some BB is changed) do {
//      perform abstract interpration of all bytecodes in BB;
//      merge exit state of BB into entry state of all successor BBs,
//      noting if any of these change;
//    }
//
//  One additional complication is necessary. The jsr instruction pushes
//  a return PC on the stack (a 'p' type in the abstract interpretation).
//  To be able to process "ret" bytecodes, we keep track of these return
//  PC's in a 'retAddrs' structure in abstract interpreter context (when
//  processing a "ret" bytecodes, it is not sufficient to know that it gets
//  an argument of the right type 'p'; we need to know which address it
//  returns to).
//
// (Note this comment is borrowed form the original author of the algorithm)

// ComputeCallStack
//
// Specialization of SignatureIterator - compute the effects of a call
//
class ComputeCallStack : public SignatureIterator {
  CellTypeState *_effect;
  int _idx;

  void setup();
  void set(CellTypeState state)         { _effect[_idx++] = state; }
  int  length()                         { return _idx; };

  virtual void do_bool  ()              { set(CellTypeState::value); };
  virtual void do_char  ()              { set(CellTypeState::value); };
  virtual void do_float ()              { set(CellTypeState::value); };
  virtual void do_byte  ()              { set(CellTypeState::value); };
  virtual void do_short ()              { set(CellTypeState::value); };
  virtual void do_int   ()              { set(CellTypeState::value); };
  virtual void do_void  ()              { set(CellTypeState::bottom);};
  virtual void do_object(int begin, int end)  { set(CellTypeState::ref); };
  virtual void do_array (int begin, int end)  { set(CellTypeState::ref); };

  void do_double()                      { set(CellTypeState::value);
                                          set(CellTypeState::value); }
  void do_long  ()                      { set(CellTypeState::value);
                                           set(CellTypeState::value); }

public:
  ComputeCallStack(Symbol* signature) : SignatureIterator(signature) {};

  // Compute methods
  int compute_for_parameters(bool is_static, CellTypeState *effect) {
    _idx    = 0;
    _effect = effect;

    if (!is_static)
      effect[_idx++] = CellTypeState::ref;

    iterate_parameters();

    return length();
  };

  int compute_for_returntype(CellTypeState *effect) {
    _idx    = 0;
    _effect = effect;
    iterate_returntype();
    set(CellTypeState::bottom);  // Always terminate with a bottom state, so ppush works

    return length();
  }
};

//=========================================================================================
// ComputeEntryStack
//
// Specialization of SignatureIterator - in order to set up first stack frame
//
class ComputeEntryStack : public SignatureIterator {
  CellTypeState *_effect;
  int _idx;

  void setup();
  void set(CellTypeState state)         { _effect[_idx++] = state; }
  int  length()                         { return _idx; };

  virtual void do_bool  ()              { set(CellTypeState::value); };
  virtual void do_char  ()              { set(CellTypeState::value); };
  virtual void do_float ()              { set(CellTypeState::value); };
  virtual void do_byte  ()              { set(CellTypeState::value); };
  virtual void do_short ()              { set(CellTypeState::value); };
  virtual void do_int   ()              { set(CellTypeState::value); };
  virtual void do_void  ()              { set(CellTypeState::bottom);};
  virtual void do_object(int begin, int end)  { set(CellTypeState::make_slot_ref(_idx)); }
  virtual void do_array (int begin, int end)  { set(CellTypeState::make_slot_ref(_idx)); }

  void do_double()                      { set(CellTypeState::value);
                                          set(CellTypeState::value); }
  void do_long  ()                      { set(CellTypeState::value);
                                          set(CellTypeState::value); }

public:
  ComputeEntryStack(Symbol* signature) : SignatureIterator(signature) {};

  // Compute methods
  int compute_for_parameters(bool is_static, CellTypeState *effect) {
    _idx    = 0;
    _effect = effect;

    if (!is_static)
      effect[_idx++] = CellTypeState::make_slot_ref(0);

    iterate_parameters();

    return length();
  };

  int compute_for_returntype(CellTypeState *effect) {
    _idx    = 0;
    _effect = effect;
    iterate_returntype();
    set(CellTypeState::bottom);  // Always terminate with a bottom state, so ppush works

    return length();
  }
};

//=====================================================================================
//
// Implementation of RetTable/RetTableEntry
//
// Contains function to itereate through all bytecodes
// and find all return entry points
//
int RetTable::_init_nof_entries = 10;
int RetTableEntry::_init_nof_jsrs = 5;

void RetTableEntry::add_delta(int bci, int delta) {
  if (_target_bci > bci) _target_bci += delta;

  for (int k = 0; k < _jsrs->length(); k++) {
    int jsr = _jsrs->at(k);
    if (jsr > bci) _jsrs->at_put(k, jsr+delta);
  }
}

void RetTable::compute_ret_table(methodHandle method) {
  //
}

void RetTable::add_jsr(int return_bci, int target_bci) {
  RetTableEntry* entry = _first;

  // Scan table for entry
  for (;entry && entry->target_bci() != target_bci; entry = entry->next());

  if (!entry) {
    // Allocate new entry and put in list
    entry = new RetTableEntry(target_bci, _first);
    _first = entry;
  }

  // Now "entry" is set.  Make sure that the entry is initialized
  // and has room for the new jsr.
  entry->add_jsr(return_bci);
}

RetTableEntry* RetTable::find_jsrs_for_target(int targBci) {
  RetTableEntry *cur = _first;

  while(cur) {
    assert(cur->target_bci() != -1, "sanity check");
    if (cur->target_bci() == targBci)  return cur;
    cur = cur->next();
  }
  ShouldNotReachHere();
  return NULL;
}

// The instruction at bci is changing size by "delta".  Update the return map.
void RetTable::update_ret_table(int bci, int delta) {
  RetTableEntry *cur = _first;
  while(cur) {
    cur->add_delta(bci, delta);
    cur = cur->next();
  }
}

//
// Celltype state
//

CellTypeState CellTypeState::bottom      = CellTypeState::make_bottom();
CellTypeState CellTypeState::uninit      = CellTypeState::make_any(uninit_value);
CellTypeState CellTypeState::ref         = CellTypeState::make_any(ref_conflict);
CellTypeState CellTypeState::value       = CellTypeState::make_any(val_value);
CellTypeState CellTypeState::refUninit   = CellTypeState::make_any(ref_conflict | uninit_value);
CellTypeState CellTypeState::top         = CellTypeState::make_top();
CellTypeState CellTypeState::addr        = CellTypeState::make_any(addr_conflict);

// Commonly used constants
static CellTypeState epsilonCTS[1] = { CellTypeState::bottom };
static CellTypeState   refCTS   = CellTypeState::ref;
static CellTypeState   valCTS   = CellTypeState::value;
static CellTypeState    vCTS[2] = { CellTypeState::value, CellTypeState::bottom };
static CellTypeState    rCTS[2] = { CellTypeState::ref,   CellTypeState::bottom };
static CellTypeState   rrCTS[3] = { CellTypeState::ref,   CellTypeState::ref,   CellTypeState::bottom };
static CellTypeState   vrCTS[3] = { CellTypeState::value, CellTypeState::ref,   CellTypeState::bottom };
static CellTypeState   vvCTS[3] = { CellTypeState::value, CellTypeState::value, CellTypeState::bottom };
static CellTypeState  rvrCTS[4] = { CellTypeState::ref,   CellTypeState::value, CellTypeState::ref,   CellTypeState::bottom };
static CellTypeState  vvrCTS[4] = { CellTypeState::value, CellTypeState::value, CellTypeState::ref,   CellTypeState::bottom };
static CellTypeState  vvvCTS[4] = { CellTypeState::value, CellTypeState::value, CellTypeState::value, CellTypeState::bottom };
static CellTypeState vvvrCTS[5] = { CellTypeState::value, CellTypeState::value, CellTypeState::value, CellTypeState::ref,   CellTypeState::bottom };
static CellTypeState vvvvCTS[5] = { CellTypeState::value, CellTypeState::value, CellTypeState::value, CellTypeState::value, CellTypeState::bottom };

char CellTypeState::to_char() const {
  if (can_be_reference()) {
    if (can_be_value() || can_be_address())
      return '#';    // Conflict that needs to be rewritten
    else
      return 'r';
  } else if (can_be_value())
    return 'v';
  else if (can_be_address())
    return 'p';
  else if (can_be_uninit())
    return ' ';
  else
    return '@';
}


// Print a detailed CellTypeState.  Indicate all bits that are set.  If
// the CellTypeState represents an address or a reference, print the
// value of the additional information.
void CellTypeState::print(outputStream *os) {
  if (can_be_address()) {
    os->print("(p");
  } else {
    os->print("( ");
  }
  if (can_be_reference()) {
    os->print("r");
  } else {
    os->print(" ");
  }
  if (can_be_value()) {
    os->print("v");
  } else {
    os->print(" ");
  }
  if (can_be_uninit()) {
    os->print("u|");
  } else {
    os->print(" |");
  }
  if (is_info_top()) {
    os->print("Top)");
  } else if (is_info_bottom()) {
    os->print("Bot)");
  } else {
    if (is_reference()) {
      int info = get_info();
      int data = info & ~(ref_not_lock_bit | ref_slot_bit);
      if (info & ref_not_lock_bit) {
        // Not a monitor lock reference.
        if (info & ref_slot_bit) {
          // slot
          os->print("slot%d)", data);
        } else {
          // line
          os->print("line%d)", data);
        }
      } else {
        // lock
        os->print("lock%d)", data);
      }
    } else {
      os->print("%d)", get_info());
    }
  }
}

//
// Basicblock handling methods
//

void GenerateOopMap ::initialize_bb() {
  _gc_points = 0;
  _bb_count  = 0;
  _bb_hdr_bits.clear();
  _bb_hdr_bits.resize(method()->code_size());
}

void GenerateOopMap::bb_mark_fct(GenerateOopMap *c, int bci, int *data) {
  assert(bci>= 0 && bci < c->method()->code_size(), "index out of bounds");
  if (c->is_bb_header(bci))
     return;

  if (TraceNewOopMapGeneration) {
     tty->print_cr("Basicblock#%d begins at: %d", c->_bb_count, bci);
  }
  c->set_bbmark_bit(bci);
  c->_bb_count++;
}


void GenerateOopMap::mark_bbheaders_and_count_gc_points() {
  D_WARN_Unimplement;
}

void GenerateOopMap::reachable_basicblock(GenerateOopMap *c, int bci, int *data) {
  assert(bci>= 0 && bci < c->method()->code_size(), "index out of bounds");
  BasicBlock* bb = c->get_basic_block_at(bci);
  if (bb->is_dead()) {
    bb->mark_as_alive();
    *data = 1; // Mark basicblock as changed
  }
}


void GenerateOopMap::mark_reachable_code() {
  D_WARN_Unimplement;
}

/* If the current instruction in "c" has no effect on control flow,
   returns "true".  Otherwise, calls "jmpFct" one or more times, with
   "c", an appropriate "pcDelta", and "data" as arguments, then
   returns "false".  There is one exception: if the current
   instruction is a "ret", returns "false" without calling "jmpFct".
   Arrangements for tracking the control flow of a "ret" must be made
   externally. */

/* Requires "pc" to be the head of a basic block; returns that basic
   block. */
BasicBlock *GenerateOopMap::get_basic_block_at(int bci) const {
  BasicBlock* bb = get_basic_block_containing(bci);
  assert(bb->_bci == bci, "should have found BB");
  return bb;
}

// Requires "pc" to be the start of an instruction; returns the basic
//   block containing that instruction. */
BasicBlock  *GenerateOopMap::get_basic_block_containing(int bci) const {
  BasicBlock *bbs = _basic_blocks;
  int lo = 0, hi = _bb_count - 1;

  while (lo <= hi) {
    int m = (lo + hi) / 2;
    int mbci = bbs[m]._bci;
    int nbci;

    if ( m == _bb_count-1) {
      assert( bci >= mbci && bci < method()->code_size(), "sanity check failed");
      return bbs+m;
    } else {
      nbci = bbs[m+1]._bci;
    }

    if ( mbci <= bci && bci < nbci) {
      return bbs+m;
    } else if (mbci < bci) {
      lo = m + 1;
    } else {
      assert(mbci > bci, "sanity check");
      hi = m - 1;
    }
  }

  fatal("should have found BB");
  return NULL;
}

void GenerateOopMap::restore_state(BasicBlock *bb)
{
  memcpy(_state, bb->_state, _state_len*sizeof(CellTypeState));
  _stack_top = bb->_stack_top;
  _monitor_top = bb->_monitor_top;
}

int GenerateOopMap::next_bb_start_pc(BasicBlock *bb) {
 int bbNum = bb - _basic_blocks + 1;
 if (bbNum == _bb_count)
    return method()->code_size();

 return _basic_blocks[bbNum]._bci;
}

//
// CellType handling methods
//

// Allocate memory and throw LinkageError if failure.
#define ALLOC_RESOURCE_ARRAY(var, type, count) \
  var = NEW_RESOURCE_ARRAY_RETURN_NULL(type, count);              \
  if (var == NULL) {                                              \
    report_error("Cannot reserve enough memory to analyze this method"); \
    return;                                                       \
  }


void GenerateOopMap::init_state() {
  _state_len     = _max_locals + _max_stack + _max_monitors;
  ALLOC_RESOURCE_ARRAY(_state, CellTypeState, _state_len);
  memset(_state, 0, _state_len * sizeof(CellTypeState));
  int count = MAX3(_max_locals, _max_stack, _max_monitors) + 1/*for null terminator char */;
  ALLOC_RESOURCE_ARRAY(_state_vec_buf, char, count);
}

void GenerateOopMap::make_context_uninitialized() {
  CellTypeState* vs = vars();

  for (int i = 0; i < _max_locals; i++)
      vs[i] = CellTypeState::uninit;

  _stack_top = 0;
  _monitor_top = 0;
}

int GenerateOopMap::methodsig_to_effect(Symbol* signature, bool is_static, CellTypeState* effect) {
  ComputeEntryStack ces(signature);
  return ces.compute_for_parameters(is_static, effect);
}

// Return result of merging cts1 and cts2.
CellTypeState CellTypeState::merge(CellTypeState cts, int slot) const {
  CellTypeState result;

  assert(!is_bottom() && !cts.is_bottom(),
         "merge of bottom values is handled elsewhere");

  result._state = _state | cts._state;

  // If the top bit is set, we don't need to do any more work.
  if (!result.is_info_top()) {
    assert((result.can_be_address() || result.can_be_reference()),
           "only addresses and references have non-top info");

    if (!equal(cts)) {
      // The two values being merged are different.  Raise to top.
      if (result.is_reference()) {
        result = CellTypeState::make_slot_ref(slot);
      } else {
        result._state |= info_conflict;
      }
    }
  }
  assert(result.is_valid_state(), "checking that CTS merge maintains legal state");

  return result;
}

// Merge the variable state for locals and stack from cts into bbts.
bool GenerateOopMap::merge_local_state_vectors(CellTypeState* cts,
                                               CellTypeState* bbts) {
  int i;
  int len = _max_locals + _stack_top;
  bool change = false;

  for (i = len - 1; i >= 0; i--) {
    CellTypeState v = cts[i].merge(bbts[i], i);
    change = change || !v.equal(bbts[i]);
    bbts[i] = v;
  }

  return change;
}

// Merge the monitor stack state from cts into bbts.
bool GenerateOopMap::merge_monitor_state_vectors(CellTypeState* cts,
                                                 CellTypeState* bbts) {
  bool change = false;
  if (_max_monitors > 0 && _monitor_top != bad_monitors) {
    // If there are no monitors in the program, or there has been
    // a monitor matching error before this point in the program,
    // then we do not merge in the monitor state.

    int base = _max_locals + _max_stack;
    int len = base + _monitor_top;
    for (int i = len - 1; i >= base; i--) {
      CellTypeState v = cts[i].merge(bbts[i], i);

      // Can we prove that, when there has been a change, it will already
      // have been detected at this point?  That would make this equal
      // check here unnecessary.
      change = change || !v.equal(bbts[i]);
      bbts[i] = v;
    }
  }

  return change;
}

void GenerateOopMap::copy_state(CellTypeState *dst, CellTypeState *src) {
  int len = _max_locals + _stack_top;
  for (int i = 0; i < len; i++) {
    if (src[i].is_nonlock_reference()) {
      dst[i] = CellTypeState::make_slot_ref(i);
    } else {
      dst[i] = src[i];
    }
  }
  if (_max_monitors > 0 && _monitor_top != bad_monitors) {
    int base = _max_locals + _max_stack;
    len = base + _monitor_top;
    for (int i = base; i < len; i++) {
      dst[i] = src[i];
    }
  }
}


// Merge the states for the current block and the next.  As long as a
// block is reachable the locals and stack must be merged.  If the
// stack heights don't match then this is a verification error and
// it's impossible to interpret the code.  Simultaneously monitor
// states are being check to see if they nest statically.  If monitor
// depths match up then their states are merged.  Otherwise the
// mismatch is simply recorded and interpretation continues since
// monitor matching is purely informational and doesn't say anything
// about the correctness of the code.
void GenerateOopMap::merge_state_into_bb(BasicBlock *bb) {
  guarantee(bb != NULL, "null basicblock");
  assert(bb->is_alive(), "merging state into a dead basicblock");

  if (_stack_top == bb->_stack_top) {
    // always merge local state even if monitors don't match.
    if (merge_local_state_vectors(_state, bb->_state)) {
      bb->set_changed(true);
    }
    if (_monitor_top == bb->_monitor_top) {
      // monitors still match so continue merging monitor states.
      if (merge_monitor_state_vectors(_state, bb->_state)) {
        bb->set_changed(true);
      }
    } else {
      if (TraceMonitorMismatch) {
        report_monitor_mismatch("monitor stack height merge conflict");
      }
      // When the monitor stacks are not matched, we set _monitor_top to
      // bad_monitors.  This signals that, from here on, the monitor stack cannot
      // be trusted.  In particular, monitorexit bytecodes may throw
      // exceptions.  We mark this block as changed so that the change
      // propagates properly.
      bb->_monitor_top = bad_monitors;
      bb->set_changed(true);
      _monitor_safe = false;
    }
  } else if (!bb->is_reachable()) {
    // First time we look at this  BB
    copy_state(bb->_state, _state);
    bb->_stack_top = _stack_top;
    bb->_monitor_top = _monitor_top;
    bb->set_changed(true);
  } else {
    verify_error("stack height conflict: %d vs. %d",  _stack_top, bb->_stack_top);
  }
}

void GenerateOopMap::merge_state(GenerateOopMap *gom, int bci, int* data) {
   gom->merge_state_into_bb(gom->get_basic_block_at(bci));
}

void GenerateOopMap::set_var(int localNo, CellTypeState cts) {
  assert(cts.is_reference() || cts.is_value() || cts.is_address(),
         "wrong celltypestate");
  if (localNo < 0 || localNo > _max_locals) {
    verify_error("variable write error: r%d", localNo);
    return;
  }
  vars()[localNo] = cts;
}

CellTypeState GenerateOopMap::get_var(int localNo) {
  assert(localNo < _max_locals + _nof_refval_conflicts, "variable read error");
  if (localNo < 0 || localNo > _max_locals) {
    verify_error("variable read error: r%d", localNo);
    return valCTS; // just to pick something;
  }
  return vars()[localNo];
}

CellTypeState GenerateOopMap::pop() {
  if ( _stack_top <= 0) {
    verify_error("stack underflow");
    return valCTS; // just to pick something
  }
  return  stack()[--_stack_top];
}

void GenerateOopMap::push(CellTypeState cts) {
  if ( _stack_top >= _max_stack) {
    verify_error("stack overflow");
    return;
  }
  stack()[_stack_top++] = cts;
}

CellTypeState GenerateOopMap::monitor_pop() {
  assert(_monitor_top != bad_monitors, "monitor_pop called on error monitor stack");
  if (_monitor_top == 0) {
    // We have detected a pop of an empty monitor stack.
    _monitor_safe = false;
     _monitor_top = bad_monitors;

    if (TraceMonitorMismatch) {
      report_monitor_mismatch("monitor stack underflow");
    }
    return CellTypeState::ref; // just to keep the analysis going.
  }
  return  monitors()[--_monitor_top];
}

void GenerateOopMap::monitor_push(CellTypeState cts) {
  assert(_monitor_top != bad_monitors, "monitor_push called on error monitor stack");
  if (_monitor_top >= _max_monitors) {
    // Some monitorenter is being executed more than once.
    // This means that the monitor stack cannot be simulated.
    _monitor_safe = false;
    _monitor_top = bad_monitors;

    if (TraceMonitorMismatch) {
      report_monitor_mismatch("monitor stack overflow");
    }
    return;
  }
  monitors()[_monitor_top++] = cts;
}

//
// Interpretation handling methods
//

void GenerateOopMap::do_interpretation()
{
  // "i" is just for debugging, so we can detect cases where this loop is
  // iterated more than once.
  int i = 0;
  do {
#ifndef PRODUCT
    if (TraceNewOopMapGeneration) {
      tty->print("\n\nIteration #%d of do_interpretation loop, method:\n", i);
      method()->print_name(tty);
      tty->print("\n\n");
    }
#endif
    _conflict = false;
    _monitor_safe = true;
    // init_state is now called from init_basic_blocks.  The length of a
    // state vector cannot be determined until we have made a pass through
    // the bytecodes counting the possible monitor entries.
    if (!_got_error) init_basic_blocks();
    if (!_got_error) setup_method_entry_state();
    if (!_got_error) interp_all();
    if (!_got_error) rewrite_refval_conflicts();
    i++;
  } while (_conflict && !_got_error);
}

void GenerateOopMap::init_basic_blocks() {
  D_WARN_Unimplement;
}

void GenerateOopMap::setup_method_entry_state() {

    // Initialize all locals to 'uninit' and set stack-height to 0
    make_context_uninitialized();

    // Initialize CellState type of arguments
    methodsig_to_effect(method()->signature(), method()->is_static(), vars());

    // If some references must be pre-assigned to null, then set that up
    initialize_vars();

    // This is the start state
    merge_state_into_bb(&_basic_blocks[0]);

    assert(_basic_blocks[0].changed(), "we are not getting off the ground");
}

// The instruction at bci is changing size by "delta".  Update the basic blocks.
void GenerateOopMap::update_basic_blocks(int bci, int delta,
                                         int new_method_size) {
  assert(new_method_size >= method()->code_size() + delta,
         "new method size is too small");

  BitMap::bm_word_t* new_bb_hdr_bits =
    NEW_RESOURCE_ARRAY(BitMap::bm_word_t,
                       BitMap::word_align_up(new_method_size));
  _bb_hdr_bits.set_map(new_bb_hdr_bits);
  _bb_hdr_bits.set_size(new_method_size);
  _bb_hdr_bits.clear();


  for(int k = 0; k < _bb_count; k++) {
    if (_basic_blocks[k]._bci > bci) {
      _basic_blocks[k]._bci     += delta;
      _basic_blocks[k]._end_bci += delta;
    }
    _bb_hdr_bits.at_put(_basic_blocks[k]._bci, true);
  }
}

//
// Initvars handling
//

void GenerateOopMap::initialize_vars() {
  for (int k = 0; k < _init_vars->length(); k++)
    _state[_init_vars->at(k)] = CellTypeState::make_slot_ref(k);
}

void GenerateOopMap::add_to_ref_init_set(int localNo) {

  if (TraceNewOopMapGeneration)
    tty->print_cr("Added init vars: %d", localNo);

  // Is it already in the set?
  if (_init_vars->contains(localNo) )
    return;

   _init_vars->append(localNo);
}

//
// Interpreration code
//

void GenerateOopMap::interp_all() {
  bool change = true;

  while (change && !_got_error) {
    change = false;
    for (int i = 0; i < _bb_count && !_got_error; i++) {
      BasicBlock *bb = &_basic_blocks[i];
      if (bb->changed()) {
         if (_got_error) return;
         change = true;
         bb->set_changed(false);
         interp_bb(bb);
      }
    }
  }
}

void GenerateOopMap::interp_bb(BasicBlock *bb) {
  D_WARN_Unimplement;
}


void GenerateOopMap::report_monitor_mismatch(const char *msg) {
#ifndef PRODUCT
  tty->print("    Monitor mismatch in method ");
  method()->print_short_name(tty);
  tty->print_cr(": %s", msg);
#endif
}

void GenerateOopMap::print_states(outputStream *os,
                                  CellTypeState* vec, int num) {
  for (int i = 0; i < num; i++) {
    vec[i].print(tty);
  }
}

void GenerateOopMap::check_type(CellTypeState expected, CellTypeState actual) {
  if (!expected.equal_kind(actual)) {
    verify_error("wrong type on stack (found: %c expected: %c)", actual.to_char(), expected.to_char());
  }
}

void GenerateOopMap::ppstore(CellTypeState *in, int loc_no) {
  while(!(*in).is_bottom()) {
    CellTypeState expected =*in++;
    CellTypeState actual   = pop();
    check_type(expected, actual);
    assert(loc_no >= 0, "sanity check");
    set_var(loc_no++, actual);
  }
}

void GenerateOopMap::ppload(CellTypeState *out, int loc_no) {
  while(!(*out).is_bottom()) {
    CellTypeState out1 = *out++;
    CellTypeState vcts = get_var(loc_no);
    assert(out1.can_be_reference() || out1.can_be_value(),
           "can only load refs. and values.");
    if (out1.is_reference()) {
      assert(loc_no>=0, "sanity check");
      if (!vcts.is_reference()) {
        // We were asked to push a reference, but the type of the
        // variable can be something else
        _conflict = true;
        if (vcts.can_be_uninit()) {
          // It is a ref-uninit conflict (at least). If there are other
          // problems, we'll get them in the next round
          add_to_ref_init_set(loc_no);
          vcts = out1;
        } else {
          // It wasn't a ref-uninit conflict. So must be a
          // ref-val or ref-pc conflict. Split the variable.
          record_refval_conflict(loc_no);
          vcts = out1;
        }
        push(out1); // recover...
      } else {
        push(vcts); // preserve reference.
      }
      // Otherwise it is a conflict, but one that verification would
      // have caught if illegal. In particular, it can't be a topCTS
      // resulting from mergeing two difference pcCTS's since the verifier
      // would have rejected any use of such a merge.
    } else {
      push(out1); // handle val/init conflict
    }
    loc_no++;
  }
}

void GenerateOopMap::ppdupswap(int poplen, const char *out) {
  CellTypeState actual[5];
  assert(poplen < 5, "this must be less than length of actual vector");

  // pop all arguments
  for(int i = 0; i < poplen; i++) actual[i] = pop();

  // put them back
  char push_ch = *out++;
  while (push_ch != '\0') {
    int idx = push_ch - '1';
    assert(idx >= 0 && idx < poplen, "wrong arguments");
    push(actual[idx]);
    push_ch = *out++;
  }
}

void GenerateOopMap::ppop1(CellTypeState out) {
  CellTypeState actual = pop();
  check_type(out, actual);
}

void GenerateOopMap::ppop(CellTypeState *out) {
  while (!(*out).is_bottom()) {
    ppop1(*out++);
  }
}

void GenerateOopMap::ppush1(CellTypeState in) {
  assert(in.is_reference() | in.is_value(), "sanity check");
  push(in);
}

void GenerateOopMap::ppush(CellTypeState *in) {
  while (!(*in).is_bottom()) {
    ppush1(*in++);
  }
}

void GenerateOopMap::pp(CellTypeState *in, CellTypeState *out) {
  ppop(in);
  ppush(out);
}

void GenerateOopMap::pp_new_ref(CellTypeState *in, int bci) {
  ppop(in);
  ppush1(CellTypeState::make_line_ref(bci));
}

void GenerateOopMap::ppop_any(int poplen) {
  if (_stack_top >= poplen) {
    _stack_top -= poplen;
  } else {
    verify_error("stack underflow");
  }
}

// Replace all occurences of the state 'match' with the state 'replace'
// in our current state vector.
void GenerateOopMap::replace_all_CTS_matches(CellTypeState match,
                                             CellTypeState replace) {
  int i;
  int len = _max_locals + _stack_top;
  bool change = false;

  for (i = len - 1; i >= 0; i--) {
    if (match.equal(_state[i])) {
      _state[i] = replace;
    }
  }

  if (_monitor_top > 0) {
    int base = _max_locals + _max_stack;
    len = base + _monitor_top;
    for (i = len - 1; i >= base; i--) {
      if (match.equal(_state[i])) {
        _state[i] = replace;
      }
    }
  }
}

void GenerateOopMap::do_checkcast() {
  CellTypeState actual = pop();
  check_type(refCTS, actual);
  push(actual);
}

void GenerateOopMap::do_monitorenter(int bci) {
  CellTypeState actual = pop();
  if (_monitor_top == bad_monitors) {
    return;
  }

  // Bail out when we get repeated locks on an identical monitor.  This case
  // isn't too hard to handle and can be made to work if supporting nested
  // redundant synchronized statements becomes a priority.
  //
  // See also "Note" in do_monitorexit(), below.
  if (actual.is_lock_reference()) {
    _monitor_top = bad_monitors;
    _monitor_safe = false;

    if (TraceMonitorMismatch) {
      report_monitor_mismatch("nested redundant lock -- bailout...");
    }
    return;
  }

  CellTypeState lock = CellTypeState::make_lock_ref(bci);
  check_type(refCTS, actual);
  if (!actual.is_info_top()) {
    replace_all_CTS_matches(actual, lock);
    monitor_push(lock);
  }
}

void GenerateOopMap::do_monitorexit(int bci) {
  CellTypeState actual = pop();
  if (_monitor_top == bad_monitors) {
    return;
  }
  check_type(refCTS, actual);
  CellTypeState expected = monitor_pop();
  if (!actual.is_lock_reference() || !expected.equal(actual)) {
    // The monitor we are exiting is not verifiably the one
    // on the top of our monitor stack.  This causes a monitor
    // mismatch.
    _monitor_top = bad_monitors;
    _monitor_safe = false;

    // We need to mark this basic block as changed so that
    // this monitorexit will be visited again.  We need to
    // do this to ensure that we have accounted for the
    // possibility that this bytecode will throw an
    // exception.
    BasicBlock* bb = get_basic_block_containing(bci);
    guarantee(bb != NULL, "no basic block for bci");
    bb->set_changed(true);
    bb->_monitor_top = bad_monitors;

    if (TraceMonitorMismatch) {
      report_monitor_mismatch("improper monitor pair");
    }
  } else {
    // This code is a fix for the case where we have repeated
    // locking of the same object in straightline code.  We clear
    // out the lock when it is popped from the monitor stack
    // and replace it with an unobtrusive reference value that can
    // be locked again.
    //
    // Note: when generateOopMap is fixed to properly handle repeated,
    //       nested, redundant locks on the same object, then this
    //       fix will need to be removed at that time.
    replace_all_CTS_matches(actual, CellTypeState::make_line_ref(bci));
  }
}

void GenerateOopMap::do_return_monitor_check() {
  if (_monitor_top > 0) {
    // The monitor stack must be empty when we leave the method
    // for the monitors to be properly matched.
    _monitor_safe = false;

    // Since there are no successors to the *return bytecode, it
    // isn't necessary to set _monitor_top to bad_monitors.

    if (TraceMonitorMismatch) {
      report_monitor_mismatch("non-empty monitor stack at return");
    }
  }
}

void GenerateOopMap::do_jsr(int targ_bci) {
  push(CellTypeState::make_addr(targ_bci));
}



void GenerateOopMap::do_ldc(int bci) {
  Bytecode_loadconstant ldc(method(), bci);
  ConstantPool* cp  = method()->constants();
  constantTag tag = cp->tag_at(ldc.pool_index()); // idx is index in resolved_references
  BasicType       bt  = ldc.result_type();
  CellTypeState   cts;
  if (tag.basic_type() == T_OBJECT) {
    assert(!tag.is_string_index() && !tag.is_klass_index(), "Unexpected index tag");
    assert(bt == T_OBJECT, "Guard is incorrect");
    cts = CellTypeState::make_line_ref(bci);
  } else {
    assert(bt != T_OBJECT, "Guard is incorrect");
    cts = valCTS;
  }
  ppush1(cts);
}

void GenerateOopMap::do_multianewarray(int dims, int bci) {
  assert(dims >= 1, "sanity check");
  for(int i = dims -1; i >=0; i--) {
    ppop1(valCTS);
  }
  ppush1(CellTypeState::make_line_ref(bci));
}

void GenerateOopMap::do_astore(int idx) {
  CellTypeState r_or_p = pop();
  if (!r_or_p.is_address() && !r_or_p.is_reference()) {
    // We actually expected ref or pc, but we only report that we expected a ref. It does not
    // really matter (at least for now)
    verify_error("wrong type on stack (found: %c, expected: {pr})", r_or_p.to_char());
    return;
  }
  set_var(idx, r_or_p);
}

// Copies bottom/zero terminated CTS string from "src" into "dst".
//   Does NOT terminate with a bottom. Returns the number of cells copied.
int GenerateOopMap::copy_cts(CellTypeState *dst, CellTypeState *src) {
  int idx = 0;
  while (!src[idx].is_bottom()) {
    dst[idx] = src[idx];
    idx++;
  }
  return idx;
}

void GenerateOopMap::do_field(int is_get, int is_static, int idx, int bci) {
  // Dig up signature for field in constant pool
  ConstantPool* cp     = method()->constants();
  int nameAndTypeIdx     = cp->name_and_type_ref_index_at(idx);
  int signatureIdx       = cp->signature_ref_index_at(nameAndTypeIdx);
  Symbol* signature      = cp->symbol_at(signatureIdx);

  // Parse signature (espcially simple for fields)
  assert(signature->utf8_length() > 0, "field signatures cannot have zero length");
  // The signature is UFT8 encoded, but the first char is always ASCII for signatures.
  char sigch = (char)*(signature->base());
  CellTypeState temp[4];
  CellTypeState *eff  = sigchar_to_effect(sigch, bci, temp);

  CellTypeState in[4];
  CellTypeState *out;
  int i =  0;

  if (is_get) {
    out = eff;
  } else {
    out = epsilonCTS;
    i   = copy_cts(in, eff);
  }
  if (!is_static) in[i++] = CellTypeState::ref;
  in[i] = CellTypeState::bottom;
  assert(i<=3, "sanity check");
  pp(in, out);
}

void GenerateOopMap::do_method(int is_static, int is_interface, int idx, int bci) {
  D_WARN_Unimplement;
}

// This is used to parse the signature for fields, since they are very simple...
CellTypeState *GenerateOopMap::sigchar_to_effect(char sigch, int bci, CellTypeState *out) {
  // Object and array
  if (sigch=='L' || sigch=='[') {
    out[0] = CellTypeState::make_line_ref(bci);
    out[1] = CellTypeState::bottom;
    return out;
  }
  if (sigch == 'J' || sigch == 'D' ) return vvCTS;  // Long and Double
  if (sigch == 'V' ) return epsilonCTS;             // Void
  return vCTS;                                      // Otherwise
}

long GenerateOopMap::_total_byte_count = 0;

// This function assumes "bcs" is at a "ret" instruction and that the vars
// state is valid for that instruction. Furthermore, the ret instruction
// must be the last instruction in "bb" (we store information about the
// "ret" in "bb").

//
// Debug method
//
char* GenerateOopMap::state_vec_to_string(CellTypeState* vec, int len) {
#ifdef ASSERT
  int checklen = MAX3(_max_locals, _max_stack, _max_monitors) + 1;
  assert(len < checklen, "state_vec_buf overflow");
#endif
  for (int i = 0; i < len; i++) _state_vec_buf[i] = vec[i].to_char();
  _state_vec_buf[len] = 0;
  return _state_vec_buf;
}

void GenerateOopMap::print_time() {
}

//
//  ============ Main Entry Point ===========
//
GenerateOopMap::GenerateOopMap(methodHandle method) {
  // We have to initialize all variables here, that can be queried directly
  _method = method;
  _max_locals=0;
  _init_vars = NULL;

#ifndef PRODUCT
  // If we are doing a detailed trace, include the regular trace information.
  if (TraceNewOopMapGenerationDetailed) {
    TraceNewOopMapGeneration = true;
  }
#endif
}

void GenerateOopMap::compute_map(TRAPS) {
#ifndef PRODUCT
  if (TimeOopMap2) {
    method()->print_short_name(tty);
    tty->print("  ");
  }
  if (TimeOopMap) {
    _total_byte_count += method()->code_size();
  }
#endif

  // Initialize values
  _got_error      = false;
  _conflict       = false;
  _max_locals     = method()->max_locals();
  _max_stack      = method()->max_stack();
  _has_exceptions = (method()->has_exception_handler());
  _nof_refval_conflicts = 0;
  _init_vars      = new GrowableArray<intptr_t>(5);  // There are seldom more than 5 init_vars
  _report_result  = false;
  _report_result_for_send = false;
  _new_var_map    = NULL;
  _ret_adr_tos    = new GrowableArray<intptr_t>(5);  // 5 seems like a good number;
  _did_rewriting  = false;
  _did_relocation = false;

  if (TraceNewOopMapGeneration) {
    tty->print("Method name: %s\n", method()->name()->as_C_string());
    if (Verbose) {
      _method->print_codes();
      tty->print_cr("Exception table:");
      ExceptionTable excps(method());
      for(int i = 0; i < excps.length(); i ++) {
        tty->print_cr("[%d - %d] -> %d",
                      excps.start_pc(i), excps.end_pc(i), excps.handler_pc(i));
      }
    }
  }

  // if no code - do nothing
  // compiler needs info
  if (method()->code_size() == 0 || _max_locals + method()->max_stack() == 0) {
    fill_stackmap_prolog(0);
    fill_stackmap_epilog();
    return;
  }
  // Step 1: Compute all jump targets and their return value
  if (!_got_error)
    _rt.compute_ret_table(_method);

  // Step 2: Find all basic blocks and count GC points
  if (!_got_error)
    mark_bbheaders_and_count_gc_points();

  // Step 3: Calculate stack maps
  if (!_got_error)
    do_interpretation();

  // Step 4:Return results
  if (!_got_error && report_results())
     report_result();

  if (_got_error) {
    ShouldNotReachHere();
  }
}

// Error handling methods
// These methods create an exception for the current thread which is thrown
// at the bottom of the call stack, when it returns to compute_map().  The
// _got_error flag controls execution.  NOT TODO: The VM exception propagation
// mechanism using TRAPS/CHECKs could be used here instead but it would need
// to be added as a parameter to every function and checked for every call.
// The tons of extra code it would generate didn't seem worth the change.
//
void GenerateOopMap::error_work(const char *format, va_list ap) {
  D_WARN_Unimplement;
}

void GenerateOopMap::report_error(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  error_work(format, ap);
}

void GenerateOopMap::verify_error(const char *format, ...) {
  D_WARN_Unimplement;
}

//
// Report result opcodes
//
void GenerateOopMap::report_result() {

  if (TraceNewOopMapGeneration) tty->print_cr("Report result pass");

  // We now want to report the result of the parse
  _report_result = true;

  // Prolog code
  fill_stackmap_prolog(_gc_points);

   // Mark everything changed, then do one interpretation pass.
  for (int i = 0; i<_bb_count; i++) {
    if (_basic_blocks[i].is_reachable()) {
      _basic_blocks[i].set_changed(true);
      interp_bb(&_basic_blocks[i]);
    }
  }

  // Note: Since we are skipping dead-code when we are reporting results, then
  // the no. of encountered gc-points might be fewer than the previously number
  // we have counted. (dead-code is a pain - it should be removed before we get here)
  fill_stackmap_epilog();

  // Report initvars
  fill_init_vars(_init_vars);

  _report_result = false;
}

void GenerateOopMap::result_for_basicblock(int bci) {
 if (TraceNewOopMapGeneration) tty->print_cr("Report result pass for basicblock");

  // We now want to report the result of the parse
  _report_result = true;

  // Find basicblock and report results
  BasicBlock* bb = get_basic_block_containing(bci);
  guarantee(bb != NULL, "no basic block for bci");
  assert(bb->is_reachable(), "getting result from unreachable basicblock");
  bb->set_changed(true);
  interp_bb(bb);
}

//
// Conflict handling code
//

void GenerateOopMap::record_refval_conflict(int varNo) {
  assert(varNo>=0 && varNo< _max_locals, "index out of range");

  if (TraceOopMapRewrites) {
     tty->print("### Conflict detected (local no: %d)\n", varNo);
  }

  if (!_new_var_map) {
    _new_var_map = NEW_RESOURCE_ARRAY(int, _max_locals);
    for (int k = 0; k < _max_locals; k++)  _new_var_map[k] = k;
  }

  if ( _new_var_map[varNo] == varNo) {
    // Check if max. number of locals has been reached
    if (_max_locals + _nof_refval_conflicts >= MAX_LOCAL_VARS) {
      report_error("Rewriting exceeded local variable limit");
      return;
    }
    _new_var_map[varNo] = _max_locals + _nof_refval_conflicts;
    _nof_refval_conflicts++;
  }
}

void GenerateOopMap::rewrite_refval_conflicts()
{
  // We can get here two ways: Either a rewrite conflict was detected, or
  // an uninitialize reference was detected. In the second case, we do not
  // do any rewriting, we just want to recompute the reference set with the
  // new information

  int nof_conflicts = 0;              // Used for debugging only

  if ( _nof_refval_conflicts == 0 )
     return;

  // Check if rewrites are allowed in this parse.
  if (!allow_rewrites() && !IgnoreRewrites) {
    fatal("Rewriting method not allowed at this stage");
  }


  // This following flag is to tempoary supress rewrites. The locals that might conflict will
  // all be set to contain values. This is UNSAFE - however, until the rewriting has been completely
  // tested it is nice to have.
  if (IgnoreRewrites) {
    if (Verbose) {
       tty->print("rewrites suppressed for local no. ");
       for (int l = 0; l < _max_locals; l++) {
         if (_new_var_map[l] != l) {
           tty->print("%d ", l);
           vars()[l] = CellTypeState::value;
         }
       }
       tty->cr();
    }

    // That was that...
    _new_var_map = NULL;
    _nof_refval_conflicts = 0;
    _conflict = false;

    return;
  }

  // Tracing flag
  _did_rewriting = true;

  if (TraceOopMapRewrites) {
    tty->print_cr("ref/value conflict for method %s - bytecodes are getting rewritten", method()->name()->as_C_string());
    method()->print();
    method()->print_codes();
  }

  assert(_new_var_map!=NULL, "nothing to rewrite");
  assert(_conflict==true, "We should not be here");

  compute_ret_adr_at_TOS();
  if (!_got_error) {
    for (int k = 0; k < _max_locals && !_got_error; k++) {
      if (_new_var_map[k] != k) {
        if (TraceOopMapRewrites) {
          tty->print_cr("Rewriting: %d -> %d", k, _new_var_map[k]);
        }
        rewrite_refval_conflict(k, _new_var_map[k]);
        if (_got_error) return;
        nof_conflicts++;
      }
    }
  }

  assert(nof_conflicts == _nof_refval_conflicts, "sanity check");

  // Adjust the number of locals
  method()->set_max_locals(_max_locals+_nof_refval_conflicts);
  _max_locals += _nof_refval_conflicts;

  // That was that...
  _new_var_map = NULL;
  _nof_refval_conflicts = 0;
}

void GenerateOopMap::rewrite_refval_conflict(int from, int to) {
  D_WARN_Unimplement;
}

/* If the current instruction is one that uses local variable "from"
   in a ref way, change it to use "to". There's a subtle reason why we
   renumber the ref uses and not the non-ref uses: non-ref uses may be
   2 slots wide (double, long) which would necessitate keeping track of
   whether we should add one or two variables to the method. If the change
   affected the width of some instruction, returns "TRUE"; otherwise, returns "FALSE".
   Another reason for moving ref's value is for solving (addr, ref) conflicts, which
   both uses aload/astore methods.
*/

// Returns true if expanding was succesful. Otherwise, reports an error and
// returns false.
void GenerateOopMap::expand_current_instr(int bci, int ilen, int newIlen, u_char inst_buffer[]) {
  D_WARN_Unimplement;
}

// Return true iff the top of the operand stack holds a return address at
// the current instruction
bool GenerateOopMap::stack_top_holds_ret_addr(int bci) {
  for(int i = 0; i < _ret_adr_tos->length(); i++) {
    if (_ret_adr_tos->at(i) == bci)
      return true;
  }

  return false;
}

void GenerateOopMap::compute_ret_adr_at_TOS() {
  D_WARN_Unimplement;
}

void GenerateOopMap::update_ret_adr_at_TOS(int bci, int delta) {
  for(int i = 0; i < _ret_adr_tos->length(); i++) {
    int v = _ret_adr_tos->at(i);
    if (v > bci)  _ret_adr_tos->at_put(i, v + delta);
  }
}

// ===================================================================

#ifndef PRODUCT
int ResolveOopMapConflicts::_nof_invocations  = 0;
int ResolveOopMapConflicts::_nof_rewrites     = 0;
int ResolveOopMapConflicts::_nof_relocations  = 0;
#endif

methodHandle ResolveOopMapConflicts::do_potential_rewrite(TRAPS) {
  compute_map(CHECK_(methodHandle()));

#ifndef PRODUCT
  // Tracking and statistics
  if (PrintRewrites) {
    _nof_invocations++;
    if (did_rewriting()) {
      _nof_rewrites++;
      if (did_relocation()) _nof_relocations++;
      tty->print("Method was rewritten %s: ", (did_relocation()) ? "and relocated" : "");
      method()->print_value(); tty->cr();
      tty->print_cr("Cand.: %d rewrts: %d (%d%%) reloc.: %d (%d%%)",
          _nof_invocations,
          _nof_rewrites,    (_nof_rewrites    * 100) / _nof_invocations,
          _nof_relocations, (_nof_relocations * 100) / _nof_invocations);
    }
  }
#endif
  return methodHandle(THREAD, method());
}
