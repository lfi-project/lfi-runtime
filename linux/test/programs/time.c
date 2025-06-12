#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main() {
    struct timespec start, end;

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime - start");
        return 1;
    }

    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 1000;
    if (nanosleep(&req, NULL) == -1) {
        perror("nanosleep");
        return 1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime - end");
        return 1;
    }

    printf("done\n");

    return 0;
}
