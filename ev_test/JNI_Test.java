public class JNI_Test{
     public void sgx_hook(){
          new JNI_Test().print();
     }

     private native void print();

     static {
     //     System.loadLibrary("JNI_Test");
     }
     
     public static int sgx_hook_1(){
          double x = 9.997;
          return (int)Math.round(x);
     }
     public static void main(String[] args){

          int k = sgx_hook_1();
          assert( k == 10):"JNI Call fail";
          System.out.println(k);
          //JNI_Test a = new JNI_Test();
          //a.sgx_hook();
     }
}
