public class HelloLdcTest {
  String sgx_hook() {
//    Boolean b = "aaa".equals("aa");
    return "aaaa";
  }
  public static void main( String[] args ) {
    HelloLdcTest h = new HelloLdcTest();
    int sum = 0;
    for (int i = 0;i < 1;i++) {
      String ccc = h.sgx_hook();
      System.out.println(ccc);
    }
    System.out.println( "Hello World!" + sum);
    System.exit( 0 ); //success
  }
}
