/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2013, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TComRom.cpp
    \brief    global variables & functions
*/

#include "TComRom.h"
#include "threading.h"
#include "common.h"

namespace x265 {
//! \ingroup TLibCommon
//! \{

// lambda = pow(2, (double)q / 6 - 2);
double x265_lambda_tab[MAX_MAX_QP + 1] =
{
    0.2500, 0.2806, 0.3150, 0.3536, 0.3969,
    0.4454, 0.5000, 0.5612, 0.6300, 0.7071,
    0.7937, 0.8909, 1.0000, 1.1225, 1.2599,
    1.4142, 1.5874, 1.7818, 2.0000, 2.2449,
    2.5198, 2.8284, 3.1748, 3.5636, 4.0000,
    4.4898, 5.0397, 5.6569, 6.3496, 7.1272,
    8.0000, 8.9797, 10.0794, 11.3137, 12.6992,
    14.2544, 16.0000, 17.9594, 20.1587, 22.6274,
    25.3984, 28.5088, 32.0000, 35.9188, 40.3175,
    45.2548, 50.7968, 57.0175, 64.0000, 71.8376,
    80.6349, 90.5097, 101.5937, 114.0350, 128.0000,
    143.6751, 161.2699, 181.0193, 203.1873, 228.0701,
    256.0000, 287.3503, 322.5398, 362.0387, 406.3747,
    456.1401, 512.0000, 574.7006, 645.0796, 724.0773
};

// lambda2 = pow(lambda, 2) * scale (0.85);
double x265_lambda2_tab[MAX_MAX_QP + 1] =
{
    0.0531, 0.0669, 0.0843, 0.1063, 0.1339,
    0.1687, 0.2125, 0.2677, 0.3373, 0.4250,
    0.5355, 0.6746, 0.8500, 1.0709, 1.3493,
    1.7000, 2.1419, 2.6986, 3.4000, 4.2837,
    5.3970, 6.8000, 8.5675, 10.7943, 13.6000,
    17.1345, 21.5887, 27.2004, 34.2699, 43.1773,
    54.4000, 68.5397, 86.3551, 108.7998, 137.0792,
    172.7097, 217.6000, 274.1590, 345.4172, 435.1993,
    548.3169, 690.8389, 870.4000, 1096.6362, 1381.6757,
    1740.7974, 2193.2676, 2763.3460, 3481.6000, 4386.5446,
    5526.6890, 6963.2049, 8773.0879, 11053.3840, 13926.4000,
    17546.1542, 22106.7835, 27852.7889, 35092.3170, 44213.5749,
    55705.6000, 70184.6657, 88427.1342, 111411.2172, 140369.3373,
    176854.2222, 222822.4000, 280738.6627, 353708.5368, 445644.7459
};

const uint16_t x265_chroma_lambda2_offset_tab[MAX_CHROMA_LAMBDA_OFFSET+1] =
{
       16,    20,    25,    32,    40,    50,
       64,    80,   101,   128,   161,   203,
      256,   322,   406,   512,   645,   812,
     1024,  1290,  1625,  2048,  2580,  3250,
     4096,  5160,  6501,  8192, 10321, 13003,
    16384, 20642, 26007, 32768, 41285, 52015,
    65535
};

static int initialized /* = 0 */;

// initialize ROM variables
void initROM()
{
    if (ATOMIC_CAS32(&initialized, 0, 1) == 1)
        return;

    int i, c;

    memset(g_convertToBit, -1, sizeof(g_convertToBit));
    c = 0;
    for (i = 4; i <= MAX_CU_SIZE; i *= 2)
    {
        g_convertToBit[i] = c;
        c++;
    }
}

void destroyROM()
{
    if (ATOMIC_CAS32(&initialized, 1, 0) == 0)
        return;
}

// ====================================================================================================================
// Data structure related table & variable
// ====================================================================================================================

uint32_t g_maxLog2CUSize = MAX_LOG2_CU_SIZE;
uint32_t g_maxCUSize   = MAX_CU_SIZE;
uint32_t g_maxCUDepth  = MAX_FULL_DEPTH;
uint32_t g_addCUDepth  = 1;
uint32_t g_log2UnitSize = 2;
uint32_t g_zscanToRaster[MAX_NUM_SPU_W * MAX_NUM_SPU_W] = { 0, };
uint32_t g_rasterToZscan[MAX_NUM_SPU_W * MAX_NUM_SPU_W] = { 0, };
uint32_t g_rasterToPelX[MAX_NUM_SPU_W * MAX_NUM_SPU_W] = { 0, };
uint32_t g_rasterToPelY[MAX_NUM_SPU_W * MAX_NUM_SPU_W] = { 0, };

const uint32_t g_puOffset[8] = { 0, 8, 4, 4, 2, 10, 1, 5 };

void initZscanToRaster(int maxDepth, int depth, uint32_t startVal, uint32_t*& curIdx)
{
    int stride = 1 << (maxDepth - 1);

    if (depth == maxDepth)
    {
        curIdx[0] = startVal;
        curIdx++;
    }
    else
    {
        int step = stride >> depth;
        initZscanToRaster(maxDepth, depth + 1, startVal,                        curIdx);
        initZscanToRaster(maxDepth, depth + 1, startVal + step,                 curIdx);
        initZscanToRaster(maxDepth, depth + 1, startVal + step * stride,        curIdx);
        initZscanToRaster(maxDepth, depth + 1, startVal + step * stride + step, curIdx);
    }
}

void initRasterToZscan(uint32_t maxCUSize, uint32_t maxDepth)
{
    uint32_t  unitSize = maxCUSize  >> (maxDepth - 1);

    uint32_t  numPartInCUSize  = (uint32_t)maxCUSize / unitSize;

    for (uint32_t i = 0; i < numPartInCUSize * numPartInCUSize; i++)
    {
        g_rasterToZscan[g_zscanToRaster[i]] = i;
    }
}

void initRasterToPelXY(uint32_t maxCUSize, uint32_t maxDepth)
{
    uint32_t i;

    uint32_t* tempX = &g_rasterToPelX[0];
    uint32_t* tempY = &g_rasterToPelY[0];

    uint32_t  unitSize  = maxCUSize >> (maxDepth - 1);

    uint32_t  numPartInCUSize = maxCUSize / unitSize;

    tempX[0] = 0;
    tempX++;
    for (i = 1; i < numPartInCUSize; i++)
    {
        tempX[0] = tempX[-1] + unitSize;
        tempX++;
    }

    for (i = 1; i < numPartInCUSize; i++)
    {
        memcpy(tempX, tempX - numPartInCUSize, sizeof(uint32_t) * numPartInCUSize);
        tempX += numPartInCUSize;
    }

    for (i = 1; i < numPartInCUSize * numPartInCUSize; i++)
    {
        tempY[i] = (i / numPartInCUSize) * unitSize;
    }
}

const int16_t g_lumaFilter[4][NTAPS_LUMA] =
{
    {  0, 0,   0, 64,  0,   0, 0,  0 },
    { -1, 4, -10, 58, 17,  -5, 1,  0 },
    { -1, 4, -11, 40, 40, -11, 4, -1 },
    {  0, 1,  -5, 17, 58, -10, 4, -1 }
};

const int16_t g_chromaFilter[8][NTAPS_CHROMA] =
{
    {  0, 64,  0,  0 },
    { -2, 58, 10, -2 },
    { -4, 54, 16, -2 },
    { -6, 46, 28, -4 },
    { -4, 36, 36, -4 },
    { -4, 28, 46, -6 },
    { -2, 16, 54, -4 },
    { -2, 10, 58, -2 }
};

const int16_t g_t4[4][4] =
{
    { 64, 64, 64, 64 },
    { 83, 36, -36, -83 },
    { 64, -64, -64, 64 },
    { 36, -83, 83, -36 }
};

const int16_t g_t8[8][8] =
{
    { 64, 64, 64, 64, 64, 64, 64, 64 },
    { 89, 75, 50, 18, -18, -50, -75, -89 },
    { 83, 36, -36, -83, -83, -36, 36, 83 },
    { 75, -18, -89, -50, 50, 89, 18, -75 },
    { 64, -64, -64, 64, 64, -64, -64, 64 },
    { 50, -89, 18, 75, -75, -18, 89, -50 },
    { 36, -83, 83, -36, -36, 83, -83, 36 },
    { 18, -50, 75, -89, 89, -75, 50, -18 }
};

const int16_t g_t16[16][16] =
{
    { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 },
    { 90, 87, 80, 70, 57, 43, 25,  9, -9, -25, -43, -57, -70, -80, -87, -90 },
    { 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89 },
    { 87, 57,  9, -43, -80, -90, -70, -25, 25, 70, 90, 80, 43, -9, -57, -87 },
    { 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83 },
    { 80,  9, -70, -87, -25, 57, 90, 43, -43, -90, -57, 25, 87, 70, -9, -80 },
    { 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75 },
    { 70, -43, -87,  9, 90, 25, -80, -57, 57, 80, -25, -90, -9, 87, 43, -70 },
    { 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64 },
    { 57, -80, -25, 90, -9, -87, 43, 70, -70, -43, 87,  9, -90, 25, 80, -57 },
    { 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50 },
    { 43, -90, 57, 25, -87, 70,  9, -80, 80, -9, -70, 87, -25, -57, 90, -43 },
    { 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36 },
    { 25, -70, 90, -80, 43,  9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -25 },
    { 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18 },
    {  9, -25, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 25, -9 }
};

const int16_t g_t32[32][32] =
{
    { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 },
    { 90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13,  4, -4, -13, -22, -31, -38, -46, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90 },
    { 90, 87, 80, 70, 57, 43, 25,  9, -9, -25, -43, -57, -70, -80, -87, -90, -90, -87, -80, -70, -57, -43, -25, -9,  9, 25, 43, 57, 70, 80, 87, 90 },
    { 90, 82, 67, 46, 22, -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13, 13, 38, 61, 78, 88, 90, 85, 73, 54, 31,  4, -22, -46, -67, -82, -90 },
    { 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89, 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89 },
    { 88, 67, 31, -13, -54, -82, -90, -78, -46, -4, 38, 73, 90, 85, 61, 22, -22, -61, -85, -90, -73, -38,  4, 46, 78, 90, 82, 54, 13, -31, -67, -88 },
    { 87, 57,  9, -43, -80, -90, -70, -25, 25, 70, 90, 80, 43, -9, -57, -87, -87, -57, -9, 43, 80, 90, 70, 25, -25, -70, -90, -80, -43,  9, 57, 87 },
    { 85, 46, -13, -67, -90, -73, -22, 38, 82, 88, 54, -4, -61, -90, -78, -31, 31, 78, 90, 61,  4, -54, -88, -82, -38, 22, 73, 90, 67, 13, -46, -85 },
    { 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83 },
    { 82, 22, -54, -90, -61, 13, 78, 85, 31, -46, -90, -67,  4, 73, 88, 38, -38, -88, -73, -4, 67, 90, 46, -31, -85, -78, -13, 61, 90, 54, -22, -82 },
    { 80,  9, -70, -87, -25, 57, 90, 43, -43, -90, -57, 25, 87, 70, -9, -80, -80, -9, 70, 87, 25, -57, -90, -43, 43, 90, 57, -25, -87, -70,  9, 80 },
    { 78, -4, -82, -73, 13, 85, 67, -22, -88, -61, 31, 90, 54, -38, -90, -46, 46, 90, 38, -54, -90, -31, 61, 88, 22, -67, -85, -13, 73, 82,  4, -78 },
    { 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75, 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75 },
    { 73, -31, -90, -22, 78, 67, -38, -90, -13, 82, 61, -46, -88, -4, 85, 54, -54, -85,  4, 88, 46, -61, -82, 13, 90, 38, -67, -78, 22, 90, 31, -73 },
    { 70, -43, -87,  9, 90, 25, -80, -57, 57, 80, -25, -90, -9, 87, 43, -70, -70, 43, 87, -9, -90, -25, 80, 57, -57, -80, 25, 90,  9, -87, -43, 70 },
    { 67, -54, -78, 38, 85, -22, -90,  4, 90, 13, -88, -31, 82, 46, -73, -61, 61, 73, -46, -82, 31, 88, -13, -90, -4, 90, 22, -85, -38, 78, 54, -67 },
    { 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64 },
    { 61, -73, -46, 82, 31, -88, -13, 90, -4, -90, 22, 85, -38, -78, 54, 67, -67, -54, 78, 38, -85, -22, 90,  4, -90, 13, 88, -31, -82, 46, 73, -61 },
    { 57, -80, -25, 90, -9, -87, 43, 70, -70, -43, 87,  9, -90, 25, 80, -57, -57, 80, 25, -90,  9, 87, -43, -70, 70, 43, -87, -9, 90, -25, -80, 57 },
    { 54, -85, -4, 88, -46, -61, 82, 13, -90, 38, 67, -78, -22, 90, -31, -73, 73, 31, -90, 22, 78, -67, -38, 90, -13, -82, 61, 46, -88,  4, 85, -54 },
    { 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50, 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50 },
    { 46, -90, 38, 54, -90, 31, 61, -88, 22, 67, -85, 13, 73, -82,  4, 78, -78, -4, 82, -73, -13, 85, -67, -22, 88, -61, -31, 90, -54, -38, 90, -46 },
    { 43, -90, 57, 25, -87, 70,  9, -80, 80, -9, -70, 87, -25, -57, 90, -43, -43, 90, -57, -25, 87, -70, -9, 80, -80,  9, 70, -87, 25, 57, -90, 43 },
    { 38, -88, 73, -4, -67, 90, -46, -31, 85, -78, 13, 61, -90, 54, 22, -82, 82, -22, -54, 90, -61, -13, 78, -85, 31, 46, -90, 67,  4, -73, 88, -38 },
    { 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36 },
    { 31, -78, 90, -61,  4, 54, -88, 82, -38, -22, 73, -90, 67, -13, -46, 85, -85, 46, 13, -67, 90, -73, 22, 38, -82, 88, -54, -4, 61, -90, 78, -31 },
    { 25, -70, 90, -80, 43,  9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -25, -25, 70, -90, 80, -43, -9, 57, -87, 87, -57,  9, 43, -80, 90, -70, 25 },
    { 22, -61, 85, -90, 73, -38, -4, 46, -78, 90, -82, 54, -13, -31, 67, -88, 88, -67, 31, 13, -54, 82, -90, 78, -46,  4, 38, -73, 90, -85, 61, -22 },
    { 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18, 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18 },
    { 13, -38, 61, -78, 88, -90, 85, -73, 54, -31,  4, 22, -46, 67, -82, 90, -90, 82, -67, 46, -22, -4, 31, -54, 73, -85, 90, -88, 78, -61, 38, -13 },
    {  9, -25, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 25, -9, -9, 25, -43, 57, -70, 80, -87, 90, -90, 87, -80, 70, -57, 43, -25,  9 },
    {  4, -13, 22, -31, 38, -46, 54, -61, 67, -73, 78, -82, 85, -88, 90, -90, 90, -90, 88, -85, 82, -78, 73, -67, 61, -54, 46, -38, 31, -22, 13, -4 }
};

const uint8_t g_chromaScale[chromaQPMappingTableSize] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51
};

const uint8_t g_chroma422IntraAngleMappingTable[36] =
{ 0, 1, 2, 2, 2, 2, 3, 5, 7, 8, 10, 12, 13, 15, 17, 18, 19, 20, 21, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 29, 30, 31, DM_CHROMA_IDX };

uint8_t g_convertToBit[MAX_CU_SIZE + 1];

// ====================================================================================================================
// Scanning order & context model mapping
// ====================================================================================================================

const uint16_t g_scan2x2[][2*2] =
{
    { 0, 2, 1, 3 },
    { 0, 1, 2, 3 },
};

const uint16_t g_scan8x8[NUM_SCAN_TYPE][8 * 8] =
{
    { 0,   8,  1, 16,  9,  2, 24, 17, 10,  3, 25, 18, 11, 26, 19, 27, 32, 40, 33, 48, 41, 34, 56, 49, 42, 35, 57, 50, 43, 58, 51, 59,
      4,  12,  5, 20, 13,  6, 28, 21, 14,  7, 29, 22, 15, 30, 23, 31, 36, 44, 37, 52, 45, 38, 60, 53, 46, 39, 61, 54, 47, 62, 55, 63 },
    { 0,   1,  2,  3,  8,  9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27,  4,  5,  6,  7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31,
      32, 33, 34, 35, 40, 41, 42, 43, 48, 49, 50, 51, 56, 57, 58, 59, 36, 37, 38, 39, 44, 45, 46, 47, 52, 53, 54, 55, 60, 61, 62, 63 },
    { 0,   8, 16, 24,  1,  9, 17, 25,  2, 10, 18, 26,  3, 11, 19, 27, 32, 40, 48, 56, 33, 41, 49, 57, 34, 42, 50, 58, 35, 43, 51, 59,
      4,  12, 20, 28,  5, 13, 21, 29,  6, 14, 22, 30,  7, 15, 23, 31, 36, 44, 52, 60, 37, 45, 53, 61, 38, 46, 54, 62, 39, 47, 55, 63 }
};

const uint16_t g_scan4x4[NUM_SCAN_TYPE][4 * 4] =
{
    { 0,  4,  1,  8,  5,  2, 12,  9,  6,  3, 13, 10,  7, 14, 11, 15 },
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 0,  4,  8, 12,  1,  5,  9, 13,  2,  6, 10, 14,  3,  7, 11, 15 }
};

const uint16_t g_scan16x16[16 * 16] =
{
    0, 16, 1, 32, 17, 2, 48, 33, 18, 3, 49, 34, 19, 50, 35, 51,
    64, 80, 65, 96, 81, 66, 112, 97, 82, 67, 113, 98, 83, 114, 99, 115,
    4, 20, 5, 36, 21, 6, 52, 37, 22, 7, 53, 38, 23, 54, 39, 55,
    128, 144, 129, 160, 145, 130, 176, 161, 146, 131, 177, 162, 147, 178, 163, 179,
    68, 84, 69, 100, 85, 70, 116, 101, 86, 71, 117, 102, 87, 118, 103, 119,
    8, 24, 9, 40, 25, 10, 56, 41, 26, 11, 57, 42, 27, 58, 43, 59,
    192,208, 193,224,209, 194,240,225,210, 195,241,226,211,242,227,243,
    132, 148, 133, 164, 149, 134, 180, 165, 150, 135, 181, 166, 151, 182, 167, 183,
    72, 88, 73, 104, 89, 74, 120, 105, 90, 75, 121, 106, 91, 122, 107, 123,
    12, 28, 13, 44, 29, 14, 60, 45, 30, 15, 61, 46, 31, 62, 47, 63,
    196,212, 197,228,213, 198,244,229,214, 199,245,230,215,246,231,247,
    136, 152, 137, 168, 153, 138, 184, 169, 154, 139, 185, 170, 155, 186, 171, 187,
    76, 92, 77, 108, 93, 78, 124, 109, 94, 79, 125, 110, 95, 126, 111, 127,
    200,216,201,232,217,202,248,233,218,203,249,234,219,250,235,251,
    140, 156, 141, 172, 157, 142, 188, 173, 158, 143, 189, 174, 159, 190, 175, 191,
    204,220,205,236,221,206,252,237,222,207,253,238,223,254,239,255
};

const uint16_t g_scan8x8diag[8 * 8] =
{
    0,   8,  1, 16,  9,  2, 24, 17,
    10,  3, 32, 25, 18, 11,  4, 40,
    33, 26, 19, 12,  5, 48, 41, 34,
    27, 20, 13,  6, 56, 49, 42, 35,
    28, 21, 14,  7, 57, 50, 43, 36,
    29, 22, 15, 58, 51, 44, 37, 30,
    23, 59, 52, 45, 38, 31, 60, 53,
    46, 39, 61, 54, 47, 62, 55, 63
};

const uint16_t g_scan32x32[32 * 32] =
{
    0,32,1,64,33,2,96,65,34,3,97,66,35,98,67,99,128,160,129,192,161,130,224,193,162,131,225,194,163,226,195,227,
    4,36,5,68,37,6,100,69,38,7,101,70,39,102,71,103,256,288,257,320,289,258,352,321,290,259,353,322,291,354,323,355,
    132,164,133,196,165,134,228,197,166,135,229,198,167,230,199,231,8,40,9,72,41,10,104,73,42,11,105,74,43,106,75,107,
    384,416,385,448,417,386,480,449,418,387,481,450,419,482,451,483,260,292,261,324,293,262,356,325,294,263,357,326,295,358,327,359,
    136,168,137,200,169,138,232,201,170,139,233,202,171,234,203,235,12,44,13,76,45,14,108,77,46,15,109,78,47,110,79,111,
    512,544,513,576,545,514,608,577,546,515,609,578,547,610,579,611,388,420,389,452,421,390,484,453,422,391,485,454,423,486,455,487,
    264,296,265,328,297,266,360,329,298,267,361,330,299,362,331,363,140,172,141,204,173,142,236,205,174,143,237,206,175,238,207,239,
    16,48,17,80,49,18,112,81,50,19,113,82,51,114,83,115,640,672,641,704,673,642,736,705,674,643,737,706,675,738,707,739,
    516,548,517,580,549,518,612,581,550,519,613,582,551,614,583,615,392,424,393,456,425,394,488,457,426,395,489,458,427,490,459,491,
    268,300,269,332,301,270,364,333,302,271,365,334,303,366,335,367,144,176,145,208,177,146,240,209,178,147,241,210,179,242,211,243,
    20,52,21,84,53,22,116,85,54,23,117,86,55,118,87,119,768,800,769,832,801,770,864,833,802,771,865,834,803,866,835,867,
    644,676,645,708,677,646,740,709,678,647,741,710,679,742,711,743,520,552,521,584,553,522,616,585,554,523,617,586,555,618,587,619,
    396,428,397,460,429,398,492,461,430,399,493,462,431,494,463,495,272,304,273,336,305,274,368,337,306,275,369,338,307,370,339,371,
    148,180,149,212,181,150,244,213,182,151,245,214,183,246,215,247,24,56,25,88,57,26,120,89,58,27,121,90,59,122,91,123,
    896,928,897,960,929,898,992,961,930,899,993,962,931,994,963,995,772,804,773,836,805,774,868,837,806,775,869,838,807,870,839,871,
    648,680,649,712,681,650,744,713,682,651,745,714,683,746,715,747,524,556,525,588,557,526,620,589,558,527,621,590,559,622,591,623,
    400,432,401,464,433,402,496,465,434,403,497,466,435,498,467,499,276,308,277,340,309,278,372,341,310,279,373,342,311,374,343,375,
    152,184,153,216,185,154,248,217,186,155,249,218,187,250,219,251,28,60,29,92,61,30,124,93,62,31,125,94,63,126,95,127,
    900,932,901,964,933,902,996,965,934,903,997,966,935,998,967,999,776,808,777,840,809,778,872,841,810,779,873,842,811,874,843,875,
    652,684,653,716,685,654,748,717,686,655,749,718,687,750,719,751,528,560,529,592,561,530,624,593,562,531,625,594,563,626,595,627,
    404,436,405,468,437,406,500,469,438,407,501,470,439,502,471,503,280,312,281,344,313,282,376,345,314,283,377,346,315,378,347,379,
    156,188,157,220,189,158,252,221,190,159,253,222,191,254,223,255,904,936,905,968,937,906,1000,969,938,907,1001,970,939,1002,971,1003,
    780,812,781,844,813,782,876,845,814,783,877,846,815,878,847,879,656,688,657,720,689,658,752,721,690,659,753,722,691,754,723,755,
    532,564,533,596,565,534,628,597,566,535,629,598,567,630,599,631,408,440,409,472,441,410,504,473,442,411,505,474,443,506,475,507,
    284,316,285,348,317,286,380,349,318,287,381,350,319,382,351,383,908,940,909,972,941,910,1004,973,942,911,1005,974,943,1006,975,1007,
    784,816,785,848,817,786,880,849,818,787,881,850,819,882,851,883,660,692,661,724,693,662,756,725,694,663,757,726,695,758,727,759,
    536,568,537,600,569,538,632,601,570,539,633,602,571,634,603,635,412,444,413,476,445,414,508,477,446,415,509,478,447,510,479,511,
    912,944,913,976,945,914,1008,977,946,915,1009,978,947,1010,979,1011,788,820,789,852,821,790,884,853,822,791,885,854,823,886,855,887,
    664,696,665,728,697,666,760,729,698,667,761,730,699,762,731,763,540,572,541,604,573,542,636,605,574,543,637,606,575,638,607,639,
    916,948,917,980,949,918,1012,981,950,919,1013,982,951,1014,983,1015,792,824,793,856,825,794,888,857,826,795,889,858,827,890,859,891,
    668,700,669,732,701,670,764,733,702,671,765,734,703,766,735,767,920,952,921,984,953,922,1016,985,954,923,1017,986,955,1018,987,1019,
    796,828,797,860,829,798,892,861,830,799,893,862,831,894,863,895,924,956,925,988,957,926,1020,989,958,927,1021,990,959,1022,991,1023
};

const uint16_t* const g_scanOrder[NUM_SCAN_TYPE][NUM_SCAN_SIZE] =
{
    { g_scan4x4[0], g_scan8x8[0], g_scan16x16, g_scan32x32 },
    { g_scan4x4[1], g_scan8x8[1], g_scan16x16, g_scan32x32 },
    { g_scan4x4[2], g_scan8x8[2], g_scan16x16, g_scan32x32 }
};

const uint16_t* const g_scanOrderCG[NUM_SCAN_TYPE][NUM_SCAN_SIZE] =
{
    { g_scan4x4[0], g_scan2x2[0], g_scan4x4[0], g_scan8x8diag },
    { g_scan4x4[1], g_scan2x2[1], g_scan4x4[0], g_scan8x8diag },
    { g_scan4x4[2], g_scan2x2[0], g_scan4x4[0], g_scan8x8diag }
};

const uint8_t g_minInGroup[10] = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 24 };

// Rice parameters for absolute transform levels
const uint8_t g_goRiceRange[5] = { 7, 14, 26, 46, 78 };

const int g_winUnitX[] = { 1, 2, 2, 1 };
const int g_winUnitY[] = { 1, 2, 1, 1 };

const uint8_t g_lpsTable[64][4] =
{
    { 128, 176, 208, 240 },
    { 128, 167, 197, 227 },
    { 128, 158, 187, 216 },
    { 123, 150, 178, 205 },
    { 116, 142, 169, 195 },
    { 111, 135, 160, 185 },
    { 105, 128, 152, 175 },
    { 100, 122, 144, 166 },
    {  95, 116, 137, 158 },
    {  90, 110, 130, 150 },
    {  85, 104, 123, 142 },
    {  81,  99, 117, 135 },
    {  77,  94, 111, 128 },
    {  73,  89, 105, 122 },
    {  69,  85, 100, 116 },
    {  66,  80,  95, 110 },
    {  62,  76,  90, 104 },
    {  59,  72,  86,  99 },
    {  56,  69,  81,  94 },
    {  53,  65,  77,  89 },
    {  51,  62,  73,  85 },
    {  48,  59,  69,  80 },
    {  46,  56,  66,  76 },
    {  43,  53,  63,  72 },
    {  41,  50,  59,  69 },
    {  39,  48,  56,  65 },
    {  37,  45,  54,  62 },
    {  35,  43,  51,  59 },
    {  33,  41,  48,  56 },
    {  32,  39,  46,  53 },
    {  30,  37,  43,  50 },
    {  29,  35,  41,  48 },
    {  27,  33,  39,  45 },
    {  26,  31,  37,  43 },
    {  24,  30,  35,  41 },
    {  23,  28,  33,  39 },
    {  22,  27,  32,  37 },
    {  21,  26,  30,  35 },
    {  20,  24,  29,  33 },
    {  19,  23,  27,  31 },
    {  18,  22,  26,  30 },
    {  17,  21,  25,  28 },
    {  16,  20,  23,  27 },
    {  15,  19,  22,  25 },
    {  14,  18,  21,  24 },
    {  14,  17,  20,  23 },
    {  13,  16,  19,  22 },
    {  12,  15,  18,  21 },
    {  12,  14,  17,  20 },
    {  11,  14,  16,  19 },
    {  11,  13,  15,  18 },
    {  10,  12,  15,  17 },
    {  10,  12,  14,  16 },
    {   9,  11,  13,  15 },
    {   9,  11,  12,  14 },
    {   8,  10,  12,  14 },
    {   8,   9,  11,  13 },
    {   7,   9,  11,  12 },
    {   7,   9,  10,  12 },
    {   7,   8,  10,  11 },
    {   6,   8,   9,  11 },
    {   6,   7,   9,  10 },
    {   6,   7,   8,   9 },
    {   2,   2,   2,   2 }
};

const uint8_t x265_exp2_lut[64] =
{
    0,  3,  6,  8,  11, 14,  17,  20,  23,  26,  29,  32,  36,  39,  42,  45,
    48,  52,  55,  58,  62,  65,  69,  72,  76,  80,  83,  87,  91,  94,  98,  102,
    106,  110,  114,  118,  122,  126,  130,  135,  139,  143,  147,  152,  156,  161,  165,  170,
    175,  179,  184,  189,  194,  198,  203,  208,  214,  219,  224,  229,  234,  240,  245,  250
};
}
//! \}
