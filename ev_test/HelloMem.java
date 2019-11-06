public class HelloMem {
    HelloMem sgx_hook() {
        return new HelloMem();
      }
    int me() {return 1;}
    public static void main( String[] args ) {
      HelloMem hhh = new HelloMem();
//      HelloMem hh = hhh.sgx_hook();
      for (int i = 0;i < 100000000;i++) {
        HelloMem h = new HelloMem();
      }
//      System.out.println(hh.me());
      
    }
}
