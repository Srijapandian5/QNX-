#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <sys/procfs.h>
#include <devctl.h>
#include <screen/screen.h>

#include "sup_proto.h"

#define W 1920
#define H 1080
#define MAX_CPUS 8

#define LED_GREEN_PIN  5
#define LED_YELLOW_PIN 6
#define LED_RED_PIN    13

#define COL_BG        0xFFE5E7EB
#define COL_CARD      0xFFF8FAFC
#define COL_HEADER1   0xFF4B5563
#define COL_HEADER2   0xFF374151
#define COL_ACCENT    0xFF0D9488
#define COL_TEXT      0xFF111827
#define COL_LABEL     0xFF6B7280
#define COL_BORDER    0xFFD1D5DB
#define COL_DIVIDER   0xFFE5E7EB
#define COL_GREEN     0xFF10B981
#define COL_YELLOW    0xFFF59E0B
#define COL_RED       0xFFEF4444
#define COL_BLUE      0xFF3B82F6
#define COL_BAR_BG    0xFFE5E7EB
#define COL_ROAD      0xFF374151
#define COL_ROAD_EDGE 0xFF1F2937
#define COL_STRIPE    0xFFF3F4F6

struct dash_state local_dash;

unsigned char *fb = NULL;
int fb_stride = 0;
int fb_w = 0;
int fb_h = 0;

static int anim_frame = 0;
static int stripe_offset = 0;
static int last_led_state = -1;

static const unsigned char font5x7[96][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x04,0x04,0x04,0x04,0x00,0x04,0x00},
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
    {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
    {0x04,0x04,0x00,0x00,0x00,0x00,0x00},
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00},
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x04,0x08},
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x04,0x00},
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x00,0x04,0x00,0x00,0x00,0x04,0x00},
    {0x00,0x04,0x00,0x00,0x00,0x04,0x08},
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02},
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08},
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x11,0x15,0x1B,0x11},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
    {0x10,0x08,0x08,0x04,0x02,0x02,0x01},
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
    {0x08,0x04,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
    {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E},
    {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F},
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
    {0x06,0x09,0x08,0x1C,0x08,0x08,0x08},
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E},
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
    {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12},
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x00,0x00,0x1A,0x15,0x15,0x11,0x11},
    {0x00,0x00,0x1E,0x11,0x11,0x11,0x11},
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10},
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01},
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
    {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},
    {0x08,0x08,0x1C,0x08,0x08,0x09,0x06},
    {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},
    {0x02,0x04,0x04,0x08,0x04,0x04,0x02},
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x08,0x04,0x04,0x02,0x04,0x04,0x08},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

void put_pixel(int x, int y, unsigned int color)
{
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    unsigned int *row = (unsigned int *)(fb + y * fb_stride);
    row[x] = color;
}

void blend_pixel(int x, int y, unsigned int color, int alpha)
{
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    if (alpha <= 0) return;
    if (alpha >= 255) { put_pixel(x, y, color); return; }
    unsigned int *row = (unsigned int *)(fb + y * fb_stride);
    unsigned int dst = row[x];
    int r = (((color >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * (255 - alpha)) / 255;
    int g = (((color >>  8) & 0xFF) * alpha + ((dst >>  8) & 0xFF) * (255 - alpha)) / 255;
    int b = (((color      ) & 0xFF) * alpha + ((dst      ) & 0xFF) * (255 - alpha)) / 255;
    row[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
}

void fill_rect(int x, int y, int w, int h, unsigned int color)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            put_pixel(x + i, y + j, color);
}

void fill_rect_gradient(int x, int y, int w, int h, unsigned int c1, unsigned int c2)
{
    int j;
    for (j = 0; j < h; j++) {
        int r = ((c1 >> 16) & 0xFF) * (h - j) / h + ((c2 >> 16) & 0xFF) * j / h;
        int g = ((c1 >>  8) & 0xFF) * (h - j) / h + ((c2 >>  8) & 0xFF) * j / h;
        int b = ((c1      ) & 0xFF) * (h - j) / h + ((c2      ) & 0xFF) * j / h;
        unsigned int col = 0xFF000000 | (r << 16) | (g << 8) | b;
        fill_rect(x, y + j, w, 1, col);
    }
}

void fill_rounded_rect(int x, int y, int w, int h, int r, unsigned int color)
{
    int i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            int skip = 0;
            int dx, dy;
            if (i < r && j < r) { dx = r - i; dy = r - j; if (dx*dx+dy*dy > r*r) skip = 1; }
            else if (i >= w - r && j < r) { dx = i - (w - r - 1); dy = r - j; if (dx*dx+dy*dy > r*r) skip = 1; }
            else if (i < r && j >= h - r) { dx = r - i; dy = j - (h - r - 1); if (dx*dx+dy*dy > r*r) skip = 1; }
            else if (i >= w - r && j >= h - r) { dx = i - (w - r - 1); dy = j - (h - r - 1); if (dx*dx+dy*dy > r*r) skip = 1; }
            if (!skip) put_pixel(x + i, y + j, color);
        }
    }
}

void fill_circle(int cx, int cy, int r, unsigned int color)
{
    int y, x;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) put_pixel(cx + x, cy + y, color);
}

void fill_circle_alpha(int cx, int cy, int r, unsigned int color, int alpha)
{
    int y, x;
    for (y = -r; y <= r; y++) {
        for (x = -r; x <= r; x++) {
            int d2 = x*x + y*y;
            if (d2 <= r*r) {
                int a2 = alpha;
                if (d2 > (r-4)*(r-4)) a2 = alpha / 3;
                else if (d2 > (r-8)*(r-8)) a2 = alpha * 2 / 3;
                blend_pixel(cx + x, cy + y, color, a2);
            }
        }
    }
}

void draw_char_scaled(int x, int y, char c, unsigned int color, int scale)
{
    if (c < 32 || c > 127) return;
    const unsigned char *g = font5x7[c - 32];
    int row, col, sx, sy;
    for (row = 0; row < 7; row++) {
        for (col = 0; col < 5; col++) {
            if (g[row] & (1 << (4 - col))) {
                for (sy = 0; sy < scale; sy++)
                    for (sx = 0; sx < scale; sx++)
                        put_pixel(x + col*scale + sx, y + row*scale + sy, color);
            }
        }
    }
}

void draw_text_scaled(int x, int y, const char *s, unsigned int color, int scale)
{
    while (*s) {
        draw_char_scaled(x, y, *s, color, scale);
        x += 6 * scale;
        s++;
    }
}

void draw_text(int x, int y, const char *s, unsigned int color)
{
    draw_text_scaled(x, y, s, color, 2);
}

void draw_text_big(int x, int y, const char *s, unsigned int color)
{
    draw_text_scaled(x, y, s, color, 3);
}

int text_width(const char *s, int scale)
{
    return strlen(s) * 6 * scale;
}

int read_shared_state(void)
{
    int fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (fd == -1) return -1;
    void *p = mmap(NULL, sizeof(struct dash_state),
                   PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { close(fd); return -1; }
    memcpy(&local_dash, p, sizeof(local_dash));
    munmap(p, sizeof(local_dash));
    close(fd);
    return 0;
}

static int cached_cpu = 0;
static int cached_ram_pct = 0;
static int cached_ram_used_mb = 0;
static int cached_ram_total_mb = 0;
static int sys_counter = 0;

void update_cpu(void)
{
    sys_counter++;
    if (sys_counter % 15 != 1 && cached_cpu > 0) return;

    FILE *fp = popen("/tmp/cpusum", "r");
    if (!fp) return;
    char line[32];
    if (fgets(line, sizeof(line), fp)) {
        float v = atof(line);
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        cached_cpu = (int)v;
    }
    pclose(fp);
}

void update_ram(void)
{
    if (sys_counter % 30 != 1 && cached_ram_total_mb > 0) return;

    FILE *fp = popen("/tmp/ramsum", "r");
    if (!fp) return;
    char line[64];
    if (fgets(line, sizeof(line), fp)) {
        unsigned long long used = 0, total = 0;
        if (sscanf(line, "%llu %llu", &used, &total) == 2) {
            if (total > 0) {
                cached_ram_used_mb = (int)used;
                cached_ram_total_mb = (int)total;
                cached_ram_pct = (int)(100ULL * used / total);
            }
        }
    }
    pclose(fp);
}

void drive_leds(int state)
{
    if (state == last_led_state) return;
    last_led_state = state;

    char cmd[128];

    snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d op", LED_GREEN_PIN);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d op", LED_YELLOW_PIN);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d op", LED_RED_PIN);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d dl", LED_GREEN_PIN);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d dl", LED_YELLOW_PIN);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d dl", LED_RED_PIN);
    system(cmd);

    if (state == STATE_NORMAL) {
        snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d dh", LED_GREEN_PIN);
        system(cmd);
    } else if (state == STATE_DEGRADED) {
        snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d dh", LED_YELLOW_PIN);
        system(cmd);
    } else {
        snprintf(cmd, sizeof(cmd), "gpio-rp1 set %d dh", LED_RED_PIN);
        system(cmd);
    }
}

void draw_card(int x, int y, int w, int h, const char *title)
{
    fill_rect(x, y, w, h, COL_CARD);
    fill_rect_gradient(x, y, w, 56, COL_HEADER1, COL_HEADER2);
    fill_rect(x, y + 56, w, 1, COL_BORDER);
    draw_text(x + 20, y + 20, title, COL_CARD);
}

void draw_background(void)
{
    fill_rect(0, 0, W, H, COL_BG);
}

void draw_top_bar(void)
{
    fill_rect(0, 0, W, 96, COL_CARD);
    fill_rect(0, 96, W, 3, COL_ACCENT);

    draw_text_big(30, 30, "AEGIS-QNX", COL_HEADER1);

    char buf[64];
    unsigned long up = local_dash.uptime_sec;
    int h = up / 3600;
    int m = (up % 3600) / 60;
    int s = up % 60;
    snprintf(buf, sizeof(buf), "UPTIME %02d:%02d:%02d", h, m, s);
    draw_text(W - 640, 50, buf, COL_TEXT);

    const char *state_str = "NORMAL";
    unsigned int state_col = COL_GREEN;
    if (local_dash.state == STATE_DEGRADED) { state_str = "DEGRADED"; state_col = COL_YELLOW; }
    else if (local_dash.state == STATE_EMERGENCY) { state_str = "EMERGENCY"; state_col = COL_RED; }

    int bw = text_width(state_str, 3) + 60;
    int bx = W - 30 - bw;
    fill_rounded_rect(bx, 22, bw, 50, 25, state_col);
    draw_text_big(bx + 30, 32, state_str, COL_CARD);
}

void draw_speed_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "SPEED");

    int cx = x + w/2;
    int cy = y + h/2 + 40;
    int r = 120;

    int a;
    for (a = 0; a < 270; a++) {
        double rad = (a + 135) * 3.14159 / 180.0;
        int x1 = cx + (int)(r * cos(rad));
        int y1 = cy + (int)(r * sin(rad));
        int x2 = cx + (int)((r - 4) * cos(rad));
        int y2 = cy + (int)((r - 4) * sin(rad));
        put_pixel(x1, y1, COL_DIVIDER);
        put_pixel(x2, y2, COL_DIVIDER);
    }

    int speed = local_dash.speed_kmh;
    if (speed < 0) speed = 0;
    if (speed > 100) speed = 100;
    int arc_end = (speed * 270) / 100;

    for (a = 0; a < arc_end; a++) {
        double rad = (a + 135) * 3.14159 / 180.0;
        unsigned int fill_col = COL_GREEN;
        if (a > 135) fill_col = COL_YELLOW;
        if (a > 200) fill_col = COL_RED;
        int t;
        for (t = 4; t <= 18; t++) {
            int xx = cx + (int)((r - t) * cos(rad));
            int yy = cy + (int)((r - t) * sin(rad));
            put_pixel(xx, yy, fill_col);
        }
    }

    double needle_rad = (arc_end + 135) * 3.14159 / 180.0;
    int nx = cx + (int)((r - 50) * cos(needle_rad));
    int ny = cy + (int)((r - 50) * sin(needle_rad));
    int steps = 40;
    int k;
    for (k = 0; k <= steps; k++) {
        int mx = cx + (nx - cx) * k / steps;
        int my = cy + (ny - cy) * k / steps;
        fill_circle(mx, my, 2, COL_TEXT);
    }
    fill_circle(cx, cy, 14, COL_TEXT);
    fill_circle(cx, cy, 9, COL_CARD);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", speed);
    int tw = text_width(buf, 5);
    draw_text_scaled(cx - tw/2, cy + 20, buf, COL_TEXT, 5);
    draw_text_scaled(cx - 42, cy + 90, "km/h", COL_LABEL, 2);
}

void draw_commands_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "COMMANDS");

    char buf[64];
    int row = y + 90;

    draw_text(x + 30, row, "STEER", COL_LABEL);
    snprintf(buf, sizeof(buf), "%+d deg", local_dash.steer);
    draw_text_big(x + w - 260, row - 10, buf, COL_TEXT);

    row += 55;
    draw_text(x + 30, row, "THROTTLE", COL_LABEL);
    snprintf(buf, sizeof(buf), "%d %%", local_dash.throttle);
    draw_text_big(x + w - 200, row - 10, buf, COL_TEXT);

    row += 55;
    draw_text(x + 30, row, "LATENCY", COL_LABEL);
    snprintf(buf, sizeof(buf), "%lu us", local_dash.last_latency_ns / 1000);
    draw_text_big(x + w - 260, row - 10, buf, COL_TEXT);

    row += 55;
    draw_text(x + 30, row, "FAULTS", COL_LABEL);
    snprintf(buf, sizeof(buf), "%d", local_dash.fault_count);
    draw_text_big(x + w - 100, row - 10, buf, COL_TEXT);
}

void draw_status_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "PROCESS STATUS");

    struct { const char *name; int ok; } rows[4] = {
        {"Supervisor",  1},
        {"Recovery",    1},
        {"Sensor",      1},
        {"Navigation",  1}
    };

    int i;
    for (i = 0; i < 4; i++) {
        int ry = y + 90 + i * 58;
        unsigned int dot_col = rows[i].ok ? COL_GREEN : COL_RED;
        if (rows[i].ok) {
            fill_circle_alpha(x + 40, ry + 12, 22, dot_col, 100);
            fill_circle(x + 40, ry + 12, 11, dot_col);
        } else {
            fill_circle(x + 40, ry + 12, 11, dot_col);
        }
        draw_text(x + 80, ry + 2, rows[i].name, COL_TEXT);
    }
}

void draw_road_vehicle_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "VEHICLE  /  ROAD VIEW");

    int road_w = 280;
    int road_x = x + w/2 - road_w/2;
    int road_y = y + 70;
    int road_h = h - 80;

    fill_rect(road_x, road_y, road_w, road_h, COL_ROAD);
    fill_rect(road_x, road_y, 4, road_h, COL_ROAD_EDGE);
    fill_rect(road_x + road_w - 4, road_y, 4, road_h, COL_ROAD_EDGE);

    int speed = local_dash.speed_kmh;
    if (speed > 5) {
        stripe_offset += (speed / 8) + 1;
    }

    int stripe_x = road_x + road_w/2 - 4;
    int stripe_h = 50;
    int stripe_gap = 50;
    int stripe_total = stripe_h + stripe_gap;
    int p;
    for (p = -stripe_total + (stripe_offset % stripe_total); p < road_h; p += stripe_total) {
        int sy = road_y + p;
        int ey = sy + stripe_h;
        if (sy < road_y) sy = road_y;
        if (ey > road_y + road_h) ey = road_y + road_h;
        if (ey > sy) fill_rect(stripe_x, sy, 8, ey - sy, COL_STRIPE);
    }

    int cx = x + w/2;
    int cy = y + h/2 + 60;

    int body_w = 100;
    int body_h = 200;
    int bx = cx - body_w/2;
    int by = cy - body_h/2;

    fill_rect(bx - 6, by - 4, body_w + 12, body_h + 8, 0x80000000);

    unsigned int body_top = 0xFF4A90D0;
    unsigned int body_bot = 0xFF1E4F7E;

    if (local_dash.distance_cm <= 15) {
        body_top = 0xFFD04A4A;
        body_bot = 0xFF7E1E1E;
    } else if (local_dash.distance_cm < 60) {
        body_top = 0xFFD0B04A;
        body_bot = 0xFF7E6A1E;
    }

    fill_rect_gradient(bx, by, body_w, body_h, body_top, body_bot);
    fill_rect(bx, by, body_w, 4, 0xFF80B8E8);
    fill_rect(bx, by + body_h - 4, body_w, 4, 0xFF0E2F4E);

    int cabin_y = by + 40;
    fill_rect_gradient(bx + 14, cabin_y, body_w - 28, 60, 0xFFB0D8F0, 0xFF80B8E0);
    fill_rect(bx + 14, cabin_y + 30, body_w - 28, 2, 0xFF5080B0);
    fill_rect(bx + 14, cabin_y + 58, body_w - 28, 2, 0xFF5080B0);

    int rear_y = by + 130;
    fill_rect_gradient(bx + 14, rear_y, body_w - 28, 40, 0xFF90C0E0, 0xFF6098C0);
    fill_rect(bx + 14, rear_y + 38, body_w - 28, 2, 0xFF5080B0);

    int hl1 = bx + 12;
    int hl2 = bx + body_w - 36;
    fill_circle(hl1 + 12, by + 8, 10, 0xFFFFF0A0);
    fill_circle(hl1 + 12, by + 8, 6, 0xFFFFFFFF);
    fill_circle(hl2 + 12, by + 8, 10, 0xFFFFF0A0);
    fill_circle(hl2 + 12, by + 8, 6, 0xFFFFFFFF);

    int braking = (local_dash.distance_cm <= 30);
    unsigned int tl_col = braking ? 0xFFFF0000 : 0xFFFF4040;
    int tl_r = braking ? 10 : 8;

    int tl1 = bx + 12;
    int tl2 = bx + body_w - 36;
    fill_circle(tl1 + 12, by + body_h - 8, tl_r, tl_col);
    fill_circle(tl1 + 12, by + body_h - 8, tl_r - 4, 0xFFFFA0A0);
    fill_circle(tl2 + 12, by + body_h - 8, tl_r, tl_col);
    fill_circle(tl2 + 12, by + body_h - 8, tl_r - 4, 0xFFFFA0A0);

    int steer = local_dash.steer;
    int wheel_shift = (steer * 15) / 30;
    if (wheel_shift > 20) wheel_shift = 20;
    if (wheel_shift < -20) wheel_shift = -20;

    fill_rect(bx - 14 + wheel_shift, by + 40, 14, 40, 0xFF181818);
    fill_rect(bx + body_w + wheel_shift, by + 40, 14, 40, 0xFF181818);
    fill_rect(bx - 14, by + body_h - 90, 14, 40, 0xFF181818);
    fill_rect(bx + body_w, by + body_h - 90, 14, 40, 0xFF181818);

    if (local_dash.distance_cm <= 15) {
        int obs_y = by - 60;
        int blink = (anim_frame / 4) % 2;
        if (blink) {
            fill_rounded_rect(cx - 70, obs_y, 140, 40, 8, COL_RED);
            draw_text(cx - 55, obs_y + 12, "STOP", COL_CARD);
        }
    } else if (local_dash.distance_cm < 60) {
        int obs_y = by - 60;
        fill_rounded_rect(cx - 70, obs_y, 140, 40, 8, COL_YELLOW);
        draw_text(cx - 45, obs_y + 12, "SLOW", COL_TEXT);
    }
}

void draw_sensor_health_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "SENSOR HEALTH");

    char buf[64];
    int row = y + 85;

    draw_text(x + 30, row, "IMU", COL_LABEL);
    if (local_dash.imu_health == HEALTH_OK) {
        fill_circle(x + 130, row + 8, 10, COL_GREEN);
        draw_text(x + 160, row, "HEALTHY", COL_GREEN);
    } else if (local_dash.imu_health == HEALTH_LATE) {
        fill_circle(x + 130, row + 8, 10, COL_YELLOW);
        draw_text(x + 160, row, "LATE", COL_YELLOW);
    } else {
        fill_circle(x + 130, row + 8, 10, COL_RED);
        draw_text(x + 160, row, "FAILED", COL_RED);
    }
    snprintf(buf, sizeof(buf), "YAW %+d", local_dash.yaw_dps);
    draw_text(x + w - 200, row, buf, COL_TEXT);

    row += 55;
    draw_text(x + 30, row, "ULTRASONIC", COL_LABEL);
    if (local_dash.us_health == HEALTH_OK) {
        fill_circle(x + 230, row + 8, 10, COL_GREEN);
        draw_text(x + 260, row, "HEALTHY", COL_GREEN);
    } else if (local_dash.us_health == HEALTH_LATE) {
        fill_circle(x + 230, row + 8, 10, COL_YELLOW);
        draw_text(x + 260, row, "LATE", COL_YELLOW);
    } else {
        fill_circle(x + 230, row + 8, 10, COL_RED);
        draw_text(x + 260, row, "FAILED", COL_RED);
    }

    row += 55;
    draw_text(x + 30, row, "DISTANCE", COL_LABEL);
    if (local_dash.distance_cm <= 15) {
        snprintf(buf, sizeof(buf), "OBSTACLE");
        draw_text(x + 230, row, buf, COL_RED);
    } else if (local_dash.distance_cm < 60) {
        snprintf(buf, sizeof(buf), "%d cm", local_dash.distance_cm);
        draw_text(x + 230, row, buf, COL_YELLOW);
    } else {
        snprintf(buf, sizeof(buf), "%d cm", local_dash.distance_cm);
        draw_text(x + 230, row, buf, COL_TEXT);
    }

    row += 55;
    draw_text(x + 30, row, "HEARTBEAT", COL_LABEL);
    snprintf(buf, sizeof(buf), "%d ms", local_dash.heartbeat_age_ms);
    unsigned int hb_col = COL_TEXT;
    if (local_dash.heartbeat_age_ms > 200) hb_col = COL_RED;
    else if (local_dash.heartbeat_age_ms > 100) hb_col = COL_YELLOW;
    draw_text(x + 230, row, buf, hb_col);
}

void draw_system_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "SYSTEM");

    int row = y + 90;
    draw_text(x + 30, row, "CPU", COL_LABEL);
    fill_rounded_rect(x + 150, row + 2, 420, 30, 8, COL_BAR_BG);
    int cpu_w = (cached_cpu * 420) / 100;
    unsigned int cpu_col = COL_GREEN;
    if (cached_cpu > 50) cpu_col = COL_YELLOW;
    if (cached_cpu > 80) cpu_col = COL_RED;
    if (cpu_w > 0) fill_rounded_rect(x + 150, row + 2, cpu_w, 30, 8, cpu_col);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d %%", cached_cpu);
    draw_text_big(x + 600, row - 8, buf, COL_TEXT);

    row += 70;
    draw_text(x + 30, row, "RAM", COL_LABEL);
    fill_rounded_rect(x + 150, row + 2, 420, 30, 8, COL_BAR_BG);
    int ram_w = (cached_ram_pct * 420) / 100;
    if (ram_w > 0) fill_rounded_rect(x + 150, row + 2, ram_w, 30, 8, COL_BLUE);
    snprintf(buf, sizeof(buf), "%d %%", cached_ram_pct);
    draw_text_big(x + 600, row - 8, buf, COL_TEXT);

    row += 60;
    snprintf(buf, sizeof(buf), "%d MB / %d MB", cached_ram_used_mb, cached_ram_total_mb);
    draw_text(x + 150, row, buf, COL_LABEL);
}

void draw_led_card(int x, int y, int w, int h)
{
    draw_card(x, y, w, h, "LED STATUS");

    struct { const char *name; unsigned int col; int state; } leds[3] = {
        {"GREEN    NORMAL",     COL_GREEN,  STATE_NORMAL},
        {"YELLOW   DEGRADED",   COL_YELLOW, STATE_DEGRADED},
        {"RED      EMERGENCY",  COL_RED,    STATE_EMERGENCY}
    };

    int i;
    for (i = 0; i < 3; i++) {
        int ry = y + 90 + i * 60;
        int active = (local_dash.state == leds[i].state);

        unsigned int dull = 0xFFB0B8C0;
        if (i == 0) dull = 0xFFA8D8B8;
        if (i == 1) dull = 0xFFE0D090;
        if (i == 2) dull = 0xFFE0A8A8;

        if (active) {
            int pulse = (anim_frame / 3) % 30;
            if (pulse > 15) pulse = 30 - pulse;
            int glow = 80 + pulse * 8;
            if (glow > 220) glow = 220;
            fill_circle_alpha(x + 50, ry + 12, 30, leds[i].col, glow);
            fill_circle(x + 50, ry + 12, 15, leds[i].col);
            fill_circle(x + 50, ry + 12, 8, 0xFFFFFFFF);
        } else {
            fill_circle(x + 50, ry + 12, 15, dull);
        }
        draw_text(x + 100, ry + 2, leds[i].name, active ? COL_TEXT : COL_LABEL);
    }
}

int main(void)
{
    screen_context_t ctx;
    screen_window_t win;
    screen_buffer_t buf;
    int rc, nbuffers = 1;
    int size[2] = { W, H };
    int usage = SCREEN_USAGE_WRITE;
    int pos[2] = { 0, 0 };
    int bufsize[2];
    int stride;

    rc = screen_create_context(&ctx, SCREEN_APPLICATION_CONTEXT);
    if (rc) { perror("screen_create_context"); return 1; }

    rc = screen_create_window(&win, ctx);
    if (rc) { perror("screen_create_window"); screen_destroy_context(ctx); return 1; }

    screen_set_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(win, SCREEN_PROPERTY_USAGE, &usage);
    screen_set_window_property_iv(win, SCREEN_PROPERTY_POSITION, pos);

    rc = screen_create_window_buffers(win, nbuffers);
    if (rc) { perror("screen_create_window_buffers"); }

    rc = screen_get_window_property_pv(win, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)&buf);
    if (rc) { perror("screen_get_window_property_pv"); }

    rc = screen_get_buffer_property_pv(buf, SCREEN_PROPERTY_POINTER, (void**)&fb);
    if (rc) { perror("screen_get_buffer_property_pv"); return 1; }

    screen_get_buffer_property_iv(buf, SCREEN_PROPERTY_STRIDE, &stride);
    screen_get_buffer_property_iv(buf, SCREEN_PROPERTY_BUFFER_SIZE, bufsize);

    fb_stride = stride;
    fb_w = bufsize[0];
    fb_h = bufsize[1];

    memset(&local_dash, 0, sizeof(local_dash));

    while (1) {
        read_shared_state();
        update_cpu();
        update_ram();
        drive_leds(local_dash.state);

        draw_background();
        draw_top_bar();

        draw_speed_card(30, 120, 480, 320);
        draw_commands_card(530, 120, 620, 320);
        draw_status_card(1170, 120, 720, 320);

        draw_road_vehicle_card(30, 460, 720, 600);
        draw_led_card(770, 460, 440, 280);
        draw_sensor_health_card(770, 760, 440, 300);
        draw_system_card(1230, 460, 660, 600);

        screen_post_window(win, buf, 0, NULL, 0);

        anim_frame++;
        usleep(33000);
    }

    return 0;
}