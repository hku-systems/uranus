//
// Created by jianyu on 11/16/18.
//

#ifndef HOTSPOT_METADATAACCESSOR_HPP
#define HOTSPOT_METADATAACCESSOR_HPP

#include <oops/oop.hpp>

class MetadataAccessor {
public:
    static BasicType get_field_type_by_index(ConstantPool* pool, int idx);
    static TosState basicType2tosState(BasicType t) {
        switch (t) {
            case T_CHAR:        return ctos;
            case T_BOOLEAN:     return ztos;
            case T_BYTE:        return btos;
            case T_INT:         return itos;
            case T_LONG:        return ltos;
            case T_DOUBLE:      return dtos;
            case T_FLOAT:       return ftos;
            case T_SHORT:       return stos;
            case T_ARRAY:
            case T_OBJECT:      return atos;
            case T_VOID:        return vtos;
            default:            return ilgl;
        }
    }
    static Klass* get_field_klass_if_loaded(ConstantPool* pool, int idx);
    static Klass* get_field_holder_klass_if_loaded(ConstantPool* pool, int idx);
    static Klass* get_method_holder_klass_if_loaded(ConstantPool* pool, int idx);
    static int method_parameter_size(Method* m, int, TosState &tos);
};


#endif //HOTSPOT_METADATAACCESSOR_HPP
