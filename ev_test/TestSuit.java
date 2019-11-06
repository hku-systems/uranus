public class TestSuit {
    public int f_a = 0;
    public int f_b = 1;
    public Object f_c;

    static class TestException extends Exception {

    }
    static public interface HelloInterface {
      public int me();
    }
    static public class HelloInterfaceImpl implements HelloInterface {
      public int me() {
        return 1;
      }
    }

    static public void static_call() {}



    static public class B { int m = 10; }

    int add(int a, int b) { return a + b; }
    int constance() {return 0; }

    int sgx_hook_add(int a, int b) { return a + b; }
    String sgx_hook_string() { return "hook"; }
    int sgx_hook_invokevirtual(int a, int b) { return add(1, 2); }
    int sgx_hook_new_invokevirtual() {
      TestSuit s = new TestSuit();
      return s.add(1, 2);
    }
    void sgx_hook_invokestatic() { static_call(); }
    int sgx_hook_invokeinterface(HelloInterface i) { return i.me(); }
    int sgx_hook_getfield(TestSuit s) { return s.f_b; }
    int sgx_hook_putfiled(TestSuit s, int i) { s.f_a = i; return s.f_a; }
    int sgx_hook_newobj() { return new B().m; }
    int sgx_hook_syn() { 
      Object obj = new Object();
      int sum = 0;
      synchronized (obj) {
        for (int i = 0;i < 10;i++) {
          sum += i;
        }
      }
      return sum;
    }
    int sgx_hook_newintarr() {
      int[] arr = new int[3];
      arr[0] = 2;
      return arr[0];
    }
    int sgx_hook_newmultiintarr() {
      int[][] arr = new int[3][3];
      arr[0][0] = 2;
      return arr[0][0];
    }
    Object sgx_hook_newobjarr(Object o) {
      B[] arr_b = new B[3];
      B b = new B();
      arr_b[0] = b;
      assert(arr_b[0] == b);

      Object[] arr = new Object[10];
      arr[0] = o;
      return arr[0];
    }
    int sgx_hook_aastore(int[] m, int i) {
      m[0] = i;
      return m[0];
    }
    Object sgx_hook_aastore(Object[] m, Object i) {
      m[0] = i;
      return m[0];
    }
    Object sgx_hook_putgetfield(TestSuit s, Object o) {
      s.f_c = o;
      return o;
    }
    boolean sgx_hook_instanceof(TestSuit s) {
      Object o = (Object)s;
      TestSuit ss = (TestSuit)o;
      return o instanceof TestSuit;
    }
    int sgx_hook_forloop() {
      int sum = 0;
      for (int i = 0;i < 10;i++) {
        sum += 1;
      }
      return sum;
    }
    void sgx_hook_exception_active() {
      try {
        int a = 0;
          throw new TestException();
      } catch(TestException e) {
        // System.out.println("new");
      }
    }
    void sgx_hook_runtime_exception() {
      try {
        int a = 0;
        int c = 1 / a;
      } catch (java.lang.ArithmeticException e) {
        System.out.println("arith error");
      }
    }
    int sgx_hook_native_hashcode(Object o) { return o.hashCode(); }
    int sgx_hook_native_arraycopy(Object o) { return o.hashCode(); }
    int sgx_hook_native_reflect_new(Object o) { return o.hashCode(); }
    public static void main( String[] args ) {
      TestSuit h = new TestSuit();
//      assert(h.sgx_hook_syn() == 45);
//      System.out.println("Test: syn");
      assert(h.sgx_hook_add(1, 2) == 3);
      System.out.println("Test: 1 + 1 = 2");
      assert(h.sgx_hook_string() == "hook");
      System.out.println("Test: ldc string");
      assert(h.sgx_hook_invokevirtual(1, 2) == 3);
      System.out.println("Test: invokevirtual");
      assert(h.sgx_hook_new_invokevirtual() == 3);
      System.out.println("Test: new invokevirtual");
      assert(h.sgx_hook_invokeinterface(new TestSuit.HelloInterfaceImpl()) == 1);
      System.out.println("Test: invokeinterface");
      assert(h.sgx_hook_getfield(new TestSuit()) == 1);
      System.out.println("Test: getfield");
      assert(h.sgx_hook_putfiled(new TestSuit(), 3) == 3);
      System.out.println("Test: putfiled");
      assert(h.sgx_hook_putgetfield(h, (Object)h) == h);
      System.out.println("Test: putget object");
      assert(h.sgx_hook_forloop() == 10);
      System.out.println("Test: branch, while");
      assert(h.sgx_hook_instanceof(h) == true);
      System.out.println("Test: instanceOf, cast");
      assert(h.sgx_hook_aastore(new TestSuit[3], h) == h);
      System.out.println("Test: aastore, objarr");
      assert(h.sgx_hook_aastore(new int[3], 1) == 1);
      System.out.println("Test: aastore, intarr");
      assert(h.sgx_hook_newobj() == 10);
      System.out.println("Test: new obj");
      assert(h.sgx_hook_newintarr() == 2);
      System.out.println("Test: new int arr");
      assert(h.sgx_hook_newobjarr(h) == h);
      System.out.println("Test: new obj arr");
      assert(h.sgx_hook_newmultiintarr() == 2);
      System.out.println("Test: new multi int arr");
      assert(h.sgx_hook_native_hashcode(h) != 0);
      System.out.println("Test: native hashCode");
      h.sgx_hook_exception_active();
      h.sgx_hook_invokestatic();
      // h.sgx_hook_runtime_exception();
    }
}
