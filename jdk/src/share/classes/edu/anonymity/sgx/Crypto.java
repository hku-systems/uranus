package edu.anonymity.sgx;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;

public class Crypto {

    /*
    * encrypt data inside enclave, throw RuntimeError if not in enclave
    */
    public static native byte[] sgx_encrypt(byte[] array, boolean en) throws NegativeArraySizeException;

    /**
     * decrypt data inside enclave, throw RuntimeError if not in enclave
     */
    public static native byte[] sgx_decrypt(byte[] array, boolean en) throws NegativeArraySizeException;

    public static native byte[] sgx_encrypt_int(int[] array, boolean en) throws NegativeArraySizeException;

    public static native int[] sgx_decrypt_int(byte[] array, boolean en) throws NegativeArraySizeException;

    public static native byte[] sgx_encrypt_double(double[] array, boolean en) throws NegativeArraySizeException;

    public static native double[] sgx_decrypt_double(byte[] array, boolean en) throws NegativeArraySizeException;

    public static native byte[] sgx_hash(byte[] array, boolean en) throws NegativeArraySizeException;

    public static native boolean sgx_verify(byte[] array, byte[] hash) throws NegativeArraySizeException;
}
