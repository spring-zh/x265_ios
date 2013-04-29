/*****************************************************************************
 * Copyright (C) 2013 x265 project
 *
 * Authors: Steve Borho <steve@borho.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@multicorewareinc.com.
 *****************************************************************************/

#include "pixelharness.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

using namespace x265;

// Initialize the Func Names for all the Pixel Comp
static const char *FuncNames[NUM_PARTITIONS] =
{
    "  4x4", "  4x8", " 4x12", " 4x16", " 4x24", " 4x32", " 4x64",
    "  8x4", "  8x8", " 8x12", " 8x16", " 8x24", " 8x32", " 8x64",
    " 12x4", " 12x8", "12x12", "12x16", "12x24", "12x32", "12x64",
    " 16x4", " 16x8", "16x12", "16x16", "16x24", "16x32", "16x64",
    " 24x4", " 24x8", "24x12", "24x16", "24x24", "24x32", "24x64",
    " 32x4", " 32x8", "32x12", "32x16", "32x24", "32x32", "32x64",
    " 64x4", " 64x8", "64x12", "64x16", "64x24", "64x32", "64x64"
};

#if HIGH_BIT_DEPTH
#define BIT_DEPTH 10
#else
#define BIT_DEPTH 8
#endif

#define PIXEL_MAX ((1 << BIT_DEPTH) - 1)

PixelHarness::PixelHarness()
{
    pbuf1 = (pixel*)TestHarness::alignedMalloc(sizeof(pixel), 0x1e00, 32);
    pbuf2 = (pixel*)TestHarness::alignedMalloc(sizeof(pixel), 0x1e00, 32);

    if (!pbuf1 || !pbuf2)
    {
        fprintf(stderr, "malloc failed, unable to initiate tests!\n");
        exit(1);
    }

    for (int i = 0; i < 0x1e00; i++)
    {
        //Generate the Random Buffer for Testing
        pbuf1[i] = rand() & PIXEL_MAX;
        pbuf2[i] = rand() & PIXEL_MAX;
    }
}

PixelHarness::~PixelHarness()
{
    TestHarness::alignedFree(pbuf1);
    TestHarness::alignedFree(pbuf2);
}

#define INCR 16
#define STRIDE 16

bool PixelHarness::check_pixel_primitive(pixelcmp ref, pixelcmp opt)
{
    int j = 0;

    for (int i = 0; i <= 100; i++)
    {
        int vres = opt(pbuf1, STRIDE, pbuf2 + j, STRIDE);
        int cres = ref(pbuf1, STRIDE, pbuf2 + j, STRIDE);
        if (vres != cres)
            return false;

        j += INCR;
    }

    return true;
}

bool PixelHarness::testCorrectness(const EncoderPrimitives& ref, const EncoderPrimitives& opt)
{
    for (uint16_t curpar = 0; curpar < NUM_PARTITIONS; curpar++)
    {
        if (opt.satd[curpar])
        {
            if (!check_pixel_primitive(ref.satd[curpar], opt.satd[curpar]))
            {
                printf("satd[%s]: failed!\n", FuncNames[curpar]);
                return false;
            }
        }

        if (opt.sad[curpar])
        {
            if (!check_pixel_primitive(ref.sad[curpar], opt.sad[curpar]))
            {
                printf("sad[%s]: failed!\n", FuncNames[curpar]);
                return false;
            }
        }
    }

    if (opt.sa8d_8x8)
    {
        if (!check_pixel_primitive(ref.sa8d_8x8, opt.sa8d_8x8))
        {
            printf("sa8d_8x8: failed!\n");
            return false;
        }
    }

    if (opt.sa8d_16x16)
    {
        if (!check_pixel_primitive(ref.sa8d_16x16, opt.sa8d_16x16))
        {
            printf("sa8d_16x16: failed!\n");
            return false;
        }
    }

    return true;
}

void PixelHarness::measureSpeed(const EncoderPrimitives& ref, const EncoderPrimitives& opt)
{
    Timer *t = Timer::CreateTimer();

    int iters = 2000000;

    for (int curpar = 0; curpar < NUM_PARTITIONS; curpar++)
    {
        if (opt.satd[curpar])
        {
            printf("satd[%s]", FuncNames[curpar]);
            REPORT_SPEEDUP(iters,
                           opt.satd[curpar](pbuf1, STRIDE, pbuf2, STRIDE),
                           ref.satd[curpar](pbuf1, STRIDE, pbuf2, STRIDE));
        }

        if (opt.sad[curpar])
        {
            printf(" sad[%s]", FuncNames[curpar]);
            REPORT_SPEEDUP(iters,
                opt.sad[curpar](pbuf1, STRIDE, pbuf2, STRIDE),
                ref.sad[curpar](pbuf1, STRIDE, pbuf2, STRIDE));
        }

        // adaptive iteration count, reduce as partition size increases
        if ((curpar & 15) == 15) iters >>= 1;
    }

    if (opt.sa8d_8x8)
    {
        printf("sa8d_8x8");
        REPORT_SPEEDUP(iters,
            opt.sa8d_8x8(pbuf1, STRIDE, pbuf2, STRIDE),
            ref.sa8d_8x8(pbuf1, STRIDE, pbuf2, STRIDE));
    }

    if (opt.sa8d_16x16)
    {
        printf("sa8d_16x16");
        REPORT_SPEEDUP(iters,
            opt.sa8d_16x16(pbuf1, STRIDE, pbuf2, STRIDE),
            ref.sa8d_16x16(pbuf1, STRIDE, pbuf2, STRIDE));
    }

    t->Release();
}
