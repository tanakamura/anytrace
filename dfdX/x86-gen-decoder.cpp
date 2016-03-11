#include "x86.hpp"
#include "dfdX.h"

static const char *opc_name_table[] = {
#define DFDX_DEFINE_OPCODE_STR(N)               \
    "DFDX_OPC_" #N ,

    DFDX_FOR_EACH_OPCODE(DFDX_DEFINE_OPCODE_STR)
};


struct ModRMOp {

};

struct FirstByteDecoder {
    enum x86_insn_format fmt;
    FirstByteDecoder() {
        fmt = FMT_UNDEF;
    }

    /* single_byte, next_modrm */
    struct x86_opcode *opc;
    struct x86_insn_mnemonic *insn;

    /* modrm_op */
    std::vector<ModRMOp> modrm_op_next;

    enum DFDX_opcode modrm_op_opcode[8];
};

static void
gen_decoder(FILE *out)
{
    bool x86_64 = false;
#ifdef __x86_64__
    x86_64 = true;
#endif

    FirstByteDecoder dec0[256];

    dec0[0x66].fmt = FMT_PREFIX;
    dec0[0x67].fmt = FMT_PREFIX;
    dec0[0x2e].fmt = FMT_PREFIX;
    dec0[0x3e].fmt = FMT_PREFIX;
    dec0[0xf0].fmt = FMT_PREFIX;
    dec0[0xf2].fmt = FMT_PREFIX;
    dec0[0xf3].fmt = FMT_PREFIX;

    for (auto &&f : x86_format_table) {
        for (auto &&o : f.opcodes) {
            unsigned char opc = o.opc;
            dec0[opc].opc = &o;
            dec0[opc].insn = &f;
            dec0[opc].fmt = o.operand_format.class_;

            switch (o.operand_format.class_) {
            case FMT_MODRM_OP:
            case FMT_MODRM_OP_IMM8:
            case FMT_MODRM_OP_IMM:
                dec0[opc].modrm_op_opcode[o.operand_format.v0] = o.dfdx_opcode;
                default:
                break;
            }
        }
    }

    for (int ci=0; ci<256; ci++) {
        FirstByteDecoder *fd = &dec0[ci];

        switch (fd->fmt) {
        case FMT_IMM8:
            fprintf(out, "case 0x%x:\n", ci);
            fprintf(out, " opc = %s;\n", opc_name_table[fd->opc->dfdx_opcode]);
            fprintf(out, " op_8bit = %s;\n", fd->opc->effect_8bit?"1":"0");
            fprintf(out, " dst_reg = 0; /* rax */\n");
            fprintf(out, " goto operand_imm8;\n");
            break;

        }
    }
}

int
main(int argc, char **argv)
{
    FILE *out0 = fopen(argv[1], "wb");
    FILE *out1 = fopen(argv[2], "wb");

    gen_decoder(out0);
}