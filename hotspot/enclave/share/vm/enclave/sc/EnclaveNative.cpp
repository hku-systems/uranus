
#include <string>
#include "EnclaveNative.h"
#include "native_libs/java_lang_StrictMath.h"

char* EnclaveNative::native_name[] = {0};
void* EnclaveNative::native_entry[] = {0};

static void mangle_name_on_4(outputStream* st, Symbol* name, int begin, int end) {
    char* bytes = (char*)name->bytes() + begin;
    char* end_bytes = (char*)name->bytes() + end;
    while (bytes < end_bytes) {
        jchar c;
        bytes = UTF8::next(bytes, &c);
        if (c <= 0x7f && isalnum(c)) {
            st->put((char) c);
        } else {
            if (c == '_') st->print("_1");
            else if (c == '/') st->print("_");
            else if (c == ';') st->print("_2");
            else if (c == '[') st->print("_3");
            else               st->print("_%.5x", c);
        }
    }
}


static void mangle_name_on(outputStream* st, Symbol* name) {
    mangle_name_on_4(st, name, 0, name->utf8_length());
}

const char* pure_jni_name(methodHandle method) {
    stringStream st;
    // Prefix
    st.print("Java_");
    // Klass name
    mangle_name_on(&st, method->klass_name());
    st.print("_");
    // Method name
    mangle_name_on(&st, method->name());
    return st.as_string();
}

void EnclaveNative::init(){
   DO_ENCLAVE_NATIVE(VM_ENCLAVE_ENTRY)
}

void* EnclaveNative::resolve_function(Method* m) {
    const char* name = pure_jni_name(m);
    for (int i = 0;i < EnclaveNative::NATIVE_COUNT;i++) {
        if (strstr(name, EnclaveNative::native_name[i]) != NULL) {
            m->set_enclave_native_function(CAST_FROM_FN_PTR(address, EnclaveNative::native_entry[i]));
//            printf("set entry %s %lx\n", name, (intptr_t)EnclaveNative::native_name[i]);
            return CAST_FROM_FN_PTR(address, EnclaveNative::native_entry[i]);
        }
    }
    printf(D_ERROR("Native")" cannot find native func %s\n", name);
}

extern "C" {

    JNIEXPORT jdouble JNICALL
    Double_longBitsToDouble(JNIEnv *env, jclass unused, jlong v)
    {
        union {
            jlong l;
            double d;
        } u;
        u.l = v;
        return (jdouble)u.d;
    }

    /*
     * Find the bit pattern corresponding to a given double float, NOT collapsing NaNs
     */
    JNIEXPORT jlong JNICALL
            Double_doubleToRawLongBits(JNIEnv *env, jclass unused, jdouble v)
    {
        union {
            jlong l;
            double d;
        } u;
        u.d = (double)v;
        return u.l;
    }

    JNIEXPORT jint JNICALL
    System_identityHashCode(JNIEnv *env, jobject ignore, jobject x) {
        return JVM_IHashCode(env, x);
    }

    jboolean
    JVM_IsInstanceOf(JNIEnv *env, jclass c, jobject o) {
        return jni_functions()->IsInstanceOf(env, o, c);
    }

}