#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <sys/ptrace.h>

#include "npr/strbuf.h"
#include "npr/varray.h"

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

    long pt_result = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    if (pt_result != 0) {
        ATR_set_libc_path_error(atr,
                                &atr->last_error,
                                errno,
                                "ptrace");
        return -1;
    }

    int wait_st;
    waitpid(pid, &wait_st, 0);

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

    struct npr_varray modules;
    npr_varray_init(&modules, 4, sizeof(struct ATR_Module));

    struct npr_varray mappings;
    npr_varray_init(&mappings, 4, sizeof(struct ATR_Mapping));

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


        struct ATR_Module *m;
        char *module_path = npr_strbuf_c_str(&path_buf);
        int mi;

        for (mi=0; mi<modules.nelem; mi++) {
            m = VA_ELEM_PTR(struct ATR_Module, &modules, mi);
            if (strcmp(m->path, module_path) == 0) {
                break;
            }
        }

        if (mi == modules.nelem) {
            VA_NEWELEM_LASTPTR(struct ATR_Module, &modules, m);

            m->path = npr_strbuf_strdup_pool(&path_buf, &dst->allocator);
        }

        struct ATR_Mapping *ma;
        VA_NEWELEM_LASTPTR(struct ATR_Mapping, &mappings, ma);

        ma->start = start;
        ma->end = end;
        ma->offset = off;
        ma->module = mi;

        path_buf.cur = 0;
    }

    fclose(fp);
    npr_strbuf_fini(&path_buf);

    dst->pid = pid;

    dst->num_mapping = mappings.nelem;
    dst->mappings = npr_varray_close(&mappings, &dst->allocator);

    dst->num_module = modules.nelem;
    dst->modules = npr_varray_close(&modules, &dst->allocator);

    return 0;
}

void
ATR_close_process(struct ATR *atr,
                  struct ATR_Process *proc)
{
    npr_mempool_fini(&proc->allocator);
}


void
ATR_dump_process(FILE *fp,
                 struct ATR *atr,
                 struct ATR_Process *proc)
{
    fprintf(fp, "==modules==\n");
    for (int i=0; i<proc->num_module; i++) {
        fprintf(fp, "%s\n", proc->modules[i].path);
    }


    fprintf(fp, "==mappings==\n");
    for (int i=0; i<proc->num_mapping; i++) {
        fprintf(fp, "%32s(offset=%16" PRIxPTR "):start=%16"PRIxPTR", end=%16"PRIxPTR"\n",
                proc->modules[proc->mappings[i].module].path,
                proc->mappings[i].offset,
                proc->mappings[i].start,
                proc->mappings[i].end);
    }
}
