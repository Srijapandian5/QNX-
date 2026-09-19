#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <sys/procfs.h>
#include <devctl.h>

#define MAX_CPUS 8

static uint64_t nanoseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int main(void)
{
    int i, ncpu;
    int fd;
    uint64_t sut1[MAX_CPUS], sut2[MAX_CPUS];
    uint64_t t1, t2;

    ncpu = _syspage_ptr->num_cpu;
    fd = open("/proc/1/as", O_RDONLY);
    if (fd == -1) { printf("0.00\n"); return 1; }

    t1 = nanoseconds();
    for (i = 0; i < ncpu; i++) {
        debug_thread_t d;
        memset(&d, 0, sizeof(d));
        d.tid = i + 1;
        devctl(fd, DCMD_PROC_TIDSTATUS, &d, sizeof(d), NULL);
        sut1[i] = d.sutime;
    }

    usleep(500000);

    t2 = nanoseconds();
    for (i = 0; i < ncpu; i++) {
        debug_thread_t d;
        memset(&d, 0, sizeof(d));
        d.tid = i + 1;
        devctl(fd, DCMD_PROC_TIDSTATUS, &d, sizeof(d), NULL);
        sut2[i] = d.sutime;
    }

    close(fd);

    uint64_t idle_delta_total = 0;
    for (i = 0; i < ncpu; i++)
        idle_delta_total += (sut2[i] - sut1[i]);

    uint64_t time_delta = t2 - t1;
    uint64_t total_capacity = time_delta * ncpu;

    float idle_pct = (float)(idle_delta_total * 100) / (float)total_capacity;
    float busy_pct = 100.0f - idle_pct;
    if (busy_pct < 0) busy_pct = 0;
    if (busy_pct > 100) busy_pct = 100;

    printf("%.1f\n", busy_pct);
    return 0;
}