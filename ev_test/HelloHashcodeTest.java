public class HelloHashcodeTest {
  int sgx_hook() {
    return hashCode();
  }
  public static void main( String[] args ) {
    HelloHashcodeTest h = new HelloHashcodeTest();
//    h.hashCode();
    int sum = 0;
    for (int i = 0;i < 1;i++) {
      int ccc = h.sgx_hook();
      sum = ccc;
    }
    System.out.println( "Hello World!" + sum);
    System.exit( 0 ); //success
  }
}
