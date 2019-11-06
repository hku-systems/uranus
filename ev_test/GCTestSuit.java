import java.util.*;
public class GCTestSuit{
	Object[] arr = new Object[3];
	Object obj;
     public GCTestSuit sgx_hook_new(){
       GCTestSuit s = new GCTestSuit();
	s.arr[0] = new GCTestSuit();
	s.obj = new GCTestSuit();
	return s;
     }
     public void sgx_hook_call(GCTestSuit h) {
     }
	public void sgx_hook(){}

     public static void main(String[] args){
         GCTestSuit gc = new GCTestSuit();
         GCTestSuit obj = gc.sgx_hook_new();
         Object a;
         for (int i = 0;i < 1000000;i++) {
           a = new int[1000];
         }
	        // obj.sgx_hook();
          gc.sgx_hook_call(obj);
         }
}
