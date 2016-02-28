#ifndef ATR_FILE_H
#define ATR_FILE_H

#include "npr/symbol.h"
#include "anytrace/atr-errors.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR_process;
struct ATR_backtracer;

struct ATR_section {
    uintptr_t length;           // 0 if empty
    uintptr_t start;
    uintptr_t vaddr;
    unsigned int entsize;
};

struct ATR_file {
    struct npr_symbol *path;
    int fd;

    size_t mapped_length;
    unsigned char *mapped_addr;

    struct ATR_section text, debug_abbrev, debug_info, eh_frame, symtab, strtab;
};

/* return negative if failed */
int ATR_file_open(struct ATR_file *fp, struct ATR *atr, struct npr_symbol *path);
void ATR_file_close(struct ATR *atr, struct ATR_file *fp);

struct ATR_addr_info {
    int flags;                  // 0 if notfound
#define ATR_ADDR_INFO_HAVE_SYMBOL (1<<0)
#define ATR_ADDR_INFO_HAVE_LOCATION (1<<1)

    struct npr_symbol *sym;
    uintptr_t sym_offset;

    struct npr_symbol *source_path;
};


void ATR_file_lookup_addr_info(struct ATR_addr_info *info,
                               struct ATR *atr,
                               struct ATR_backtracer *tr);

void ATR_addr_info_fini(struct ATR *atr,
                        struct ATR_addr_info *info);

#ifdef __cplusplus
}
#endif

#endif