# Uranus

Uranus is a easy-to-use SGX runtime based on OpenJDK. It provides
two annotation primitives: JECall and JOCall. For more details
on Uranus's design, please see our [AsiaCCS'20 paper](https://i.cs.hku.hk/~heming/papers/asiaccs20-uranus.pdf).

## How to build Uranus

Building Uranus is as easy as building OpenJDK. We have tested running Uranus with the following setup:

- Linux Ubuntu 16.04 LTS x64


First, we need to install a modified version of SGX SDK

```Bash
git clone https://github.com/intel/linux-sgx -b sgx_1.9
cd linux-sgx
./download_prebuilt.sh
# download dependencies
sudo apt-get install build-essential ocaml automake autoconf libtool wget python
# psw dependencies
sudo apt-get install libssl-dev libcurl4-openssl-dev protobuf-compiler libprotobuf-dev
# build sdk and psw
make sdk_install_pkg
make psw_install_pkg
# install sdk and psw
./linux/installer/bin/sgx_linux_x64_sdk_${version}.bin
./linux/installer/bin/sgx_linux_x64_psw_${version}.bin

# install simulation .so (for simulation only)
sudo cp ./linux/installer/common/sdk/output/package/lib64/libsgx_uae_service_sim.so /usr/lib/
sudo cp ./linux/installer/common/sdk/output/package/lib64/libsgx_urts_sim.so /usr/lib/
```

Then, install dependencies for openjdk

```Bash
sudo apt-get install cmake
```

Finally, compile Uranus:

```Bash
./confiure; make
```

If you want to build with the simulation mode, use

```Bash
make SGX_MODE=SIM
```


## How to run Uranus

Test your built:

```bash
cd ev_test
# compile your Java program
../build/linux-x86_64-normal-server-release/jdk/bin/javac TestSuit.java
# run test cases
../build/linux-x86_64-normal-server-release/jdk/bin/java TestSuit
```

Run a simple Fibonacci example

```java
import edu.anonymity.sgx.IntelSGX;
import edu.anonymity.sgx.IntelSGXOcall;
import edu.anonymity.sgx.Tools;
class SimpleExample {

    // JOCall
    @IntelSGXOcall
    public static void print_info() {
        System.out.println("call print_info");
    }

    // fibonacci calculation, JECall
    @IntelSGX
    public static int[] calculate(int n) {
        int cal_n = n;
        if (n <= 2)
            cal_n = 2;
        int[] arr = new int[cal_n];
        arr[0] = arr[1] = 1;
        for (int i = 2;i < cal_n;i++) {
            arr[i] = arr[i - 1] + arr[i - 2];
        }
        print_info();
        return (int[])Tools.deep_copy(arr);
    }

    public static void main(String[] args) {
        int[] arr = calculate(10);
        for (int i : arr) {
                System.out.println(i);
        }
    }
}
```

In the example above, calculate is a JECall (annotated using IntelSGX), and print\_info is a JOCall (annotated as IntelSGXOcall).
After computated the result, arr is an enclave object and needs to be copied out of enclave. Therefore, we use Tools.deep\_copy
to copy all data from enclave to non-enclave heap.

To run this example,

```bash
cd ev_test
# compile your Java program
../build/linux-x86_64-normal-server-release/jdk/bin/javac SimpleExample.java
# run test cases
../build/linux-x86_64-normal-server-release/jdk/bin/java SimpleExample
```
