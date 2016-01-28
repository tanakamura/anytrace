#ifndef ATR_FILE_H
#define ATR_FILE_H

#include "npr/symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR_section {
    uintptr_t length;           // 0 if empty
    uintptr_t start;
};

struct ATR_file {
    int fd;

    size_t mapped_length;
    unsigned char *mapped_addr;

    struct ATR_section text, debug_abbrev, debug_info, eh_frame;
};

/* return negative if failed */
int ATR_file_open(struct ATR_file *fp, struct ATR *atr, const char *path);
void ATR_file_close(struct ATR *atr, struct ATR_file *fp);

struct ATR_return_addr_pos {
    int sp_offset;
};

struct ATR_addr_info {
    struct npr_symbol *sym;

    int flags;
#define ATR_ADDR_INFO_HAVE_LOCATION (1<<0)
#define ATR_ADDR_INFO_HAVE_RETURN_ADDR (1<<1)

    struct npr_symbol *path;
    struct ATR_return_addr_pos return_addr_pos;
};

int ATR_file_lookup_addr_info(struct ATR_addr_info *info,
                              struct ATR *atr,
                              struct ATR_file *fp,
                              uintptr_t offset);

#ifdef __cplusplus
}
#endif

#endif