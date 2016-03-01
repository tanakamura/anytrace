#include <vector>
#include <string>


enum x86_insn_effect {
    MOV_8,
    MOV,
    ADD_8,
    ADD,
    SUB_8,
    SUB,
};

struct x86_operand_format {
    enum x86_insn_format_class {
        FMT_MODRM,
        FMT_MODRM_IMM8,
        FMT_MODRM_OP,
        FMT_MODRM_OP_IMM8,
    } class_;

    int v0;
};

static inline constexpr x86_operand_format
ModRM()
{
    x86_operand_format modrm = {x86_operand_format::FMT_MODRM, 0};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Imm8()
{
    x86_operand_format modrm = {x86_operand_format::FMT_MODRM_IMM8, 0};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Op(int opc)
{
    x86_operand_format modrm = {x86_operand_format::FMT_MODRM_OP, opc};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Op_Imm8(int opc)
{
    x86_operand_format modrm = {x86_operand_format::FMT_MODRM_OP_IMM8, opc};
    return modrm;
}



struct x86_opc {
    unsigned char opc;
    enum x86_insn_effect effect;
    struct x86_operand_format operand_format;
    int rm_operand_pos;
};

struct x86_insn_format {
    std::string mnemonic;

    std::vector<x86_opc> opcodes;
};
