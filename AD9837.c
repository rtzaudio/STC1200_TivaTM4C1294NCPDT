/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
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
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>

/* NDK BSD support */
#include <sys/socket.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* STC1200 Board Header file */

#include "STC1200.h"
#include "Board.h"
#include "CLITask.h"
#include "AD9837.h"

static SPI_Handle g_handleSpi3 = 0;

//*****************************************************************************
//
//*****************************************************************************

void AD9837_Init(void)
{
    SPI_Params spiParams;

    /* Open SPI port to AD9732  on the STC SMPTE daughter card */

    /* 1 Mhz, Moto fmt, polarity 1, phase 0 */
    SPI_Params_init(&spiParams);

    spiParams.transferMode  = SPI_MODE_BLOCKING;
    spiParams.mode          = SPI_MASTER;
    spiParams.frameFormat   = SPI_POL1_PHA0;
    spiParams.bitRate       = 250000;
    spiParams.dataSize      = 16;
    spiParams.transferCallbackFxn = NULL;

    g_handleSpi3 = SPI_open(Board_SPI_EXPIO_SIO3, &spiParams);

    if (g_handleSpi3 == NULL)
        System_abort("Error initializing SPI0\n");
}

/***************************************************************************//**
 * @brief Writes the value to a register.
 *
 * @param -  regValue - The value to write to the register.
 *
 * @return  None.
*******************************************************************************/

void AD9837_WriteRegister(uint16_t regValue)
{
    bool status;
    unsigned char data[2];
    uint16_t ulReply;
    SPI_Transaction transaction;

    data[0] = (unsigned char)((regValue & 0xFF00) >> 8);
    data[1] = (unsigned char)((regValue & 0x00FF) >> 0);

    /* Write AD9837 Control Word Bits */

    transaction.count = 1;
    transaction.txBuf = (Ptr)&data;
    transaction.rxBuf = (Ptr)&ulReply;

    status = SPI_transfer(g_handleSpi3, &transaction);
}

//*****************************************************************************
// Helper function, used to calculate the integer value to be written to a
// frequency register for a desired output frequency.
// The output frequency is fclk/2^28 * FREQREG. For us, fclk is 16MHz. We can
// save processor time by specifying a constant for fclk/2^28 - 0.0596. That is,
// in Hz, the smallest step size for adjusting the output frequency.
//*****************************************************************************

uint32_t AD9837_freqCalc(uint32_t freq)
{
    /*
     * Freq out put is flck/2^28 * FREQREG
     * at flck 16Mhz const = 12Mhz/2^28 = 0.04470hz
     * flck is the 16Mhz clock attached to the Gen.
     * Per AN-1070 Freq is calculated by
     * FregReg = (Fout * 2^28)/fmclk
     */
    const float flck_16_const = 0.04470f;
    return (uint32_t)(freq/flck_16_const);
}

//*****************************************************************************
// @brief Sets the Reset bit of the AD9837.
//
// @return None.
//*****************************************************************************

void AD9837_Reset(void)
{
    /* Place AD9837 in reset mode */
    AD9837_WriteRegister(AD9837_REG_CMD | AD9837_RESET);
    /* Settling time */
    Task_sleep(100);
    /* Take the AD9837 out of reset mode */
    AD9837_WriteRegister(AD9837_REG_CMD);
    Task_sleep(100);

    /* Set the frequency Registers */
    AD9837_SetFrequency(AD9837_REG_FREQ0, 9600);
    AD9837_SetFrequency(AD9837_REG_FREQ1, 9600);

    /* Set the phase registers */
    AD9837_SetPhase(AD9837_REG_PHASE0, 0);
    AD9837_SetPhase(AD9837_REG_PHASE1, 0);
}

/***************************************************************************//**
 * @brief Writes to the frequency registers.
 *
 * @param -  reg - Frequence register to be written to.
 * @param -  val - The value to be written.
 *
 * @return  None.
*******************************************************************************/

void AD9837_SetFrequency(uint16_t reg, uint32_t val)
{
    uint16_t freqHi = reg;
    uint16_t freqLo = reg;

    freqHi |= (val & 0xFFFC000) >> 14 ;
    freqLo |= (val & 0x3FFF);

    AD9837_WriteRegister(AD9837_B28);
    AD9837_WriteRegister(freqLo);
    AD9837_WriteRegister(freqHi);
}

/***************************************************************************//**
 * @brief Writes to the phase registers.
 *
 * @param -  reg - Phase register to be written to.
 * @param -  val - The value to be written.
 *
 * @return  None.
*******************************************************************************/

void AD9837_SetPhase(uint16_t reg, uint16_t val)
{
    uint16_t phase = reg;

    phase |= val;

    AD9837_WriteRegister(phase);
}

/***************************************************************************//**
 * @brief Selects the Frequency,Phase and Waveform type.
 *
 * @param -  freq  - Frequency register used.
 * @param -  phase - Phase register used.
 * @param -  type  - Type of waveform to be output.
 *
 * @return  None.
*******************************************************************************/

void AD9837_Setup(uint16_t freq,
                  uint16_t phase,
                  uint16_t type)
{
    uint16_t val = 0;

    val = freq | phase | type;

    AD9837_WriteRegister(val);
}

/***************************************************************************//**
 * @brief Sets the type of waveform to be output.
 *
 * @param -  type - type of waveform to be output.
 *
 * @return  None.
*******************************************************************************/

void AD9837_SetWave(uint16_t type)
{
    AD9837_WriteRegister(type);
}

// End-Of-File
