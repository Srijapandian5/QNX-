#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/syspage.h>
#include <sys/stat.h>

#include "sup_proto.h"

static int log_fd = -1;
static unsigned long long cycles_per_sec;

static void log_fmt(const char *fmt, int a, int b, int c)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), fmt, a, b, c);
    if (n > 0) {
        printf("%s", buf);
        if (log_fd >= 0) write(log_fd, buf, n);
    }
}

static unsigned long long now_cycles(void)
{
    return ClockCycles();
}

static int phase = 0;
static int phase_counter = 0;
static int throttle = 0;

static void update_profile(void)
{
    phase_counter++;

    if (phase == 0) {
        throttle += 2;
        if (throttle >= 55) { throttle = 55; phase = 1; phase_counter = 0; }
    } else if (phase == 1) {
        throttle += 1;
        if (throttle >= 75) { throttle = 75; phase = 2; phase_counter = 0; }
    } else if (phase == 2) {
        if (phase_counter > 50) {
            throttle -= 2;
            if (throttle <= 40) { phase = 3; phase_counter = 0; }
        }
    } else if (phase == 3) {
        if (phase_counter > 40) {
            throttle -= 3;
            if (throttle <= 0) { throttle = 0; phase = 4; phase_counter = 0; }
        }
    } else if (phase == 4) {
        if (phase_counter > 30) {
            phase = 0;
            phase_counter = 0;
        }
    }
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);

    log_fd = open("/tmp/nav.log", O_WRONLY | O_CREAT | O_APPEND, 0666);

    int coid;
    int step = 0;
    struct nav_cmd cmd;
    int reply;

    cycles_per_sec = SYSPAGE_ENTRY(qtime)->cycles_per_sec;

    coid = name_open("supervisor", 0);
    if (coid == -1) {
        log_fmt("name_open failed %d\n", errno, 0, 0);
        return 1;
    }

    log_fmt("navigation connected\n", 0, 0, 0);

    while (1) {
        update_profile();

        memset(&cmd, 0, sizeof(cmd));
        cmd.throttle = throttle;

        if (step % 40 < 20) {
            cmd.steer = 0;
        } else {
            cmd.steer = 10;
        }

        if (step % 40 == 35) {
            cmd.steer = 45;
        }

        cmd.t0 = now_cycles();

        reply = MsgSend(coid, &cmd, sizeof(cmd), NULL, 0);

        if (reply == EOK) {
            log_fmt("nav step %d steer %d throttle %d ACCEPTED\n",
                    step, cmd.steer, cmd.throttle);
        } else {
            log_fmt("nav step %d steer %d throttle %d REJECTED\n",
                    step, cmd.steer, cmd.throttle);
        }

        step++;
        usleep(100000);
    }

    return 0;
}