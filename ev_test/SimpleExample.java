import edu.anonymity.sgx.IntelSGX;
import edu.anonymity.sgx.IntelSGXOcall;
import edu.anonymity.sgx.Tools;
class SimpleExample {
    public static int f;
    @IntelSGXOcall
    public static void print_info() {
	System.out.println("call print_info");
    }

    // fibonacci calculation
    @IntelSGX
    public static int calculate(int n) {
        return SimpleExample.f;
    }

    public static void main(String[] args) {
        SimpleExample.f = 2;
        System.out.println(calculate(100));
    }
}
