import java.util.*;
public class arrayReflection{
     static final int MAX_INT = 20;
     public int sgx_hook(int c){
       return 93259921;
      //  int[] s = new int[3];
      //  int[] d = new int[3];
//       s[0] = 1;
//       System.arraycopy(s, 0, d, 0, 3);
//       int[] c = d.clone();
      //  return s;
     }

     public static void main(String[] args){
         arrayReflection ct = new arrayReflection();
         int k = ct.sgx_hook(10);
         System.out.println(k);
     }
}
