# AEGIS-QNX

**Autonomous Emergency Guard & Intervention Supervisor**

An independent safety supervisor built on QNX SDP 8.0 and Raspberry Pi 5 that validates every autonomous driving command against real vehicle physics and forces the vehicle into a safe state within a bounded, measured deadline.

---

## What It Is

AEGIS-QNX sits between the path planner and the actuators. It intercepts every driving command and either allows it, limits it, or rejects it based on physics — steering limits, obstacle distance, yaw rate, and sensor health. It runs as five independent processes on the QNX microkernel, each with its own priority, its own channel, and its own failure mode. If one process dies, the others keep running.

---

## Features

- Physics-based command validation — rollover, obstacle distance, yaw mismatch, heartbeat
- ADAS speed envelope — smooth acceleration and deceleration, not hard binary reject
- Sub-10ms override latency, measured in nanoseconds with `ClockCycles()` on every fault
- Dual sensor health monitoring — IMU and ultrasonic tracked independently
- Five isolated processes — no shared-memory corruption possible
- 1920×1080 QNX Screen HMI on HDMI at 30fps
- Live CLI logger for every command, fault, and state transition
- Green, Yellow, Red LEDs driven directly from the state machine

---

## Architecture


NAVIGATION (P50)
   │ MsgSend
   ▼
SAFETY SUPERVISOR (P85)  /dev/name/local/supervisor
   │ pulse(heartbeat)  pulse(fault)   write
   ▼                   ▼              ▼
SENSOR (P70)      RECOVERY (P80)   SHARED MEMORY
MPU-6050 I2C      State machine    /dev/shmem/aegis_dash
HC-SR04 GPIO      LEDs + Buzzer          │ read
                                         ▼
                                   DASHBOARD (Screen)
                                   HDMI 1920×1080 + CLI
`



## Why QNX

QNX was the only RTOS that combines hard real-time scheduling, true process isolation, native message passing, an official Raspberry Pi 5 BSP, and a path to ISO 26262 certification — all free for students. FreeRTOS, Zephyr, and Linux RT each fail at least one of these.

---

## Hardware

| Component | Interface | Purpose |
|---|---|---|
| Raspberry Pi 5 | — | Target platform |
| MPU-6050 IMU | I2C `/dev/i2c1` addr 0x68 | Yaw rate |
| HC-SR04 Ultrasonic | GPIO 17 (Trig) / GPIO 27 (Echo) | Distance |
| Voltage divider 1kΩ + 2kΩ | on Echo line | 5V → 3.3V level shift |
| Green LED | GPIO 5 | NORMAL |
| Yellow LED | GPIO 6 | DEGRADED |
| Red LED | GPIO 13 | EMERGENCY |
| Buzzer | GPIO 22 | Audible alarm |
| HDMI monitor | — | Dashboard |

---

## Source Files

| File | Purpose |
|---|---|
| `sup_proto.h` | Shared header — structs, constants, pulse codes |
| `supervisor.c` | Safety Supervisor — command validation |
| `navigation.c` | Driving command producer |
| `sensor.c` | MPU-6050 + HC-SR04 reader |
| `recovery.c` | State machine + LED driver |
| `dashboard.c` | QNX Screen HMI |
| `logger.c` | CLI aggregator |
| `cpusum.c` | CPU usage helper |
| `ramsum.c` | RAM usage helper |

---

## Build


C:\Users\<user>\qnx800\qnxsdp-env.bat
cd C:\Users\<user>\Desktop\aegis

qcc -Vgcc_ntoaarch64le -o supervisor supervisor.c -lm
qcc -Vgcc_ntoaarch64le -o recovery   recovery.c -lm
qcc -Vgcc_ntoaarch64le -o sensor     sensor.c -lm
qcc -Vgcc_ntoaarch64le -o navigation navigation.c -lm
qcc -Vgcc_ntoaarch64le -o logger     logger.c
qcc -Vgcc_ntoaarch64le -o dashboard  dashboard.c -lscreen -lm
qcc -Vgcc_ntoaarch64le -o cpusum     cpusum.c
qcc -Vgcc_ntoaarch64le -o ramsum     ramsum.c


---

## Deploy


scp -o MACs=hmac-sha2-256 supervisor qnxuser@<pi-ip>:/tmp/supervisor
scp -o MACs=hmac-sha2-256 recovery   qnxuser@<pi-ip>:/tmp/recovery
scp -o MACs=hmac-sha2-256 sensor     qnxuser@<pi-ip>:/tmp/sensor
scp -o MACs=hmac-sha2-256 navigation qnxuser@<pi-ip>:/tmp/navigation
scp -o MACs=hmac-sha2-256 dashboard  qnxuser@<pi-ip>:/tmp/dashboard
scp -o MACs=hmac-sha2-256 logger     qnxuser@<pi-ip>:/tmp/logger
scp -o MACs=hmac-sha2-256 cpusum     qnxuser@<pi-ip>:/tmp/cpusum
scp -o MACs=hmac-sha2-256 ramsum     qnxuser@<pi-ip>:/tmp/ramsum


---

## Run


ssh -o MACs=hmac-sha2-256 qnxuser@<pi-ip>
su

cd /tmp
slay -f supervisor; slay -f recovery; slay -f navigation
slay -f sensor; slay -f dashboard
rm -f /dev/shmem/aegis_dash

./recovery   </dev/null >/dev/null 2>&1 & sleep 2
./supervisor </dev/null >/dev/null 2>&1 & sleep 2
./sensor     </dev/null >/dev/null 2>&1 & sleep 2
./navigation </dev/null >/dev/null 2>&1 & sleep 2
./dashboard  </dev/null >/dev/null 2>&1 & sleep 3

pidin | grep -E "supervisor|recovery|navigation|sensor|dashboard"

Five processes should be running. Dashboard is live on HDMI.

---

## CLI

In a separate SSH terminal:


tail -f /tmp/sup.log /tmp/rec.log /tmp/nav.log


Streams every command, fault, latency, and state transition with `[SUP]`, `[REC]`, `[NAV]` labels.

---

## Runtime Behavior

1. Normal driving — speed 40–80 km/h, green LED, NORMAL state
2. Obstacle 30–60cm — speed limited, SLOW indicator, DEGRADED state
3. Obstacle <30cm — speed ramps to 0, STOP blinks, EMERGENCY state, buzzer
4. Obstacle removed — car accelerates smoothly back to cruise
5. Sensor killed — health FAILED, heartbeat spikes, car stops

---

## Performance

| Metric | Value |
|---|---|
| Override latency (fault → GPIO) | **< 10 ms** measured, typically 1–3 ms |
| Command-to-rejection latency | 200–500 µs |
| Scheduling jitter under load | sub-millisecond |
| CPU usage (all five processes) | 5–15% on 4 cores |

Every fault logs its own latency in nanoseconds. Nothing is claimed — everything is measured.

---

## Testing Faults Live


slay -f sensor
`

Within 200ms the heartbeat goes red, both sensor health cards show FAILED, recovery enters EMERGENCY_STOP, the Red LED lights, and the car stops. Restart with:


cd /tmp
./sensor </dev/null >/dev/null 2>&1 &


Health returns to HEALTHY within a second and the car resumes.


## QNX Concepts Used

- Microkernel architecture with five isolated processes
- Message passing (`MsgSend` / `MsgReceive` / `MsgReply`)
- QNX pulses (`MsgSendPulse`) for heartbeats and faults
- Named channels (`name_attach` / `name_open`)
- `SCHED_FIFO` priorities 20–85 for preemptive safety enforcement
- Kernel timers (`TimerTimeout` + `SIGEV_PULSE`)
- `ClockCycles()` for nanosecond latency measurement
- Shared memory (`shm_open` + `mmap`) for dashboard updates
- `devctl(DCMD_I2C_SEND/RECV)` for MPU-6050
- `devctl(DCMD_PROC_TIDSTATUS)` for per-core CPU telemetry
- QNX Screen graphics subsystem for the HMI
- Resource manager design pattern

## License

Educational and non-commercial use. QNX SDP is a trademark of BlackBerry QNX.
```
