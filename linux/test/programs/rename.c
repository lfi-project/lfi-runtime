#include <stdio.h>
#include <errno.h>

int main(int argc, char **argv) {
    (void) argc;
    const char *original_name = argv[1];
    const char *temp_name = "temp_file.txt";

    // Rename original_file.txt to temp_file.txt
    if (rename(original_name, temp_name) == 0) {
        printf("Renamed '%s' to '%s'.\n", original_name, temp_name);
    } else {
        perror("rename to temp_name");
        return 1;
    }

    // Rename temp_file.txt back to original_file.txt
    if (rename(temp_name, original_name) == 0) {
        printf("Renamed '%s' back to '%s'.\n", temp_name, original_name);
    } else {
        perror("rename back to original_name");
        return 1;
    }

    return 0;
}
