package edu.hku.cs.uranus.attestation;

public class Untrust {
    public static Untrust default_untrst = null;
    public static Untrust getUntrust() {
        if (default_untrst == null) {
            default_untrst = new Untrust();
        }
        return default_untrst;
    }

    public char[] jar_hash;

    public boolean init_hash(char[] enclave_jar_hash) {
        if (enclave_jar_hash == null || enclave_jar_hash.length != 32) {
            return false;
        }
        jar_hash = enclave_jar_hash;
        return true;
    }
    public native char[] process_msg01(char[] msg0, char[] msg1);
    public native char[] process_msg3(char[] msg3);

    public native boolean process_msg4(char[] msg4);
    public byte[] getKey() {
        byte[] key = {'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b',
        'c', 'd'};
        return key;
    }
}