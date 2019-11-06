import java.util.regex.*;

public class stringTest{
     //public int val = 1;
     //public int get(){return this.val;}
     public int sgx_hook_1(){
         String stx = "stx";
	 String jianyu = "jianyu";
         if(stx.concat(jianyu).equals("stxjianyu")){return 1;}
         else{return 2;}
        
     }

     public int sgx_hook_2(){
         String stx = "stx";
         String jianyu = "jianyu";
         if(stx.replace("x",jianyu).equals("stjianyu")){return 1;}
         else{return 2;}

     }

     public int sgx_hook_3(){
         String stx = "stx";
         String jianyu = "jianyu";
         if(stx.split("t")[1].equals("x")){return 1;}
         else{return 2;}

     } 
 
     public int sgx_hook_4(){
         String stx = "stx  ";
         String jianyu = "jianyu";
         if(stx.trim().length() == 3){return 1;}
         else{return 2;}

     }

     public int sgx_hook_5(){
         String stx = "stx";
         String jianyu = "jianyu";
         if((stx+jianyu).equals("stxjianyu")){return 1;}
         else{return 2;}

     }

     public static void main(String[] args){
           stringTest st = new stringTest();
           
           //string API test: concat()
           int k1 = st.sgx_hook_1();
           assert(k1 == 1):"concat() function is wrong!";

           //string API test: replace()
           int k2 = st.sgx_hook_2();
           assert(k2 == 1):"replace() function is wrong!";

           //string API test: split()
           int k3 = st.sgx_hook_3();
           assert(k3 == 1):"split() function is wrong!";

           //string API test: trim()
           int k4 = st.sgx_hook_4();
           assert(k4 == 1):"trim() function is wrong!";
        
           //string API test: +
           int k5 = st.sgx_hook_5();
           assert(k5 == 1):"+ function is wrong!";
           System.out.println("pass string test");

     }
}
