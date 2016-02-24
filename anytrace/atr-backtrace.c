#include <sys/user.h>
#include <sys/ptrace.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dwarf.h>

#include "anytrace/atr.h"
#include "anytrace/atr-process.h"
#include "anytrace/atr-file.h"

#include "anytrace/atr-backtrace.h"

static uint32_t
read4(unsigned char *p)
{
#ifdef __x86_64__
    return *(uint32_t*)p;
#elif ANYTRACE_BIG_ENDIAN == 1
    return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | (p[3]);
#else
    return (p[0]<<0) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
#endif

}

static uint64_t
read8(unsigned char *p)
{
#ifdef __x86_64__
    return *(uint64_t*)p;
#elif ANYTRACE_BIG_ENDIAN == 1
    return (((uint64_t)p[0])<<(8*7)) |
        (((uint64_t)p[1])<<(8*6)) |
        (((uint64_t)p[2])<<(8*5)) |
        (((uint64_t)p[3])<<(8*4)) |
        (((uint64_t)p[4])<<(8*3)) |
        (((uint64_t)p[5])<<(8*2)) |
        (((uint64_t)p[6])<<(8*1)) |
        (((uint64_t)p[7])<<(8*0));

#else
    return (((uint64_t)p[0])<<(8*0)) |
        (((uint64_t)p[1])<<(8*1)) |
        (((uint64_t)p[2])<<(8*2)) |
        (((uint64_t)p[3])<<(8*3)) |
        (((uint64_t)p[4])<<(8*4)) |
        (((uint64_t)p[5])<<(8*5)) |
        (((uint64_t)p[6])<<(8*6)) |
        (((uint64_t)p[7])<<(8*7));

#endif

}

static signed int
read_leb128(unsigned char *ptr,
            unsigned int *cur)
{
    unsigned int shift = 0;

    int32_t val = ptr[(*cur)++];

    /* sign extend */
    int32_t ret = ((val << 26)>>26);

    shift = 6;

    if (! (val & 0x80)) {
        return ret;
    }

    while (1) {
        signed int val = ptr[(*cur)++];

        ret |= (val & 0x7f) << shift;
        if ((ret & 0x80) == 0) {
            break;
        }

        shift += 7;
    }

    return ret;
}

static signed int
read_uleb128(unsigned char *ptr,
             unsigned int *cur)
{
    unsigned int shift = 0;

    uint32_t val = ptr[(*cur)++];
    uint32_t ret = val;

    shift = 6;

    if (! (val & 0x80)) {
        return ret;
    }

    while (1) {
        unsigned int val = ptr[(*cur)++];

        ret |= (val & 0x7f) << shift;
        if ((ret & 0x80) == 0) {
            break;
        }

        shift += 7;
    }

    return ret;
}


struct cfa_reg {
    int cfa_offset;
};

struct cfa_exec_env {
    struct cfa_reg *regs;

    int data_align;
    int code_align;

    int column_width;
    int cfa_offset;
    int cfa_reg;
    int return_address_column;
};

static void
dump_cfa_exec_env(struct cfa_exec_env *env)
{
    printf("cfa_offset = %d\n", env->cfa_offset);
    printf("cfa_reg = %d\n", env->cfa_reg);
    printf("nreg = %d\n", env->column_width);
    for (int ri=0; ri<env->column_width; ri++) {
        printf("reg[%3d] = %8d\n", ri, env->regs[ri].cfa_offset);
    }
}

static void
reserve_column_width(struct cfa_exec_env *env,
                     int column)
{
    if (env->column_width < column+1) {
        int prev = env->column_width;

        env->column_width = column + 1;
        env->regs = realloc(env->regs, env->column_width * sizeof(struct cfa_reg));

        for (int ri=prev; ri<column+1; ri++) {
            env->regs[ri].cfa_offset = 0;
        }

    }
}

static void
copy_cfa_env(struct cfa_exec_env *dst,
             const struct cfa_exec_env *src)
{
    reserve_column_width(dst, src->column_width);

    memcpy(dst->regs, src->regs,
           src->column_width * sizeof(struct cfa_reg));

    dst->cfa_offset = src->cfa_offset;
    dst->cfa_reg = src->cfa_reg;
    dst->return_address_column = src->return_address_column;

}


void
ATR_backtrace_init(struct ATR_backtracer *tr,
                   struct ATR_process *proc)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, proc->pid, NULL, &regs);

    tr->cfa_regs[0] = regs.rax;
    tr->cfa_regs[1] = regs.rdx;
    tr->cfa_regs[2] = regs.rcx;
    tr->cfa_regs[3] = regs.rbx;
    tr->cfa_regs[4] = regs.rsi;
    tr->cfa_regs[5] = regs.rdi;
    tr->cfa_regs[6] = regs.rbp;
    tr->cfa_regs[7] = regs.rsp;
    tr->cfa_regs[8] = regs.r8;
    tr->cfa_regs[9] = regs.r9;
    tr->cfa_regs[10] = regs.r10;
    tr->cfa_regs[11] = regs.r11;
    tr->cfa_regs[12] = regs.r12;
    tr->cfa_regs[13] = regs.r13;
    tr->cfa_regs[14] = regs.r14;
    tr->cfa_regs[15] = regs.r15;
    tr->cfa_regs[16] = regs.rip;
}

void
ATR_backtrace_fini(struct ATR_backtracer *tr)
{

}



/* return 1 if finished */
static int
exec_cfa(struct ATR *atr,
         struct cfa_exec_env *env,
         unsigned char *base,
         unsigned int *cur,
         unsigned int end,
         uintptr_t cfa_pc,
         uintptr_t real_pc)
{
    while (*(cur) < end) {
        unsigned int opc = base[*cur];

        //printf("%08x:opc = %x\n", *cur, opc);
        //dump_cfa_exec_env(env);

        (*cur)++;

        switch (opc & 0xc0) {
        case DW_CFA_advance_loc: {
            unsigned int advance_val = opc & 0x3f;
            cfa_pc += advance_val;
            //printf("advance %d (pc=%x, %x)\n", advance_val, (int)cfa_pc, (int)real_pc);
            if (cfa_pc >= real_pc) {
                return 1;
            }
        }
            break;

        case DW_CFA_offset: {
            int reg = opc & 0x3f;
            int off = read_leb128(base, cur);
            reserve_column_width(env, reg);
            env->regs[reg].cfa_offset = off * env->data_align;

            //printf("reg=%x off=%x\n", reg, off);
        }
            break;

        case DW_CFA_restore:
            //puts("dw_cfa_restore");
            break;

        default:
            switch (opc) {
            case DW_CFA_def_cfa: {
                signed int reg = read_leb128(base, cur);
                signed int off = read_leb128(base, cur);

                env->cfa_reg = reg;
                env->cfa_offset = off;
            }
                break;

            case DW_CFA_register: {        /* def cfa register */
                signed int reg = read_leb128(base, cur);
                env->cfa_reg = reg;
            }
                break;

            case DW_CFA_def_cfa_offset:{          /* def cfa offset */
                unsigned int off = read_leb128(base, cur);
                env->cfa_offset = off;
            }
                break;

            case DW_CFA_nop:
                break;

            case DW_CFA_undefined: {
                unsigned int reg = read_uleb128(base, cur);
                reserve_column_width(env, reg);
            }
                break;

            case DW_CFA_def_cfa_register: {
                unsigned int reg = read_uleb128(base, cur);
                env->cfa_reg = reg;
            }
                break;

            default:
                ATR_set_dwarf_unimplemented_cfa_op(atr, &atr->last_error, opc);
                printf("DW_?? (%x)\n", opc);
                return -1;
            }
            break;
        }

    }

    return 0;
}


int
ATR_backtrace_up(struct ATR *atr,
                 struct ATR_backtracer *tr,
                 struct ATR_process *proc)
{
    /* frame info */

    /* 1. extract from .eh_frame
     * 2. extract from debug?
     * 3. extract from 16(%rbp)?
     */
    struct ATR_file fp;

    struct cfa_exec_env cie_env;
    struct cfa_exec_env fde_env;
    uintptr_t pc = tr->cfa_regs[X8664_CFA_REG_RIP];
    uintptr_t pc_offset = 0;
    struct ATR_map_info mapi;

    int r = ATR_lookup_map_info(&mapi, atr, proc, pc);
    if (r != 0) {
        return -1;
    }
    //printf("pc = %llx\n", pc);

    r = ATR_file_open(&fp, atr, mapi.path);
    if (r != 0) {
        return -1;
    }

    pc_offset = mapi.offset;

    cie_env.regs = NULL;
    cie_env.column_width = 0;

    fde_env.regs = NULL;
    fde_env.column_width = 0;

    if (fp.eh_frame.length == 0) {
        ATR_file_close(atr, &fp);
        ATR_set_frame_info_not_found(atr, &atr->last_error, fp.path, pc);
        return -1;
    }

    int ret = -1;

    uintptr_t cur = 0;
    size_t length = fp.eh_frame.length;
    unsigned char *base = fp.mapped_addr + fp.eh_frame.start;

    unsigned int have_aug = 0;

    while (cur < length) {
        /* CIE */
        uint32_t length = read4(base + cur);
        if (length == 0) {
            break;
        }

        uint32_t id = read4(base + cur + 4);

        if (id == (0ULL)) {
            /* CIE */
            /* length  4
             * id      4
             * version 1
             */
            unsigned int cie_cur = cur + 9;
            have_aug = 0;

            while (1){ /* ?? */
                unsigned int c = base[cie_cur++];
                if (c == '\0') {
                    break;
                }
                if (c == 'z') {
                    have_aug = 1;
                }
            }

            cie_env.code_align = read_leb128(base, &cie_cur); /* code alignment factor */
            cie_env.data_align = read_leb128(base, &cie_cur); /* data lignment factor */

            cie_env.return_address_column = read_leb128(base, &cie_cur); /* return address register */

            reserve_column_width(&cie_env, cie_env.return_address_column);

            if (have_aug) {
                unsigned int length = read_leb128(base, &cie_cur);
                cie_cur += length;
            }

            //printf("cie = %x, cur = %x, pc = %x\n", (int)cie_cur, (int)cur, (int)pc_offset);
            r = exec_cfa(atr, &cie_env, base, &cie_cur, cur+length+4, 0, pc_offset);
            if (r < 0) {
                goto fini;
            }

        } else {
            uint32_t begin = read4(base + cur + 8);
            uint32_t range = read4(base + cur + 12);

            begin = (uint32_t)(fp.eh_frame.start + cur + 8 + begin);
            uint32_t end = begin + range;
            unsigned int fde_cur = cur + 16;

            if (pc_offset>=begin && pc_offset < end) {
                if (have_aug) {
                    unsigned int length = read_leb128(base, &fde_cur);
                    fde_cur += length;
                }

                copy_cfa_env(&fde_env, &cie_env);
                //printf("fde = %x, cur = %x, pc = %x\n", (int)fde_cur, (int)cur, (int)pc_offset);

                int r = exec_cfa(atr, &fde_env, base, &fde_cur, cur+length+4, begin, pc_offset);
                if (r == -1) {
                    goto fini;
                }

                struct user_regs_struct regs;
                r = ptrace(PTRACE_GETREGS, proc->pid, NULL, &regs);
                if (r == -1) {
                    ATR_set_libc_path_error(atr,
                                            &atr->last_error,
                                            errno, "ptrace");
                    goto fini;
                }

                uintptr_t cfa_val = 0;
                switch(fde_env.cfa_reg) {
                case 0: cfa_val = regs.rax; break;
                case 1: cfa_val = regs.rdx; break;
                case 2: cfa_val = regs.rcx; break;
                case 3: cfa_val = regs.rbx; break;
                case 4: cfa_val = regs.rsi; break;
                case 5: cfa_val = regs.rdi; break;
                case 6: cfa_val = regs.rbp; break;
                case 7: cfa_val = regs.rsp; break;

                default:
                    ATR_set_dwarf_unknown_cfa_reg(atr,
                                                  &atr->last_error,
                                                  fde_env.cfa_reg,
                                                  fp.path,
                                                  pc);
                    break;
                }

                uintptr_t cfa_top = cfa_val + fde_env.cfa_offset;
                uintptr_t ret_addr_pos = cfa_top + fde_env.regs[fde_env.return_address_column].cfa_offset;

                uintptr_t return_addr;
                errno = 0;
                return_addr = ptrace(PTRACE_PEEKDATA, proc->pid,
                                     (void*)ret_addr_pos, 0);

                if (errno != 0) {
                    ATR_set_read_frame_failed(atr, &atr->last_error,
                                              ret_addr_pos, errno);

                    goto fini;
                }

                tr->cfa_regs[X8664_CFA_REG_RIP] = return_addr;
                tr->cfa_regs[X8664_CFA_REG_RSP] = ret_addr_pos + 8;

                printf("return_addr=%p, ret_addr_pos=%p\n",
                       (int*)return_addr, (int*)ret_addr_pos);

                ret = 0;
                goto fini;
            } else {
                //printf("FDE[%x,%x] != %x\n", (int)begin, (int)end, (int)pc_offset);
            }
            /* FDE */
        }

        cur += length + 4;
    }

    ATR_set_frame_info_not_found(atr, &atr->last_error, fp.path, pc);

fini:
    ATR_file_close(atr, &fp);

    free(fde_env.regs);
    free(cie_env.regs);

    return ret;
}
