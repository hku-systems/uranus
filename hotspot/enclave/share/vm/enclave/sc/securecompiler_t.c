#include "sgx_trts.h"
#include "enclave/sc/securecompiler_t.h"
#include "enclave/sc/securecompiler.h"
#include "sys/types.h"

const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv;} ecall_table[3];
} g_ecall_table = {
3,
	{
		{(void*)(uintptr_t)sgx_c1_initialize, 0},
		{(void*)(uintptr_t)sgx_gc_scavenge, 0},
		{(void*)(uintptr_t)sgx_interpreted_entry_zero_locals, 0},
	}
};

typedef struct ms_ocall_interpreter_t {
	void* ms_retval;
	void* ms_r14;
	int ms_size;
	void* ms_method;
	void* ms_thread;
	void* ms_sender;
} ms_ocall_interpreter_t;

sgx_status_t SGX_CDECL ocall_interpreter(void** retval, void* r14, int size, void* method, void* thread, void* sender) {
	ms_ocall_interpreter_t ms;
	ms.ms_r14 = r14;
	ms.ms_size = size;
	ms.ms_method = method;
	ms.ms_thread = thread;
	ms.ms_sender = sender;
	return sgx_ocall(0, &ms);
}

typedef struct ms_ocall_jvm_resolve_invoke_t {
	int ms_byte;
	void* ms_mh;
	int ms_bci;
	void* ms_recv;
	int ms_idx;
	void* ms_bcp;
	void* ms_recv_klass;
} ms_ocall_jvm_resolve_invoke_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke(int byte, void* mh, int bci, void* recv, int idx, void* bcp, void* recv_klass) {
	ms_ocall_jvm_resolve_invoke_t ms;
	ms.ms_byte = byte;
	ms.ms_mh = mh;
	ms.ms_bci = bci;
	ms.ms_recv = recv;
	ms.ms_idx = idx;
	ms.ms_bcp = bcp;
	ms.ms_recv_klass = recv_klass;
	return sgx_ocall(1, &ms);
}

typedef struct ms_ocall_jvm_resolve_method_C_t {
	void* ms_retval;
	int ms_bc;
	void* ms_constant;
	int ms_idx;
} ms_ocall_jvm_resolve_method_C_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_method_C(void** retval, int bc, void* constant, int idx) {
	ms_ocall_jvm_resolve_method_C_t ms;
	ms.ms_bc = bc;
	ms.ms_constant = constant;
	ms.ms_idx = idx;
	return sgx_ocall(2, &ms);
}

typedef struct ms_ocall_jvm_resolve_invoke_C_t {
	void* ms_retval;
	void* ms_recv;
	void* ms_constant;
	int ms_bidx;
	int ms_bc;
	void* ms_recv_klass;
} ms_ocall_jvm_resolve_invoke_C_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_C(void** retval, void* recv, void* constant, int bidx, int bc, void* recv_klass) {
	ms_ocall_jvm_resolve_invoke_C_t ms;
	ms.ms_recv = recv;
	ms.ms_constant = constant;
	ms.ms_bidx = bidx;
	ms.ms_bc = bc;
	ms.ms_recv_klass = recv_klass;
	return sgx_ocall(3, &ms);
}

typedef struct ms_ocall_jvm_resolve_invoke_handle_t {
} ms_ocall_jvm_resolve_invoke_handle_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_handle() {
	ms_ocall_jvm_resolve_invoke_handle_t ms;
	return sgx_ocall(4, &ms);
}

typedef struct ms_ocall_jvm_resolve_invoke_dynamic_t {
} ms_ocall_jvm_resolve_invoke_dynamic_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_dynamic() {
	ms_ocall_jvm_resolve_invoke_dynamic_t ms;
	return sgx_ocall(5, &ms);
}

typedef struct ms_ocall_jvm_ldc_t {
	void* ms_retval;
	bool ms_wide;
	void* ms_p;
	int ms_index;
} ms_ocall_jvm_ldc_t;

sgx_status_t SGX_CDECL ocall_jvm_ldc(void** retval, bool wide, void* p, int index) {
	ms_ocall_jvm_ldc_t ms;
	ms.ms_wide = wide;
	ms.ms_p = p;
	ms.ms_index = index;
	return sgx_ocall(6, &ms);
}

typedef struct ms_ocall_jvm_resolve_ldc_t {
	void* ms_retval;
	void* ms_p;
	int ms_index;
	int ms_byte;
} ms_ocall_jvm_resolve_ldc_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_ldc(void** retval, void* p, int index, int byte) {
	ms_ocall_jvm_resolve_ldc_t ms;
	ms.ms_p = p;
	ms.ms_index = index;
	ms.ms_byte = byte;
	return sgx_ocall(7, &ms);
}

typedef struct ms_ocall_jvm_resolve_get_put_t {
	int ms_retval;
	int ms_byte;
	void* ms_mh;
	int ms_idx;
	void* ms_bcp;
} ms_ocall_jvm_resolve_get_put_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_get_put(int* retval, int byte, void* mh, int idx, void* bcp) {
	ms_ocall_jvm_resolve_get_put_t ms;
	ms.ms_byte = byte;
	ms.ms_mh = mh;
	ms.ms_idx = idx;
	ms.ms_bcp = bcp;
	return sgx_ocall(8, &ms);
}

typedef struct ms_ocall_jvm_resolve_get_put_klass_t {
	void* ms_retval;
	int ms_byte;
	void* ms_mh;
	int ms_idx;
	void* ms_bcp;
} ms_ocall_jvm_resolve_get_put_klass_t;

sgx_status_t SGX_CDECL ocall_jvm_resolve_get_put_klass(void** retval, int byte, void* mh, int idx, void* bcp) {
	ms_ocall_jvm_resolve_get_put_klass_t ms;
	ms.ms_byte = byte;
	ms.ms_mh = mh;
	ms.ms_idx = idx;
	ms.ms_bcp = bcp;
	return sgx_ocall(9, &ms);
}

typedef struct ms_ocall_klass_type_array_t {
	void* ms_retval;
} ms_ocall_klass_type_array_t;

sgx_status_t SGX_CDECL ocall_klass_type_array(void** retval) {
	ms_ocall_klass_type_array_t ms;
	return sgx_ocall(10, &ms);
}

typedef struct ms_ocall_klass_type_t {
	void* ms_retval;
} ms_ocall_klass_type_t;

sgx_status_t SGX_CDECL ocall_klass_type(void** retval) {
	ms_ocall_klass_type_t ms;
	return sgx_ocall(11, &ms);
}

typedef struct ms_ocall_obj_array_klass_get_t {
	void* ms_retval;
	void* ms_pool;
	int ms_index;
} ms_ocall_obj_array_klass_get_t;

sgx_status_t SGX_CDECL ocall_obj_array_klass_get(void** retval, void* pool, int index) {
	ms_ocall_obj_array_klass_get_t ms;
	ms.ms_pool = pool;
	ms.ms_index = index;
	return sgx_ocall(12, &ms);
}

typedef struct ms_ocall_klass_resolve_or_fail_t {
	void* ms_retval;
	char* ms_name;
} ms_ocall_klass_resolve_or_fail_t;

sgx_status_t SGX_CDECL ocall_klass_resolve_or_fail(void** retval, char* name) {
	ms_ocall_klass_resolve_or_fail_t ms;
	ms.ms_name = name;
	return sgx_ocall(13, &ms);
}

typedef struct ms_ocall_klass_find_or_null_t {
	void* ms_retval;
	char* ms_name;
} ms_ocall_klass_find_or_null_t;

sgx_status_t SGX_CDECL ocall_klass_find_or_null(void** retval, char* name) {
	ms_ocall_klass_find_or_null_t ms;
	ms.ms_name = name;
	return sgx_ocall(14, &ms);
}

typedef struct ms_ocall_klass_get_t {
	void* ms_retval;
	void* ms_pool;
	int ms_index;
} ms_ocall_klass_get_t;

sgx_status_t SGX_CDECL ocall_klass_get(void** retval, void* pool, int index) {
	ms_ocall_klass_get_t ms;
	ms.ms_pool = pool;
	ms.ms_index = index;
	return sgx_ocall(15, &ms);
}

typedef struct ms_ocall_klass_compute_oopmap_t {
	void* ms_klass;
	void* ms_m;
	int ms_bci;
} ms_ocall_klass_compute_oopmap_t;

sgx_status_t SGX_CDECL ocall_klass_compute_oopmap(void* klass, void* m, int bci) {
	ms_ocall_klass_compute_oopmap_t ms;
	ms.ms_klass = klass;
	ms.ms_m = m;
	ms.ms_bci = bci;
	return sgx_ocall(16, &ms);
}

typedef struct ms_ocall_array_klass_t {
	void* ms_retval;
	void* ms_klass;
	int ms_rank;
	int ms_get_all;
} ms_ocall_array_klass_t;

sgx_status_t SGX_CDECL ocall_array_klass(void** retval, void* klass, int rank, int get_all) {
	ms_ocall_array_klass_t ms;
	ms.ms_klass = klass;
	ms.ms_rank = rank;
	ms.ms_get_all = get_all;
	return sgx_ocall(17, &ms);
}

typedef struct ms_ocall_multi_array_klass_get_t {
	void* ms_retval;
	void* ms_pool;
	int ms_index;
} ms_ocall_multi_array_klass_get_t;

sgx_status_t SGX_CDECL ocall_multi_array_klass_get(void** retval, void* pool, int index) {
	ms_ocall_multi_array_klass_get_t ms;
	ms.ms_pool = pool;
	ms.ms_index = index;
	return sgx_ocall(18, &ms);
}

typedef struct ms_ocall_jvm_pre_native_t {
	void* ms_method;
	int ms_resolve;
} ms_ocall_jvm_pre_native_t;

sgx_status_t SGX_CDECL ocall_jvm_pre_native(void* method, int resolve) {
	ms_ocall_jvm_pre_native_t ms;
	ms.ms_method = method;
	ms.ms_resolve = resolve;
	return sgx_ocall(19, &ms);
}

typedef struct ms_ocall_jvm_malloc_t {
	void* ms_retval;
	int ms_size;
} ms_ocall_jvm_malloc_t;

sgx_status_t SGX_CDECL ocall_jvm_malloc(void** retval, int size) {
	ms_ocall_jvm_malloc_t ms;
	ms.ms_size = size;
	return sgx_ocall(20, &ms);
}

typedef struct ms_c1_initialize_t {
	void* ms_retval;
	void* ms_cpuid;
	void** ms_top;
	void** ms_bottom;
	void** ms_klass_list;
	int ms_debug;
} ms_c1_initialize_t;

sgx_status_t SGX_CDECL sgx_c1_initialize(void* pms) {
	ms_c1_initialize_t* ms = (ms_c1_initialize_t*)pms;
	sgx_status_t status = SGX_SUCCESS;
	ms->ms_retval = c1_initialize(ms->ms_cpuid, ms->ms_top, ms->ms_bottom, ms->ms_klass_list, ms->ms_debug);
	return status;
}
typedef struct ms_gc_scavenge_t {
	void* ms_tasks;
	int ms_n;
} ms_gc_scavenge_t;

sgx_status_t SGX_CDECL sgx_gc_scavenge(void* pms) {
	ms_gc_scavenge_t* ms = (ms_gc_scavenge_t*)pms;
	sgx_status_t status = SGX_SUCCESS;
	gc_scavenge(ms->ms_tasks, ms->ms_n);
	return status;
}
typedef struct ms_interpreted_entry_zero_locals_t {
	void* ms_retval;
	void* ms_rbx;
	void* ms_method;
	int* ms_has_exception;
} ms_interpreted_entry_zero_locals_t;

sgx_status_t SGX_CDECL sgx_interpreted_entry_zero_locals(void* pms) {
	ms_interpreted_entry_zero_locals_t* ms = (ms_interpreted_entry_zero_locals_t*)pms;
	sgx_status_t status = SGX_SUCCESS;
	ms->ms_retval = interpreted_entry_zero_locals(ms->ms_rbx, ms->ms_method, ms->ms_has_exception);
	return status;
}
