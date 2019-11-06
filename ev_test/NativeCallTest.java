public class NativeCallTest{
     public int val = 1;
     public void set(int i){this.val = i;}
     public int get(){return this.val;}

     public int sgx_hook() throws Exception{
          int c = 0;
          while(c < 5){
             Thread.sleep(1000);
             c++;
             set(c);
          }
          return get();
     }

     public static void main(String[] args) throws Exception{
          NativeCallTest a = new NativeCallTest();
          int k = a.sgx_hook();
          System.out.println(k);
     }
}
