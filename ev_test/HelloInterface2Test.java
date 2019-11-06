import java.util.function.Function;
public class HelloInterface2Test {
  // String call_me() { return "aaa"; }
  String sgx_hook(Function<String, String> fn, String a, int d, int f) {
    return fn.apply(a) + "aaaaa";
  }
  public static void main( String[] args ) {
    HelloInterface2Test h = new HelloInterface2Test();
    // int sum = 0;
    Function<String, String> fn = parameter -> parameter + 'm';
    // fn.apply("aaa");
    for (int i = 0;i < 1;i++) {
      String cc = h.sgx_hook(fn, " cc", 1, 2);
      System.out.println(cc);
    }
    // System.out.println( "Hello World!" + sum);
    System.exit( 0 ); //success
  }
}
