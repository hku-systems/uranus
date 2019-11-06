
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;
import java.lang.Thread;
import java.util.Random;
import java.util.concurrent.*;
class ConcurrentSyn extends Thread {

    static ConcurrentHashMap<Integer, Integer> table = null;

    static int val = 0;

    public int count;

    public ConcurrentSyn(int _c) {
        count = _c;
    }

    @Override
    public void run() {
        long s = System.currentTimeMillis();
        sgx_hook_compute(count);
        System.out.println(System.currentTimeMillis() - s);
    }

    static long sgx_hook_compute(int n) {
        Random random = new Random(100);
        long sum = 0;
        int h;
        for (int i = 0;i < 100;i++) {
            int key = random.nextInt();
            int value = random.nextInt();
            synchronized (table) {
                table.put(key, value);
            }
        }
        return sum;
    }

    static int sgx_hook_iterate() {
        return table.size();
    }


    public static void main(String[] args) {
        table = new ConcurrentHashMap<Integer, Integer>();
        for (int i = 0; i < Integer.parseInt(args[0]); i++) {
            new ConcurrentSyn(40000).start();
        }
        try {
            Thread.sleep(2000);
        } catch (InterruptedException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        System.out.println(sgx_hook_iterate());
    }
}