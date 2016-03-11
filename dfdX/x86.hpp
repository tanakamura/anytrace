#include <vector>
#include <string>
#include "dfdX.h"

enum x86_insn_format {
    FMT_IMM8,
    FMT_IMM,
    FMT_MODRM,
    FMT_MODRM_IMM8,
    FMT_MODRM_IMM,
    FMT_MODRM_OP,
    FMT_MODRM_OP_IMM8,
    FMT_MODRM_OP_IMM,

    FMT_PREFIX,

    FMT_UNDEF
};

struct x86_operand_format {
    enum x86_insn_format class_;
    int v0;
};

static inline constexpr x86_operand_format
ModRM()
{
    x86_operand_format modrm = {FMT_MODRM, 0};
    return modrm;
}

static inline constexpr x86_operand_format
Imm8()
{
    x86_operand_format modrm = {FMT_IMM8, 0};
    return modrm;
}
static inline constexpr x86_operand_format
Imm()
{
    x86_operand_format modrm = {FMT_IMM, 0};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Imm8()
{
    x86_operand_format modrm = {FMT_MODRM_IMM8, 0};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Imm()
{
    x86_operand_format modrm = {FMT_MODRM_IMM, 0};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Op(int opc)
{
    x86_operand_format modrm = {FMT_MODRM_OP, opc};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Op_Imm8(int opc)
{
    x86_operand_format modrm = {FMT_MODRM_OP_IMM8, opc};
    return modrm;
}

static inline constexpr x86_operand_format
ModRM_Op_Imm(int opc)
{
    x86_operand_format modrm = {FMT_MODRM_OP_IMM, opc};
    return modrm;
}

struct x86_insn_mnemonic;

struct x86_opcode {
    unsigned char opc;
    enum DFDX_opcode dfdx_opcode;
    bool effect_8bit;

    struct x86_operand_format operand_format;
    int rm_operand_pos;

    struct x86_insn_mnemonic *mnemonics;
};

struct x86_insn_mnemonic {
    std::string mnemonic;

    std::vector<x86_opcode> opcodes;
};


void gen_decoder(bool x86_64);

extern std::vector<x86_insn_mnemonic> x86_format_table;
