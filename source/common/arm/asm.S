/*****************************************************************************
 * asm.S: arm utility macros
 *****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Mans Rullgard <mans@mansr.com>
 *          David Conrad <lessen42@gmail.com>
 *          Dnyaneshwar Gorade <dnyaneshwar@multicorewareinc.com>
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
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

.syntax unified

#ifdef __APPLE__
#define PREFIX 1
#endif

#define PFX3(prefix, name) prefix ## name
#define PFX(prefix, name) PFX3(EXTERN_ASM, name)

#if   HAVE_NEON
        .arch           armv7-a
#elif HAVE_ARMV6T2
        .arch           armv6t2
#elif HAVE_ARMV6
        .arch           armv6
#endif

.fpu neon

#ifdef PREFIX
#   define EXTERN_ASM _
#else
#   define EXTERN_ASM
#endif

#ifdef __ELF__
#   define ELF
#else
#   define ELF @
#endif

#if HAVE_AS_FUNC
#   define FUNC
#else
#   define FUNC @
#endif

.macro require8, val=1
ELF     .eabi_attribute 24, \val
.endm

.macro preserve8, val=1
ELF     .eabi_attribute 25, \val
.endm

.macro function name, export=1
    .macro endfunc
ELF     .size   \name, . - \name
FUNC    .endfunc
        .purgem endfunc
    .endm
        .align  2
.if \export == 1
        .global EXTERN_ASM\name
ELF     .hidden EXTERN_ASM\name
ELF     .type   EXTERN_ASM\name, %function
FUNC    .func   EXTERN_ASM\name
EXTERN_ASM\name:
.else
ELF     .hidden \name
ELF     .type   \name, %function
FUNC    .func   \name
\name:
.endif
.endm

.macro movrel rd, val
#if HAVE_ARMV6T2 && !defined(PIC)
        movw            \rd, #:lower16:\val
        movt            \rd, #:upper16:\val
#else
        ldr             \rd, =\val
#endif
.endm

.macro movconst rd, val
#if HAVE_ARMV6T2
    movw        \rd, #:lower16:\val
.if \val >> 16
    movt        \rd, #:upper16:\val
.endif
#else
    ldr         \rd, =\val
#endif
.endm

#define GLUE(a, b) a ## b
#define JOIN(a, b) GLUE(a, b)
#define X(s) JOIN(EXTERN_ASM, s)

#define FENC_STRIDE 64
#define FDEC_STRIDE 32

.macro HORIZ_ADD dest, a, b
.ifnb \b
    vadd.u16    \a, \a, \b
.endif
    vpaddl.u16  \a, \a
    vpaddl.u32  \dest, \a
.endm

.macro SUMSUB_AB sum, diff, a, b
    vadd.s16    \sum,  \a, \b
    vsub.s16    \diff, \a, \b
.endm

.macro SUMSUB_ABCD s1, d1, s2, d2, a, b, c, d
    SUMSUB_AB   \s1, \d1, \a, \b
    SUMSUB_AB   \s2, \d2, \c, \d
.endm

.macro ABS2 a b
    vabs.s16 \a, \a
    vabs.s16 \b, \b
.endm

// dist = distance in elements (0 for vertical pass, 1/2 for horizontal passes)
// op = sumsub/amax (sum and diff / maximum of absolutes)
// d1/2 = destination registers
// s1/2 = source registers
.macro HADAMARD dist, op, d1, d2, s1, s2
.if \dist == 1
    vtrn.16     \s1, \s2
.else
    vtrn.32     \s1, \s2
.endif
.ifc \op, sumsub
    SUMSUB_AB   \d1, \d2, \s1, \s2
.else
    vabs.s16    \s1, \s1
    vabs.s16    \s2, \s2
    vmax.s16    \d1, \s1, \s2
.endif
.endm

.macro TRANSPOSE8x8 r0 r1 r2 r3 r4 r5 r6 r7
    vtrn.32         \r0, \r4
    vtrn.32         \r1, \r5
    vtrn.32         \r2, \r6
    vtrn.32         \r3, \r7
    vtrn.16         \r0, \r2
    vtrn.16         \r1, \r3
    vtrn.16         \r4, \r6
    vtrn.16         \r5, \r7
    vtrn.8          \r0, \r1
    vtrn.8          \r2, \r3
    vtrn.8          \r4, \r5
    vtrn.8          \r6, \r7
.endm

.macro TRANSPOSE4x4 r0 r1 r2 r3
    vtrn.16         \r0, \r2
    vtrn.16         \r1, \r3
    vtrn.8          \r0, \r1
    vtrn.8          \r2, \r3
.endm

.macro TRANSPOSE4x4_16  r0, r1, r2, r3
    vtrn.32     \r0, \r2            // r0 = [21 20 01 00], r2 = [23 22 03 02]
    vtrn.32     \r1, \r3            // r1 = [31 30 11 10], r3 = [33 32 13 12]
    vtrn.16     \r0, \r1            // r0 = [30 20 10 00], r1 = [31 21 11 01]
    vtrn.16     \r2, \r3            // r2 = [32 22 12 02], r3 = [33 23 13 03]
.endm

.macro TRANSPOSE4x4x2_16  rA0, rA1, rA2, rA3, rB0, rB1, rB2, rB3
    vtrn.32     \rA0, \rA2          // r0 = [21 20 01 00], r2 = [23 22 03 02]
    vtrn.32     \rA1, \rA3          // r1 = [31 30 11 10], r3 = [33 32 13 12]
    vtrn.32     \rB0, \rB2
    vtrn.32     \rB1, \rB3
    vtrn.16     \rA0, \rA1          // r0 = [30 20 10 00], r1 = [31 21 11 01]
    vtrn.16     \rA2, \rA3          // r2 = [32 22 12 02], r3 = [33 23 13 03]
    vtrn.16     \rB0, \rB1
    vtrn.16     \rB2, \rB3
.endm
