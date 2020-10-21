package edu.hku.cs.uranus.attestation;

public class Enclave {
    public static Enclave default_enclave = null;
    public static Enclave getEnclave() {
        if (default_enclave == null) {
            default_enclave = new Enclave();
        }
        return default_enclave;
    }

    public native boolean sgx_ra_init();

    public native char[] sgx_ra_get_msg0();

    public native char[] sgx_ra_get_msg1();

    public native char[] sgx_ra_proc_msg2(char[] msg2);
    
    public native boolean sgx_ra_proc_msg4(char[] msg4);

}

