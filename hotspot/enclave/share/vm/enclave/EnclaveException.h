//
// Created by Maxxie Jiang on 15/3/2018.
//

#ifndef HOTSPOT_ENCLAVEEXCEPTION_H
#define HOTSPOT_ENCLAVEEXCEPTION_H
// In our design, we first put every kind of exception klass in the cache

// exception handling in enclave
// this are three kinds of excpetions in our design
// 1. exceptions throw manually
// 2. runtime exceptions
//  -> we need to manually create the exception oop
// 3. runtime exceptions (capture by signal handler)
//  -> we need to create exception oop and the handler

// ExceptionFlow
// ENCLAVE_THROW -> create exception oop(1) -> set pending exception(2)
// -> handle by call_VM(3) -> jmp to rethrow_exception_entry(4)
// (active flow starts here)-> throw_exception_entry(5) -> get_handler_with_oop(6)
// (if succeed) -> to handler_bci(7)
// (if fail) -> remove_actication -> check_addr -> (in_interpreter) -> (4)
//                                              -> (not_in_interpreter) -> return
#include "oops/oop.hpp"
#include "EnclaveDebug.h"

#ifdef DB_EXP
#define EXP_DEBUG(...) printf(__VA_ARGS__)
#else
#define EXP_DEBUG(...)
#endif

#define EXCEPTION_EUNM(name)    name
#define ENCLAVE_SYMBOL_ENUM(name, string) EXCEPTION_EUNM(name),
#define DO_EXCEPTIONS(template) \
  template(java_lang_ArithmeticException,             "java/lang/ArithmeticException")            \
  template(java_lang_ArrayIndexOutOfBoundsException,  "java/lang/ArrayIndexOutOfBoundsException") \
  template(java_lang_ArrayStoreException,             "java/lang/ArrayStoreException")            \
  template(java_lang_ClassCastException,              "java/lang/ClassCastException")             \
  template(java_lang_IllegalAccessException,          "java/lang/IllegalAccessException")         \
  template(java_lang_IllegalArgumentException,        "java/lang/IllegalArgumentException")       \
  template(java_lang_IllegalStateException,           "java/lang/IllegalStateException")          \
  template(java_lang_IndexOutOfBoundsException,       "java/lang/IndexOutOfBoundsException")      \
  template(java_lang_NegativeArraySizeException,      "java/lang/NegativeArraySizeException")     \
  template(java_lang_NoSuchFieldException,            "java/lang/NoSuchFieldException")           \
  template(java_lang_NoSuchMethodException,           "java/lang/NoSuchMethodException")          \
  template(java_lang_NullPointerException,            "java/lang/NullPointerException")           \
  template(java_lang_StringIndexOutOfBoundsException, "java/lang/StringIndexOutOfBoundsException")\
  template(java_lang_Exception,                       "java/lang/Exception")                      \
  template(java_lang_RuntimeException,                "java/lang/RuntimeException")               \
  template(java_lang_OutOfMemoryError,                "java/lang/OutOfMemoryError")               \
  /* error klasses: at least all errors thrown by the VM have entries here */                     \
  template(java_lang_AbstractMethodError,             "java/lang/AbstractMethodError")            \
  template(java_lang_IllegalAccessError,              "java/lang/IllegalAccessError")             \
  template(java_lang_StackOverflowError,              "java/lang/StackOverflowError")

// unsupported exception
//  template(java_io_IOException,                       "java/io/IOException")                      \
//  template(java_lang_ClassNotFoundException,          "java/lang/ClassNotFoundException")       \
//  template(java_lang_CloneNotSupportedException,      "java/lang/CloneNotSupportedException")   \

//  template(java_lang_IllegalThreadStateException,     "java/lang/IllegalThreadStateException")    \
//  template(java_lang_InstantiationException,          "java/lang/InstantiationException")         \
//  template(java_lang_InstantiationError,              "java/lang/InstantiationError")             \
//  template(java_lang_InterruptedException,            "java/lang/InterruptedException")           \
//  template(java_lang_BootstrapMethodError,            "java/lang/BootstrapMethodError")           \
//  template(java_lang_IllegalMonitorStateException,    "java/lang/IllegalMonitorStateException")   \
//  template(java_lang_LinkageError,                    "java/lang/LinkageError")                   \
//  template(java_lang_UnsupportedOperationException,   "java/lang/UnsupportedOperationException")  \
//  template(java_lang_InvalidClassException,           "java/lang/InvalidClassException")          \
//  template(java_lang_reflect_InvocationTargetException, "java/lang/reflect/InvocationTargetException") \
//  template(java_security_PrivilegedActionException,   "java/security/PrivilegedActionException")  \
//  template(java_lang_ClassCircularityError,           "java/lang/ClassCircularityError")          \
//  template(java_lang_ClassFormatError,                "java/lang/ClassFormatError")               \
//  template(java_lang_UnsupportedClassVersionError,    "java/lang/UnsupportedClassVersionError")   \
//  template(java_lang_Error,                           "java/lang/Error")                          \
//  template(java_lang_ExceptionInInitializerError,     "java/lang/ExceptionInInitializerError")    \
//  template(java_lang_IncompatibleClassChangeError,    "java/lang/IncompatibleClassChangeError")   \
//  template(java_lang_InternalError,                   "java/lang/InternalError")                  \
//  template(java_lang_NoClassDefFoundError,            "java/lang/NoClassDefFoundError")           \
//  template(java_lang_NoSuchFieldError,                "java/lang/NoSuchFieldError")               \
//  template(java_lang_NoSuchMethodError,               "java/lang/NoSuchMethodError")              \
//  template(java_lang_UnsatisfiedLinkError,            "java/lang/UnsatisfiedLinkError")           \
//  template(java_lang_VerifyError,                     "java/lang/VerifyError")                    \
//  template(java_lang_SecurityException,               "java/lang/SecurityException")              \
//  template(java_lang_VirtualMachineError,             "java/lang/VirtualMachineError")            \

#define ENCLAVE_THROW(name) \
{ EnclaveException::throw_msg((JavaThread*)THREAD, name, __FILE__, __LINE__); return; }

#define ENCLAVE_THROW_0(name) \
{ EnclaveException::throw_msg((JavaThread*)THREAD, name, __FILE__, __LINE__); return 0; }
#define ENCLAVE_THROW_EXIT(name) \
{ EnclaveException::throw_msg((JavaThread*)THREAD, name, __FILE__, __LINE__); return 0; }
class EnclaveException {
public:
    enum Exceptions {
        NO_EXP,
        DO_EXCEPTIONS(ENCLAVE_SYMBOL_ENUM)
        EXCEPTION_COUNT
    };

    // static class ExceptionEntry {
    // public:
    //   Exceptions e;
    //   char* name;
    //   void* klass;
    //   ExceptionEntry(Exceptions _e, char* _name, char* _klass): e(_e), name(_name), klass(_klass) {}
    // } ;

    static void* entry_list[EXCEPTION_COUNT + 1];
    static char* name_list[EXCEPTION_COUNT + 1];

    static void init();
    static oop get_exception_oop(JavaThread* ,const char* name);
    static inline Klass* get_exception_klass(Exceptions e) { return (Klass*)entry_list[e]; }
    static void throw_msg(JavaThread*, Exceptions e, char* filename, int line);
    // return 0 if it is not in interpreter any more
    static address check_exit_addr(address ret_addr, JavaThread* thread);
};


#endif //HOTSPOT_ENCLAVEEXCEPTION_H
