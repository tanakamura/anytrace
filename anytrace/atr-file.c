#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>
#include <inttypes.h>

#include "anytrace/atr.h"
#include "anytrace/atr-file.h"
#include "config.h"

#if ANYTRACE_POINTER_SIZE == 8
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Off Elf_Off;
typedef Elf64_Half Elf_Half;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Off Elf_Off;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Half Elf_Half;
#endif

int
ATR_file_open(struct ATR_file *fp, struct ATR *atr, struct npr_symbol *path)
{
    int fd = open(path->symstr, O_RDONLY);

    if (fd < -1) {
        ATR_set_libc_path_error(atr, &atr->last_error, errno, path->symstr);
        return -1;
    }

    struct stat st;
    int r = fstat(fd, &st);
    if (r < 0) {
        close(fd);
        ATR_set_libc_path_error(atr, &atr->last_error, errno, path->symstr);
        return -1;
    }

    if (! S_ISREG(st.st_mode)) {
        close(fd);
        ATR_set_unknown_mapped_file_type(atr, &atr->last_error, path->symstr);
        return -1;
    }

    size_t length = st.st_size;

    void *mapped_addr = mmap(0, length, PROT_READ,
                             MAP_PRIVATE, fd, 0);

    if (mapped_addr == MAP_FAILED) {
        close(fd);
        ATR_set_libc_path_error(atr, &atr->last_error, errno, path->symstr);
        return -1;
    }

    fp->fd = fd;
    fp->mapped_length = length;
    fp->mapped_addr = mapped_addr;

    Elf_Ehdr *ehdr = (Elf_Ehdr*)mapped_addr;

    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3)
    {
        munmap(fp->mapped_addr, fp->mapped_length);
        close(fd);
        ATR_set_unknown_mapped_file_type(atr, &atr->last_error, path->symstr);

        return -1;
    }

    Elf_Off e_shoff = ehdr->e_shoff;
    Elf_Half e_shentsize = ehdr->e_shentsize;
    Elf_Half e_shnum =  ehdr->e_shnum;
    int str = ehdr->e_shstrndx;

    unsigned char *base = (unsigned char*)mapped_addr;
    Elf_Shdr *shstrtab = (Elf_Shdr*)(base + e_shoff + e_shentsize * str);

    unsigned char *strtab = base + shstrtab->sh_offset;

    fp->text.length = 0;
    fp->debug_abbrev.length = 0;
    fp->debug_info.length = 0;
    fp->eh_frame.length = 0;
    fp->text.start = 0;
    fp->debug_abbrev.start = 0;
    fp->debug_info.start = 0;
    fp->eh_frame.start = 0;
    fp->path = path;

    for (int si=0; si<e_shnum; si++) {
        Elf_Shdr *sh = (Elf_Shdr*)(base + e_shoff + e_shentsize * si);
        char *name = (char*)(strtab + sh->sh_name);


#define SET_SECTION(st_name, sec_name)               \
        if (strcmp(name,sec_name) == 0) {            \
            fp->st_name.length = sh->sh_size;        \
            fp->st_name.start = sh->sh_offset;                          \
            fprintf(stderr, "%s : %x %x\n", sec_name, (int)sh->sh_size, (int)sh->sh_offset); \
        }

        SET_SECTION(text, ".text");
        SET_SECTION(debug_abbrev, ".debug_abbrev");
        SET_SECTION(debug_info, ".debug_info");
        SET_SECTION(eh_frame, ".eh_frame");
    }

    return 0;
}

void
ATR_file_close(struct ATR *atr, struct ATR_file *fp)
{
    munmap(fp->mapped_addr, fp->mapped_length);
    close(fp->fd);
}

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

static unsigned int
read_leb128(unsigned char *ptr,
            unsigned int *cur)
{
    unsigned int ret = 0;
    unsigned int shift = 0;
    int neg = 0;

    signed int val = ptr[(*cur)++];

    if (val & 0x40) {
        neg = 1;
    }
    ret = val & 0x3f;
    shift = 6;

    if (! (val & 0x80)) {
        return neg?-val:val;
    }

    while (1) {
        signed int val = ptr[(*cur)++];

        ret |= (val & 0x7f) << shift;
        if ((ret & 0x80) == 0) {
            break;
        }

        shift += 7;
    }

    return neg ? -ret : ret;
}

struct cfa_reg {
    int cfa_offset;
};

struct cfa_exec_env {
    struct cfa_reg *regs;

    int column_width;
    int cfa_offset;
    int cfa_reg;
    int return_address_column;
};

static void
reserve_column_width(struct cfa_exec_env *env,
                     int column)
{
    if (env->column_width < column+1) {
        env->column_width = column + 1;
        env->regs = realloc(env->regs, env->column_width * sizeof(struct cfa_reg));
    }
}

static void
copy_cfa_env(struct cfa_exec_env *dst,
             const struct cfa_exec_env *src)
{
    reserve_column_width(dst, src->column_width);

    memcpy(dst->regs, src->regs,
           sizeof(src->column_width) * sizeof(struct cfa_reg));

    dst->cfa_offset = src->cfa_offset;
    dst->cfa_reg = src->cfa_reg;
    dst->return_address_column = src->return_address_column;

}



/* return 1 if finished */
static int
exec_cfa(struct cfa_exec_env *env,
         unsigned char *base,
         unsigned int *cur,
         unsigned int end,
         uintptr_t cfa_pc,
         uintptr_t real_pc)
{
                
    while (*(cur) < end) {
        unsigned int opc = base[*cur];

        //printf("opc = %x\n", opc);
        (*cur)++;

        switch (opc & 0xc0) {
        case 0x40: {
            unsigned int advance_val = opc & 0x3f;
            cfa_pc += advance_val;
            //printf("advance %d (pc=%x, %x)\n", advance_val, (int)cfa_pc, (int)real_pc);
            if (cfa_pc >= real_pc) {
                return 1;
            }
        }
            break;

        case 0x80: {
            unsigned int reg = opc & 0x3f;
            unsigned int off = read_leb128(base, cur);

            reserve_column_width(env, reg);

            env->regs[reg].cfa_offset = off;
        }
            break;

        case 0xc0:
            //puts("dw_cfa_restore");
            break;

        default:
            switch (opc) {      /* def cfa */
            case 0xc: {
                signed int reg = read_leb128(base, cur);
                signed int off = read_leb128(base, cur);

                //printf("def_cfa %d %d\n", reg, off);

                env->cfa_reg = reg;
                env->cfa_offset = off;
            }
                break;

            case 0x09: {        /* def cfa register */
                signed int reg = read_leb128(base, cur);
                env->cfa_reg = reg;
            }
                break;

            case 0xe:{          /* def cfa offset */
                unsigned int off = read_leb128(base, cur);
                env->cfa_offset = off;
            }
                break;

            case 0:             /* nop */
                break;

            default:
                printf("DW_?? (%x)\n", opc);
                break;
            }
            break;
        }

    }

    return 0;
}


void
ATR_file_lookup_addr_info(struct ATR_addr_info *info,
                          struct ATR *atr,
                          struct ATR_process *proc,
                          struct ATR_file *fp,
                          uintptr_t offset)
{
    info->sym_lookup_error.code = ATR_NO_ERROR;
    info->location_lookup_error.code = ATR_NO_ERROR;
    info->frame_lookup_error.code = ATR_NO_ERROR;

    {
        /* frame info */

        /* 1. extract from .eh_frame
         * 2. extract from debug?
         * 3. extract from 16(%rbp)?
         */
        struct cfa_exec_env cie_env;
        struct cfa_exec_env fde_env;

        cie_env.regs = NULL;
        cie_env.column_width = 0;

        fde_env.regs = NULL;
        fde_env.column_width = 0;

        if (fp->eh_frame.length == 0) {
            ATR_set_frame_info_not_found(atr, &info->frame_lookup_error, fp->path, offset);

            goto lookup_symbol;
        }

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

                read_leb128(base, &cie_cur); /* code alignment factor */
                read_leb128(base, &cie_cur); /* data lignment factor */

                cie_env.return_address_column = read_leb128(base, &cie_cur); /* return address register */

                reserve_column_width(&cie_env, cie_env.return_address_column);

                if (have_aug) {
                    unsigned int length = read_leb128(base, &cie_cur);
                    cie_cur += length;
                }

                exec_cfa(&cie_env, base, &cie_cur, cur+length+4, 0, offset);
            } else {
                uint32_t begin = read4(base + cur + 8);
                uint32_t range = read4(base + cur + 12);

                begin = (uint32_t)(fp->eh_frame.start + cur + 8 + begin);
                uint32_t end = begin + range;
                unsigned int fde_cur = cur + 16;

                if (offset>=begin && offset < end) {
                    if (have_aug) {
                        unsigned int length = read_leb128(base, &fde_cur);
                        fde_cur += length;
                    }

                    copy_cfa_env(&fde_env, &cie_env);
                    int find = exec_cfa(&fde_env, base, &fde_cur, cur+length+4, begin, offset);
                    if (find) {
                        goto lookup_symbol;
                    }
                }
                /* FDE */
            }

            cur += length + 4;
        }

lookup_symbol:;
        free(cie_env.regs);
        free(fde_env.regs);
    }

    {
        /* symbol info */

        /* 1. extract from .debug_info?
         * 2. extract from .symtab
         * 3. extract from .dynsym
         */
    }

    
lookup_location:;
    {
        /* location info */

        /* 1. extract from .debug_info? */
        
    }

    return;
}

void
ATR_addr_info_fini(struct ATR *atr,
                   struct ATR_addr_info *info)
{
    ATR_error_clear(atr, &info->sym_lookup_error);
    ATR_error_clear(atr, &info->location_lookup_error);
    ATR_error_clear(atr, &info->frame_lookup_error);
}