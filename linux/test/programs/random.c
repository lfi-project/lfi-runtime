#define _GNU_SOURCE  // Required for getrandom
#include <stdio.h>
#include <unistd.h>
#include <sys/random.h>
#include <errno.h>
#include <string.h>

int main() {
    unsigned char buffer[16];

    ssize_t bytes_read = getrandom(buffer, sizeof(buffer), 0);
    if (bytes_read == -1) {
        perror("getrandom");
        return 1;
    }

    printf("done\n");

    return 0;
}
