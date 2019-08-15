//
// Created by jianyu on 11/16/18.
//

#include <precompiled.hpp>
#include <runtime/fieldType.hpp>
#include "MetadataAccessor.hpp"


BasicType MetadataAccessor::get_field_type_by_index(ConstantPool *pool, int idx) {
    int nt_index = pool->name_and_type_ref_index_at(idx);
    int sig_index = pool->signature_ref_index_at(nt_index);
    Symbol* signature = pool->symbol_at(sig_index);
    return FieldType::basic_type(signature);
}

Klass* MetadataAccessor::get_field_klass_if_loaded(ConstantPool *pool, int idx) {
    int nt_index = pool->name_and_type_ref_index_at(idx);
    int sig_index = pool->signature_ref_index_at(nt_index);
    return pool->klass_at_if_loaded(constantPoolHandle(pool), sig_index);
}

Klass* MetadataAccessor::get_field_holder_klass_if_loaded(ConstantPool *pool, int idx) {
    int nt_index = pool->klass_ref_index_at(idx);
    return pool->klass_at_if_loaded(constantPoolHandle(pool), nt_index);
}

Klass* MetadataAccessor::get_method_holder_klass_if_loaded(ConstantPool *pool, int idx) {
    int nt_index = pool->klass_ref_index_at(idx);
    return pool->klass_at_if_loaded(constantPoolHandle(pool), nt_index);
}

int MetadataAccessor::method_parameter_size(Method *method, int bci, TosState &tos) {
    Bytecode_invoke invoke_call(methodHandle(method), bci);
    SignatureStream ss(invoke_call.signature());
    int len = 0;
    for (;;ss.next()) {
        if (ss.at_return_type()) {
            switch (ss.type()) {
                case T_SHORT:   tos = stos; break;
                case T_BOOLEAN: tos = btos; break;
                case T_INT:     tos = itos; break;
                case T_BYTE:    tos = btos; break;
                case T_CHAR:    tos = ctos; break;
                case T_FLOAT:   tos = ftos; break;
                case T_DOUBLE:  tos = dtos; break;
                case T_LONG:    tos = ltos; break;
                case T_ARRAY:
                case T_OBJECT:  tos = atos; break;
                case T_VOID:    tos = vtos; break;
                default:
                    ShouldNotReachHere();
            }
            break;
        }
        switch (ss.type()) {
            case T_SHORT:
            case T_BOOLEAN:
            case T_INT:
            case T_BYTE:
            case T_CHAR:
            case T_FLOAT:
                len += 1;
                break;
            case T_DOUBLE:
            case T_LONG:
                len += 2;
                break;
            case T_ARRAY:
            case T_OBJECT:
                len += 1;
                break;
            default:
                ShouldNotReachHere();
        }
    }
    return len;
}