/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2024, RTZ Professional Audio, LLC
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

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/UART.h>

/* Standard C library header files */
#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

/* STC1200 Board Header file */
#include "STC1200.h"
#include "Board.h"
#include "SMPTE.h"

/* Default AT45DB parameters structure */
const SMPTE_Params SMPTE_defaultParams = {
    NULL,
    0
};

/* Static Data Items */
static SMPTE_Handle g_smpteHandle;

static TAPETIME tapeTime;

/* Static Function Prototypes */
static bool SMPTE_Write(uint16_t opcode);
static bool SMPTE_Read(uint16_t opcode, uint16_t *result);
static Void gpioSMPTEHwi(unsigned int index);

Void gpioSMPTESwi(UArg arg0, UArg arg1);

//*****************************************************************************
// SMPTE Controller Construction/Destruction
//*****************************************************************************

SMPTE_Handle SMPTE_construct(SMPTE_Object *obj, SMPTE_Params *params)
{
    /* Initialize the object's fields */
    obj->spiHandle = params->spiHandle;
    obj->gpioCS    = params->gpioCS;

    GateMutex_construct(&(obj->gate), NULL);

    return (SMPTE_Handle)obj;
}

SMPTE_Handle SMPTE_create(SMPTE_Params *params)
{
    SMPTE_Handle handle;
    Error_Block eb;

    Error_init(&eb);

    handle = Memory_alloc(NULL, sizeof(SMPTE_Object), NULL, &eb);

    if (handle == NULL)
        return NULL;

    handle = SMPTE_construct(handle, params);

    return handle;
}

Void SMPTE_delete(SMPTE_Handle handle)
{
    SMPTE_destruct(handle);

    Memory_free(NULL, handle, sizeof(SMPTE_Object));
}

Void SMPTE_destruct(SMPTE_Handle handle)
{
    Assert_isTrue((handle != NULL), NULL);

    GateMutex_destruct(&(handle->gate));
}

Void SMPTE_Params_init(SMPTE_Params *params)
{
    Assert_isTrue(params != NULL, NULL);

    *params = SMPTE_defaultParams;
}

//*****************************************************************************
// Initialize SPI0 to expansion connector for SMPTE daughter card
//*****************************************************************************

extern Swi_Handle mySwi;

bool SMPTE_init(void)
{
    SPI_Handle spiHandle;
    SPI_Params spiParams;
    SMPTE_Params smpteParams;

    /* 1 Mhz, Moto fmt, polarity 1, phase 0 */
    SPI_Params_init(&spiParams);

    spiParams.transferMode  = SPI_MODE_BLOCKING;
    spiParams.mode          = SPI_MASTER;
    spiParams.frameFormat   = SPI_POL1_PHA0;
    spiParams.bitRate       = 250000;
    spiParams.dataSize      = 16;

    spiHandle = SPI_open(Board_SPI_EXPIO_SMPTE, &spiParams);

    if (spiHandle == NULL)
        System_abort("Error initializing SPI0\n");

    /* Create the SMPTE object handle */
    SMPTE_Params_init(&smpteParams);

    smpteParams.spiHandle = spiHandle;
    smpteParams.gpioCS    = 0;  //Board_SMPTE_FS;

    g_smpteHandle = SMPTE_create(&smpteParams);

    /* Setup the GPIO pin interrupt handler and enable it */
    GPIO_setCallback(Board_SMPTE_INT_N, gpioSMPTEHwi);

	return true;
}

/* The SMPTE board INT pin handler gets called when the pin goes low. This indicates
 * the SMPTE board has decoded a time packet and the data is available. So, we queue
 * up a software interrupt to read the data and exit the hardware interrupt as
 * quickly as possible. The SWI will execute at a higher priority than task threads
 * and the data will be delivered at that point. THe SWI serializes the SPI port
 * between decode interrupts and other tasks sending commands to the SMPTE
 * board processor.
 */
Void gpioSMPTEHwi(unsigned int index)
{
    /* INT pin went low, trigger swi to handle it */
    Swi_post(mySwi);
}

/* The SWI handles the SPI bus communication with gate mutex protection
 * from other tasks that might be making calls to the SPI module.
 */
Void gpioSMPTESwi(UArg arg0, UArg arg1)
{
    uint16_t txbuf[4];
    uint16_t rxbuf[4];
    SPI_Transaction transaction;
    IArg key;

    /* Serialize access to SMPTE controller */
    key = GateMutex_enter(GateMutex_handle(&(g_smpteHandle->gate)));
#if 0
    /* Set the read flag to send response */
    txbuf[0] = SMPTE_REG_SET(SMPTE_REG_DATA) | SMPTE_F_READ;
    rxbuf[0] = 0;

    /* Send the command */
    transaction.count = 3;
    transaction.txBuf = (Ptr)&txbuf[0];
    transaction.rxBuf = (Ptr)&rxbuf[0];

    /* Send the SPI transaction */
    //GPIO_write(Board_SMPTE_FS, PIN_LOW);
    SPI_transfer(g_smpteHandle->spiHandle, &transaction);
    //GPIO_write(Board_SMPTE_FS, PIN_HIGH);


    rxbuf[1] = rxbuf[2] = rxbuf[3] = 0;

    /* Send the command */
    transaction.count = 3;
    transaction.txBuf = (Ptr)&txbuf[1];
    transaction.rxBuf = (Ptr)&rxbuf[1];

    /* Send the SPI transaction */
    //GPIO_write(Board_SMPTE_FS, PIN_LOW);
    SPI_transfer(g_smpteHandle->spiHandle, &transaction);
    //GPIO_write(Board_SMPTE_FS, PIN_HIGH);


    /* Pull out time members into local struct buffer */
    tapeTime.flags = (uint8_t)(rxbuf[0] & 0xFF);
    tapeTime.tens  = 0;
    tapeTime.frame = (uint8_t)((rxbuf[1]) & 0xFF);
    tapeTime.secs  = (uint8_t)((rxbuf[1] >> 8) & 0xFF);
    tapeTime.mins  = (uint8_t)((rxbuf[2]) & 0xFF);
    tapeTime.hour  = (uint8_t)((rxbuf[2] >> 8) & 0xFF);
#endif
    /* Leave thread safe access to SMPTE controller */
    GateMutex_leave(GateMutex_handle(&(g_smpteHandle->gate)), key);
}

//*****************************************************************************
// Read/Write a command to the SMPTE slave board
//*****************************************************************************

static bool SMPTE_Write(uint16_t opcode)
{
    bool success;
    uint16_t reply = 0;
    SPI_Transaction transaction;
    IArg key;

    /* Serialize access to SMPTE controller */
    key = GateMutex_enter(GateMutex_handle(&(g_smpteHandle->gate)));

    transaction.count = 1;
    transaction.txBuf = (Ptr)&opcode;
    transaction.rxBuf = (Ptr)&reply;

    /* Assert the SPI chip select */
    //GPIO_write(Board_SMPTE_FS, PIN_LOW);

    /* Send the SPI transaction */
    success = SPI_transfer(g_smpteHandle->spiHandle, &transaction);

    /* Release the chip select to high */
    //GPIO_write(Board_SMPTE_FS, PIN_HIGH);

    /* Leave safe access to SMPTE controller */
    GateMutex_leave(GateMutex_handle(&(g_smpteHandle->gate)), key);

    return success;
}

static bool SMPTE_Read(uint16_t opcode, uint16_t *result)
{
    bool success;
    uint16_t txbuf[2];
    uint16_t rxbuf[2];
    SPI_Transaction transaction;
    IArg key;

    /* Serialize access to SMPTE controller */
    key = GateMutex_enter(GateMutex_handle(&(g_smpteHandle->gate)));

    /* Set the read flag to send response */
    txbuf[0] = opcode | SMPTE_F_READ;
    rxbuf[0] = 0;

    /* Send the command */
    transaction.count = 1;
    transaction.txBuf = (Ptr)&txbuf[0];
    transaction.rxBuf = (Ptr)&rxbuf[0];

    /* Send the SPI transaction */
    //GPIO_write(Board_SMPTE_FS, PIN_LOW);
    success = SPI_transfer(g_smpteHandle->spiHandle, &transaction);
    //GPIO_write(Board_SMPTE_FS, PIN_HIGH);

    if (success)
        *result = rxbuf[0];

    /* Leave thread safe access to SMPTE controller */
    GateMutex_leave(GateMutex_handle(&(g_smpteHandle->gate)), key);

    return success;
}

//*****************************************************************************
// SMPTE Controller Commands
//*****************************************************************************

/* Test for presence of SMPTE controller daughter card */

bool SMPTE_probe(void)
{
    uint16_t cmd;
    uint16_t revid = 0;

    cmd = SMPTE_REG_SET(SMPTE_REG_REVID);

    if (!SMPTE_Read(cmd, &revid))
        return false;

    return (revid == SMPTE_REVID) ? true : false;
}

/* Get SMPTE module firmware version */

bool SMPTE_get_revid(uint16_t *revid)
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_REVID);

    if (!SMPTE_Read(cmd, revid))
        return false;

    return true;
}

//*****************************************************************************
// SMPTE Encoder Commands
//*****************************************************************************


bool SMPTE_encoder_start(bool reset)
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_ENCCTL) |
          SMPTE_ENCCTL_FPS(g_sys.cfgSTC.smpteFPS) |
          SMPTE_ENCCTL_ENABLE;

    /* Reset start time to zero if reset flag specified,
     * otherwise resume counting at the last stop time.
     */
    if (reset)
        cmd |=  SMPTE_ENCCTL_RESET;

    g_sys.smpteMode = STC_SMPTE_ENCODER;

    return SMPTE_Write(cmd);
}

bool SMPTE_encoder_stop()
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_ENCCTL) |
          SMPTE_ENCCTL_DISABLE;

    g_sys.smpteMode = STC_SMPTE_OFF;

    return SMPTE_Write(cmd);
}

/* Set the encoder starting time in HH:MM:SS:Frame.
 * The encoder must not be running when calling this.
 */
bool SMPTE_encoder_set_time(uint8_t hours, uint8_t mins,
                              uint8_t secs, uint8_t frame)
{
    uint16_t cmd;

    /* Encoder must be stopped before changing the time */
    SMPTE_encoder_stop();

    /* Set the hours register */
    cmd = SMPTE_REG_SET(SMPTE_REG_HOUR) | SMPTE_DATA_SET(hours);
    SMPTE_Write(cmd);

    /* Set the minutes register */
    cmd = SMPTE_REG_SET(SMPTE_REG_MINS) | SMPTE_DATA_SET(mins);
    SMPTE_Write(cmd);

    /* Set the seconds register */
    cmd = SMPTE_REG_SET(SMPTE_REG_SECS) | SMPTE_DATA_SET(secs);
    SMPTE_Write(cmd);

    /* Set the frame register */
    cmd = SMPTE_REG_SET(SMPTE_REG_FRAME) | SMPTE_DATA_SET(frame);
    SMPTE_Write(cmd);

   return true;
}

//*****************************************************************************
// SMPTE Decoder Commands
//*****************************************************************************

bool SMPTE_decoder_start(void)
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_DECCTL) |
          SMPTE_DECCTL_FPS(g_sys.cfgSTC.smpteFPS) |
          SMPTE_DECCTL_INT |
          SMPTE_DECCTL_ENABLE;

    GPIO_enableInt(Board_SMPTE_INT_N);

    return SMPTE_Write(cmd);
}

bool SMPTE_decoder_stop(void)
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_DECCTL) |
          SMPTE_DECCTL_DISABLE;

    GPIO_disableInt(Board_SMPTE_INT_N);

    return SMPTE_Write(cmd);
}

/* End-Of-File */
