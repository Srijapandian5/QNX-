#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define N_FILES 3

static const char *files[N_FILES] = {
    "/tmp/sup.log",
    "/tmp/rec.log",
    "/tmp/nav.log"
};

static const char *labels[N_FILES] = {
    "SUP",
    "REC",
    "NAV"
};

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);

    int fds[N_FILES];
    off_t positions[N_FILES];
    int i;
    char buf[1024];

    for (i = 0; i < N_FILES; i++) {
        fds[i] = open(files[i], O_RDONLY | O_CREAT, 0666);
        if (fds[i] == -1) {
            printf("cannot open %s\n", files[i]);
            return 1;
        }
        positions[i] = lseek(fds[i], 0, SEEK_END);
    }

    printf("=== AEGIS CLI ===\n");

    while (1) {
        int activity = 0;

        for (i = 0; i < N_FILES; i++) {
            off_t cur = lseek(fds[i], 0, SEEK_END);

            if (cur > positions[i]) {
                lseek(fds[i], positions[i], SEEK_SET);

                int n = read(fds[i], buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = 0;

                    char *line = strtok(buf, "\n");
                    while (line) {
                        printf("[%s] %s\n", labels[i], line);
                        line = strtok(NULL, "\n");
                    }

                    activity = 1;
                }

                positions[i] = cur;
            }
        }

        if (!activity) usleep(50000);
    }

    return 0;
}