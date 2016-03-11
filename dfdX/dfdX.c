#include "dfdX/dfdX.h"

#include "npr/varray.h"

static void
set_error_code(struct DFDX_error *error,
               enum DFDX_error_code code)
{
    error->code = code;
}

#include "dfdX/dfdX-decode-x86.c"

int
dfdx_decode_function(struct DFDX_function *func,
                     unsigned char *code,
                     size_t function_length,
                     struct npr_mempool *allocator,
                     struct DFDX_error *error)
{
    struct Decoder dec;
    npr_varray_init(&dec.insns, 16, sizeof(struct DFDX_insn));
    int r = decode(&dec, code, function_length, error);
    if (r < 0) {
        return r;
    }

    return 0;
}