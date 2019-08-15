/*
 * Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

// Precompiled headers are turned off for Sun Studion,
// or if the user passes USE_PRECOMPILED_HEADER=0 to the makefiles.

#ifndef DONT_USE_PRECOMPILED_HEADER
# include "enclave/sc/EnclaveDebug.h"
# include "enclave/sc/EnclaveABI.h"
# include "enclave/sc/EnclaveMemory.h"
# include "enclave/sc/EnclaveGC.h"
# include "enclave/sc/EnclaveOcall.h"
# include "enclave/sc/EnclaveCrypto.h"
# include "enclave/sc/EnclaveException.h"
# include "enclave/sc/EnclaveRuntime.h"
# include "asm/assembler.hpp"
# include "asm/assembler.inline.hpp"
# include "asm/codeBuffer.hpp"
# include "asm/register.hpp"
# include "classfile/classLoader.hpp"
# include "classfile/javaClasses.hpp"
# include "classfile/systemDictionary.hpp"
# include "classfile/vmSymbols.hpp"
# include "code/codeBlob.hpp"
# include "code/stubs.hpp"
# include "interpreter/abstractInterpreter.hpp"
# include "interpreter/bytecode.hpp"
# include "interpreter/bytecodes.hpp"
# include "interpreter/interpreter.hpp"
# include "interpreter/templateInterpreter.hpp"
# include "interpreter/templateTable.hpp"
# include "memory/allocation.hpp"
# include "memory/allocation.inline.hpp"
# include "memory/iterator.hpp"
# include "memory/memRegion.hpp"
# include "memory/resourceArea.hpp"
# include "memory/specialized_oop_closures.hpp"
# include "memory/universe.hpp"
# include "memory/universe.inline.hpp"
# include "oops/arrayKlass.hpp"
# include "oops/arrayOop.hpp"
# include "oops/constMethod.hpp"
# include "oops/instanceKlass.hpp"
# include "oops/instanceOop.hpp"
# include "oops/klass.hpp"
# include "oops/klassPS.hpp"
# include "oops/klassVtable.hpp"
# include "oops/markOop.hpp"
# include "oops/markOop.inline.hpp"
# include "oops/method.hpp"
# include "oops/objArrayKlass.hpp"
# include "oops/objArrayOop.hpp"
# include "oops/oop.hpp"
# include "oops/oop.inline.hpp"
# include "oops/oop.inline2.hpp"
# include "oops/oopsHierarchy.hpp"
# include "oops/symbol.hpp"
# include "oops/typeArrayKlass.hpp"
# include "oops/typeArrayOop.hpp"
# include "prims/jni.h"
# include "prims/jvm.h"
# include "runtime/atomic.hpp"
# include "runtime/frame.hpp"
# include "runtime/frame.inline.hpp"
# include "runtime/globals.hpp"
# include "runtime/globals_extension.hpp"
# include "runtime/handles.hpp"
# include "runtime/handles.inline.hpp"
# include "runtime/icache.hpp"
# include "runtime/jniHandles.hpp"
# include "runtime/orderAccess.hpp"
# include "runtime/orderAccess.inline.hpp"
# include "runtime/os.hpp"
# include "runtime/registerMap.hpp"
# include "runtime/signature.hpp"
# include "runtime/stubCodeGenerator.hpp"
# include "runtime/synchronizer.hpp"
# include "runtime/thread.hpp"
# include "utilities/accessFlags.hpp"
# include "utilities/array.hpp"
# include "utilities/bitMap.hpp"
# include "utilities/bitMap.inline.hpp"
# include "utilities/constantTag.hpp"
# include "utilities/copy.hpp"
# include "utilities/exceptions.hpp"
# include "utilities/globalDefinitions.hpp"
# include "utilities/growableArray.hpp"
# include "utilities/hashtable.hpp"
# include "utilities/macros.hpp"
# include "utilities/ostream.hpp"
# include "utilities/sizes.hpp"
# include "utilities/top.hpp"
# include "utilities/utf8.hpp"


#endif // !DONT_USE_PRECOMPILED_HEADER
