

#define HAVE_66H (1<<0)
#define HAVE_67H (1<<1)
#define HAVE_2EH (1<<2)
#define HAVE_3EH (1<<3)
#define HAVE_F0H (1<<4)
#define HAVE_F2H (1<<5)
#define HAVE_F3H (1<<6)

#define REX_W (1<<3)
#define REX_R (1<<2)
#define REX_X (1<<1)
#define REX_B (1<<0)

struct Decoder {
    struct npr_varray insns;
};

static int
emit_imm(struct Decoder *dec,
         unsigned int imm)
{
    struct DFDX_insn *insn;
    int ret = dec->insns.nelem;

    VA_NEWELEM_LASTPTR(struct DFDX_insn, &dec->insns, insn);

    insn->opc = DFDX_OPC_LOAD_IMM;
    insn->op_byte_size = 8;
    insn->dst_locs[0].code = DFDX_VALUE_LOC_TMP;
    insn->srcs[0] = imm;

    return ret;
}

static void
emit_operand1(struct Decoder *dec,
              enum DFDX_opcode opc,
              int op_8bit,
              unsigned int prefix,
              unsigned int rex,
              unsigned int dst_reg,
              unsigned int operand)
{
    int op_size = 1;
    if (!op_8bit) {
        if (rex & REX_W) {
            op_size = 8;
        } else if (prefix & HAVE_66H) {
            op_size = 2;
        } else {
            op_size = 1;
        }
    }

    struct DFDX_insn *insn;
    VA_NEWELEM_LASTPTR(struct DFDX_insn, &dec->insns, insn);

    insn->opc = opc;
    insn->op_byte_size = op_size;
    insn->dst_locs[0].code = DFDX_VALUE_LOC_REG;
    insn->dst_locs[0].offset = dst_reg;
}


static int
decode(struct Decoder *dec,
       unsigned char *code,
       size_t code_length,
       struct DFDX_error *error)
{
    unsigned int cur = 0;

    while (1) {
        int prefix = 0;
        unsigned int rex = 0;
        enum DFDX_opcode opc = 0;
        unsigned int operand = 0;
        unsigned int dst_reg = 0;
        int op_8bit = 0;

    decode_c0:
        if (cur >= code_length) {
            /* ok */

            return 0;
        }

        unsigned char c0 = code[cur++];

#ifdef __x86_64__
        if ((c0 & 0xf0) == 0x40) {
            rex = c0 & 0xf;
            goto decode_c0;
        }
#endif

        switch (c0) {
        case 0x66:
            prefix |= HAVE_66H;
            cur++;
            goto decode_c0;
        case 0x67:
            prefix |= HAVE_67H;
            cur++;
            goto decode_c0;
        case 0x2E:
            prefix |= HAVE_2EH;
            cur++;
            goto decode_c0;
        case 0x3E:
            prefix |= HAVE_3EH;
            cur++;
            goto decode_c0;
        case 0xF0:
            prefix |= HAVE_F0H;
            cur++;
            goto decode_c0;
        case 0xF2:
            prefix |= HAVE_F2H;
            cur++;
            goto decode_c0;
        case 0xF3:
            prefix |= HAVE_F3H;
            cur++;
            goto decode_c0;

#include "dfdX/x86-b0.h"

        default:
            set_error_code(error, DFDX_ERROR_UNDEFINED);
            error->u.undef_locatin = cur - 1;
            return -1;
        }


    operand_imm8:
        operand = emit_imm(dec, code[cur++]);
        emit_operand1(dec, opc, op_8bit, prefix, rex, operand, dst_reg);
        break;

    }

    return 0;
}