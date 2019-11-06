import java.util.*;

public class ContainerTest{

     public static int sgx_hook_1(ArrayList<Integer> k){
         TreeSet<Integer> a = new TreeSet<>();
	 a.add(22);	
         return a.size();
     }

     public static int sgx_hook_2(ArrayList<Integer> k){
         k.add(22);
         return k.size();
     }

     public static void main(String[] args){

         //case 1
         ArrayList<Integer> a = new ArrayList<>();
         int k = sgx_hook_1(a);
         assert( k == 1):"fault in TreeSet.add()";

         //case 2
         int k2 = sgx_hook_2(a);
         assert( k2 == 1):"fault in ArrayList.add()";
     
         System.out.println("pass container test");
     }
} 
