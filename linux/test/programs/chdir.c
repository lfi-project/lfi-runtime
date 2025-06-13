#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int
main(int argc, char **argv)
{
    if (argc <= 2) {
        fprintf(stderr, "no file given\n");
        return 1;
    }

    char wd[FILENAME_MAX];
    printf("cwd: %s\n", getcwd(wd, sizeof(wd)));

    if (chdir(argv[1]) != 0) {
        fprintf(stderr, "could not chdir to %s\n", argv[1]);
        return 1;
    }
    printf("cwd: %s\n", getcwd(wd, sizeof(wd)));

    /* FILE *f = fopen(argv[2], "r"); */
    /* if (!f) { */
    /*     fprintf(stderr, "%s does not exist\n", argv[2]); */
    /*     return 1; */
    /* } */
    /* char buf[1024]; */
    /* int n; */
    /* while ((n = fread(buf, 1, sizeof(buf), f)) != 0) { */
    /*     fwrite(buf, 1, n, stdout); */
    /* } */

    printf("going to ..\n");
    if (chdir("..") != 0) {
        fprintf(stderr, "could not chdir to ..\n");
        return 1;
    }
    printf("cwd: %s\n", getcwd(wd, sizeof(wd)));
    return 0;
}
