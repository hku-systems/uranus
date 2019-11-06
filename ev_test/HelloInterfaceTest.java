public class HelloInterfaceTest {
  public interface HelloInterface {
    public int me();
  }
  public class A implements HelloInterface {
    public int me() {
      return 1;
    }
  }
  int sgx_hook() {
    HelloInterface i = new A();
    return i.me();
  }
  public static void main( String[] args ) {
    HelloInterfaceTest h = new HelloInterfaceTest();
    int sum = 0;
    for (int i = 0;i < 1;i++) {
      sum += h.sgx_hook();
    }
    System.out.println( "Hello World!" + sum);
    System.exit( 0 ); //success
  }
}
