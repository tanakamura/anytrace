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

    ATR_dump_process(stderr, &atr, &proc);

    ATR_close_process(&atr, &proc);
    ATR_fini(&atr);
}