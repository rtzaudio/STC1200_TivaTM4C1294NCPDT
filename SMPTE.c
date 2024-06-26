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

static Semaphore_Handle g_smpteIntSemaphore;

/* Static Function Prototypes */
static bool SMPTE_WriteReg(uint16_t opcode);
static bool SMPTE_ReadReg(uint16_t opcode, uint16_t *result);
static Void gpioSMPTEHwi(unsigned int index);
static Void SMPTEReadTask(UArg arg0, UArg arg1);

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

bool SMPTE_init(void)
{
    SPI_Handle spiHandle;
    SPI_Params spiParams;
    SMPTE_Params smpteParams;
    Task_Params taskParams;
    Error_Block eb;

    /* 1 Mhz, Moto fmt, polarity 1, phase 0 */
    SPI_Params_init(&spiParams);

    spiParams.mode         = SPI_MASTER;
    spiParams.transferMode = SPI_MODE_BLOCKING;
    spiParams.frameFormat  = SPI_POL1_PHA0;
    spiParams.bitRate      = 1000000;   //250000;
    spiParams.dataSize     = 16;

    spiHandle = SPI_open(Board_SPI_EXPIO_SMPTE, &spiParams);

    /* Create the SMPTE object handle */
    SMPTE_Params_init(&smpteParams);

    smpteParams.spiHandle = spiHandle;
    smpteParams.gpioCS    = 0;

    g_smpteHandle = SMPTE_create(&smpteParams);

    /* Create semaphore for smpte interrupt signal */
    g_smpteIntSemaphore = Semaphore_create(0, NULL, NULL);

    /* Create interrupt read task */
    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 10;
    Task_create((Task_FuncPtr)SMPTEReadTask, &taskParams, &eb);

    /* Setup the GPIO pin interrupt handler and enable it */
    GPIO_setCallback(Board_SMPTE_INT_N, gpioSMPTEHwi);
    //GPIO_enableInt(Board_SMPTE_INT_N);

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
    /* wake up the SPI read task to read the SMPTE time code packet data */
    Semaphore_post(g_smpteIntSemaphore);
}

Void SMPTEReadTask(UArg arg0, UArg arg1)
{
    uint16_t txbuf[4];
    uint16_t rxbuf[4];
    uint16_t txbuf2[4];
    uint16_t rxbuf2[4];
    SPI_Transaction transaction1;
    SPI_Transaction transaction2;
    uint32_t key;

    while (TRUE)
    {
        /* Block until a semaphore is posted indicating a SMPTE
         * time code packet has been decoded and available to read.
         */
        Semaphore_pend(g_smpteIntSemaphore, BIOS_WAIT_FOREVER);

        /* Serialize access to SMPTE controller */
        key = GateMutex_enter(GateMutex_handle(&(g_smpteHandle->gate)));

        /*** Send the command ***/

        transaction1.count = 1;
        transaction1.txBuf = (Ptr)&txbuf[0];
        transaction1.rxBuf = (Ptr)&rxbuf[0];

        /* Set the read flag to send response */
        txbuf[0] = SMPTE_REG_SET(SMPTE_REG_DATA) | SMPTE_F_READ;
        rxbuf[0] = 0;

        /* Send the SPI transaction */
        SPI_transfer(g_smpteHandle->spiHandle, &transaction1);

        /*** Read the data ***/

        transaction2.count = 3;
        transaction2.txBuf = (Ptr)&txbuf2[0];
        transaction2.rxBuf = (Ptr)&rxbuf2[0];

        txbuf2[0] = txbuf2[2] = txbuf2[3] = 0;
        rxbuf2[1] = rxbuf2[2] = rxbuf2[3] = 0;

        /* Allow time for slave to enable its receiver */
        Task_sleep(6);

        /* Send the SPI transaction */
        SPI_transfer(g_smpteHandle->spiHandle, &transaction2);

        /* Pull out time members into global data buffer */
        key = Hwi_disable();
        g_sys.smpteTime.flags = (uint8_t)((rxbuf2[0]) & 0xFF);
        g_sys.smpteTime.frame = (uint8_t)((rxbuf2[1]) & 0xFF);
        g_sys.smpteTime.secs  = (uint8_t)((rxbuf2[1] >> 8) & 0xFF);
        g_sys.smpteTime.mins  = (uint8_t)((rxbuf2[2]) & 0xFF);
        g_sys.smpteTime.hour  = (uint8_t)((rxbuf2[2] >> 8) & 0xFF);
        g_sys.smpteTime.tens  = 0;
        Hwi_restore(key);

        /* Leave thread safe access to SMPTE controller */
        GateMutex_leave(GateMutex_handle(&(g_smpteHandle->gate)), key);

        //System_printf("%2.2u:%2.2u:%2.2u:%2.2u\n",
        //              g_sys.smpteTime.hour, g_sys.smpteTime.mins,
        //              g_sys.smpteTime.secs, g_sys.smpteTime.frame);
        //System_flush();


        /* Signal the TCP worker thread that position has changed */
        Event_post(g_eventTransport, Event_Id_00);
    }
}

//*****************************************************************************
// Read/Write a command to the SMPTE slave board
//*****************************************************************************

static bool SMPTE_WriteReg(uint16_t opcode)
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

    /* Send the SPI transaction */
    success = SPI_transfer(g_smpteHandle->spiHandle, &transaction);

    /* Leave safe access to SMPTE controller */
    GateMutex_leave(GateMutex_handle(&(g_smpteHandle->gate)), key);

    return success;
}

static bool SMPTE_ReadReg(uint16_t opcode, uint16_t *result)
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
    success = SPI_transfer(g_smpteHandle->spiHandle, &transaction);

    txbuf[0] = rxbuf[0] = 0;

    /* Allow time for slave to enable its receiver */
    Task_sleep(6);

    /* Read the response */
    transaction.count = 1;
    transaction.txBuf = (Ptr)&txbuf[0];
    transaction.rxBuf = (Ptr)&rxbuf[0];

    /* Send the SPI transaction */
    success = SPI_transfer(g_smpteHandle->spiHandle, &transaction);

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

    if (!SMPTE_ReadReg(cmd, &revid))
        return false;

    return (revid == SMPTE_REVID) ? true : false;
}

/* Get SMPTE module firmware version */

bool SMPTE_get_revid(uint16_t *revid)
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_REVID);

    if (!SMPTE_ReadReg(cmd, revid))
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

    return SMPTE_WriteReg(cmd);
}

bool SMPTE_encoder_stop()
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_ENCCTL) |
          SMPTE_ENCCTL_DISABLE;

    g_sys.smpteMode = STC_SMPTE_OFF;

    return SMPTE_WriteReg(cmd);
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
    SMPTE_WriteReg(cmd);

    /* Set the minutes register */
    cmd = SMPTE_REG_SET(SMPTE_REG_MINS) | SMPTE_DATA_SET(mins);
    SMPTE_WriteReg(cmd);

    /* Set the seconds register */
    cmd = SMPTE_REG_SET(SMPTE_REG_SECS) | SMPTE_DATA_SET(secs);
    SMPTE_WriteReg(cmd);

    /* Set the frame register */
    cmd = SMPTE_REG_SET(SMPTE_REG_FRAME) | SMPTE_DATA_SET(frame);
    SMPTE_WriteReg(cmd);

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

    return SMPTE_WriteReg(cmd);
}

bool SMPTE_decoder_stop(void)
{
    uint16_t cmd;

    cmd = SMPTE_REG_SET(SMPTE_REG_DECCTL) |
          SMPTE_DECCTL_DISABLE;

    GPIO_disableInt(Board_SMPTE_INT_N);

    return SMPTE_WriteReg(cmd);
}

/* End-Of-File */
