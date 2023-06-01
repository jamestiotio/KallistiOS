/* KallistiOS ##version##

   util/vmu_fb.c
   Copyright (C) 2023 Paul Cercueil

*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <dc/maple/vmu.h>
#include <dc/vmu_fb.h>

#define GENMASK(h, l) \
    (((unsigned int)-1 << (l)) & ((unsigned int)-1 >> (31 - (h))))

static uint64_t extract_bits(const uint8_t *data,
                             unsigned int offt, unsigned int w) {
    uint32_t tmp, lsb, nb_bits;
    uint64_t bits = 0;

    /* This algorithm will extract "w" bits starting from bit "offt", and
       place them right-adjusted in "bits".

       Since we manipulate 8 bits at a time, and neither "w" nor "offt" are
       required to be byte-aligned, we need to compute a mask of valid bits
       for each byte that is processed. */
    while (w) {
        tmp = data[offt / 8];

        if (8 - (offt & 0x7) > w)
            lsb = 8 - (offt & 0x7) - w;
        else
            lsb = 0;

        nb_bits = 8 - (offt & 0x7) - lsb;
        bits <<= nb_bits;

        tmp &= GENMASK(7 - (offt & 0x7), lsb);

        bits |= tmp >> lsb;

        offt += nb_bits;
        w -= nb_bits;
    }

    return bits;
}

static void insert_bits(uint8_t *data,
                        unsigned int offt, unsigned int w, uint64_t bits) {
    uint32_t tmp, lsb, nb_bits, mask;

    while (w) {
        tmp = data[offt / 8];

        if (8 - (offt & 0x7) > w)
            lsb = 8 - (offt & 0x7) - w;
        else
            lsb = 0;

        nb_bits = 8 - (offt & 0x7) - lsb;
        mask = GENMASK(7 - (offt & 0x7), lsb);
        tmp &= ~mask;

        tmp |= ((bits >> (w - nb_bits)) << lsb) & mask;

        data[offt / 8] = tmp;

        offt += nb_bits;
        w -= nb_bits;
    }
}

void vmufb_paint_area(vmufb_t *fb,
                      unsigned int x, unsigned int y,
                      unsigned int w, unsigned int h,
                      const char *data) {
    unsigned int i;
    uint64_t bits;

    for (i = 0; i < h; i++) {
        bits = extract_bits((const uint8_t *)data, i * w, w);
        insert_bits((uint8_t *)fb->data, (y + i) * 48 + x, w, bits);
    }
}

void vmufb_clear(vmufb_t *fb) {
    memset(fb->data, 0, sizeof(fb->data));
}

void vmufb_clear_area(vmufb_t *fb,
                      unsigned int x, unsigned int y,
                      unsigned int w, unsigned int h) {
    uint32_t tmp[48] = {};

    vmufb_paint_area(fb, x, y, w, h, (const char *) tmp);
}

void vmufb_present(const vmufb_t *fb, maple_device_t *dev) {
    if (dev->info.connector_direction == 0) /* 0: UP - rotated like most controllers */
        vmu_draw_lcd_rotated(dev, fb->data);
    else /* 1: DOWN - not rotated, like lightgun */
        vmu_draw_lcd(dev, fb->data);
}

void vmufb_print_string_into(vmufb_t *fb,
                             const vmufb_font_t *font,
                             unsigned int x, unsigned int y,
                             unsigned int w, unsigned int h,
                             unsigned int line_spacing,
                             const char *str) {
    unsigned int xorig = x, yorig = y;

    for (; *str; str++) {
        switch (*str) {
        case '\n':
            x = xorig;
            y += line_spacing + font->h;
            continue;
        default:
            break;
        }

        if (x > xorig + w - font->w) {
            /* We run out of horizontal space - put character on new line */
            x = xorig;
            y += line_spacing + font->h;
        }

        if (y > yorig + h - font->h) {
            /* We ran out of space */
            break;
        }

        vmufb_paint_area(fb, x, y, font->w, font->h,
                         &font->fontdata[*str * font->stride]);

        x += font->w;
    }
}