
package edu.anonymity.sgx;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/*
* IntelSGX is used for annotating function running inside enclave
* */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface IntelSGX {

}