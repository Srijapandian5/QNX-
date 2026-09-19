#ifndef SUP_PROTO_H
#define SUP_PROTO_H

#include <sys/types.h>

#define MAX_STEER_DEG         30
#define MAX_LATERAL_G         0.4
#define MIN_DISTANCE_CM       30
#define MAX_YAW_RATE_DPS      45
#define YAW_TOLERANCE_DPS     15
#define HEARTBEAT_TIMEOUT_MS  40
#define NAV_TIMEOUT_MS        200
#define MAX_SPEED_MPS         8.0
#define WHEELBASE_M           2.5
#define MAX_THROTTLE_PCT      100

#define PULSE_HEARTBEAT       (_PULSE_CODE_MINAVAIL + 1)
#define PULSE_FAULT           (_PULSE_CODE_MINAVAIL + 2)
#define PULSE_RECOVERY_DONE   (_PULSE_CODE_MINAVAIL + 3)

#define STATE_NORMAL          0
#define STATE_DEGRADED        1
#define STATE_EMERGENCY       2

#define HEALTH_OK             0
#define HEALTH_LATE           1
#define HEALTH_DEAD           2

#define SHM_NAME              "/aegis_dash"

struct nav_cmd {
    int steer;
    int throttle;
    unsigned long t0;
};

struct sensor_data {
    int yaw_dps;
    int distance_cm;
    int imu_ok;
    unsigned long t_read;
};

struct dash_state {
    int state;
    int car_x;
    int car_y;
    int heading_deg;
    int speed_kmh;
    int steer;
    int throttle;
    int yaw_dps;
    int distance_cm;
    int heartbeat_age_ms;
    int fault_count;
    int imu_health;
    int us_health;
    unsigned long last_latency_ns;
    unsigned long uptime_sec;
};

#endif