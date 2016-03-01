#include "x86.hpp"

std::vector<x86_insn_format>  x86_format_table = {
    {"mov", { {0x88, MOV_8, ModRM(), 0},
              {0x89, MOV, ModRM(), 0},
              {0x8a, MOV_8, ModRM(), 1},
              {0x8b, MOV, ModRM(), 1},
        },
    },

    {"add", { {0x83, ADD, ModRM_Op_Imm8(0), 0},
              {0x00, ADD_8, ModRM(), 0},
              {0x01, ADD, ModRM(), 0},
              {0x02, ADD_8, ModRM(), 1},
              {0x03, ADD, ModRM(), 1} },
    },

    {"sub", { {0x83, SUB, ModRM_Op_Imm8(0), 0},
              {0x00, SUB_8, ModRM(), 0},
              {0x01, SUB, ModRM(), 0},
              {0x02, SUB_8, ModRM(), 1},
              {0x03, SUB, ModRM(), 1} },
    },
};