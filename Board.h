/*
 * Copyright (c) 2014, Texas Instruments Incorporated
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

#ifndef __BOARD_H
#define __BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "STC1200_TM4C1294NCPDT.h"

#define Board_initEMAC              STC1200_initEMAC
#define Board_initGeneral           STC1200_initGeneral
#define Board_initGPIO              STC1200_initGPIO
#define Board_initI2C               STC1200_initI2C
#define Board_initPWM               STC1200_initPWM
#define Board_initSDSPI             STC1200_initSDSPI
#define Board_initSPI               STC1200_initSPI
#define Board_initUART              STC1200_initUART
#define Board_initUSB               STC1200_initUSB
#define Board_initUSBMSCHFatFs      STC1200_initUSBMSCHFatFs
#define Board_initWatchdog          STC1200_initWatchdog
#define Board_initWiFi              STC1200_initWiFi
#define Board_initQEI               STC1200_initQEI

#define Board_LED_ON                STC1200_LED_ON
#define Board_LED_OFF               STC1200_LED_OFF

#define Board_LAMP_ON               STC1200_LAMP_ON
#define Board_LAMP_OFF              STC1200_LAMP_OFF

#define Board_I2C_AT24MAC402        STC1200_I2C0
#define Board_I2C_RTC               STC1200_I2C0

#define Board_SPI_SDCARD          	STC1200_SDSPI1
#define Board_SPI_S25FL127          STC1200_SPI2
#define Board_SPI_EXPIO_SMPTE  	    STC1200_SPI0
#define Board_SPI_EXPIO_AD9837      STC1200_SPI3

#if (STC1200_HARDWARE_REV < 2)
/* REV-A Hardware */
#define Board_UART_RS232_DEBUG      STC1200_UART0
#define Board_UART_RS232_COM1       STC1200_UART1
#define Board_UART_RS232_COM2       STC1200_UART2
#define Board_UART_BLUETOOTH        STC1200_UART3
#define Board_UART_MIDI             STC1200_UART4
#define Board_UART_RS422_REMOTE     STC1200_UART5
#define Board_UART_ATMEGA88         STC1200_UART6
#define Board_UART_IPC_A            STC1200_UART7
#else
/* REV-B Hardware */
#define Board_UART_IPC_A            STC1200_UART7               /* IPC commands to/from DTC */
#define Board_UART_IPC_B            STC1200_UART0               /* IPC commands from DTC (future) */
#define Board_UART_RS232_COM1       STC1200_UART1               /* COM1 = RS232 TTY console port  */
#define Board_UART_RS232_COM2       STC1200_UART2               /* COM2 = RS232 to DCS channel switcher */
#define Board_UART_RS422_SPARE      STC1200_UART3               /* spare RS-422 port */
#define Board_UART_RS422_REMOTE     STC1200_UART5               /* RS-422 to DRC-1200 remote */
#define Board_UART_MIDI             STC1200_UART4               /* MIDI MMC port */
#define Board_UART_ATMEGA88         STC1200_UART6               /* ATMega 7-segment display driver */
#endif

#define Board_WATCHDOG0             STC1200_WATCHDOG0

#define Board_WIFI                  STC1200_WIFI

/* GPIO Pin Definitions */

#define Board_RESET_AVR_N			STC1200_RESET_AVR_N
#define Board_RESET_BL652			STC1200_RESET_BL652
#define Board_AIRLOAD				STC1200_AIRLOAD
#define Board_AUTORUN_N				STC1200_AUTORUN_N
#define Board_STAT_LED				STC1200_STAT_LED
#define Board_LAMP_PLAY				STC1200_LAMP_PLAY
#define Board_LAMP_FWDREW			STC1200_LAMP_FWDREW

/* I/O Expansion Port to SMPTE daughter card */
#define Board_AD9732_FSYNC          STC1200_EXPIO_PF2_SSI3FS
#define Board_SMPTE_FS              STC1200_EXPIO_PA3_SSI0FS
#define Board_SMPTE_CHANGE          STC1200_EXPIO_PP5
#define Board_SMPTE_FRAMESYNC       STC1200_EXPIO_PP3
#define Board_SMPTE_DIRECTION       STC1200_EXPIO_PP2
#define Board_SMPTE_BOOTLOAD        STC1200_EXPIO_PD4
#define Board_SMPTE_RESET_N         STC1200_EXPIO_PD5
#define Board_SMPTE_INT_N           STC1200_EXPIO_PK3
/* Not used on SMPTE expansion connector */
#define Board_EXPIO_PP4             STC1200_EXPIO_PP4
#define Board_EXPIO_PK2             STC1200_EXPIO_PK2
#define Board_EXPIO_PD5             STC1200_EXPIO_PD5

#define Board_RS422_RE_N			STC1200_RS422_RE_N
#define Board_RS422_DE				STC1200_RS422_DE

#define Board_BTN_RESET				STC1200_BTN_RESET
#define Board_BTN_CUE				STC1200_BTN_CUE
#define Board_BTN_SEARCH			STC1200_BTN_SEARCH

#define Board_STOP_DETECT_N			STC1200_STOP_DETECT_N
#define Board_PLAY_DETECT_N			STC1200_PLAY_DETECT_N

#define Board_STOP_N				STC1200_STOP_N
#define Board_PLAY_N				STC1200_PLAY_N
#define Board_FWD_N					STC1200_FWD_N
#define Board_REW_N					STC1200_REW_N

#define Board_DIPSW_CFG1			STC1200_DIPSW_CFG1
#define Board_DIPSW_CFG2			STC1200_DIPSW_CFG2

#define Board_MOTION_REW			STC1200_MOTION_REW
#define Board_MOTION_FWD			STC1200_MOTION_FWD

#define Board_SPEED_SELECT			STC1200_SPEED_SELECT
#define Board_SEARCHING				STC1200_SEARCHING
#define Board_TAPE_DIR				STC1200_TAPE_DIR

#define Board_gpioButtonCallbacks	STC1200_gpioPortMCallbacks

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H */
