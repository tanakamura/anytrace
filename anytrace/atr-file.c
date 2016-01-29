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

static void
inc_column_width(uintptr_t **regs,
                 int *column_width,
                 uintptr_t max)
{
    if (*column_width <= max) {
        *column_width = max+1;

        *regs = realloc(*regs, *column_width * sizeof(uintptr_t));
    }
}

/* return 1 if finished */
static int
exec_cfa(unsigned char *base,
         unsigned int *cur,
         unsigned int end,
         uintptr_t *regs,
         uintptr_t cfa_pc,
         uintptr_t real_pc)
{
                
    while (*(cur) < end) {
        unsigned int opc = base[*cur];

        switch (opc & 0xc0) {
        case 0x40: {
            unsigned int advance_val = opc & 0x3f;
            cfa_pc += advance_val;
            printf("advance %d (pc=%x, %x)\n", advance_val, (int)cfa_pc, (int)real_pc);
            if (cfa_pc >= real_pc) {
                return 1;
            }
        }
            break;

        case 0x80:
            printf("dw_cfa_offset %d\n", opc&0x3f);
            break;

        case 0xc0:
            puts("dw_cfa_restore");
            break;

        default:
            switch (opc) {
            case 0xc: {
                (*cur)++;
                signed int reg = read_leb128(base, cur);
                signed int off = read_leb128(base, cur);

                printf("def_def_cfa %d %d\n", reg, off);
                (*cur)++;
            }
                break;

            case 0xe:{
                (*cur)++;
                unsigned int off = read_leb128(base, cur);
                printf("def_cfa_offset  %d\n", off);
            }
                break;

            case 0:
                puts("DW_nop");
                break;

            default:
                printf("DW_?? (%x)\n", opc);
                break;

            }
            break;
        }

        (*cur)++;
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

        if (fp->eh_frame.length == 0) {
            ATR_set_frame_info_not_found(atr, &info->frame_lookup_error, fp->path, offset);

            goto lookup_symbol;
        }

        uintptr_t cur = 0;
        size_t length = fp->eh_frame.length;
        unsigned char *base = fp->mapped_addr + fp->eh_frame.start;

        int column_width = 0;
        int init_column_width = 0;

        uintptr_t *regs = NULL;
        uintptr_t *regs_init = NULL;
        unsigned int return_adderss_column = 0;
        unsigned int cfa_column = 0;
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
                puts("CIE");

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

                return_adderss_column = read_leb128(base, &cie_cur); /* return address register */

                inc_column_width(&regs, &column_width, return_adderss_column);
                inc_column_width(&regs_init, &init_column_width, return_adderss_column);

                if (have_aug) {
                    unsigned int length = read_leb128(base, &cie_cur);
                    cie_cur += length;
                }

                exec_cfa(base, &cie_cur, cur+length+4, regs_init, 0, offset);

            } else {
                uint32_t begin = read4(base + cur + 8);
                uint32_t range = read4(base + cur + 12);

                begin = (uint32_t)(fp->eh_frame.start + cur + 8 + begin);
                uint32_t end = begin + range;
                unsigned int fde_cur = cur + 16;

                if (offset>=begin && offset < end) {
                    puts("FDE");

                    if (have_aug) {
                        unsigned int length = read_leb128(base, &fde_cur);
                        fde_cur += length;
                    }

                    int find = exec_cfa(base, &fde_cur, cur+length+4, regs, begin, offset);
                    if (find) {
                        

                        goto lookup_symbol;
                    }


                    printf("fde %"PRIxPTR", addr=%x-%x\n",
                           cur, begin, end);

                }
                /* FDE */
            }

            cur += length + 4;
        }

/*
 1 	length 	4 Byte 	int 	以下、No.2-xの部分のデータサイズ(Byte)です
2 	CIE_id 	8 Byte(64bit) 	unsigned long 	CIEかFDEかのデータの区別。0xffffffffffffffff ならCIE。それ以外はFDE
3 	version 	1 Byte 	unsigned char 	.debug_frame情報のバージョン。DWARFのバージョンとは違うものです。
4 	augmentation 	?(0x00までのデータずっと) 	BinaryData (UTF-8文字列) 	コンパイラ拡張による (通常無視OK？)
5 	code_alignment_factor 	uLEB128のサイズ 	uLEB128 	CFIルール表の行での命令アドレスのバウンダリサイズ。これが例えば４なら、CFIの各行の命令アドレスも４の倍数になる
6 	data_alignment_factor 	sLEB128のサイズ 	sLEB128 	先のCIE／FDEの命令にあった offset(N), val_offset(N)を計算する際の、データアドレスのバウンダリ値。この後の命令説明で出て来ます
7 	return_address_register 	uLEB128のサイズ 	uLEB128 	CFIルール表内のregisterのうち、関数の戻りアドレスを示すレジスタの番号
8 	initial_instructions 	No.1 length - (No.2-7,9の総和サイズ) 	(後述) 	CFIルール表の「列の定義」と「最初の行のデータ設定」 以下参照
9 	padding 	No.1 length - (No.2-8の総和サイズ) 	0x00 	単なるパディング 
*/

    }

lookup_symbol:;
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