import sys

from lark import Lark, Transformer, v_args

edl_parser = r"""
    ?start: module
        | comment
    
    module: "enclave" "{" component* "}" ";"

    ?component: entry "{" defs* "}" ";"
        | impt
        | inc

    ?defs: func
        | comment
        | inc

    impt: "from" quote_string "import" quote_string

    inc: "include" quote_string

    quote_string: ESCAPED_STRING

    comment: line_comment
        | cross_comment

    line_comment: "//" comment_content
    cross_comment: "/*" all_content "*/"
    comment_content: /^[\n]+/
    all_content: (name|"."|"?"|","|"-")+

    entry: "trusted"    ->      trusted
        | "untrusted"   ->      untrusted

    func: level ttype name "(" pars ")" ";"

    level: "public" -> p_public 
        | "private" -> p_private
        | -> p_private

    ttype: pointer_type
        | types
    
    pointer_type: types "*"

    types: basic_type
        | struct_type
        | class_type

    struct_type: "struct" name

    class_type: name

    basic_type: "int" -> type_int
        | "long" -> type_long
        | "double" -> type_double
        | "float" -> type_float
        | "long long" -> type_ll
        | "long int" -> type_li
        | "char"     -> type_char

    name: CNAME

    pars: par
        | par "," pars

    par: cond_par
        | empty_par

    empty_par:

    cond_par: cond real_par

    real_par: ttype name

    cond: 
        | "[" conds "]"

    conds: cc 
        | cc "," conds

    cc: "in"
        | "out"
        | "len" "=" name
        | "string"

    %import common.CNAME
    %import common.ESCAPED_STRING
    %import common.SIGNED_NUMBER
    %import common.WS
    %ignore WS
"""

class TreeToJson(Transformer):

    def __init__(self, *args, **kwargs):
        self.do_import = None

    def impt(self, fname):
        return None

    @v_args(inline=True)
    def func(self, t, name, ps):
        return {"privilege" : level, "private": t, "name": name, "args": ps}
    
    @v_args(inline=True)
    def inc(self, f):
        return {"include": f}

    @v_args(inline=True)
    def func(self, level, t, name, ps):
        return {"privilege" : level, "return": t, "name": name, "args": ps}

    @v_args(inline=True)
    def component(self, entry, *func):
        ret = []
        inc = []
        for f in list(func):
            if not f == None:
                if 'include' in f:
                    inc.append(f)
                else:
                    ret.append(f)
        return {"entry": entry, "func": ret, "include": inc}
        
    @v_args(inline=True)
    def real_par(self, t, n):
        return [t, n]

    @v_args(inline=True)
    def name(self, c):
        return str(c[:])

    @v_args(inline=True)
    def quote_string(self, c):
        return str(c[:])

    @v_args(inline=True)
    def pars(self, p, ps = None):
        if not ps == None:
            ps = [p] + ps
            return ps
        return [p]
        # if pars == None:
        #     if par == None:
        #         return []
        #     return [par]
        # if par == None:
        #     return pars
        # pars.append(par)
        # return pars

    par = lambda self, p: p[0]
    level = lambda self, p: p
    defs = lambda self, p: p
    module = lambda self, p: p
    start = lambda self, p: p
    ttype = lambda self, p: p[0]
    types = lambda self, p: p[0]
    basic_type = lambda self, p: p[0]
    pointer_type = lambda self, p: p[0] + "*"
    struct_type = lambda self, p: "struct " + p[0]
    class_type = lambda self, p: p[0]
    empty_par = lambda self, _: None
    cond_par = lambda self, p: p[1]
    comment = lambda self, _: None

    trusted = lambda self, _: "trusted"
    untrusted = lambda self, _: "untrusted"
    type_int = lambda self, _: "int"
    type_li = lambda self, _: "long int"
    type_ll = lambda self, _: "long long"
    type_char = lambda self, _: "char"

    p_public = lambda self, _: "public"
    p_private = lambda self, _: "private"

# json_parser = Lark(json_grammar, parser='earley', lexer='standard')
# def parse(x):
#     return TreeToJson().transform(json_parser.parse(x))

transformer = TreeToJson()

json_parser = Lark(edl_parser, parser='lalr', lexer='standard', transformer=transformer)
parse = json_parser.parse

def parse_file(filename):
    with open(sys.argv[1]) as f:
        return parse(f.read())

transformer.do_import = parse_file

def test():
    j = parse_file(sys.argv[1])
    trusts = j[0]['func']
    untrusts = j[1]['func']
    print(trusts)
    print(untrusts)
        
    # start generate trust file header
    filename = sys.argv[1]

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



if __name__ == '__main__':
    test()
    # with open(sys.argv[1]) as f:
    #     print(parse(f.read()))