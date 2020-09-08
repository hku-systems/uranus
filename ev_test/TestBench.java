
public class TestBench {
    public int field_1 = 1;
    public int field_2 = 2;
    public int field_3 = 3;

    static int static_field_1 = 4;
    static int static_field_2 = 5;

    static class B {
        public int field_1 = 6;
    };

    static public interface TestBenchInterface {
        public int do_compute();
    }

    static public class TestBenchInterfaceImpl implements TestBenchInterface {
        public int do_compute() {
          return 10;
        }
    }

    static int add(int a, int b) { return a + b; }
    public int add_virtual(int a, int b) { return a + b; }

    // test op2
    static int sgx_hook_int_add(int a, int b) { return a + b; }
    static int sgx_hook_int_minus(int a, int b) { return a - b; }
    static int sgx_hook_int_mul(int a, int b) { return a * b; }
    static int sgx_hook_int_div(int a, int b) { return a / b; }

    // test new
    static Object sgx_hook_newobj() { return new TestBench(); }
    static Object sgx_hook_newobj_2() { return new B(); }
    static Object sgx_hook_newarray() { return new int[10]; }
    static Object sgx_hook_newobjarray() { return new TestBench[10]; }
    static Object sgx_hook_multiarray() { return new int[10][10]; }
    static Object sgx_hook_multiobjarray() { return new TestBench[10][10]; }

    // test function call
    static int sgx_hook_string() { return "hook".length(); }
    static int sgx_hook_static(int a, int b) { return add(1, 2); }
    static int sgx_hook_virtual(int a, int b) { return  new TestBench().add_virtual(a, b); }
    static int sgx_hook_interface() {
        TestBenchInterface face = new TestBenchInterfaceImpl();
        return face.do_compute();
    }

    static int sgx_hook_getstatic_int() { return static_field_1; }
    static int sgx_hook_getfield_int() { return new TestBench().field_1; }

    static int sgx_hook_putstatic_int(int a) {
        static_field_1 = a;
        return static_field_1;
    }
    static int sgx_hook_putfield_int(int a) {
        TestBench test = new TestBench();
        test.field_1 = a;
        return test.field_1;
    }

    // test new init
    static int sgx_hook_newobj_init() { return new TestBench().field_1; }
    static int sgx_hook_newobj_2_init() { return new B().field_1; }
    static int sgx_hook_newarray_init(int a) { 
        int arr[] = new int[10]; 
        arr[3] = a;
        return arr[3];
    }
    static boolean sgx_hook_newobjarray_init() { 
        Object arr[] = new Object[10]; 
        Object o = new TestBench();
        arr[3] = o;
        return arr[3] == o;
    }
    static int sgx_hook_multiarray_init(int a) { 
        int arr[][] = new int[10][10]; 
        arr[1][1] = a;
        return arr[1][1];
    }
    static boolean sgx_hook_multiobjarray_init() {
        Object arr[][] = new Object[10][10]; 
        TestBench o = new TestBench();
        arr[1][1] = o;
        return arr[1][1] == o;
    }

    static boolean sgx_hook_instanceof() {
        Object o = (Object)new TestBench();
        return o instanceof TestBench; 
    }

    static int sgx_hook_forloop(int lp) {
        int sum = 0;
        for (int i = 0;i < lp;i++) {
          sum += 1;
        }
        return sum;
    }

    public static void main( String[] args ) {
        System.out.println("sgx_hook_int_add " + (sgx_hook_int_add(1, 2) == (1 + 2)));
        System.out.println("sgx_hook_int_minus " + (sgx_hook_int_minus(1, 2) == (1 - 2)));
        System.out.println("sgx_hook_int_mul " + (sgx_hook_int_mul(1, 2) == ( 1 * 2)));
        System.out.println("sgx_hook_int_div " + (sgx_hook_int_div(1, 2) == ( 1 / 2)));
        System.out.println("sgx_hook_newobj " + (sgx_hook_newobj() != null));
        System.out.println("sgx_hook_newobj_2 " + (sgx_hook_newobj_2() != null));
        System.out.println("sgx_hook_newarray " + (sgx_hook_newarray() != null));
        System.out.println("sgx_hook_newobjarray " + (sgx_hook_newobjarray() != null));
        System.out.println("sgx_hook_multiarray " + (sgx_hook_multiarray()!= null));
        System.out.println("sgx_hook_multiobjarray " + (sgx_hook_multiobjarray() != null));
        
        System.out.println("sgx_hook_string " + (sgx_hook_string() == "hook".length()));
        System.out.println("sgx_hook_static " + (sgx_hook_static(1, 2) == 3));
        System.out.println("sgx_hook_virtual " + (sgx_hook_virtual(1, 2) == 3));
        System.out.println("sgx_hook_interface " + (sgx_hook_interface() == 10));
        System.out.println("sgx_hook_getstatic_int " + (sgx_hook_getstatic_int() == 4));
        System.out.println("sgx_hook_getfield_int " + (sgx_hook_getfield_int() == 1));
        
        System.out.println("sgx_hook_putstatic_int " + (sgx_hook_putstatic_int(10) == 10));
        System.out.println("sgx_hook_putfield_int " + (sgx_hook_putfield_int(11) == 11));
        System.out.println("sgx_hook_newobj_init " + (sgx_hook_newobj_init() == 1));
        System.out.println("sgx_hook_newobj_2_init " + (sgx_hook_newobj_2_init() == 6));
        System.out.println("sgx_hook_newarray_init " + (sgx_hook_newarray_init(3) == 3));
        System.out.println("sgx_hook_newobjarray_init " + (sgx_hook_newobjarray_init() == true));
        System.out.println("sgx_hook_multiarray_init " + (sgx_hook_multiarray_init(4) == 4));
        System.out.println("sgx_hook_multiobjarray_init " + (sgx_hook_multiobjarray_init()));
        System.out.println("sgx_hook_instanceof " + (sgx_hook_instanceof()));
        System.out.println("sgx_hook_forloop " + (sgx_hook_forloop(10) == 10));
    }

};
