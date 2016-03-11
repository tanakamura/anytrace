#ifndef DFDX_H
#define DFDX_H

/* Data Flow Decoder for eXecutable code */

#include <stddef.h>
#include <stdint.h>
#include "npr/mempool.h"

enum DFDX_error_code {
    DFDX_OK,
    DFDX_ERROR_ADDR_OUT_OF_RANGE,
    DFDX_ERROR_AMBIGUOUS_LOCATION,
    DFDX_ERROR_UNDEFINED
};

struct DFDX_error {
    enum DFDX_error_code code;

    union {
        uintptr_t undef_locatin;
    }u;
};

#define DFDX_FOR_EACH_OPCODE(F)                \
    F(MOV)                                     \
    F(ADD)                                     \
    F(SUB)                                     \
    F(LOAD)                                    \
    F(STORE)                                   \
    F(LOAD_IMM)                                \

enum DFDX_opcode {

#define DFDX_DEFINE_OPCODE(N)                   \
    DFDX_OPC_##N,

    DFDX_FOR_EACH_OPCODE(DFDX_DEFINE_OPCODE)
};

#define DFDX_INSN_SRC_OPERAND_MAX 3
#define DFDX_INSN_DST_OPERAND_MAX 2

#define DFDX_NUM_ARCH_REG 16

enum DFDX_value_location_code {
    DFDX_VALUE_LOC_STACK,
    DFDX_VALUE_LOC_REG,
    DFDX_VALUE_LOC_TMP
};

struct DFDX_value_location {
    enum DFDX_value_location_code code;
    int offset;
};

struct DFDX_arg_location_map {
    struct DFDX_value_location regs[DFDX_NUM_ARCH_REG];
    struct DFDX_value_location *stack;
};

struct DFDX_insn {
    enum DFDX_opcode opc;
    int op_byte_size;           // 1::8bit, 2::16bit, 4::32bit, 8::64bit

    unsigned int srcs[DFDX_INSN_SRC_OPERAND_MAX];

    struct DFDX_value_location dst_locs[DFDX_INSN_DST_OPERAND_MAX];
    struct DFDX_value_location src_locs[DFDX_INSN_SRC_OPERAND_MAX];
};

struct DFDX_bb {
    uintptr_t orig_addr_start, orig_addr_end;

    int num_succs;
    int num_preds;

    int *succs;
    int *preds;

    int insn_start, insn_end;
};

struct DFDX_function {
    int num_arg;
    int frame_size;

    int num_bb;
    struct DFDX_BB *bbs;

    int num_insn;
    struct DFDX_insn *insns;
};

int dfdx_decode_function(struct DFDX_function *func,
                         unsigned char *code,
                         size_t function_length,
                         struct npr_mempool *allocator,
                         struct DFDX_error *error);

/* return -1 if failed */
int dfdx_get_arg_location(struct DFDX_value_location *current_loc,
                          const struct DFDX_function *function,
                          uintptr_t pc,
                          const struct DFDX_value_location *start_loc,
                          struct DFDX_error *error);

void dfdx_error_init(struct DFDX_error *error);

#endif