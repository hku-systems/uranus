import  java.io.*;
import  java.util.*;
import edu.anonymity.sgx.*;
class ExceptionSuit {
  static class TestException extends Exception {}
    TestException t = new TestException();
    synchronized void s_call() {}
  @IntelSGX
  void runtime_exception() {
    try {
      int a = 0;
      int c = 1 / a;
    } catch (java.lang.ArithmeticException e) {
      System.out.println("arith error");
    }
  }
  @IntelSGX
  int exception_active() {
      // new TestException();
      // Collections.unmodifiableList(new ArrayList<Integer>(0));
      // StackTraceElement[] UNASSIGNED_STACK = new StackTraceElement[0];
      // s_call();
      try {
        throw t;
      }
      catch(Exception e) {
        return 1;
      }
  }
  @IntelSGX
  int runtime_throw_active() {
    int[] a = new int[3];
    int c = 3;
    try {
      c = a[-4];
    } catch(ArrayIndexOutOfBoundsException e) {
      return 2;
    }
    return c;
  }
  @IntelSGX
  int throw_ArithmeticException() {
    try {
      int c = 0;
      int a = 1 / c;
    } catch(ArithmeticException e) {
      return 2;
    }
    return 0;
  }
  @IntelSGX
  int throw_NullPointer() {
    try {
      Object a = this;
      a = null;
      a.hashCode();
    } catch(NullPointerException e) {
      return 2;
    }
    return 0;
  }
  int throw_not_catch() {
    int c = 0;
    int a = 1 / c;
    return a;
  }
  @IntelSGX
  int exception_nested() {
    try {
      return throw_not_catch();
    }catch(ArithmeticException e) {
      return 2;
    }
  }
  @IntelSGX
  int exception_not_catch() {
      return throw_not_catch();
  }
  public static void main( String[] args ) {
    ExceptionSuit s = new ExceptionSuit();
    // s.hashCode();
    // s.sgx_hook_exception_active();
    System.out.println(s.exception_active());
    System.out.println(s.runtime_throw_active());
    System.out.println(s.throw_ArithmeticException());
    System.out.println(s.throw_NullPointer());
    System.out.println(s.exception_nested());
    // System.out.println(s.exception_not_catch());
  }
}
