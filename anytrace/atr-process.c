#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <sys/ptrace.h>

#include "npr/strbuf.h"
#include "npr/varray.h"
#include "npr/symbol.h"
#include "npr/red-black-tree.h"

#include "anytrace/atr.h"
#include "anytrace/atr-process.h"
#include "anytrace/atr-file.h"
#include "anytrace/atr-backtrace.h"

int
ATR_open_process(struct ATR_process *dst,
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
        if (errno == EPERM) {
            FILE *yama = fopen("/proc/sys/kernel/yama/ptrace_scope", "rb");
            int c = fgetc(yama);
            fclose(yama);

            if (c != '0') {
                ATR_set_error_code(atr, &atr->last_error, ATR_YAMA_ENABLED);
                return -1;
            }
        }

        ATR_set_libc_path_error(atr,
                                &atr->last_error,
                                errno,
                                "ptrace");
        return -1;
    }

    int wait_st;
    waitpid(pid, &wait_st, 0);

    char buf[1024];
    sprintf(buf, "/proc/%d/task", pid);
    DIR *tasks = opendir(buf);
    if (tasks == NULL) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        ATR_set_libc_path_error(atr, &atr->last_error, errno, buf);
        return -1;
    }

    dst->allocator = (struct npr_mempool*)malloc(sizeof(struct npr_mempool));
    npr_mempool_init(dst->allocator, 128);

    struct npr_varray tid_list;
    npr_varray_init(&tid_list, 4, sizeof(int));

    while (1) {
        struct dirent entry, *result;
        int r = readdir_r(tasks, &entry, &result);
        if (r != 0 || result == NULL) {
            closedir(tasks);
            break;
        }

        if (result->d_name[0] == '.') {
            continue;
        }

        int tid = atoi(result->d_name);
        VA_PUSH(int, &tid_list, tid);
    }

    dst->num_task = tid_list.nelem;
    dst->tasks = (int*)npr_varray_close(&tid_list, dst->allocator);

    sprintf(buf, "/proc/%d/maps", pid);
    FILE *fp = fopen(buf, "rb");

    if (fp == NULL) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        ATR_set_libc_path_error(atr, &atr->last_error, errno, buf);
        npr_mempool_fini(dst->allocator);
        free(dst->allocator);
        return -1;
    }

    struct npr_strbuf path_buf;
    npr_strbuf_init(&path_buf);

    struct npr_varray modules;
    npr_varray_init(&modules, 4, sizeof(struct ATR_module));

    struct npr_varray mappings;
    npr_varray_init(&mappings, 4, sizeof(struct ATR_mapping));

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


        struct ATR_module *m;
        char *module_path = npr_strbuf_c_str(&path_buf);
        int mi;

        for (mi=0; mi<modules.nelem; mi++) {
            m = VA_ELEM_PTR(struct ATR_module, &modules, mi);
            if (strcmp(m->path->symstr, module_path) == 0) {
                break;
            }
        }

        if (mi == modules.nelem) {
            VA_NEWELEM_LASTPTR(struct ATR_module, &modules, m);

            m->path = npr_intern(npr_strbuf_c_str(&path_buf));
        }

        struct ATR_mapping *ma;
        VA_NEWELEM_LASTPTR(struct ATR_mapping, &mappings, ma);

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
    dst->mappings = npr_varray_close(&mappings, dst->allocator);

    dst->num_module = modules.nelem;
    dst->modules = npr_varray_close(&modules, dst->allocator);

    return 0;
}

void
ATR_close_process(struct ATR *atr,
                  struct ATR_process *proc)
{
    ptrace(PTRACE_DETACH, proc->pid, NULL, NULL);

    npr_mempool_fini(proc->allocator);
    free(proc->allocator);
}


int
ATR_lookup_map_info(struct ATR_map_info *info,
                    struct ATR *atr,
                    struct ATR_process *proc,
                    uintptr_t addr)
{
    int nm = proc->num_mapping;
    for (int mi=0; mi<nm; mi++) {
        struct ATR_mapping *map = &proc->mappings[mi];

        if (addr >= map->start &&
            addr < map->end)
        {
            /* found */

            struct ATR_module *m = &proc->modules[map->module];

            info->path = m->path;

            uintptr_t map_offset = addr - map->start;
            info->offset = map_offset + map->offset;

            return 0;
        }
    }

    return -1;
}


void
ATR_dump_process(FILE *fp,
                 struct ATR *atr,
                 struct ATR_process *proc)
{
    fprintf(fp, "==modules==\n");
    for (int i=0; i<proc->num_module; i++) {
        fprintf(fp, "%s\n", proc->modules[i].path->symstr);
    }

    fprintf(fp, "==mappings==\n");
    for (int i=0; i<proc->num_mapping; i++) {
        fprintf(fp, "%32s(offset=%16" PRIxPTR "):start=%16"PRIxPTR", end=%16"PRIxPTR"\n",
                proc->modules[proc->mappings[i].module].path->symstr,
                proc->mappings[i].offset,
                proc->mappings[i].start,
                proc->mappings[i].end);
    }

    for (int ti=0; ti<proc->num_task; ti++) {
        int tid = proc->tasks[ti];
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, tid, NULL, &regs);

        fprintf(fp, "<task tid=%d>\n", tid);

        fprintf(fp, "==regs==\n");
        fprintf(fp, "bp=%16llx\n", regs.rbp);
        fprintf(fp, "sp=%16llx\n", regs.rsp);

        struct ATR_map_info map;

        int r = ATR_lookup_map_info(&map, atr, 
                                    proc, regs.rip);

        if (r < 0) {
            char *str = ATR_strerror(atr, &atr->last_error);
            fprintf(fp, "pc=%16llx unmapped? (%s)\n", regs.rip, str);
            ATR_free(atr, str);
            ATR_error_clear(atr, &atr->last_error);
        } else {
            fprintf(fp, "pc=%16llx (path=%s, file_offset=%"PRIxPTR")\n",
                    regs.rip,
                    map.path->symstr,
                    map.offset);

            struct ATR_file file;
            int r = ATR_file_open(&file, atr, map.path);

            if (r < 0) {
                ATR_perror(atr);
                return;
            }

            fprintf(fp,
                    "  text        =%16"PRIxPTR"-%16"PRIxPTR"\n"
                    "  debug_abbrev=%16"PRIxPTR"-%16"PRIxPTR"\n"
                    "  debug_info  =%16"PRIxPTR"-%16"PRIxPTR"\n"
                    "  eh_frame    =%16"PRIxPTR"-%16"PRIxPTR"\n",
                    file.text.start,
                    file.text.start + file.text.length,
                    file.debug_abbrev.start,
                    file.debug_abbrev.start + file.debug_abbrev.length,
                    file.debug_info.start,
                    file.debug_info.start + file.debug_info.length,
                    file.eh_frame.start,
                    file.eh_frame.start + file.eh_frame.length);

            ATR_file_close(atr, &file);

            struct ATR_backtracer tr;
            r = ATR_backtrace_init(atr, &tr, proc);
            if (r < 0) {
                ATR_perror(atr);
                return;
            }

            struct npr_rbtree visited;
            npr_rbtree_init(&visited);

            for (int depth=0; ; depth++) {
                struct ATR_addr_info ai;

                if (tr.state == ATR_BACKTRACER_OK) {
                    ATR_file_lookup_addr_info(&ai, atr, &tr);

                    if (ai.flags & ATR_ADDR_INFO_HAVE_SYMBOL) {
                        fprintf(fp,
                                "#%d %s+0x%x(addr=%p) (%s)\n",
                                depth,
                                ai.sym->symstr,
                                (int)ai.sym_offset,
                                (void*)tr.cfa_regs[X8664_CFA_REG_RIP],
                                tr.current_module.path->symstr);
                    } else {
                        fprintf(fp,
                                "#%d %p (%s)\n",
                                depth,
                                (void*)tr.cfa_regs[X8664_CFA_REG_RIP],
                                tr.current_module.path->symstr);
                    }

                    ATR_addr_info_fini(atr, &ai);
                } else {
                    fprintf(fp, "#%d %p\n",
                            depth,
                            (void*)tr.cfa_regs[X8664_CFA_REG_RIP]);
                    break;
                }

                int insert = npr_rbtree_insert(&visited, tr.cfa_regs[X8664_CFA_REG_RSP], 1);

                if (insert == 0) {
                    /* detect loop */
                    break;
                }

                r = ATR_backtrace_up(atr, &tr, proc);
                if (r != 0) {
                    ATR_perror(atr);
                    break;
                }

            }

            npr_rbtree_fini(&visited);

            ATR_backtrace_fini(atr, &tr);
        }
    }
}
