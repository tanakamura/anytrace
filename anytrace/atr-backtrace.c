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

static uint32_t
read2(unsigned char *p)
{
#ifdef __x86_64__
    return *(uint16_t*)p;
#elif ANYTRACE_BIG_ENDIAN == 1
    return (p[0]<<8) | (p[1]<<0);
#else
    return (p[0]<<0) | (p[1]<<8);
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
    uint32_t ret = 0;
    unsigned int val = 0;

    while (1) {
        val = ptr[(*cur)++];

        ret |= (val & 0x7f) << shift;
        shift += 7;
        if (! (val & 0x80)) {
            break;
        }
    }

    if (val & 0x40) {
        ret |= -(1<<shift);
    }

    return ret;
}

static signed int
read_uleb128(unsigned char *ptr,
             unsigned int *cur)
{
    unsigned int shift = 0;

    uint32_t ret = 0;

    while (1) {
        unsigned int val = ptr[(*cur)++];

        ret |= (val & 0x7f) << shift;
        shift += 7;
        if (! (val & 0x80)) {
            break;
        }
    }

    return ret;
}


struct cfa_reg {
    int defined;
    int cfa_offset;
};

struct cfa_exec_env {
    struct cfa_exec_env *chain;

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
            env->regs[ri].defined = 0;
        }
    }
}

static void
copy_cfa_env(struct cfa_exec_env *dst,
             struct cfa_exec_env *src)
{
    reserve_column_width(dst, src->column_width);

    memcpy(dst->regs, src->regs,
           src->column_width * sizeof(struct cfa_reg));

    dst->data_align = src->data_align;
    dst->code_align = src->code_align;
    dst->cfa_offset = src->cfa_offset;
    dst->cfa_reg = src->cfa_reg;
    dst->return_address_column = src->return_address_column;
}

static struct cfa_exec_env *
push_cfa_env(struct cfa_exec_env *env)
{
    struct cfa_exec_env *top = malloc(sizeof(struct cfa_exec_env));

    top->regs = NULL;
    top->column_width = 0;
    copy_cfa_env(top, env);

    top->chain = env;

    return top;
}

static struct cfa_exec_env *
pop_cfa_env(struct cfa_exec_env *env)
{
    struct cfa_exec_env *next = env->chain;

    free(env->regs);
    free(env);

    return next;
}

static void
free_cfa_env(struct cfa_exec_env *env)
{
    free(env->regs);
    free(env);
}

static void
cleanup_cfa_stack(struct cfa_exec_env *env,
                  struct cfa_exec_env *last)
{
    if (env == last) {
        return;
    }

    copy_cfa_env(last, env);

    while (env != last) {
        struct cfa_exec_env *n = env->chain;
        free_cfa_env(env);
        env = n;
    }
}


int
ATR_backtrace_init(struct ATR *atr,
                   struct ATR_backtracer *tr,
                   struct ATR_process *proc)
{
    struct user_regs_struct regs;
    errno = 0;
    ptrace(PTRACE_GETREGS, proc->pid, NULL, &regs);
    if (errno != 0) {
        ATR_set_libc_path_error(atr, &atr->last_error, errno, "ptrace");
        return -1;
    }

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


    struct ATR_map_info mapi;

    int r = ATR_lookup_map_info(&mapi, atr, proc, regs.rip);
    if (r != 0) {
        return -1;
    }
    //printf("file=%s, pc = %llx\n", mapi.path->symstr, pc);

    r = ATR_file_open(&tr->current_module, atr, mapi.path);
    if (r != 0) {
        return -1;
    }

    tr->pc_offset_in_module = mapi.offset;
    tr->state = ATR_BACKTRACER_OK;

    return 0;
}

void
ATR_backtrace_fini(struct ATR *atr, struct ATR_backtracer *tr)
{
    if (tr->state == ATR_BACKTRACER_OK) {
        ATR_file_close(atr, &tr->current_module);
    }
}


static void
set_cfa_offset(struct cfa_reg *reg, int offset)
{
    reg->defined = 1;
    reg->cfa_offset = offset;
}

/* return 1 if finished */
static int
exec_cfa(struct ATR *atr,
         struct cfa_exec_env *env_start,
         unsigned char *base,
         unsigned int *cur,
         unsigned int end,
         uintptr_t cfa_pc,
         uintptr_t real_pc)
{
    struct cfa_exec_env *env = env_start;
    int ret = -1;

    while (*(cur) < end) {
        unsigned int opc = base[*cur];

        //printf("%08x:opc = %x\n", *cur, opc);
        //dump_cfa_exec_env(env);

        (*cur)++;

#define ADVANCE_PC(A)                                                   \
        cfa_pc += A;                                                    \
        if (cfa_pc >= real_pc) {                                        \
            ret = 0;                                                    \
            goto fini;                                                  \
        }

        switch (opc & 0xc0) {
        case DW_CFA_advance_loc: {
            unsigned int advance_val = opc & 0x3f;
            ADVANCE_PC(advance_val);
        }
            break;

        case DW_CFA_offset: {
            int reg = opc & 0x3f;
            int off = read_leb128(base, cur);
            reserve_column_width(env, reg);
            set_cfa_offset(&env->regs[reg], off * env->data_align);

            //printf("reg=%x off=%x\n", reg, off);
        }
            break;

        case DW_CFA_restore:
            //puts("dw_cfa_restore");
            break;

        default:
            switch (opc) {
            case DW_CFA_advance_loc1: {
                unsigned int advance_val = base[(*cur)++];
                ADVANCE_PC(advance_val);
            }
                break;

            case DW_CFA_advance_loc2: {
                unsigned int advance_val = read2(base + *cur);
                (*cur) += 2;
                ADVANCE_PC(advance_val);
            }
                break;

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
                unsigned int off = read_uleb128(base, cur);
                //printf("def_cfa_offset %d\n", off);
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

            case DW_CFA_remember_state:
                env = push_cfa_env(env);
                break;

            case DW_CFA_restore_state:
                if (env == env_start) {
                    ATR_set_dwarf_invalid_cfa(atr, &atr->last_error, opc);
                    ret = -1;
                    goto fini;
                }

                env = pop_cfa_env(env);
                break;


            default:
                ATR_set_dwarf_unimplemented_cfa_op(atr, &atr->last_error, opc);
                printf("DW_?? (%x)\n", opc);
                ret = -1;
                goto fini;
            }
            break;
        }
    }

    ret = 0;

fini:
    cleanup_cfa_stack(env, env_start);
    return ret;
}

int
ATR_backtrace_up(struct ATR *atr,
                 struct ATR_backtracer *tr,
                 struct ATR_process *proc)
{
    if (tr->state != ATR_BACKTRACER_OK) {
        return 0;
    }

    /* frame info */

    /* 1. extract from .eh_frame
     * 2. extract from debug?
     * 3. extract from 16(%rbp)?
     */

    struct cfa_exec_env exec_env, *fde_env = NULL;
    uintptr_t pc = tr->cfa_regs[X8664_CFA_REG_RIP];
    uintptr_t pc_offset = pc_offset = tr->pc_offset_in_module;

    exec_env.chain = NULL;
    exec_env.regs = NULL;
    exec_env.column_width = 0;

    struct ATR_file *fp = &tr->current_module;

    if (fp->eh_frame.length == 0) {
        ATR_file_close(atr, &tr->current_module);
        ATR_set_frame_info_not_found(atr, &atr->last_error, fp->path, pc);
        return -1;
    }

    int ret = -1;

    uintptr_t cur = 0;
    size_t length = fp->eh_frame.length;
    unsigned char *base = fp->mapped_addr + fp->eh_frame.start;

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

            exec_env.code_align = read_leb128(base, &cie_cur); /* code alignment factor */
            exec_env.data_align = read_leb128(base, &cie_cur); /* data lignment factor */

            exec_env.return_address_column = read_leb128(base, &cie_cur); /* return address register */

            reserve_column_width(&exec_env, exec_env.return_address_column);

            if (have_aug) {
                unsigned int length = read_leb128(base, &cie_cur);
                cie_cur += length;
            }

            //printf("cie = %x, cur = %x, pc = %x\n", (int)cie_cur, (int)cur, (int)pc_offset);
            int r = exec_cfa(atr, &exec_env, base, &cie_cur, cur+length+4, 0, pc_offset);
            if (r < 0) {
                goto fini;
            }
        } else {
            uint32_t begin0 = read4(base + cur + 8);
            uint32_t range = read4(base + cur + 12);

            uint32_t begin = (uint32_t)(fp->eh_frame.start + cur + 8 + begin0);
            uint32_t end = begin + range;
            unsigned int fde_cur = cur + 16;

            if (pc_offset>=begin && pc_offset < end) {
                if (have_aug) {
                    unsigned int length = read_leb128(base, &fde_cur);
                    fde_cur += length;
                }

                fde_env = push_cfa_env(&exec_env);
                //printf("fde = %x, cur = %x, pc = %x\n", (int)fde_cur, (int)cur, (int)pc_offset);

                int r = exec_cfa(atr, fde_env, base, &fde_cur, cur+length+4, begin, pc_offset);
                if (r == -1) {
                    goto fini;
                }

                if (r == -1) {
                    ATR_set_libc_path_error(atr,
                                            &atr->last_error,
                                            errno, "ptrace");
                    goto fini;
                }

                //printf("offset = %d, %d, cfa_reg = %d\n",
                //       (int)fde_env->cfa_offset,
                //       (int)fde_env->regs[fde_env->return_address_column].cfa_offset,
                //       (int)fde_env->cfa_reg);

                uintptr_t cfa_val = tr->cfa_regs[fde_env->cfa_reg];
                uintptr_t cfa_top = cfa_val + fde_env->cfa_offset;

                int nc = exec_env.column_width;

                for (int ci=0; ci<nc; ci++) {
                    if (fde_env->regs[ci].defined) {
                        uintptr_t value_pos = cfa_top + fde_env->regs[ci].cfa_offset;
                        uintptr_t reg_value;
                        errno = 0;
                        reg_value = ptrace(PTRACE_PEEKDATA, proc->pid,
                                           (void*)value_pos, 0);

                        if (errno != 0) {
                            //printf("ci = %d %llx %llx\n", ci, (long long)fde_env->regs[ci].cfa_offset, (long long)cfa_top);
                            ATR_set_read_frame_failed(atr, &atr->last_error,
                                                      value_pos, errno);
                            goto fini;
                        }

                        if (ci < ATR_TRACER_NUM_REG) {
                            tr->cfa_regs[ci] = reg_value;
                        }
                    }
                }

                uintptr_t return_addr = tr->cfa_regs[fde_env->return_address_column];

                //dump_cfa_exec_env(fde_env);
                //printf("return_addr=%p, ret_addr_pos=%p, cfa_top=%p\n",
                //       (int*)return_addr,
                //       (int*)tr->cfa_regs[X8664_CFA_REG_RSP],
                //       (int*)cfa_top);

                tr->cfa_regs[X8664_CFA_REG_RSP] = cfa_top;

                ATR_file_close(atr, &tr->current_module);
                struct ATR_map_info mapi;
                r = ATR_lookup_map_info(&mapi, atr, proc, return_addr);
                if (r == 0) {
                    r = ATR_file_open(&tr->current_module, atr, mapi.path);
                    tr->pc_offset_in_module = mapi.offset;
                }

                if (r != 0) {
                    ATR_error_clear(atr, &atr->last_error);
                    tr->state = ATR_BACKTRACER_FRAME_BOTTOM;
                }
                //printf("file=%s, pc = %llx\n", mapi.path->symstr, pc);

                ret = 0;
                goto fini;
            } else {
                //printf("FDE[%x,%x] != %x\n", (int)begin, (int)end, (int)pc_offset);
            }
            /* FDE */
        }

        cur += length + 4;
    }

    ATR_set_frame_info_not_found(atr, &atr->last_error, fp->path, pc);

fini:

    if (fde_env) {
        free_cfa_env(fde_env);
        fde_env = NULL;
    }

    free(exec_env.regs);

    if (ret == -1) {
        ATR_file_close(atr, &tr->current_module);
        tr->state = ATR_BACKTRACER_HAVE_ERROR;
    }

    return ret;
}
