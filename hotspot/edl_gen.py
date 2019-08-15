#!/bin/bash

import sys
import re
import nltk
filename=sys.argv[1]

trust_lines = []

# func_def = {
#     "privilege": "public",
#     "return": "void",
#     "args": [
#         ["int", "c"]
#     ]
# }

status = []
comments = []
def push_state(state):
    status.append(state)

def pop_state():
    if len(status) == 0:
        raise EnvironmentError
    status.remove(current_state())

def current_state():
    return status[len(status) - 1]

def process_entry(line, tokens):
    func = {
        "args": []
    }
    func_state = None
    par_pre_type = None
    for token in tokens:
        if func_state == None:
            if token == "public" or token == "private":
                func['privilege'] = token
                func_state = "privilege"
            else:
                func['privilege'] = "private"
                func['return'] = token
                func_state = "return"
        elif func_state == "privilege":
            func['return'] = token
            func_state = "return"
        elif func_state == "return":
            func['name'] = token
            func_state = "name"
        elif func_state == "name":
            assert(token == "(")
            func_state = "par"
        elif func_state == "par":
            # expect end/start of a para
            if token == "[":
                func_state = "ignore"
            elif token == ")":
                func_state = "par_end"
            elif token == ",":
                pass
            else:
                par_pre_type = token
                func_state = "par_type"
        elif func_state == "par_type":
            func['args'].append([par_pre_type, token])
            func_state = "par"
        elif func_state == "ignore":
            if token == "]":
                func_state = "par"
        elif func_state == "par_end":
            assert(token == ";")
        else:
            print func_state
            print token
            raise EnvironmentError
    return func

trusts = []
untrusts = []
with open(filename, 'r') as f:
    for line in f.readlines():
        tokens = nltk.word_tokenize(line)
        if len(tokens) == 0:
            continue
        key = tokens[0]
        if key == "include" or key == "from":
            continue
        if key == "enclave":
            push_state(key)
        elif key == "trusted":
            push_state(key)
        elif key == "untrusted":
            push_state(key)
        elif key == "}":
            pop_state()
        elif key == "/*" or key == "//":
            comments.append(line)
        else:
            # process entry
            func = process_entry(line, tokens)
            if current_state() == "trusted":
                trusts.append(func)
            elif current_state() == "untrusted":
                untrusts.append(func)
            else:
                print current_state()
    
# start generate trust file header

def generate_header_start(f, filename):
    f.write("#ifndef LINUX_SGX_SIM_%s\n"
            "#define LINUX_SGX_SIM_%s\n\n" 
            % (filename, filename))

def generate_header_end(f, filename):
    f.write("#endif //LINUX_SGX_SIM_%s\n"
            % filename)

def generate_C_start(f):
    f.write("#ifdef  __cplusplus\n"
            "extern \"C\" {\n"
            "#endif\n\n")

def generate_C_end(f):
    f.write("#ifdef  __cplusplus\n"
            "}\n"
            "#endif\n\n")

header_trts_base = filename.split(".edl")[0]

def pars2stream(par_arr, ret):
    s = ""
    start = True
    if not ret == "void":
        start = False
        s += "%s* retval" % ret
    for par in par_arr:
        if not start:
            s += ", "
        s += "%s %s" % (par[0], par[1])
        start = False
    return s

with open(header_trts_base + "_t.h", 'w') as f:
    generate_header_start(f, header_trts_base + "_T_H")
    generate_C_start(f)

    f.write("#include <sgx_trts.h>\n")

    for func in trusts:
        f.write("sgx_status_t sgx_%s(void* pms);\n\n" % func['name'])
    
    for func in untrusts:
        f.write("sgx_status_t SGX_CDECL %s(%s);\n\n" % (func['name'], pars2stream(func['args'], func['return'])))

    generate_C_end(f)
    generate_header_end(f, header_trts_base + "_T_H")

def par2struct(ret, args):
    struct_stream = ""
    if not ret == "void":
        struct_stream += "\t%s ms_%s;\n" % (ret, "retval")
    for arg in args:
        struct_stream += "\t%s ms_%s;\n" % (arg[0], arg[1])
    return struct_stream

def par2wrapinst(ret, args):
    parameter_stream = ""
    for arg in args:
        parameter_stream += "\tms.ms_%s = %s;\n" % (arg[1], arg[1])
    return parameter_stream

def par2unwrapinst(ret, args, name):
    parameter_stream = ""
    parameters = ""
    if not ret == "void":
        parameters = "ms->ms_retval = "
    start = True
    for arg in args:
        if not start:
            parameter_stream += ", "
        parameter_stream += "ms->ms_%s" % arg[1]
        start = False
    return "%s%s(%s);" % (parameters, name, parameter_stream)

def gen_struct(func, f):
    ret = func['return']
    name = func['name']
    args = func['args']
    struct_stream = par2struct(ret, args)
    struct = "typedef struct ms_%s_t {\n" \
                "%s" \
                "} ms_%s_t;\n\n" % (name, struct_stream, name)
    f.write(struct)

# generate trust file
with open(header_trts_base + "_t.c", 'w') as f:
    f.write("#include \"sgx_trts.h\"\n"
            "#include \"%s_t.h\"\n"
            "#include \"%s.h\"\n"
            "#include \"sys/types.h\"\n\n"
            % (header_trts_base, header_trts_base))

    t_lists = ""
    for t in trusts:
        t_lists += "\t\t{(void*)(uintptr_t)sgx_%s, 0},\n" % t['name']
    f.write("const struct {\n"
            "\tsize_t nr_ecall;\n"
            "\tstruct {void* ecall_addr; uint8_t is_priv;} ecall_table[%d];\n"
            "} g_ecall_table = {\n"
            "%d,\n"
            "\t{\n%s\t}\n"
            "};\n\n" % (len(trusts), len(trusts), t_lists))

    for idx, t in enumerate(untrusts):
        parameters = pars2stream(t['args'], t['return'])
        parameter_stream = par2wrapinst(t['return'], t['args'])
        gen_struct(t, f)

        func =  "sgx_status_t SGX_CDECL %s(%s) {\n" \
                "\tms_%s_t ms;\n" \
                "%s" \
                "\treturn sgx_ocall(%d, &ms);\n" \
                "}\n\n" % (t['name'], parameters, t['name'], parameter_stream, idx)
        f.write(func)

    for idx, t in enumerate(trusts):
        gen_struct(t, f)
        f.write("sgx_status_t SGX_CDECL sgx_%s(void* pms) {\n"
	            "\tms_%s_t* ms = (ms_%s_t*)pms;\n"
	            "\tsgx_status_t status = SGX_SUCCESS;\n"
                "\t%s\n"
                "\treturn status;\n"
                "}\n" % (t['name'], t['name'], t['name'], par2unwrapinst(t['return'], t['args'], t['name'])))
    
with open(header_trts_base + "_u.h", 'w') as f:
    generate_header_start(f, header_trts_base + "_U_H")
    generate_C_start(f)

    f.write("#include <sgx_urts.h>\n")

    for func in trusts:
        f.write("sgx_status_t %s(sgx_enclave_id_t eid, %s);\n\n" % (func['name'], pars2stream(func['args'], func['return'])))
    
    for func in untrusts:
        f.write("%s SGX_UBRIDGE(SGX_NOCONVENTION, %s, (%s));\n\n" % (func['return'], func['name'], pars2stream(func['args'], func['return'])))

    generate_C_end(f)
    generate_header_end(f, header_trts_base + "_U_H")

with open(header_trts_base + "_u.c", 'w') as f:
    f.write("#include \"sgx_urts.h\"\n"
        "#include \"%s_u.h\"\n"
        "#include \"sgx_edger8r.h\"\n"
        "#include \"sys/types.h\"\n\n"
        % header_trts_base)

    for idx, t in enumerate(untrusts):
        gen_struct(t, f)
        f.write("sgx_status_t SGX_CDECL enclave_%s(void* pms) {\n"
            "\tms_%s_t* ms = (ms_print_t*)pms;\n"
            "\t%s\n"
            "\treturn SGX_SUCCESS;\n"
            "}\n\n" % (t['name'], t['name'], par2unwrapinst(t['return'], t['args'], t['name'])))
        
    u_lists = ""
    for t in untrusts:
        u_lists += "\t\t(void*)enclave_%s,\n" % t['name']
    f.write("const struct {\n"
            "\tsize_t nr_ocall;\n"
            "\tvoid * table[%d];\n"
            "\t} ocall_table_enclave = {\n"
            "\t%d,\n"
            "\t{\n"
            "%s\n"
            "\t}\n"
            "};\n\n" % (len(untrusts), len(untrusts), u_lists))

    for idx, t in enumerate(trusts):
        gen_struct(t, f)
        parameters = pars2stream(t['args'], t['return'])
        parameter_stream = par2wrapinst(t['return'], t['args'])
        assign_return = "" if t['return'] == "void" else "\t*retval = ms.ms_retval;\n"
        func =  "sgx_status_t %s(sgx_enclave_id_t eid, %s) {\n" \
            "\tsgx_status_t status;\n" \
            "\tms_%s_t ms;\n" \
            "%s" \
            "\tstatus = sgx_ecall(eid, %d, &ocall_table_enclave, &ms);\n" \
            "%s" \
            "\treturn status;\n" \
            "}\n\n" % (t['name'], parameters, t['name'], parameter_stream, idx, assign_return)
        f.write(func)
