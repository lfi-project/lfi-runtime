#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];

    // Check for file existence
    if (access(path, F_OK) == 0) {
        printf("File '%s' exists.\n", path);
    } else {
        printf("F_OK check (file existence)\n");
    }

    // Check for read permission
    if (access(path, R_OK) == 0) {
        printf("File '%s' is readable.\n", path);
    } else {
        printf("R_OK check (read permission)\n");
    }

    // Check for write permission
    if (access(path, W_OK) == 0) {
        printf("File '%s' is writable.\n", path);
    } else {
        printf("W_OK check (write permission)\n");
    }

    // Check for execute permission
    if (access(path, X_OK) == 0) {
        printf("File '%s' is executable.\n", path);
    } else {
        printf("X_OK check (execute permission)\n");
    }

    return 0;
}
