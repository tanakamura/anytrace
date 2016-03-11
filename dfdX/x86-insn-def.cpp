#include "x86.hpp"

std::vector<x86_insn_mnemonic>  x86_format_table = {
  {"mov", { {0x88, DFDX_OPC_MOV, true,ModRM(), 0},
            {0x89, DFDX_OPC_MOV, false, ModRM(), 0},
            {0x8a, DFDX_OPC_MOV, true, ModRM(), 1},
            {0x8b, DFDX_OPC_MOV, false, ModRM(), 1},
    },
  },

  {"add", { {0x04, DFDX_OPC_ADD, false, Imm8(), 1},
            {0x05, DFDX_OPC_ADD, false, Imm(), 1},
            {0x80, DFDX_OPC_ADD, true, ModRM_Imm(), 0},
            {0x81, DFDX_OPC_ADD, false, ModRM_Imm(), 0},
            {0x83, DFDX_OPC_ADD, false, ModRM_Op_Imm8(0), 0},
            {0x00, DFDX_OPC_ADD, true, ModRM(), 0},
            {0x01, DFDX_OPC_ADD, false, ModRM(), 0},
            {0x02, DFDX_OPC_ADD, true, ModRM(), 1},
            {0x03, DFDX_OPC_ADD, false, ModRM(), 1},
    },
  },

  {"sub", { {0x2c, DFDX_OPC_SUB, false, Imm8(), 1},
            {0x2d, DFDX_OPC_SUB, false, Imm(), 1},
            {0x80, DFDX_OPC_SUB, true, ModRM_Op_Imm8(5), 0},
            {0x81, DFDX_OPC_SUB, false, ModRM_Op_Imm(5), 0},
            {0x83, DFDX_OPC_SUB, false, ModRM_Op_Imm8(5), 0},
            {0x28, DFDX_OPC_SUB, true, ModRM(), 0},
            {0x29, DFDX_OPC_SUB, false, ModRM(), 0},
            {0x2a, DFDX_OPC_SUB, true, ModRM(), 1},
            {0x2b, DFDX_OPC_SUB, false, ModRM(), 1},
    },
  },
};