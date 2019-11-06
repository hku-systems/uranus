import java.util.*;
import java.lang.reflect.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.Timer;

interface MyInterface{
    void LMethod();
}
public class HelloTest {
 
    public static void test(MyInterface k){
       k.LMethod();
    }
    public int val_i = 3;
    private int val_j = 4;
    public static int counter = 0;
  //  public void setV(int p){this.val_i = p;} 
    public static int getS(){return 789;}
    public void pt(){System.out.println("OKOKOK");}
    public int getV() { return this.val_i; }
    public int getV2() { return this.val_j; }
    public A getA(){return new A();}
    public void set(int k){this.val_i = k;} 
    public int sgx_hook()throws Exception {
        //ArrayLg.Pair p = ArrayLg.minmax(d);
        //ArrayLg lg = new ArrayLg();        
        //ArrayLg lg22 = ArrayLg.minmax(d);
        //int kk = Hello.getS();
        //System.out.println(p.get1()+" "+p.get2());
        //int k = (int)(p.get1()+p.get2());
        //int k = Hello.getS();
        pt();   

        return 1;
//        return 1;
    }
    
    public static void main( String[] args )throws Exception{
	HelloTest h = new HelloTest();
        //Hello[] hh = new Hello[3];
	
        /*test(new  MyInterface(){
             public void LMethod(){
                System.out.println("hello world");
             }
        });
        test(()->System.out.println("hello world!"));*/
        /*int sum = 0;
	for (int i = 0;i < 1;i++) {
		sum += h.sgx_hook(h);
	}*/
        ArrayList<Integer> sq = new ArrayList<>();
        sq.add(1);
        int[] a = new int[3];
        a[0] = 1;a[1] = 4; a[2] = 3;
        double[] d = new double[20];
        for(int i = 0 ; i < d.length; i++){
             d[i] = (double)i; 
        } 
        int k1 = h.sgx_hook();
        
        //int k2 = h.sgx_hook(a);
        System.out.println(k1);
        System.exit( 0 ); //success
    }

    class A{
    }
    class B{
        public int val = 55;
    }
}

class TimePrinter implements ActionListener{
    public void actionPerformed(ActionEvent event){
        Date now = new Date();
        Toolkit.getDefaultToolkit().beep();
    }
}

class ArrayLg{
    public static class Pair{
         public double first;
         public double second;

         public Pair(double a, double b){
             this.first = a;
             this.second = b;
         }
         public double get1(){
             return this.first;
         }
         public double get2(){
             return this.second;
         }
    }

   public static ArrayLg minmax(double[] values){
         double min = Double.MAX_VALUE;
         double max = Double.MIN_VALUE;
         for(double v : values){
             if(min > v){min = v;}
             if(max < v){max = v;}
         }
         //return new Pair(min,max);
         return new ArrayLg(); 
         //return 666;
    }
    public static int mm(){return 999;}
}
