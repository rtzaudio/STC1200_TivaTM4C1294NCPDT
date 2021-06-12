/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
 *
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

/*
 *    ======== tcpEcho.c ========
 *    Contains BSD sockets code.
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* NDK BSD support */
#include <sys/socket.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Tivaware Driver files */
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <driverlib/eeprom.h>
#include <driverlib/flash.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/sysctl.h>
#include <driverlib/fpu.h>
#include <grlib/grlib.h>
#include <IPCServer.h>
#include <RAMPServer.h>
#include <RemoteTask.h>
#include <MidiTask.h>
#include "drivers/offscrmono.h"

/* STC1200 Board Header file */
#include "Board.h"
#include "STC1200.h"
#include "STC1200TCP.h"
#include "IPCCommands.h"
#include "IPCMessage.h"
#include "Utils.h"
#include "SMPTE.h"

//*****************************************************************************
// This function reads the unique 128-serial number and 48-bit MAC address
// via I2C from the AT24MAC402 serial EPROM.
//*****************************************************************************

bool ReadGUIDS(I2C_Handle handle, uint8_t ui8SerialNumber[16], uint8_t ui8MAC[6])
{
    bool            ret;
    uint8_t         txByte;
    I2C_Transaction i2cTransaction;

    /* default is all FF's  in case read fails*/
    memset(ui8SerialNumber, 0xFF, 16);
    memset(ui8MAC, 0xFF, 6);

    /* Note the Upper bit of the word address must be set
     * in order to read the serial number. Thus 80H would
     * set the starting address to zero prior to reading
     * this sixteen bytes of serial number data.
     */

    txByte = 0x80;

    i2cTransaction.slaveAddress = AT24MAC_EPROM_EXT_ADDR;
    i2cTransaction.writeBuf     = &txByte;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = ui8SerialNumber;
    i2cTransaction.readCount    = 16;

    ret = I2C_transfer(handle, &i2cTransaction);

    if (!ret)
    {
        System_printf("Unsuccessful I2C transfer\n");
        System_flush();
        return false;
    }

    /* Now read the 6-byte 48-bit MAC at address 0x9A. The EUI-48 address
     * contains six or eight bytes. The first three bytes of the  UI read-only
     * address field are called the Organizationally Unique Identifier (OUI)
     * and the IEEE Registration Authority has assigned FCC23Dh as the Atmel OUI.
     */

    txByte = 0x9A;

    i2cTransaction.slaveAddress = AT24MAC_EPROM_EXT_ADDR;
    i2cTransaction.writeBuf     = &txByte;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = ui8MAC;
    i2cTransaction.readCount    = 6;

    ret = I2C_transfer(handle, &i2cTransaction);

    if (!ret)
    {
        System_printf("Unsuccessful I2C transfer\n");
        System_flush();
        return false;
    }

    return true;
}

//*****************************************************************************
// Set default runtime values
//*****************************************************************************

void InitSysDefaults(SYSCFG* p)
{
    /** Default servo parameters **/
    p->version      = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->build        = FIRMWARE_BUILD;
    p->debug        = 0;                    /* debug mode 0=off             */
    p->searchBlink  = TRUE;
    /** Remote Parameters **/
    p->showLongTime = FALSE;
    /** Locator Parameters **/
    p->jog_vel_far  = JOG_VEL_FAR;          /* 0 for DTC default velocity   */
    p->jog_vel_mid  = JOG_VEL_MID;          /* vel for mid distance locate  */
    p->jog_vel_near = JOG_VEL_NEAR;         /* vel for near distance locate */
    /** Master Reference Clock */
    p->ref_freq     = REF_FREQ;             /* default ref clock 9600.0 Hz  */
    p->tapeSpeed    = 30;                   /* default tape speed high      */
    /** SMPE card config */
    p->smpteFPS     = SMPTE_ENCCTL_FPS30;

    /* Initial track state zero for all channels */
    memset(p->trackState, 0, STC_MAX_TRACKS);
}

//*****************************************************************************
// Write system parameters from our global settings buffer to EEPROM.
//
// Returns:  0 = Success
//          -1 = Error writing EEPROM data
//*****************************************************************************

int SysParamsWrite(SYSCFG* sp)
{
    int32_t rc = 0;

    /* Initialize the version, build# and magic# */
    sp->version = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    sp->build   = FIRMWARE_BUILD;
    sp->magic   = MAGIC;

    /* Store the configuration parameters to EPROM */
    rc = EEPROMProgram((uint32_t *)sp, 0, sizeof(SYSCFG));

    System_printf("Writing System Parameters %d\n", rc);
    System_flush();

    return rc;
 }

//*****************************************************************************
// Read system parameters into our global settings buffer from EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error reading flash
//
//*****************************************************************************

int SysParamsRead(SYSCFG* sp)
{
    InitSysDefaults(sp);

    /* Read the configuration parameters from EPROM */
    EEPROMRead((uint32_t *)sp, 0, sizeof(SYSCFG));

    /* Does the magic number match? If not, set defaults and
     * store to initialize the system default parameters.
     */
    if (sp->magic != MAGIC)
    {
        System_printf("ERROR Reading System Parameters - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    /* If firmware is different version, the reset system defaults
     * and store as system default parameters.
     */
    if (sp->version != MAKEREV(FIRMWARE_VER, FIRMWARE_REV))
    {
        System_printf("WARNING New Firmware Version - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    /* If stored build number is less that minimum build number required,
     * then reset and store system defaults. This is to avoid loading old
     * configuration parameters store from an earlier build version.
     */
    if (sp->build < FIRMWARE_MIN_BUILD)
    {
        System_printf("WARNING New Firmware BUILD - Resetting Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    return 0;
}

//*****************************************************************************
// Helper Functions
//*****************************************************************************

#if 0
int GetHexStr(char* textbuf, uint8_t* databuf, int datalen)
{
    char *p = textbuf;
    uint8_t *d;
    uint32_t i;
    int32_t l;

    const uint32_t wordSize = 4;

    /* Null output text buffer initially */
    *textbuf = 0;

    /* Make sure buffer length is not zero */
    if (!datalen)
        return 0;

    /* Read data bytes in reverse order so we print most significant byte first */
    d = databuf + (datalen-1);

    for (i=0; i < datalen; i++)
    {
        l = sprintf(p, "%02X", *d--);
        p += l;

        if (((i % wordSize) == (wordSize-1)) && (i != (datalen-1)))
        {
            l = sprintf(p, "-");
            p += l;
        }
    }

    return strlen(textbuf);
}
#else
int GetHexStr(char* textbuf, uint8_t* databuf, int datalen)
{
    char fmt[8];
    uint32_t i;
    int32_t l;

    const uint32_t wordSize = 4;

    *textbuf = 0;
    strcpy(fmt, "%02X");

    for (i=0; i < datalen; i++)
    {
        l = sprintf(textbuf, fmt, *databuf++);
        textbuf += l;

        if (((i % wordSize) == (wordSize-1)) && (i != (datalen-1)))
        {
            l = sprintf(textbuf, "-");
            textbuf += l;
        }
    }

    return strlen(textbuf);
}
#endif

//*****************************************************************************
//
//*****************************************************************************

#if 0
void BurnMACAddress(void)
{
    uint32_t ulUser0, ulUser1;

    /* Get the MAC address */
    FlashUserGet(&ulUser0, &ulUser1);

    if ((ulUser0 == 0xffffffff) && (ulUser1 == 0xffffffff))
    {
        /* Combine MAC address into two 32-bit words */
        ulUser0 = ((((uint32_t)g_sysData.ui8MAC[0] & 0xff) << 0)) |
                  ((((uint32_t)g_sysData.ui8MAC[1] & 0xff) << 8)) |
                  ((((uint32_t)g_sysData.ui8MAC[2] & 0xff) << 16));

        ulUser1 = ((((uint32_t)g_sysData.ui8MAC[3] & 0xff) << 0)) |
                  ((((uint32_t)g_sysData.ui8MAC[4] & 0xff) << 8)) |
                  ((((uint32_t)g_sysData.ui8MAC[5] & 0xff) << 16));

        System_printf("Updating MAC address in user flash!\n");
        System_flush();

        /* Save the two MAC address words into the special user
         * flash area. There are four words available, but we only
         * need the first two words to store the six byte MAC address.
         */
        if (!FlashUserSet(ulUser0, ulUser1))
        {
            System_printf("FlashUserSet failed updating MAC address!\n");
            System_flush();
        }

        /* NOTE - THIS IS A ONE TIME PERMANENT WRITE OPERATION!!! */
        if (!FlashUserSave())
        {
            System_printf("FlashUserSave failed updating MAC address!\n");
            System_flush();
        }

        System_printf("MAC ADDRESS PERMANENTLY WRITTEN TO USER FLASH!\n");
        System_flush();

        /* REBOOT BY JUMPING TO BOOTLOADER! */
        SysCtlReset();
    }
}
#endif

// End-Of-File
