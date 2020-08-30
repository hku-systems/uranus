//
// Created by max on 1/20/18.
//

#include "EnclaveMemory.h"
#include "classfile/javaClasses.hpp"
#include "oops/objArrayKlass.hpp"
#include <queue>
#include "utilities/copy.hpp"
#include "oops/markOop.hpp"
#include "EnclaveOcall.h"
#include "EnclaveGC.h"
#include "EnclaveException.h"
#include "securecompiler.h"


static EnclaveMemory* EnclaveMemory::enclaveMemory = NULL;

static EnclaveMemory* EnclaveMemory::heapMemory = NULL;

static heap_allocator EnclaveMemory::fast_heap_alloc = NULL;

void** EnclaveMemory::heap_top = NULL;
void** EnclaveMemory::heap_bottom = NULL;

pthread_spinlock_t heap_lock;

#define HEAP_BUFFER_SIZE 128*M

void* EnclaveMemory::heap_buffer = NULL;

void* EnclaveMemory::heap_buffer_start = NULL;

static int object_size(int lh, int length) {
    int instance_header_size = Klass::layout_helper_header_size(lh);
    int element_shift = Klass::layout_helper_log2_element_size(lh);
    DEBUG_ONLY(BasicType etype = Klass::layout_helper_element_type(lh));
    assert(length <= arrayOopDesc::max_array_length(etype), "no overflow");

    julong size_in_bytes = (juint)length;
    size_in_bytes <<= element_shift;
    size_in_bytes += instance_header_size;
    julong size_in_words = ((size_in_bytes + (HeapWordSize-1)) >> LogHeapWordSize);
    assert(size_in_words <= (julong)max_jint, "no overflow");

    return align_object_size((intptr_t)size_in_words);
}

address EnclaveMemory::top_addr() {
    return (address)&EnclaveGC::_eden_start;
}
address EnclaveMemory::bottom_addr() {
    return (address)&EnclaveGC::_eden_end;
}

  static int layout_helper_to_size_helper(jint lh) {
    assert(lh > (jint)_lh_neutral_value, "must be instance");
    // Note that the following expression discards _lh_instance_slow_path_bit.
    return lh >> LogHeapWordSize;
  }

void* aligned_malloc(size_t required_bytes, size_t alignment)
{
    void* p1; // original block
    void** p2; // aligned block
    int offset = alignment - 1 + sizeof(void*);
    if ((p1 = (void*)malloc(required_bytes + offset)) == NULL)
    {
       return NULL;
    }
    p2 = (void**)(((size_t)(p1) + offset) & ~(alignment - 1));
    p2[-1] = p1;
    return p2;
}

int EnclaveMemory::oop_size(oop obj) {
    if (obj->is_typeArray()) {
        return object_size(obj->klass()->_layout_helper, typeArrayOop(obj)->length());
    } else if (obj->is_objArray()) {
        return objArrayOopDesc::object_size(objArrayOop(obj)->length());
    } else if (obj->is_instance()) {
        return Klass::layout_helper_to_size_helper(obj->klass()->_layout_helper);
    } else {
        return java_lang_Class::oop_size(obj);
    }
}

oop EnclaveMemory::vm_new_basic_char_final(int length, Thread* THREAD) {
    MEM_TRACE(length);
    // calculate how much space should be allocate
    size_t size = object_size(array_layout_helper(T_CHAR), length);
    HeapWord *obj = new HeapWord[size];
    if (obj == NULL) {
        THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
    // init the space
    const size_t hs = oopDesc::header_size();
    // not neccessary, as we do not use compressed class
//    ((oop)obj)->set_klass_gap(0);
    // TODO: fill all bytes to be zeros
    Copy::fill_to_aligned_words(obj + hs, size - hs);
//    memset((obj) + hs, 0,(size - hs) * sizeof(char*));

    // TODO: set up the klass
    Klass** klass_array = (Klass**)Universe::typeArrayKlassObj();
    Klass* array_klass = klass_array[T_CHAR];
    ((arrayOop)obj)->set_klass(array_klass);
    // setup array
    ((arrayOop)obj)->set_length(length);

    // it is possible that the memory is not right
    ((oop)obj)->set_mark(markOopDesc::prototype());
    return (oop)obj;
}

oop EnclaveMemory::vm_new_basic_string_final(Thread* THREAD) {
    size_t size = layout_helper_to_size_helper(SystemDictionary::String_klass()->_layout_helper);
    MEM_TRACE(size);
    HeapWord *obj = new HeapWord[size];
    if (obj == NULL) {
        THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
    const size_t hs = oopDesc::header_size();
//  ((oop)obj)->set_klass_gap(0);
    Copy::fill_to_aligned_words((HeapWord*)obj + hs, size - hs);
//    memset((obj) + hs, 0,(size - hs) * sizeof(char*));

    ((oop)obj)->set_klass(SystemDictionary::String_klass());
    ((oop)obj)->set_mark(markOopDesc::prototype());
    return (oop)obj;
}

oop EnclaveMemory::vm_new_string(JavaThread* THREAD, char* utf8_string) {
    int length = UTF8::unicode_length(utf8_string);
    jchar* chars = NEW_RESOURCE_ARRAY(jchar, length);
    UTF8::convert_to_unicode(utf8_string, chars, length);
    Handle string = java_lang_String::create_from_unicode(chars, length, (Thread*)CHECK_NULL);
    return string();
}

bool EnclaveMemory::is_type_oop(Klass *klass) {
    return klass->_layout_helper >=
            (juint)(EnclaveMemory::_lh_array_tag_type_value << EnclaveMemory::_lh_array_tag_shift);
}

void EnclaveMemory::copy_array(void* s, int s_pos, void* d, int d_pos, int length) {
    arrayOop s_oop = (arrayOop)s;
    arrayOop d_oop = (arrayOop)d;
    if (length == 0) {
/*
        printf(D_WARN("ArrayCopy")" copy array with 0 elements %lx:%s:%d -> %lx:%s:%d\n",
               (intptr_t)s, s_oop->klass()->name()->as_C_string(), s_pos,(intptr_t)d, d_oop->klass()->name()->as_C_string(), d_pos);
*/
        return;
    }

/*
    printf(D_INFO("MEMORY")" copy %s:%d %s:%d %d\n",
           s_oop->klass()->name()->as_C_string(), s_pos,
           d_oop->klass()->name()->as_C_string(), d_pos, length);
    printf(D_INFO("ArrayCopy")" copy from %lx:%s -> %lx:%s\n",
           (intptr_t)s, s_oop->klass()->name()->as_C_string(),
           (intptr_t)d, d_oop->klass()->name()->as_C_string());
*/

    if (is_type_oop(s_oop->klass())) {
        int lh = s_oop->klass()->_layout_helper;
        int l2es = ArrayKlass::layout_helper_log2_element_size(lh);
        int ihs = ArrayKlass::layout_helper_header_size(lh) / wordSize;
        char* src = (char*) ((oop*)s + ihs) + ((size_t)s_pos << l2es);
        char* dst = (char*) ((oop*)d + ihs) + ((size_t)d_pos << l2es);
        Copy::conjoint_memory_atomic(src, dst, (size_t)length << l2es);
    } else {
        oop* s_base = (oop*)((intptr_t) s_oop + arrayOopDesc::base_offset_in_bytes(T_OBJECT));
        oop* d_base = (oop*)((intptr_t) d_oop + arrayOopDesc::base_offset_in_bytes(T_OBJECT));
        Copy::conjoint_oops_atomic(s_base + s_pos, d_base + d_pos, length);
    }
}

void* EnclaveMemory::new_page() {
    return aligned_malloc(4096, 4096);
}

// void* EnclaveMemory::_type_array_klass = NULL;
bool EnclaveMemory::print_copy = true;

// Klass** EnclaveMemory::wk_classes = NULL;
//Klass* EnclaveMemory::_typeArrayKlassObjs[T_VOID+1]      = { NULL /*, NULL...*/ };

int EnclaveMemory::array_layout_helper(BasicType etype) {
    assert(etype >= T_BOOLEAN && etype <= T_OBJECT, "valid etype");
    // Note that T_ARRAY is not allowed here.
    int  hsize = arrayOopDesc::base_offset_in_bytes(etype);
    int  esize = type2aelembytes(etype);
    bool isobj = (etype == T_OBJECT);
    int  tag   =  isobj ? _lh_array_tag_obj_value : _lh_array_tag_type_value;
    int lh = array_layout_helper(tag, hsize, etype, exact_log2(esize));

    return lh;
}

int EnclaveMemory::array_layout_helper(int tag, int hsize, BasicType etype, int log2_esize) {
    return (tag        << _lh_array_tag_shift)
           |    (hsize      << _lh_header_size_shift)
           |    ((int)etype << _lh_element_type_shift)
           |    (log2_esize << _lh_log2_element_size_shift);
}

address EnclaveMemory::allocate_heap(int size, JavaThread* thread) {
    address mem = fast_heap_alloc(heap_top, heap_bottom, size * sizeof(HeapWord*));
    if (mem == NULL) {
        if (heap_buffer == NULL) {
            heap_buffer_start = heap_buffer = (void*)JVM_ENTRY_omalloc(HEAP_BUFFER_SIZE);
        }
        guarantee(heap_buffer != NULL, "heap buffer must not be NULL");
        int new_size = size * sizeof(HeapWord*);
        while (true) {
            if (heap_buffer_start + new_size < heap_buffer + HEAP_BUFFER_SIZE) {
                address obj = (address)heap_buffer_start;
                address new_top = obj + new_size;
                address result = (address)Atomic::cmpxchg_ptr(new_top, &heap_buffer_start, obj);
                if (result != obj) {
                    continue;
                }
                return obj;
            } else {
                address obj = (address)heap_buffer_start;
                address new_top = (address)(heap_buffer + new_size);
                address result = (address)Atomic::cmpxchg_ptr(new_top, &heap_buffer_start, obj);
                if (result != obj) {
                    continue;
                }
                return (address)heap_buffer;
//                printf(D_WARN("[MEM]")" allocate heap memory by ocall\n");
//                return (address)JVM_ENTRY_omalloc(size * sizeof(HeapWord*));
            }
        }
    }
    return mem;
}

// allocate heap words
address EnclaveMemory::allocate(int size, JavaThread* THREAD) {
    address ret = (address)EnclaveGC::allocate_eden(size);
    if (ret == NULL) {
        int count = 0;
        while (count < ENCLAVE_ALLOCATION_THREADHOLD && ret == NULL) {
            EnclaveGC::enclave_gc(THREAD);
            ret = (address)EnclaveGC::allocate_eden(size);
            count += 1;
        }
    }
    if (ret == NULL) {
        THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
    return ret;
}

//address EnclaveMemory::interp_allocate(ConstantPool *pool, int index) {
//
//}

address EnclaveMemory::vm_new_obj(JavaThread* THREAD, Klass *klass) {
    size_t size = layout_helper_to_size_helper(klass->_layout_helper);
    MEM_TRACE(size);
    address obj = alloc(size, THREAD);
    if (obj == NULL) {
        THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
    const size_t hs = oopDesc::header_size();
//  ((oop)obj)->set_klass_gap(0);
    Copy::fill_to_aligned_words((HeapWord*)obj + hs, size - hs);
//    memset((obj) + hs, 0,(size - hs) * sizeof(char*));

    ((oop)obj)->set_klass(klass);
    ((oop)obj)->set_mark(markOopDesc::prototype());
    return obj;
}

address EnclaveMemory::vm_type_array(JavaThread *THREAD, BasicType type, int length) {
    if (length < 0) {
        THROW_0(vmSymbols::java_lang_NegativeArraySizeException());
    }
    MEM_TRACE(length);
    // calculate how much space should be allocate
    size_t size = object_size(array_layout_helper(type), length);
    HeapWord *obj = (HeapWord*)alloc(size, THREAD);
    if (obj == NULL) {
        return NULL;
    }
    // init the space
    const size_t hs = oopDesc::header_size();
    // not neccessary, as we do not use compressed class
//    ((oop)obj)->set_klass_gap(0);
    // TODO: fill all bytes to be zeros
     Copy::fill_to_aligned_words(obj + hs, size - hs);
//    memset((obj) + hs, 0,(size - hs) * sizeof(char*));

    // TODO: set up the klass
    Klass** klass_array = (Klass**)Universe::_typeArrayKlassObjs;
    Klass* array_klass = klass_array[type];
    ((arrayOop)obj)->set_klass(array_klass);
    // setup array
    ((arrayOop)obj)->set_length(length);

    // it is possible that the memory is not right
    ((oop)obj)->set_mark(markOopDesc::prototype());
    return (address)obj;
}

void* EnclaveMemory::init() {
    EnclaveGC::init();
    enclaveMemory = new EnclaveMemory(&allocate);
    heapMemory = new EnclaveMemory(&allocate_heap);
    return EnclaveGC::_eden;
}

address EnclaveMemory::cpoll_new_array(JavaThread* THREAD, void *pool, int index, int length) {
    if (length < 0) {
        THROW_0(vmSymbols::java_lang_NegativeArraySizeException());
    }
    MEM_TRACE(length);
    Klass* klass = (Klass*)((ConstantPool*)pool)->klass_at(index, (Thread*)THREAD);

    return klass_obj_array(THREAD, klass, length);
}

address EnclaveMemory::klass_type_array(JavaThread* THREAD, Klass* klass, jint length) {
    // calculate how much space should be allocate
    size_t size = object_size(klass->_layout_helper, length);
    HeapWord *obj = (HeapWord*)alloc(size, THREAD);

    // init the space
    const size_t hs = oopDesc::header_size();
    // not neccessary, as we do not use compressed class
//    ((oop)obj)->set_klass_gap(0);
    // TODO: fill all bytes to be zeros
     Copy::fill_to_aligned_words(obj + hs, size - hs);
//    memset((obj) + hs, 0,(size - hs) * sizeof(char*));

    // TODO: set up the klass
    ((arrayOop)obj)->set_klass(klass);
    // setup array
    ((arrayOop)obj)->set_length(length);

    // it is possible that the memory is not right
    ((oop)obj)->set_mark(markOopDesc::prototype());
    return (address)obj;
}

address EnclaveMemory::klass_multi_array_helper(JavaThread* THREAD, Klass* klass, jint *len_arr, int rank) {
    int length = *len_arr;

    MEM_TRACE(length);

    if (length < 0) {
        THROW_0(vmSymbols::java_lang_NegativeArraySizeException());
    }

    if (is_type_oop(klass)) {
        return klass_type_array(THREAD, klass, *len_arr);
    }

    // TODO: reduce ocall here
    ArrayKlass *ak = ArrayKlass::cast(klass);
    address obj = klass_obj_array(THREAD, klass, length);

    if (rank > 1) {
        for (int i = 0; i < length; i++) {
            // TODO: lower dim
            ArrayKlass *ak_sub = ArrayKlass::cast(ak->_lower_dimension);
            oop sub_arr = (oop) klass_multi_array_helper(THREAD, ak_sub, &len_arr[1], rank - 1);
            // put object in the array
            oop *arr = (oop*)((intptr_t) obj + arrayOopDesc::base_offset_in_bytes(T_OBJECT));
            arr[i] = sub_arr;
        }
    } else {
        // nothing will be allocated here
    }
    return (address)obj;
}

address EnclaveMemory::klass_new_array(JavaThread* THREAD, oop klass, int size) {
    // klass is the klass of element
    MEM_TRACE(size);
    if (klass == NULL) {
        // handle null pointer exception
        THROW_0(vmSymbols::java_lang_NullPointerException());
    }
    // create a type array
    if (java_lang_Class::is_primitive(klass)) {
        BasicType type = java_lang_Class::primitive_type(klass);
        return vm_type_array(THREAD, type, size);
    } else {
        Klass* k = java_lang_Class::as_Klass(klass);
        k->array_klass((Thread*)THREAD);
        return klass_multi_array_helper(THREAD, k, &size, 1);
    }

}

address EnclaveMemory::klass_multi_array(JavaThread *THREAD, oop element_mirror, void *dim) {
    typeArrayOop dim_array = (typeArrayOop)dim;
    int len = dim_array->length();

    MEM_TRACE(len);

    if (len <= 0 || len > 10) {
        // TODO: what kind of exception?
    }

    jint dimensions[10];   // C array copy of intArrayOop
    Klass* klass;
    for (int i = 0; i < len; i++) {
        int d = dim_array->int_at(i);
        if (d < 0) {
            THROW_0(vmSymbols::java_lang_NegativeArraySizeException());
        }
        dimensions[i] = d;
    }
    int rank = len;
    if (java_lang_Class::is_primitive(element_mirror)) {
        BasicType type = java_lang_Class::primitive_type(element_mirror);
        Klass** klass_array = (Klass**)Universe::typeArrayKlassObj();
        klass = klass_array[type];
    } else {
        klass = java_lang_Class::as_Klass(element_mirror);
        if (klass->oop_is_array()) {
            int k_dim = ArrayKlass::cast(klass)->dimension();
            if (k_dim + len > 10) {
            }
            rank += k_dim;
        }
    }
    // here we may get problem
    klass = (Klass*)klass->array_klass(rank, (Thread*)THREAD);
    return klass_multi_array_helper(NULL, klass, &dimensions[0], rank);
}

address EnclaveMemory::cpoll_multi_array(JavaThread* THREAD, void *pool, int index, jint *dim_num, int nofd) {
    // TODO: when the dim is larger than 10
    // TODO: when dim is large, should we mannally init the klass??
    const int small_dims = 10;
    jint dim_array[small_dims];
    for (int index = 0;index < nofd;index++) {
        int n = - index * sizeof(char*) / sizeof(jint);
        dim_array[index] = dim_num[n];
    }
    ArrayKlass *ak = (ArrayKlass*)((ConstantPool*)pool)->klass_at(index, (Thread*)THREAD);
    return klass_multi_array_helper(THREAD, ak, &dim_array[0], nofd);
}

address EnclaveMemory::klass_obj_array(JavaThread* THREAD, Klass* klass, jint length) {
    ArrayKlass *ak = ArrayKlass::cast(klass);

    size_t size = objArrayOopDesc::object_size(length);
    address obj = (address)alloc(size, THREAD);

    if (obj == NULL) {
        return NULL;
    }

    // init the space
    const size_t hs = oopDesc::header_size();
    // not neccessary, as we do not use compressed class
//    ((oop)obj)->set_klass_gap(0);
    // TODO: fill all bytes to be zeros
    Copy::fill_to_aligned_words((HeapWord*)obj + hs, size - hs);
//    memset((obj) + hs, 0,(size - hs) * sizeof(char*));
    ((arrayOop)obj)->set_klass((Klass*)ak);
    // setup array
    ((arrayOop)obj)->set_length(length);

    // it is possible that the memory is not right
    ((oop)obj)->set_mark(markOopDesc::prototype());
    return obj;
}

oop copy_to_heap(oop o, std::queue<StarTask> *tasks, std::queue<oop> *clear_tasks) {

    oop new_obj = NULL;

    // NOTE! We must be very careful with any methods that access the mark
    // in o. There may be multiple threads racing on it, and it may be forwarded
    // at any time. Do not use oop methods for accessing the mark!
    markOop test_mark = o->mark();

    // The same test as "o->is_forwarded()"
    if (!test_mark->is_marked()) {
        bool new_obj_is_tenured = false;

        clear_tasks->push(o);

        size_t new_obj_size = o->size();

        // Try allocating obj in to-space (unless too old)
        new_obj = (oop) EnclaveMemory::allocate_heap(new_obj_size, NULL);

        guarantee(new_obj != NULL, "[ERROR] allocation should have succeeded");

        // Copy obj
        Copy::conjoint_jlongs_atomic((jlong*)o, (jlong*)new_obj,
                                     (size_t)align_object_size(new_obj_size) / HeapWordsPerLong);

        // Now we have to CAS in the header.
        if (o->cas_forward_to(new_obj, test_mark)) {
            // We won any races, we "own" this object.
            assert(new_obj == o->forwardee(), "Sanity");

            // Do the size comparison first with new_obj_size, which we
            // already have. Hopefully, only a few objects are larger than
            // _min_array_size_for_chunking, and most of them will be arrays.
            // So, the is->objArray() test would be very infrequent.
            if (false) {
                GC_DEBUG("    [GC Task] obj oop\n");
//                    // we'll chunk it
//                    oop* const masked_o = mask_chunked_array_oop(o);
//                    push_depth(masked_o);
//                    TASKQUEUE_STATS_ONLY(++_arrays_chunked; ++_masked_pushes);
            } else {
                // we'll just push its contents
                // note: jianyu, we use the same api to reduce code modifications
//                    new_obj->push_contents(NULL);
//              // do nothing if it is a type array
                Klass* k = new_obj->klass();
                if (!k->oop_is_typeArray()) {
                    // It might contain oops beyond the header, so take the virtual call.
                    if (k->oop_is_objArray()) {
                        ObjArrayKlass::oop_copy_contents((ObjArrayKlass*)k, new_obj, tasks);
                    } else {
                        InstanceKlass::oop_copy_contents((InstanceKlass*)k, new_obj, tasks);
                    }
                }
            }
        }  else {
            // We lost, someone else "owns" this object
            new_obj = o->forwardee();
        }
    } else {
        ShouldNotReachHere();
        new_obj = o->forwardee();
    }

    return new_obj;
}

void EnclaveMemory::do_copy(oop *obj, std::queue<StarTask> *tasks, std::queue<oop> *clear_tasks) {
    if (!EnclaveGC::is_obj_in_young(address(*obj)) && !within_enclave(*obj)) {
//        if (within_enclave((void*)(*obj)))
//            printf(D_ERROR("MEM")" enclave obj is not copy!!!!\n");
//        printf(D_INFO("MEM")" this is not copied %lx, %s\n", (intptr_t)(*obj), (*obj)->klass()->name()->as_C_string());
        return;
    }

    oop o = oopDesc::load_decode_heap_oop(obj);
    oop new_obj = o->is_forwarded()
                    ? o->forwardee()
                    : copy_to_heap(o, tasks, clear_tasks);
//    if (print_copy)
//        printf("copy obj from %lx -> %lx:%s\n", *obj, new_obj, new_obj->klass()->external_name());
    oopDesc::encode_store_heap_oop_not_null(obj, new_obj);
}

void EnclaveMemory::deep_copy(oop *obj) {
    std::queue<StarTask> tasks = std::queue<StarTask>();
    std::queue<oop> clear_tasks = std::queue<oop>();
    tasks.push(obj);
    oop* task;
    while(tasks.size() > 0) {
        task = tasks.front();
        tasks.pop();
/*
        if ((intptr_t)(*task) != 0)
            printf(D_INFO("DeepCopy")" copy memory %lx:%s\n", (intptr_t)(*task), (*task)->klass()->name()->as_C_string());
        else {
            printf(D_INFO("DeepCopy")" null obj\n");
        }
*/
        do_copy(task, &tasks, &clear_tasks);
    }
    oop o;
    while (clear_tasks.size() > 0) {
        o = clear_tasks.front();
        clear_tasks.pop();
        o->init_mark();
    }
}

std::list<void*>* EnclaveMemory::thread_local_mem = NULL;

void* EnclaveMemory::emalloc(size_t size) {
    void* r = malloc(size);
    guarantee(r != NULL, "malloc return NULL");
    thread_local_mem->push_back(r);
    return r;
}

void EnclaveMemory::efree(void* ptr) {
    thread_local_mem->remove(ptr);
    free(ptr);
}

void* EnclaveMemory::ermalloc(void* ptr, size_t old_size,size_t size) {
    thread_local_mem->remove(ptr);
    void* r = malloc(size);
    guarantee(r != NULL, "malloc return NULL");
    memcpy(r, ptr, old_size);
    free(ptr);
    thread_local_mem->push_back(r);
    return r;
}

std::list<void*>* EnclaveMemory::emalloc_clean() {
    std::list<void*> *tmp = thread_local_mem;
    thread_local_mem = new std::list<void*>();
    return tmp;
}