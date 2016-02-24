#include <syscall.h>
#include <stdio.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

int
test(void)
{
    while (1) {
        printf("%d %p\n", getpid(), __builtin_return_address(0));

        struct timespec ts = {1, 0};

        syscall(__NR_nanosleep, &ts);
    }

    return 0;
}

int main()
{
    test();
}