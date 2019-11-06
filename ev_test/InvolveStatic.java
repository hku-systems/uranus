public class InvolveStatic{
     public static int value = 666;
     public static int get(){ return value; }

     public static int sgx_hook_1(){
         return get();
     }
    
     public static int sgx_hook_2(){
         InvolveStatic k = new InvolveStatic();
         int kk = k.get();
         return kk;
     }

     public static int sgx_hook_3(){
         return OuterClass.get();
     }

     public static int sgx_hook_4(){
         OuterClass k2 = new OuterClass();
         int kk = k2.get();
         return kk;
     }

     public int sgx_hook_5(){
         return OuterClass.get();
     }
     public static void main(String[] args){
           
          //invoke by using feature of static class
          int k = sgx_hook_1();
          assert( k == 666):"fault in static invoke";

          // common invoke(declare+invoke)
          int k2 = sgx_hook_2();
          assert( k2 == 666):"fault in common invoke";

          //invoke by using feature of static class
          int k3 = sgx_hook_3();
          assert( k3 == 999):"fault in static invoke";

          // common invoke(declare+invoke)
          int k4 = sgx_hook_4();
          assert( k4 == 999):"fault in common invoke";
 
          InvolveStatic kk = new InvolveStatic();
          int k5 = kk.sgx_hook_5();
          assert( k5 == 999):"fault in common invoke";

          System.out.println("pass staic function invoke test");
     }
}

class OuterClass{
     public static int value = 999;
     public static int get(){ return value; }
}
