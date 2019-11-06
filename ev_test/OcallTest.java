import edu.anonymity.sgx.IntelSGX;
import edu.anonymity.sgx.UnIntelSGX;

public class OcallTest {

    int sgx_unhook_int() {
      return 1;
    }

    @IntelSGX
    Boolean is_equal() {
      return sgx_unhook_int() == 1;
    }
    public static void main( String[] args ) {
      OcallTest h = new OcallTest();
      System.out.println(h.is_equal());
    }
}
