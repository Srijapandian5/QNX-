#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/mman.h>
#include <sys/syspage.h>
#include <sys/iofunc.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "sup_proto.h"

static int log_fd = -1;

static void log_line(const char *s)
{
    printf("%s", s);
    if (log_fd >= 0) write(log_fd, s, strlen(s));
}

static struct sensor_data last_sensor;
static struct dash_state dash;
static unsigned long long last_heartbeat_cycles;
static unsigned long long last_cmd_cycles;
static unsigned long long start_cycles;
static unsigned long long cycles_per_sec;
static int fault_count = 0;
static int current_state = STATE_NORMAL;
static int main_chid = -1;
static int recovery_coid = -1;

static int current_speed_kmh = 60;
static int target_speed_kmh = 60;

static int last_steer = 0;
static int last_throttle = 0;
static unsigned long last_latency_ns = 0;

static int imu_fail_count = 0;
static int us_fail_count = 0;

static unsigned long long now_cycles(void)
{
    return ClockCycles();
}

static unsigned long long cycles_to_ns(unsigned long long c)
{
    return (c * 1000000000ULL) / cycles_per_sec;
}

static unsigned long long cycles_to_ms(unsigned long long c)
{
    return (c * 1000ULL) / cycles_per_sec;
}

static void open_shared_memory(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0666);
    if (fd == -1) return;
    ftruncate(fd, sizeof(struct dash_state));
    void *p = mmap(NULL, sizeof(struct dash_state),
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p != MAP_FAILED) {
        memset(p, 0, sizeof(struct dash_state));
        munmap(p, sizeof(struct dash_state));
    }
    close(fd);
}

static void write_shared_state(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) return;
    void *p = mmap(NULL, sizeof(struct dash_state),
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p != MAP_FAILED) {
        memcpy(p, &dash, sizeof(struct dash_state));
        munmap(p, sizeof(struct dash_state));
    }
    close(fd);
}

static void send_fault_pulse(int reason)
{
    if (recovery_coid < 0) return;
    MsgSendPulse(recovery_coid, -1, PULSE_FAULT, reason);
}

static int compute_speed_limit(void)
{
    int d = last_sensor.distance_cm;
    int yaw = last_sensor.yaw_dps;
    int limit;

    if (d <= 15) limit = 0;
    else if (d < 30) limit = 15;
    else if (d < 60) limit = 40;
    else if (d < 100) limit = 60;
    else limit = 80;

    if (yaw > 45 || yaw < -45) limit = limit / 2;
    if (yaw > 90 || yaw < -90) limit = 10;

    return limit;
}

static void update_speed_envelope(void)
{
    target_speed_kmh = compute_speed_limit();

    if (current_speed_kmh < target_speed_kmh) {
        current_speed_kmh += 2;
        if (current_speed_kmh > target_speed_kmh)
            current_speed_kmh = target_speed_kmh;
    } else if (current_speed_kmh > target_speed_kmh) {
        current_speed_kmh -= 3;
        if (current_speed_kmh < target_speed_kmh)
            current_speed_kmh = target_speed_kmh;
    }
}

static int compute_health(int fail_count)
{
    if (fail_count < 5) return HEALTH_OK;
    if (fail_count < 20) return HEALTH_LATE;
    return HEALTH_DEAD;
}

static void update_dash(void)
{
    dash.state = current_state;
    dash.steer = last_steer;
    dash.throttle = last_throttle;
    dash.yaw_dps = last_sensor.yaw_dps;
    dash.distance_cm = last_sensor.distance_cm;
    dash.heartbeat_age_ms = (int)cycles_to_ms(now_cycles() - last_heartbeat_cycles);
    dash.fault_count = fault_count;
    dash.last_latency_ns = last_latency_ns;
    dash.speed_kmh = current_speed_kmh;
    dash.uptime_sec = (unsigned long)(cycles_to_ms(now_cycles() - start_cycles) / 1000);
    dash.imu_health = compute_health(imu_fail_count);
    dash.us_health = compute_health(us_fail_count);
    write_shared_state();
}

static void handle_heartbeat(struct _pulse *p)
{
    int packed = p->value.sival_int;
    int gz = (packed >> 16) & 0xFFFF;
    int d = packed & 0xFFFF;

    if (gz & 0x8000) gz |= 0xFFFF0000;
    if (d & 0x8000)  d  |= 0xFFFF0000;

    if (gz == 0 && d == 0) {
        imu_fail_count++;
    } else {
        imu_fail_count = 0;
    }

    if (d == 15) {
        us_fail_count = 0;
    } else if (d == 200) {
        us_fail_count = 0;
    }

    last_sensor.yaw_dps = gz;
    last_sensor.distance_cm = d;
    last_sensor.t_read = now_cycles();
    last_heartbeat_cycles = now_cycles();

    update_speed_envelope();

    if (d <= 15) {
        if (current_state != STATE_EMERGENCY) {
            fault_count++;
            current_state = STATE_EMERGENCY;
            log_line("OBSTACLE CLOSE - EMERGENCY\n");
            send_fault_pulse(1);
        }
    } else if (d < 60) {
        if (current_state == STATE_NORMAL) {
            current_state = STATE_DEGRADED;
            log_line("OBSTACLE NEAR - DEGRADED\n");
        }
    } else {
        if (current_state == STATE_EMERGENCY) {
            current_state = STATE_DEGRADED;
        }
    }

    if (current_state == STATE_EMERGENCY && d > 100) current_state = STATE_DEGRADED;
    if (current_state == STATE_DEGRADED && d > 150) current_state = STATE_NORMAL;
}

static void handle_recovery_done(void)
{
    update_dash();
}

static void handle_cmd_message(int rcvid)
{
    struct nav_cmd cmd;
    unsigned long long t1, t0, latency_ns;

    MsgRead(rcvid, &cmd, sizeof(cmd), 0);

    t1 = now_cycles();
    t0 = cmd.t0;
    latency_ns = cycles_to_ns(t1 - t0);
    if (t0 == 0) latency_ns = 0;

    last_cmd_cycles = t1;
    last_steer = cmd.steer;
    last_throttle = cmd.throttle;
    last_latency_ns = latency_ns;

    update_speed_envelope();
    update_dash();
    MsgReply(rcvid, EOK, NULL, 0);
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    log_fd = open("/tmp/sup.log", O_WRONLY | O_CREAT | O_APPEND, 0666);

    name_attach_t *attach;
    struct _pulse pulse;
    int rcvid;

    cycles_per_sec = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    start_cycles = now_cycles();
    last_heartbeat_cycles = start_cycles;
    last_cmd_cycles = start_cycles;
    memset(&last_sensor, 0, sizeof(last_sensor));
    memset(&dash, 0, sizeof(dash));

    open_shared_memory();

    recovery_coid = name_open("recovery", 0);
    if (recovery_coid == -1) {
        log_line("warning: recovery not found\n");
    }

    attach = name_attach(NULL, "supervisor", 0);
    if (attach == NULL) {
        unlink("/dev/name/local/supervisor");
        usleep(100000);
        attach = name_attach(NULL, "supervisor", 0);
        if (attach == NULL) {
            log_line("name_attach failed\n");
            return 1;
        }
    }

    main_chid = attach->chid;
    printf("supervisor running chid %d\n", main_chid);

    unsigned long long last_envelope = now_cycles();
    unsigned long long envelope_interval = cycles_per_sec / 10;

    while (1) {
        rcvid = MsgReceive(main_chid, &pulse, sizeof(pulse), NULL);
        if (rcvid == 0) {
            if (pulse.code == PULSE_HEARTBEAT) handle_heartbeat(&pulse);
            else if (pulse.code == PULSE_RECOVERY_DONE) handle_recovery_done();
        } else if (rcvid > 0) {
            handle_cmd_message(rcvid);
        }

        if (now_cycles() - last_envelope > envelope_interval) {
            last_envelope = now_cycles();
            update_speed_envelope();
            update_dash();
        }
    }

    return 0;
}