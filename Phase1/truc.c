#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    int fd;
    int i;
    char str[1024];
    char exec_path[2048];
    //char *wd_ptr = NULL;

    char *MASTER_FD_ptr = getenv("MASTER_FD");
    int MASTER_FD = strtol(MASTER_FD_ptr, NULL, 10);
    printf("MASTER_FD = %i\n", MASTER_FD);

    char *DSMEXEC_FD_ptr = getenv("DSMEXEC_FD");
    int DSMEXEC_FD = strtol(DSMEXEC_FD_ptr, NULL, 10);
    printf("DSMEXEC_FD = %i\n", DSMEXEC_FD);


    getcwd(str, 1024);
    fprintf(stdout, "Working dir is %s\n", str);

    fprintf(stdout, "Number of args : %i\n", argc);
    for (i = 0; i < argc; i++) {
        fprintf(stderr, "arg[%i] : %s\n", i, argv[i]);
    }

    sprintf(exec_path, "%s/%s", str, "file.txt");
    fd = open(exec_path, O_RDONLY);
    if (fd == -1) {
        perror("open");
    }
    fprintf(stdout, "================ Valeur du descripteur : %i\n", fd);

    fflush(stdout);
    fflush(stderr);
    return 0;
}
