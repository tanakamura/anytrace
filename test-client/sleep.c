#include <syscall.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

int
main()
{
    while (1) {
        struct timespec ts = {10, 0};

        syscall(__NR_nanosleep, &ts);

    }
}