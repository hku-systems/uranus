import java.util.*;
public class EnclaveGCTestSuit{
    static class MemObj {
      int[] arr = new int[3];
      int n = 0;
      int call_me() { return 1; }
      public MemObj(int i) {
        n = i;
      }
      public MemObj() {}
    }

    MemObj m = new MemObj(100);

    // test if gc can collect object
     public int sgx_hook_gc(){
      Object s = null;
      MemObj h = new MemObj();
      for (int i = 0;i < 2000;i++) {
        s = new MemObj();
      }
      return h.call_me();
     }

     public void do_real_allocation(Object o, int z) {
      Object s = null;
      for (int i = 0;i < 2000;i++) {
        s = new MemObj();
      }
     }

     public int sgx_hook_parameter() {
       MemObj o = new MemObj();
       do_real_allocation(o, 1);
       return o.call_me();
     }

     public int sgx_hook_expression() {
       MemObj k = new MemObj(0);
       MemObj prev = k;
       int sum = 0;
       for(int i = 0;i < 9800;i++) {
         sum += prev.n;
         prev = new MemObj(i);
       }
       return sum;
     }

     public int sgx_hook_out_test(EnclaveGCTestSuit s) {
      MemObj k = new MemObj(0);
      s.m = new MemObj(10);
      MemObj prev = k;
      int sum = 0;
      for(int i = 0;i < 9800;i++) {
        sum += prev.n;
        prev = new MemObj(i);
      }
      return sum + s.m.n;
    }

     public static void main(String[] args){
      EnclaveGCTestSuit gc = new EnclaveGCTestSuit();
      // System.out.println("Test basic GC: " + (gc.sgx_hook_gc() == 1));
      // System.out.println("Test parameter GC: " + (gc.sgx_hook_parameter() == 1));
      // System.out.println("Test expression GC: " + (gc.sgx_hook_expression() + " " + 1997002));
      System.out.println("Test out GC: " + (gc.sgx_hook_out_test(gc) + " " + 48005311));
    }
}
