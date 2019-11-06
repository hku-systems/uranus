import java.util.*;
import java.lang.reflect.*;
import java.io.*;

public class TestException{
    private boolean AccessFlag = false;
    public boolean Ac = false;

    private boolean getPri(){return AccessFlag;}    
    public boolean getPub(){return Ac;}

    //Test java.lang.ArithmeticException
    public static int sgx_hook_ArithmeticException(){
        try{
            int k = 3 / 0;
            return 0;
        }catch(java.lang.ArithmeticException e){
            return 1;
        }
    }
    //Test java.lang.ArrayIndexOutOfBoundsException
    public static int sgx_hook_ArrayIndexOutOfBoundsException(){
        try{
            int[] array = new int[3];
            array[3] = 1;
            return 0;
        }catch(java.lang.ArrayIndexOutOfBoundsException e){
            return 1;
        }
    }
    //Test java.lang.ArrayStoreException
    public static int sgx_hook_ArrayStoreException(){
        try{
      Object x[] = new String[3];
     x[0] = new Integer(0);
            return 0;
        }catch(java.lang.ArrayStoreException e){
            return 1;
        }

    }
    //Test java/lang/ClassCastException
    // public static int sgx_hook_ClassCastException(){
    //     try{
    //         //Main k1 = new ClassCast1();
    //         TestException k2 = new ClassCast2();
    //         ClassCast1 kk = (ClassCast1) k2;
    //         return 0;
    //     }catch(java.lang.ClassCastException e){
    //         return 1;
    //     }

    // }

    //java/lang/NoSuchFieldException
    public static int sgx_hook_NoSuchFieldException(){
        try{
            Class z = Class.forName("TestException");
            Field f = z.getField("Ac");

            return 0;
        }catch(NoSuchFieldException e){
            return 1;
        }catch(ClassNotFoundException e){
            return 2;
        }
    }
    
    //java/lang/IndexOutOfBoundsException
    public static int sgx_hook_IndexOutOfBoundsException(){
        try{
            List<String>list=new ArrayList<String>();
            list.add("123");
            list.get(2);
            return 0;
        }catch(IndexOutOfBoundsException e){

            return 1;
        }
    }

    //java/lang/NegativeArraySizeException
    public static int sgx_hook_NegativeArraySizeException(){
        try{
            int[] array = new int[-1];
            return 0;
        }catch(NegativeArraySizeException e){

            return 1;
        }
    }

    //java/lang/NoSuchMethodException
    /*public static int sgx_hook_NoSuchMethodException(){
        try{
            Class z = Class.forName("TestException");
            Method m = z.getMethod("getPri");
            return 0;
        }catch(NoSuchMethodException e){
            return 1;
        }catch(ClassNotFoundException e){
            return 2;
        }
    }*/
 
    //java/lang/NullPointerException
    public static int sgx_hook_NullPointerException(){
        try{
            Object obj=null;
            obj.toString();
            return 0;
        }catch(NullPointerException e){
            
            return 1;
        }
    }

    //java/lang/StringIndexOutOfBoundsException
    public static int sgx_hook_StringIndexOutOfBoundsException(){
        try{
            String a = "abc";
            a.substring(4);

            return 0;
        }catch(StringIndexOutOfBoundsException e){
            //e.printStackTrace();
            return 1;
        }
    }

    //java/lang/Exception
    public static int sgx_hook_Exception(){
        try{
            String a = "abc";
            a.substring(4);

            return 0;
        }catch(Exception e){
            //e.printStackTrace();
            return 1;
        }
    }

    //java/lang/RuntimeException
    public static int sgx_hook_RuntimeException(){
        try{
            String a = "abc";
            a.substring(4);

            return 0;
        }catch(RuntimeException e){
            //e.printStackTrace();
            return 1;
        }
    }

    //java/lang/IOException
    public static int sgx_hook_IOException(){
        try{
            File f = new File("words");
            InputStream input = null ;
            input = new FileInputStream(f)  ;
            return 0;
        }catch(IOException e){
            e.printStackTrace();
            return 1;
        }
    }

    //dead recursion ------ for testing StackOverflowError
    public static void func(int k){
        if(k == 1){
            func(++k);
        }else{
            func(--k);
        }
    }

    //java.lang.StackOverflowError
    public static int sgx_hook_StackOverflowError(){
        try{
            func(4);
            return 0;
        }catch(StackOverflowError e){
            return 1;
        }
    }

    public static void main(String[] args){

//Handled exception
//**********************************************************************************************

        /*ArithmeticException --------- Ok*/
        if(sgx_hook_ArithmeticException() == 1){
            System.out.println("java.lang.ArithmeticException is handled");
        }
        /*ArrayIndexOutOfBoundsException --------- Ok*/
        if(sgx_hook_ArrayIndexOutOfBoundsException() == 1){
            System.out.println("java.lang.ArrayIndexOutOfBoundsException is handled");
        }
        /*IndexOutOfBoundsException --------- Ok*/
        if(sgx_hook_IndexOutOfBoundsException() == 1){
            System.out.println("java.lang.IndexOutOfBoundsException is handled");
        }
        /*StringIndexOutOfBoundsException --------- Ok*/
        if(sgx_hook_StringIndexOutOfBoundsException() == 1){
            System.out.println("java.lang.StringIndexOutOfBoundsException is handled");
        }       
        /*Exception --------- Ok*/
        if(sgx_hook_Exception() == 1){
            System.out.println("java.lang.Exception is handled");
        }
        /*RuntimeException --------- Ok*/
        if(sgx_hook_RuntimeException() == 1){
            System.out.println("java.lang.RuntimeException is handled");
        }
//*********************************************************************************************


//Not handled correctly
//*********************************************************************************************

        //StackOverflowError error
        if(sgx_hook_StackOverflowError() == 1){
            System.out.println("java.lang.StackOverflowException is handled");
        }       
 
        //IOException error
        /*if(sgx_hook_IOException() == 1){
            System.out.println("java.lang.IOException is handled");
        }*/     

        //NullPointerException error
        if(sgx_hook_NullPointerException() == 1){
            System.out.println("java.lang.NUllpointerException is handled");
        }

        //NegativeArraySizeException error
        if(sgx_hook_NegativeArraySizeException() == 1){
            System.out.println("java.lang.NegativeArraySizeException is handled");
        }

        //NoSuchFieldException error
        /*if(sgx_hook_NoSuchFieldException() == 1){
            System.out.println("java.lang.NoSuchFieldException is handled");
        }*/

        //ArrayStoreException error
        if(sgx_hook_ArrayStoreException() == 1){
            System.out.println("java.lang.ArrayStoreException is handled");
        }

        //ClassCastException error
        /*if(sgx_hook_ClassCastException() == 1){
            System.out.println("java.lang.ClassCastException is handled");
        }*/

//*********************************************************************************************

//About to test
//*********************************************************************************************

//   java/lang/AbstractMethodError
//   java/lang/IllegalAccessError
//   java/lang/IllegalAccessException
//   java/lang/IllegalArgumentException
//   java/lang/IllegalStateException

//*********************************************************************************************
    }
}
