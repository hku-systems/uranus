package edu.anonymity.sgx;

public class Tools {

    public static void print(String s) {
        print((s + "\0").getBytes());
    }

    public static void print(int s) { print(String.valueOf(s)); }

    public static void print(long s) { print(String.valueOf(s)); }

    public static void println(String s) { print(s + "\n"); }

    public static native void print(byte[] arr);

    public static native Object copy_out(Object obj);

    public static native Object deep_copy(Object obj);

    public static native void clean();

}
