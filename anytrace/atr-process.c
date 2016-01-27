#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "npr/strbuf.h"

#include "anytrace/atr.h"
#include "anytrace/atr-process.h"

int
ATR_open_process(struct ATR_Process *dst,
                 struct ATR *atr,
                 int pid)
{
    if (pid < 0) {
        ATR_set_invalid_argument(atr,
                                 &atr->last_error,
                                 __FILE__,
                                 __func__,
                                 __LINE__);
        return -1;
    }

    char buf[1024];
    sprintf(buf, "/proc/%d/maps", pid);
    FILE *fp = fopen(buf, "rb");

    if (fp == NULL) {
        ATR_set_libc_path_error(atr, &atr->last_error, errno, buf);
        return -1;
    }

    npr_mempool_init(&dst->allocator, 128);

    struct npr_strbuf path_buf;
    npr_strbuf_init(&path_buf);

    while (1) {
    next_line:;
        char perm[5];
        long long start, end, off, ino;
        int devmj, devmn;
        int r =fscanf(fp, "%llx-%llx %s %llx %d:%d %lld",
                      &start, &end, perm, &off, &devmj, &devmn, &ino);

        if (r != 7) {
            break;
        }

        while (1) {
            int c = fgetc(fp);
            if (c != ' ') {
                if (c == '\n') {
                    goto next_line;
                } else {
                    npr_strbuf_putc(&path_buf, c);
                }
                break;
            }
        }

        while (1) {
            int c = fgetc(fp);
            if (c == '\n') {
                break;
            }
            
            npr_strbuf_putc(&path_buf, c);
        }

        char *module_path = npr_strbuf_strdup_pool(&path_buf, &dst->allocator);
        puts(module_path);

        path_buf.cur = 0;
    }

    fclose(fp);
    npr_strbuf_fini(&path_buf);

    return 0;
}

void
ATR_close_process(struct ATR *atr,
                  struct ATR_Process *proc)
{
    npr_mempool_fini(&proc->allocator);
    
}