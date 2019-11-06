public class ReflectionTest{
      public int sgx_hook() throws Exception{
           Class z = Class.forName("ReflectionTest");
           return z.getMethods().length;
      }

      public static void main(String[] args) throws Exception{
           ReflectionTest a = new ReflectionTest();
           int kk = a.sgx_hook();
           System.out.println(kk);
      }
}
