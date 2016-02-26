#include <stdio.h>
#include <unistd.h>

int f0() {
    printf("%d f0\n", getpid());
    sleep(10);
}
int f1() {
    printf("%d f1\n", getpid());
    sleep(10);
}
int f2() {
    printf("%d f2\n", getpid());
    sleep(10);
}
int f3() {
    printf("%d f3\n", getpid());
    sleep(10);
}
int f4() {
    printf("%d f4\n", getpid());
    sleep(10);
}

int
main()
{
    while (1) {
        f0();
        f1();
        f2();
        f3();
        f4();
    }
}