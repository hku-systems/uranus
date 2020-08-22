public class TestSuit_2 {
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

    static int add(int a, int b) { return a + b; }
    static int constance() {return 0; }
    static int new_invokevirtual() {
                 TestSuit_2 s = new TestSuit_2();
                 return s.add(1, 2);
               }
    static int invokeinterface(HelloInterface i) { return i.me(); }
    static int newobj() { return new B().m; }
    static int newintarr() {
          int[] arr = new int[3];
          arr[0] = 2;
          return arr[0];
        }
    static int newmultiintarr() {
                 int[][] arr = new int[3][3];
                 arr[0][0] = 2;
                 return arr[0][0];
               }
    static Object newobjarr(Object o) {
                 B[] arr_b = new B[3];
                 B b = new B();
                 arr_b[0] = b;
                 assert(arr_b[0] == b);

                 Object[] arr = new Object[10];
                 arr[0] = o;
                 return arr[0];
               }

    static int aastore(int[] m, int i) {
          m[0] = i;
          return m[0];
        }

    static Object aastore(Object[] m, Object i) {
                 m[0] = i;
                 return m[0];
               }

    static Object putgetfield(TestSuit_2 s, Object o) {
                 s.f_c = o;
                 return o;
               }
    static boolean _instanceof(TestSuit_2 s) {
                 Object o = (Object)s;
                 TestSuit_2 ss = (TestSuit_2)o;
                 return o instanceof TestSuit_2;
               }


    int sgx_hook_add(int a, int b) { return a + b; }
    String sgx_hook_string() { return "hook"; }
    int sgx_hook_invokevirtual(int a, int b) { return add(1, 2); }
    int sgx_hook_new_invokevirtual() {
      //TestSuit_2 s = new TestSuit_2();
      //return s.add(1, 2);
      return new_invokevirtual();
    }
    void sgx_hook_invokestatic() { static_call(); }
    int sgx_hook_invokeinterface(HelloInterface i) {
        return invokeinterface(i);
        //return i.me();
    }
    int sgx_hook_getfield(TestSuit_2 s) { return s.f_b; }
    int sgx_hook_putfiled(TestSuit_2 s, int i) { s.f_a = i; return s.f_a; }
    int sgx_hook_newobj() {
        return newobj();
        //return new B().m;
    }
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
    return newintarr();
    /*
      int[] arr = new int[3];
      arr[0] = 2;
      return arr[0];
    */
    }
    int sgx_hook_newmultiintarr() {
    return newmultiintarr();
    /*
      int[][] arr = new int[3][3];
      arr[0][0] = 2;
      return arr[0][0];
    */
    }
    Object sgx_hook_newobjarr(Object o) {
    return newobjarr(o);
    /*
      B[] arr_b = new B[3];
      B b = new B();
      arr_b[0] = b;
      assert(arr_b[0] == b);

      Object[] arr = new Object[10];
      arr[0] = o;
      return arr[0];
    */
    }
    int sgx_hook_aastore(int[] m, int i) {
    return aastore(m,i);
    /*
      m[0] = i;
      return m[0];
    */
    }
    Object sgx_hook_aastore(Object[] m, Object i) {
    return aastore(m,i);
    /*
      m[0] = i;
      return m[0];
    */
    }
    Object sgx_hook_putgetfield(TestSuit_2 s, Object o) {
    return putgetfield(s,o);
    /*
      s.f_c = o;
      return o;
    */
    }
    boolean sgx_hook_instanceof(TestSuit_2 s) {
    return _instanceof(s);
    /*
      Object o = (Object)s;
      TestSuit_2 ss = (TestSuit_2)o;
      return o instanceof TestSuit_2;
    */
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
      TestSuit_2 h = new TestSuit_2();
      //assert(h.sgx_hook_syn() == 45);
      //System.out.println("Test: syn");
      assert(h.sgx_hook_add(1, 2) == 3);
      System.out.println("Test: 1 + 1 = 2");
      //assert(h.sgx_hook_string() == "hook");
      //System.out.println("Test: ldc string");

      add(1, 2);
      assert(h.sgx_hook_invokevirtual(1, 2) == 3);
      System.out.println("Test: invokevirtual");

      new_invokevirtual();
      assert(h.sgx_hook_new_invokevirtual() == 3);
      System.out.println("Test: new invokevirtual");


      invokeinterface(new TestSuit_2.HelloInterfaceImpl());
      assert(h.sgx_hook_invokeinterface(new TestSuit_2.HelloInterfaceImpl()) == 1);
      System.out.println("Test: invokeinterface");

      assert(h.sgx_hook_getfield(new TestSuit_2()) == 1);
      System.out.println("Test: getfield");
      assert(h.sgx_hook_putfiled(new TestSuit_2(), 3) == 3);
      System.out.println("Test: putfiled");

      putgetfield(h, (Object)h);
      assert(h.sgx_hook_putgetfield(h, (Object)h) == h);
      System.out.println("Test: putget object");

      assert(h.sgx_hook_forloop() == 10);
      System.out.println("Test: branch, while");
      _instanceof(h);
      assert(h.sgx_hook_instanceof(h) == true);
      System.out.println("Test: instanceOf, cast");

      aastore(new TestSuit_2[3], h);
      assert(h.sgx_hook_aastore(new TestSuit_2[3], h) == h);
      System.out.println("Test: aastore, objarr");

      aastore(new int[3], 1);
      assert(h.sgx_hook_aastore(new int[3], 1) == 1);
      System.out.println("Test: aastore, intarr");

      newobj();
      assert(h.sgx_hook_newobj() == 10);
      System.out.println("Test: new obj");

      //cannot run
      newintarr();
      assert(h.sgx_hook_newintarr() == 2);
      System.out.println("Test: new int arr");

      //cannot run
      newobjarr(h);
      assert(h.sgx_hook_newobjarr(h) == h);
      System.out.println("Test: new obj arr");

      //cannot run
      newmultiintarr();
      assert(h.sgx_hook_newmultiintarr() == 2);
      System.out.println("Test: new multi int arr");

      //cannot run
      h.hashCode();
      assert(h.sgx_hook_native_hashcode(h) != 0);
      System.out.println("Test: native hashCode");

      static_call();
      h.sgx_hook_invokestatic();

      //cannot run
      h.sgx_hook_exception_active();

      //cannot run
      h.sgx_hook_runtime_exception();
    }
}
