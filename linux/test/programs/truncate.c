#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv) {
    (void) argc;
    const char *filename = argv[1];
    const char *original_content = "This is the original content of the file.\n";
    size_t original_len = strlen(original_content);

    // Step 1: Write original content to the file
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    write(fd, original_content, original_len);
    close(fd);
    printf("Original content written to '%s'.\n", filename);

    // Step 2: Truncate the file to a shorter length
    if (truncate(filename, 10) == 0) {
        printf("File '%s' truncated to 10 bytes.\n", filename);
    } else {
        perror("truncate");
        return 1;
    }

    // Step 3: Restore the original content
    fd = open(filename, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        perror("open (restore)");
        return 1;
    }
    write(fd, original_content, original_len);
    close(fd);
    printf("Original content restored in '%s'.\n", filename);

    return 0;
}

