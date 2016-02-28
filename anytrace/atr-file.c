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
#include "anytrace/atr-process.h"
#include "anytrace/atr-backtrace.h"
#include "config.h"

#if ANYTRACE_POINTER_SIZE == 8
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Off Elf_Off;
typedef Elf64_Half Elf_Half;
typedef Elf64_Sym Elf_Sym;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Off Elf_Off;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Half Elf_Half;
typedef Elf32_Sym Elf_Sym;
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
    fp->symtab.length = 0;
    fp->strtab.length = 0;
    fp->dynsym.length = 0;
    fp->dynstr.length = 0;

    fp->text.start = 0;
    fp->debug_abbrev.start = 0;
    fp->debug_info.start = 0;
    fp->eh_frame.start = 0;
    fp->symtab.start = 0;
    fp->strtab.start = 0;
    fp->dynsym.start = 0;
    fp->dynstr.start = 0;

    fp->path = path;

    for (int si=0; si<e_shnum; si++) {
        Elf_Shdr *sh = (Elf_Shdr*)(base + e_shoff + e_shentsize * si);
        char *name = (char*)(strtab + sh->sh_name);

#define SET_SECTION(st_name, sec_name)               \
        if (strcmp(name,sec_name) == 0) {            \
            fp->st_name.length = sh->sh_size;        \
            fp->st_name.start = sh->sh_offset;       \
            fp->st_name.entsize = sh->sh_entsize;    \
            fp->st_name.vaddr = sh->sh_addr;         \
        }

        SET_SECTION(text, ".text");
        SET_SECTION(debug_abbrev, ".debug_abbrev");
        SET_SECTION(debug_info, ".debug_info");
        SET_SECTION(eh_frame, ".eh_frame");
        SET_SECTION(symtab, ".symtab");
        SET_SECTION(dynsym, ".dynsym");
        SET_SECTION(strtab, ".strtab");
        SET_SECTION(dynstr, ".dynstr");
    }

    return 0;
}

void
ATR_file_close(struct ATR *atr, struct ATR_file *fp)
{
    munmap(fp->mapped_addr, fp->mapped_length);
    close(fp->fd);
}

static int
lookup_symtab(struct ATR_addr_info *info,
              struct ATR_section *s,
              struct ATR_section *str,
              unsigned char *base,
              uintptr_t pc)
{
    if (str->length == 0) {
        return -1;
    }
    char *strbase = (char*)base + str->start;

    if (s->length) {
        uintptr_t sptr = s->start;
        uintptr_t end = sptr + s->length;
        unsigned int entsize = s->entsize;

        while (sptr < end) {
            Elf_Sym *sym = (Elf_Sym*)(base + sptr);

            if (pc >= sym->st_value &&
                pc < (sym->st_value + sym->st_size))
            {
                info->flags |= ATR_ADDR_INFO_HAVE_SYMBOL;
                info->sym = npr_intern(strbase + sym->st_name);
                info->sym_offset = pc - sym->st_value;

                return 0;
            }

            sptr += entsize;
        }
    }

    return -1;
}


void
ATR_file_lookup_addr_info(struct ATR_addr_info *info,
                          struct ATR *atr,
                          struct ATR_backtracer *tr)
{
    info->flags = 0;

    struct ATR_file *fp = &tr->current_module;
    uintptr_t pc = tr->pc_offset_in_module-fp->text.start + fp->text.vaddr;

    unsigned char *base = fp->mapped_addr;

    int r = lookup_symtab(info, &fp->symtab, &fp->strtab, base, pc);
    if (r == -1) {
        lookup_symtab(info, &fp->dynsym, &fp->dynstr, base, pc);
    }

    /* 1. .debug_info (not yet)
     * 2. .symtab
     * 3. .dynsym (not yet)
     */

    return;
}

void
ATR_addr_info_fini(struct ATR *atr,
                   struct ATR_addr_info *info)
{
}
