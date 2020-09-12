//
// Created by jianyu on 11/15/18.
//

#ifndef HOTSPOT_C0_RUNTIME_HPP
#define HOTSPOT_C0_RUNTIME_HPP

#include <code/codeBlob.hpp>

class StubAssembler;

#define BREAK_ENABLE true
#define BREAK_KLASS   "java/util/Hashtable"
#define BREAK_METHOD  "<init>"

#ifdef BREAK_KLASS
#define BREAK_IF(method) if(BREAK_ENABLE && strcmp(method->klass_name()->as_C_string(), BREAK_KLASS) == 0 && strcmp(method->name()->as_C_string(), BREAK_METHOD) == 0)
#else
#define BREAK_IF(method) if (BREAK_ENABLE)
#endif

#define RUNTIME1_STUBS(stub, last_entry) \
  stub(new_instance)                 \
  stub(new_type_array)               \
  stub(new_object_array)             \
  stub(new_multi_array)              \
  stub(gc_barrier)                   \
  stub(access_field_patching)        \
  stub(load_klass_patching)          \
  stub(load_method_patching)          \
  stub(load_mirror_patching)         \
  stub(load_appendix_patching)       \
  stub(compile_method_patching)       \
  stub(debug_method_entry)            \
  stub(debug_method_exit)             \
  last_entry(number_of_ids)

#define DECLARE_STUB_ID(x)       x ## _id ,
#define DECLARE_LAST_STUB_ID(x)  x
#define STUB_NAME(x)             #x " Runtime1 stub",
#define LAST_STUB_NAME(x)        #x " Runtime1 stub"

class Runtime0: public AllStatic {
    friend class VMStructs;
    friend class ArrayCopyStub;

public:
    enum StubID {
        RUNTIME1_STUBS(DECLARE_STUB_ID, DECLARE_LAST_STUB_ID)
    };

    // statistics

private:
    static CodeBlob* _blobs[number_of_ids];
    static const char* _blob_names[];

    // stub generation
    static void       generate_blob_for(BufferBlob* blob, StubID id);
    static void generate_code_for(StubID id, StubAssembler* sasm);

    // all the stub target
    static int access_field_patching(JavaThread* thread);
    static int move_klass_patching(JavaThread* thread);
    static int move_mirror_patching(JavaThread* thread);
    static int move_appendix_patching(JavaThread* thread);
    static int move_method_patching(JavaThread* thread);
    static int compile_method_patching(JavaThread* thread);
    static void gc_barrier(JavaThread* thread);

    static void patch_code(JavaThread* thread, StubID stub_id);

public:
    // initialization
    static void initialize();
    static void initialize_pd();

    // stubs
    static CodeBlob* blob_for (StubID id)           { return _blobs[id]; }
    static address   entry_for(StubID id)           { return blob_for(id)->code_begin(); }
    static const char* name_for (StubID id)         { return _blob_names[id]; }
    static const char* name_for_address(address entry)  { Unimplemented(); return NULL; }
    static void generate_patching(StubAssembler* sasm, address target);
};


#endif //HOTSPOT_C0_RUNTIME_HPP
