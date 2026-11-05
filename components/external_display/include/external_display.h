#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Persistent IDs: never reorder/reuse; unknown content remains unavailable. */
enum {
    EXT_AUTO,
    EXT_CLOCK,
    EXT_ALARMS,
    EXT_STOPWATCH,
    EXT_COUNTDOWN,
    EXT_PACER,
    EXT_ACTIVITY,
    EXT_WORLD
};
enum {
    EXT_DRIVER_AUTO,
    EXT_DRIVER_NOKIA,
    EXT_DRIVER_VFD
};
enum {
    EXT_BUS_AUTO,
    EXT_BUS_CATT,
    EXT_BUS_INTERNAL
};
typedef struct {
    bool    enabled;
    uint8_t driver, content, clk, din, ce, dc, rst, contrast;
    uint8_t vfd_bus, vfd_address, vfd_brightness, world_city;
} external_config_t;
#define EXTERNAL_CONFIG_DEFAULT \
    ((external_config_t){       \
        .clk = 2, .din = 3, .ce = 15, .dc = 4, .rst = 5, .contrast = 60, .vfd_address = 0x13, .vfd_brightness = 110})
/* Only the verified free CATT digital pins are exposed. GPIO 12/13 are
 * reserved for CATT I2C; all internal peripherals and USB/JTAG alternatives
 * outside this list remain protected. 2..5 require exclusive external JTAG use. */
static inline bool external_pin_valid(unsigned p) {
    return (p >= 2 && p <= 5) || p == 15 || p == 34;
}
static inline bool external_config_valid(const external_config_t* c) {
    if (c->driver > EXT_DRIVER_VFD) return false;
    if (c->driver == EXT_DRIVER_NOKIA) {
        const uint8_t p[] = {c->clk, c->din, c->ce, c->dc, c->rst};
        if (c->contrast > 127) return false;
        for (unsigned i = 0; i < 5; ++i) {
            if (!external_pin_valid(p[i])) return false;
            for (unsigned j = 0; j < i; ++j)
                if (p[i] == p[j]) return false;
        }
    } else {
        /* This module's jumper address range; 0x10 conflicts internally.
         * Brightness capped at the reference's recommended maximum. */
        if (c->vfd_bus > EXT_BUS_INTERNAL || c->vfd_address < 0x10 || c->vfd_address > 0x2f ||
            (c->vfd_bus == EXT_BUS_INTERNAL && c->vfd_address == 0x10) || c->vfd_brightness > 110)
            return false;
    }
    return true;
}
static inline bool external_config_equal(const external_config_t* a, const external_config_t* b) {
    return a->enabled == b->enabled && a->driver == b->driver && a->content == b->content && a->clk == b->clk &&
           a->din == b->din && a->ce == b->ce && a->dc == b->dc && a->rst == b->rst && a->contrast == b->contrast &&
           a->vfd_bus == b->vfd_bus && a->vfd_address == b->vfd_address && a->vfd_brightness == b->vfd_brightness &&
           a->world_city == b->world_city;
}
/* A semantic information card, independent of the internal UI framebuffer. */
typedef struct {
    uint8_t source;
    char    title[32], value[32], detail[48], extra[48];
} external_frame_t;
typedef enum {
    EXT_DISABLED,
    EXT_STARTING,
    EXT_NOT_FOUND,
    EXT_NOKIA_READY,
    EXT_VFD_READY,
    EXT_CONFIG_ERROR,
    EXT_INIT_ERROR,
    EXT_IO_ERROR
} external_status_t;
const char*       external_status_text(external_status_t status);
/* Owner-task entry points. Submit is nonblocking and copies its inputs. */
void              external_display_submit(const external_config_t* config, const external_frame_t* frame, bool retry);
external_status_t external_display_status(void);
void              external_display_stop(void);
/* Driver-specific formatting, also exercised by host tests. */
void              external_vfd_format(const external_frame_t* frame, char text[13]);
