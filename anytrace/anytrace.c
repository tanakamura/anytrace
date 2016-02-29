#include <stdio.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>

#include "anytrace/atr.h"
#include "anytrace/atr-process.h"

static void
usage(const char *prog) {
    printf("usage : %s -p <pid>\n", prog);
}

int
main(int argc, char **argv)
{
    int pid = -1;

    while (1) {
        int c;
        c = getopt(argc, argv, "p:");
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'p':
            pid = atoi(optarg);
            break;

        default :
            usage(argv[0]);
            exit(1);
        }
    }

    if (pid == -1) {
        usage(argv[0]);
        exit(1);
    }

    struct ATR atr;
    struct ATR_process proc;

    ATR_init(&atr);

    int r = ATR_open_process(&proc, &atr, pid);
    if (r < 0) {
        ATR_perror(&atr);
        exit(1);
    }

    struct ATR_stack_frame frame;
    r = ATR_get_frame(&frame, &atr, &proc, proc.tasks[0]);
    if (r < 0) {
        ATR_perror(&atr);
        exit(1);
    }

    for (int di=0; di<frame.num_entry; di++) {
        struct ATR_stack_frame_entry *e = &frame.entries[di];

        printf("#%d ", di);
        if (e->flags & ATR_FRAME_HAVE_PC) {
            if (e->flags & ATR_FRAME_HAVE_SYMBOL) {
                printf("%s+0x%x(addr=%p) ",
                       ATR_get_symstr(e->symbol),
                       (int)e->symbol_offset,
                       (void*)e->pc);
            } else {
                printf("%p ",
                       (void*)e->pc);
            }
        }
        if (e->flags & ATR_FRAME_HAVE_OBJ_PATH) {
            printf("(%s)",
                   e->obj_path);
        }

        printf("\n");
    }

    ATR_frame_fini(&atr, &frame);

    ATR_close_process(&atr, &proc);
    ATR_fini(&atr);
}