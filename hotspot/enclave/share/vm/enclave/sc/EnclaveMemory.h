//
// Created by max on 1/20/18.
//

#ifndef HOTSPOT_EV_MEMORY_H
#define HOTSPOT_EV_MEMORY_H

// this class allocate a Eden alike memory space for enclave
// currently it only initialize a fix number of bytes (10240 bytes)

#include <list>
#include <queue>
#include "oops/oop.hpp"
#include "EnclaveDebug.h"
#include "EnclaveGC.h"

#define ENCLAVE_ALLOCATION_THREADHOLD 2

#ifdef DB_MEM
#define MEM_DEBUG(...) printf(__VA_ARGS__)
#else
#define MEM_DEBUG(...)
#endif

#define EMALLOC(size) ::malloc(size); printf("malloc %d bytes %s %d\n", size, __FILE__, __LINE__)

#define MEM_TRACE(size) MEM_DEBUG("[MEM] File: %s:%d . %d\n", __FILE__, __LINE__,size)

typedef address (*mem_allocator)(int, JavaThread*);

typedef address (*heap_allocator)(void**, void**, size_t);

// Note: klass_obj_array and klass_type_array allocate array with its arrayklass (instead of elementKlass) as input
// TODO: make it a single api
#define DO_MEMORY_METHOD(template) \
    template(vm_new_obj,                vm_new_obj(JavaThread* thread, Klass* klass),                             (thread, klass))                    \
    template(vm_type_array,             vm_type_array(JavaThread* thread, BasicType type, int size),                 (thread, type, size))               \
    template(klass_new_array,           klass_new_array(JavaThread* thread, oop cls, int size),                        (thread, cls, size))                \
    template(klass_multi_array,         klass_multi_array(JavaThread* thread, oop cls, void *dim),                       (thread, cls, dim))                 \
    template(klass_multi_array_helper,  klass_multi_array_helper(JavaThread* thread, Klass *klass, jint *len_arr, int rank),    (thread, klass, len_arr, rank))     \
    template(klass_obj_array,           klass_obj_array(JavaThread* thread, Klass *klass, jint length),                (thread, klass, length))            \
    template(klass_type_array,          klass_type_array(JavaThread* thread, Klass *klass, jint length),                (thread, klass, length))            \
    template(cpoll_new_array,           cpoll_new_array(JavaThread* thread, void *pool, int index, int length),        (thread, pool, index, length))      \
    template(cpoll_multi_array,         cpoll_multi_array(JavaThread* thread, void *pool, int index, jint *dim_array, int nofd), (thread, pool, index, dim_array, nofd))

#define DEFINE_FUNC_ADDRESS(name, header, pars) static address static_##header { \
   return EnclaveMemory::enclaveMemory->name pars; \
} \

#define DEFINE_FUNC_HEADER(ret, header, pars) address header;

class EnclaveMemory {
public:
    enum {
        _lh_neutral_value           = 0,  // neutral non-array non-instance value
        _lh_instance_slow_path_bit  = 0x01,
        _lh_log2_element_size_shift = BitsPerByte*0,
        _lh_log2_element_size_mask  = BitsPerLong-1,
        _lh_element_type_shift      = BitsPerByte*1,
        _lh_element_type_mask       = right_n_bits(BitsPerByte),  // shifted mask
        _lh_header_size_shift       = BitsPerByte*2,
        _lh_header_size_mask        = right_n_bits(BitsPerByte),  // shifted mask
        _lh_array_tag_bits          = 2,
        _lh_array_tag_shift         = BitsPerInt - _lh_array_tag_bits,
        _lh_array_tag_type_value    = ~0x00,  // 0xC0000000 >> 30
        _lh_array_tag_obj_value     = ~0x01   // 0x80000000 >> 30
    };

    DO_MEMORY_METHOD(DEFINE_FUNC_ADDRESS)

    static bool print_copy;

    static void* _type_array_klass;

    static Klass** wk_classes;

    static std::list<void*> *thread_local_mem;

    static void** heap_top;
    static void** heap_bottom;

    static void* heap_buffer;

    static void* heap_buffer_start;

    static address top_addr();
    static address bottom_addr();

    static void* init();

    static address allocate(int size, JavaThread* thread);

    static address allocate_heap(int size, JavaThread* thread);

    static EnclaveMemory* enclaveMemory;

    static EnclaveMemory* heapMemory;

    static heap_allocator fast_heap_alloc;

    static int oop_size(oop obj);

    mem_allocator alloc;

    EnclaveMemory(mem_allocator m) {
        alloc = m;
    }

//    static address interp_allocate(ConstantPool *pool, int index);

    static oop vm_new_string(JavaThread*, char* name);

    static oop vm_new_basic_string_final(Thread*);
    static oop vm_new_basic_char_final(int length, Thread*);

    DO_MEMORY_METHOD(DEFINE_FUNC_HEADER)

    static bool is_type_oop(Klass *klass);

    static int array_layout_helper(BasicType etype);

    static int array_layout_helper(int tag, int hsize, BasicType etype, int log2_esize);
//    static Klass* _typeArrayKlassObjs[T_VOID+1];

    // allocate a page alligned memory
    // currently we use malloc to simulate this, we may use SGX-2 instructions to do this in the future
    static void* new_page();

    // TODO-jianyu: currently it only supports copying object array or same type array
    static void copy_array(void* s, int s_pos, void* d, int d_pos, int length);

    template <class T>
    static inline void claim_or_forward_depth(T* p, std::queue<StarTask> *tasks) {
        if (p != NULL) { // XXX: error if p != NULL here
            oop o = oopDesc::load_decode_heap_oop_not_null(p);
            if (o->is_forwarded()) {
                o = o->forwardee();
                // Card mark
//                if (PSScavenge::is_obj_in_young(o)) {
//                    PSScavenge::card_table()->inline_write_ref_field_gc(p, o);
//                }
                oopDesc::encode_store_heap_oop_not_null(p, o);
            } else {
                tasks->push(StarTask(p));
            }
        }
    }

    static void set_print() {
        print_copy = true;
    }

    static void do_copy(oop *obj, std::queue<StarTask> *tasks, std::queue<oop> *clear_tasks);

    static void deep_copy(oop* obj);

    // allocation of thread-local memory
    static void* emalloc(size_t size);
    static void efree(void* ptr);
    static void* ermalloc(void*ptr, size_t old_size, size_t size);
    static std::list<void*>* emalloc_clean();
};

#endif //HOTSPOT_EV_MEMORY_H
