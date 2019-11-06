import java.math.*;
import java.util.*;

public class ConstantTest{

     public static int p = 2;
     public static int get(){return p;}
//   public static void set(int kk){ p = kk;} 

     public double sgx_hook_1(double m){         
         return m;
     }

     public double sgx_hook_2(){
          double m2 = 9.91;
          return m2;
     }
 
     public double[] sgx_hook_3(){
          double[] m2 = new double[3];
          m2[0] = (double)9.99;
          return m2;
     }

     public double[][] sgx_hook_4(){
          double[][] m2 = new double[3][3];
          m2[0][0] = (double)9.99;
          return m2;
     }

     public double[][] sgx_hook_5(double[][] k){
          return k;
     }

     public float sgx_hook_6(float m){
         return m;
     }

     public float sgx_hook_7(){
          float m2 = (float)9.91;
          return m2;
     }

     public float[] sgx_hook_8(){
          float[] m2 = new float[3];
          m2[0] = (float)9.99;
          return m2;
     }

     public float[][] sgx_hook_9(){
          float[][] m2 = new float[3][3];
          m2[0][0] = (float)9.99;
          return m2;
     }

     public float[][] sgx_hook_10(float[][] k){
          return k;
     }


     public static void main(String[] args){
         ConstantTest c = new ConstantTest();

         // return double value test
         double kk = 9.997;
         double m = c.sgx_hook_1(kk);
         assert(m == kk):"fault in return double value of paramater";

         // return double value test
         double kk2 = 9.91;
         double m2 = c.sgx_hook_2();
         assert(m2 == kk2):"fault in return double value declared in function";

         // return double value test
         double kk3 = 9.99;
         double[] m3 = c.sgx_hook_3();
         assert(m3[0] == kk3):"fault in return double[] value declared in function";

         // return double value test
         double kk4 = 9.99;
         double[][] m4 = c.sgx_hook_4();
         assert(m4[0][0] == kk4):"fault in return double[][] value declared in function";

         // return double value test
         double[][] kk5 = new double[3][3];
         kk5[0][0] = (double)9.98;
         double[][] m5 = c.sgx_hook_5(kk5);
         assert(m5[0][0] == kk5[0][0]):"fault in return double[][] value of paramater";
         
         // return float value test
         float kk6 = (float)9.997;
         float m6 = c.sgx_hook_6(kk6);
         assert(m6 == kk6):"fault in return float value of paramater";

         // return float value test
         float kk7 = (float)9.91;
         float m7 = c.sgx_hook_7();
         assert(m7 == kk7):"fault in return float value declared in function";

         // return float value test
         float kk8 = (float)9.99;
         float[] m8 = c.sgx_hook_8();
         assert(m8[0] == kk8):"fault in return float[] value declared in function";

         // return float value test
         float kk9 = (float)9.99;
         float[][] m9 = c.sgx_hook_9();
         assert(m9[0][0] == kk9):"fault in return float[][] value declared in function";

         // return float value test
         float[][] kk10 = new float[3][3];
         kk10[0][0] = (float)9.98;
         float[][] m10 = c.sgx_hook_10(kk10);
         assert(m10[0][0] == kk10[0][0]):"fault in return float[][] value of paramater";
 
         System.out.println("pass Constant(double/float) test");
     }
}

class Constant2{
     public static final int pp = 6;
}
