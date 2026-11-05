// SPDX-License-Identifier: MIT
#include "nokia5110.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool extended, powered_down;
    unsigned x, bank, contrast, mode, begins, ends, writes, fail_at;
    bool fail_begin, fail_end;
    uint8_t ram[504];
} controller_t;
static int begin(void *ctx) {
    controller_t *c = ctx; c->begins++;
    c->extended=false; c->powered_down=true; c->x=c->bank=0;
    return c->fail_begin;
}
static int end(void *ctx) { controller_t *c=ctx; c->ends++; return c->fail_end; }
static int write_bytes(void *ctx, bool data, const uint8_t *bytes, size_t length) {
    controller_t *c=ctx;
    if (++c->writes == c->fail_at) return -1;
    for (size_t i=0; i<length; i++) {
        uint8_t v=bytes[i];
        if (data) {
            assert(!c->powered_down);
            assert(c->x<84 && c->bank<6);
            c->ram[c->bank*84+c->x]=v;
            if (++c->x==84) { c->x=0; c->bank=(c->bank+1)%6; }
        } else if ((v&0xf8)==0x20) { c->extended=v&1; c->powered_down=v&4; }
        else if (c->extended) { if (v&0x80) c->contrast=v&127; }
        else if (v&0x80) c->x=v&127;
        else if ((v&0xf8)==0x40) c->bank=v&7;
        else if ((v&0xf8)==0x08) c->mode=v;
    }
    return 0;
}
static void driver(void) {
    controller_t c={0}; nokia5110_t d={0}; nokia5110_canvas_t canvas;
    nokia5110_transport_t t={&c,begin,write_bytes,end};
    assert(nokia5110_init(&d,&t,128)==NOKIA5110_INVALID && c.begins==0);
    assert(nokia5110_flush(&d,&canvas)==NOKIA5110_STATE);
    assert(nokia5110_init(&d,&t,60)==0);
    assert(c.contrast==60 && !c.extended && !c.powered_down && c.mode==0x0c);
    assert(nokia5110_init(&d,&t,60)==NOKIA5110_STATE);
    for (unsigned i=0;i<504;i++) canvas.pixels[i]=(i*17)&255;
    assert(nokia5110_flush(&d,&canvas)==0);
    assert(!memcmp(canvas.pixels,c.ram,504));
    assert(nokia5110_set_contrast(&d,77)==0 && c.contrast==77);
    assert(nokia5110_set_inverted(&d,true)==0 && c.mode==0x0d);
    assert(nokia5110_set_sleep(&d,true)==0 && c.powered_down);
    for (unsigned i=0;i<504;i++) assert(c.ram[i]==0);
    assert(nokia5110_flush(&d,&canvas)==NOKIA5110_STATE);
    assert(nokia5110_set_contrast(&d,60)==NOKIA5110_STATE);
    assert(nokia5110_set_sleep(&d,false)==0);
    assert(c.contrast==77 && c.mode==0x0d && !c.powered_down);
    assert(nokia5110_flush(&d,&canvas)==0 && !memcmp(canvas.pixels,c.ram,504));
    c.fail_at=c.writes+1;
    assert(nokia5110_flush(&d,&canvas)==NOKIA5110_IO);
    c.fail_at=0;
    assert(nokia5110_deinit(&d)==0 && c.ends==1 && !d.initialized);
    assert(nokia5110_deinit(&d)==0 && c.ends==1);
    c.fail_begin=true;
    assert(nokia5110_init(&d,&t,60)==NOKIA5110_IO && c.ends==1);
    c.fail_begin=false; c.fail_at=c.writes+2;
    assert(nokia5110_init(&d,&t,60)==NOKIA5110_IO && c.ends==2 && !d.initialized);
    c.fail_at=0;
    assert(nokia5110_init(&d,&t,60)==0);
    c.fail_at=c.writes+1;
    assert(nokia5110_deinit(&d)==NOKIA5110_IO && c.ends==3 && !d.initialized);
}
static void graphics(void) {
    struct { uint32_t before; nokia5110_canvas_t c; uint32_t after; } guarded={.before=0x12345678,.after=0x87654321};
    nokia5110_canvas_t *c=&guarded.c;
    nokia5110_clear(c,false);
    nokia5110_pixel(c,83,47,true); assert(c->pixels[503]==0x80);
    nokia5110_pixel(c,0,8,true); assert(c->pixels[84]==1);
    nokia5110_pixel(c,83,47,false); assert(c->pixels[503]==0);
    nokia5110_pixel(c,INT_MIN,INT_MAX,true);
    nokia5110_rect(c,INT_MAX,INT_MAX,INT_MAX,INT_MAX,true,true);
    nokia5110_rect(c,-2,-2,4,4,true,true);
    assert((c->pixels[0]&3)==3 && (c->pixels[1]&3)==3);
    nokia5110_text(c,INT_MAX,INT_MIN,"CLIPPED\nTEXT",true);
    nokia5110_text(c,-3,-3,"TEST",true);
    assert(guarded.before==0x12345678 && guarded.after==0x87654321);
    nokia5110_canvas_t upper,lower;
    nokia5110_clear(&upper,false); nokia5110_clear(&lower,false);
    nokia5110_text(&upper,0,0,"ABC 123",true);
    nokia5110_text(&lower,0,0,"abc 123",true);
    assert(!memcmp(&upper,&lower,sizeof(upper)));
    nokia5110_text(&upper,0,0,"       ",true);
    for (int i=0;i<504;i++) assert(upper.pixels[i]==0);
    nokia5110_clear(c,false); nokia5110_rect(c,0,0,84,48,true,false);
    assert(c->pixels[0]==255 && c->pixels[83]==255 && c->pixels[1]==1 && c->pixels[421]==128);
}
int main(void) { driver(); graphics(); puts("Core: controller, lifecycle, failures and clipped drawing passed"); }
