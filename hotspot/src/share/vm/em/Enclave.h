//
// Created by max on 12/29/17.
//

#ifndef HOTSPOT_ENCLAVE_H
#define HOTSPOT_ENCLAVE_H

#include <sgx_eid.h>
#include <string>

class Enclave {
public:
    sgx_enclave_id_t id;
    std::string name;
    Enclave(sgx_enclave_id_t _id, std::string _name): id(_id), name(_name) {}

    Enclave(): id(0), name("null") {}
};


#endif //HOTSPOT_ENCLAVE_H
