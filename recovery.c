#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/stat.h>

#include "sup_proto.h"

static int log_fd = -1;
static int current_state = STATE_NORMAL;

static void log_line(const char *s)
{
    printf("%s", s);
    if (log_fd >= 0) write(log_fd, s, strlen(s));
}

static void log_fmt(const char *fmt, int a)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), fmt, a);
    if (n > 0) {
        printf("%s", buf);
        if (log_fd >= 0) write(log_fd, buf, n);
    }
}

static void set_state(int s)
{
    current_state = s;
    if (s == STATE_NORMAL) {
        log_line("RECOVERY state=NORMAL\n");
    } else if (s == STATE_DEGRADED) {
        log_line("RECOVERY state=DEGRADED\n");
    } else {
        log_line("RECOVERY state=EMERGENCY\n");
    }
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);

    log_fd = open("/tmp/rec.log", O_WRONLY | O_CREAT | O_APPEND, 0666);

    name_attach_t *attach;
    struct _pulse pulse;
    int rcvid;
    int step = 0;
    uint64_t timeout;

    attach = name_attach(NULL, "recovery", 0);
    if (attach == NULL) {
        log_line("name_attach failed\n");
        return 1;
    }

    log_fmt("recovery running chid %d\n", attach->chid);

    while (1) {
        timeout = 100000000ULL;
        TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &timeout, NULL);

        rcvid = MsgReceive(attach->chid, &pulse, sizeof(pulse), NULL);

        if (rcvid == 0) {
            if (pulse.code == PULSE_FAULT) {
                log_fmt("RECOVERY got FAULT reason %d\n", pulse.value.sival_int);
                if (current_state != STATE_EMERGENCY) {
                    set_state(STATE_EMERGENCY);
                    step = 0;
                }
            } else if (pulse.code == _PULSE_CODE_DISCONNECT) {
                ConnectDetach(pulse.scoid);
            }
        } else if (rcvid == -1 && errno == ETIMEDOUT) {
            step++;
            if (current_state == STATE_EMERGENCY) {
                if (step >= 30) {
                    set_state(STATE_DEGRADED);
                    step = 0;
                }
            } else if (current_state == STATE_DEGRADED) {
                if (step >= 20) {
                    set_state(STATE_NORMAL);
                    step = 0;
                }
            }
        }
    }

    return 0;
}