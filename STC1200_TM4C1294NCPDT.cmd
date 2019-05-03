/*
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Settings for standard non-boot loader compilation. Reset vectors at zero.
 */

//#define APP_BASE 0x00000000
//#define APP_LENG 0x00100000

/*
 * Settings for use with bootloader compilation (app base is offset, app length is smaller)
 *
 * NOTE - for TI-RTOS you must also edit the XDC CFG file and add the following
 *
 *     Program.sectMap[".resetVecs"].loadAddress = 4096;
 *
 * The APP_BASE and the resetVecs parameters must match for the bootloader to enter
 * our application snf interrupt vectors at the proper address. We've allowed
 * 4k space for our bootloader and the application starts at this offset.
 */

#define	APP_BASE 0x00004000
#define	APP_LENG 0x000FC000

#define	RAM_BASE 0x20000000
#define RAM_LENG 0x00040000

MEMORY
{
    /* BOOT (RX)  : origin = 0x00000000, length = 0x00001000 */
    FLASH (RX) : origin = APP_BASE, length = APP_LENG
    SRAM (RWX) : origin = RAM_BASE, length = RAM_LENG
}

/* Section allocation in memory */

SECTIONS
{
    .text   :   > FLASH
#ifdef __TI_COMPILER_VERSION
#if __TI_COMPILER_VERSION >= 15009000
    .TI.ramfunc : {} load=FLASH, run=SRAM, table(BINIT)
#endif
#endif
    .const  :   > FLASH
    .cinit  :   > FLASH
    .pinit  :   > FLASH
    .init_array : > FLASH

    .data   :   > SRAM
    .bss    :   > SRAM
    .sysmem :   > SRAM
    .stack  :   > SRAM
}
