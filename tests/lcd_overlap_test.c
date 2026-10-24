/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Revisions:
 * (Orange Cat 0.7.0) one canvas of 240 x 62 (was 240 x 124 and a second 240 x 24): taller regions drawn in bands (CV_BANDS), slices follow the canvas, partial sends a r...
 */
/* Real LCD/canvas code against a DMA that remains busy until explicitly
 * waited on. It checks source bytes at completion, catching early reuse. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __attribute__(x)
static const void *dma_source;
static uint32_t dma_bytes, dma_waits;
static uint8_t dma_copy[240u*124u*2u];
static void fm1_lcd_wait(void)
{
    if(dma_source){assert(!memcmp(dma_source,dma_copy,dma_bytes));dma_source=0;dma_waits++;}
}
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_send_cmd(uint8_t c){(void)c;assert(!dma_source);}
static void fm1_lcd_send_data(const void *p,uint32_t n)
{
    assert(!dma_source && n<=sizeof dma_copy);dma_source=p;dma_bytes=n;memcpy(dma_copy,p,n);
}
static void fm1_lcd_hw_init(void) {}
static void fm1_lcd_baud(uint32_t b){(void)b;}
#include "../firmware/src/lcd.c"
#include "../firmware/src/gfx.c"
/* (0.7.0) one canvas (gfx.c: 240 x 62): drawing waits for the transfer before it, never writes over it; a region
 * taller than the canvas goes in bands (CV_BANDS), each band's bytes as drawn when its transfer ends */
int main(void)
{
    cv_begin(240,24,RGB(20,30,40));cv_rect(3,3,10,10,RGB(200,50,0));cv_blit(0,0);
    const uint16_t *first=cv_px;uint32_t before=dma_waits;
    assert(dma_source==first);
    cv_begin(55,60,RGB(0,70,0));                 /* the next part: the transfer finished first, then the same canvas */
    assert(cv_px==first && dma_waits==before+1u && !dma_source);
    cv_rect(0,0,20,20,RGB(90,0,90));cv_blit(0,24);
    before=dma_waits;
    cv_begin(240,62,RGB(10,0,0));assert(cv_px==first && dma_waits==before+1u);
    cv_rect(0,0,240,62,RGB(100,100,100));cv_blit_from(0,24,2);
    assert(dma_source==cv_px+2u*240u);
    before=dma_waits;
    cv_begin(240,62,RGB(0,0,80));assert(dma_waits==before+1u && !dma_source);
    cv_blit(0,24);
    lcd_fill(0,0,240,1,RGB(70,0,0));lcd_sync();
    assert(!dma_source);
    {   /* a region of 124 rows: two bands, each sent whole, each drawn again with the same picture */
        uint32_t bands=0;
        CV_BANDS(0,84,240,124,RGB(5,5,5)) {
            assert(cv_h==62u && cv_band==(int32_t)(bands*62u));
            cv_rect(10,50,100,30,RGB(250,0,0));   /* across the seam: rows 50..61 in band 0, 62..79 in band 1 */
            bands++;
        }
        assert(bands==2u && !cv_band && !cv_region_h);
        assert(dma_source==cv_px);               /* the second band still on its way: its bytes checked at the wait */
        assert(cv_px[(79u-62u)*240u+10u]==swap16(RGB(250,0,0)) && cv_px[(80u-62u)*240u+10u]==swap16(RGB(5,5,5)));
    }
    cv_begin(240,62,RGB(40,50,60));cv_blit_from(0,24,3);
    before=dma_waits;
    uint32_t *work=cv_work();
    assert((void *)work==(void *)cv_primary && !dma_source && !cv_flight && dma_waits==before+1u);
    memset(work,0xA5,sizeof(cv_primary_u));     /* decoded project overwrites only after DMA completed */
    puts("LCD overlap: one canvas, each transfer done before it is drawn over; bands; project scratch");
    return 0;
}
