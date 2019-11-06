public class FillInStackTest{
 
      public static int val = 1;
      public static int get(){return val;}
      public int sgx_hook(){
            This is OK
            /*ArrayLg al = new ArrayLg();
            int k = al.minmax();
            return k;*/

            This is NOT OK
            /*int k = ArrayLg.minmax();
            return k;*/
  
            IF change sgx_hook() to static function, this can be done
            /*return FillInStackTest.get();*/
            or
            /*FillInStackTest a = new FillInStackTest();
            return a.get();*/
      }
  
      public static void main(String[] args){
           FillInStackTest fill = new FillInStackTest();
           int kk = fill.sgx_hook();
           System.out.println(kk);
      }
}

class ArrayLg{
     public static class Pair{}

     public static int minmax(){return 1;}
}
