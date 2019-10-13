//
// Created by max on 12/29/17.
//

#ifndef HOTSPOT_ENCLAVEMANAGER_H
#define HOTSPOT_ENCLAVEMANAGER_H

#include <string>

#include "Enclave.h"

class EnclaveManager {
public:
    static sgx_enclave_id_t global_id;
    static int psw_installed_check();
    static bool init;
    static sgx_enclave_id_t new_enclave(std::string enclave_path);
};


#endif //HOTSPOT_ENCLAVEMANAGER_H
