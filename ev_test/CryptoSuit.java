import edu.anonymity.sgx.*;

public class CryptoSuit {
    
    @IntelSGX
    byte[] encrypt_byte() {
        byte[] arr = new byte[10];
        arr[1] = 3;
        return Crypto.sgx_encrypt(arr, false);
    }

    @IntelSGX
    byte[] encrypt_int() {
        int[] arr = new int[100];
        arr[1] = 2;
        return Crypto.sgx_encrypt_int(arr, false);
    }

    @IntelSGX
    byte[] encrypt_double() {
        double[] arr = new double[100];
        arr[1] = 3.131415926;
        return Crypto.sgx_encrypt_double(arr, false);
    }

    @IntelSGX
    byte[] decrypt_byte(byte[] arr) {
        return Crypto.sgx_decrypt(arr, false);
    }

    @IntelSGX
    int[] decrypt_int(byte[] arr) {
        return Crypto.sgx_decrypt_int(arr, false);
    }

    @IntelSGX
    double[] decrypt_double(byte[] arr) {
        return Crypto.sgx_decrypt_double(arr, false);
    }

    @IntelSGX
    byte[] hash_bytes(byte[] arr) {
        return Crypto.sgx_hash(arr, false);
    }

    @IntelSGX
    boolean verify_hash(byte[] arr, byte[] hash) {
        return Crypto.sgx_verify(arr, hash);
    }

    @IntelSGX
    int[] copy() {
        return (int[])Tools.copy_out(new int[10]);
    }

    @IntelSGX
    int clone_length() {
        int[] arr = new int[3];
        int[] arr_clone = (int[])arr.clone();
        return arr_clone.length;
    }

    public static void main( String[] args ) {
        CryptoSuit s = new CryptoSuit();

        int[] arr_copy = s.copy();
        System.out.println("leng " + arr_copy.length);
        for (int i = 0;i < arr_copy.length;i++) {
            System.out.print(arr_copy[i] + " ");
        }

        byte[] arr = s.decrypt_byte(s.encrypt_byte());
        for (int i = 0;i < arr.length;i++) {
            System.out.print(arr[i] + " ");
        }
        System.out.println();

        int[] arr_int = s.decrypt_int(s.encrypt_int());
        for (int i = 0;i < arr.length;i++) {
            System.out.print(arr_int[i] + " ");
        }
        System.out.println();

        double[] arr_double = s.decrypt_double(s.encrypt_double());
        for (int i = 0;i < arr.length;i++) {
            System.out.print(arr_double[i] + " ");
        }
        System.out.println();

        byte[] bb = new byte[100];
        byte[] hash = s.hash_bytes(bb);
        System.out.println(s.verify_hash(bb, hash));
    }
}
