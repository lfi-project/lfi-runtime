#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const char *dirname = "test_dir";

    // Create a directory with rwxr-xr-x permissions
    if (mkdir(dirname, 0755) == 0) {
        printf("Directory '%s' created successfully.\n", dirname);
    } else {
        perror("mkdir");
        return 1;
    }

    // Remove the directory
    if (rmdir(dirname) == 0) {
        printf("Directory '%s' removed successfully.\n", dirname);
    } else {
        perror("rmdir");
        return 1;
    }

    return 0;
}

