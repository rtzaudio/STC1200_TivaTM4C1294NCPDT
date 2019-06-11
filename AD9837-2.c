/**************************************************************************//**
*   @file   AD9837.c
*   @brief  Implementation of AD9837 Driver for Microblaze processor.
*   @author Lucian Sin (Lucian.Sin@analog.com)
*
*******************************************************************************
* Copyright 2013(c) Analog Devices, Inc.
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*  - Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  - Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*  - Neither the name of Analog Devices, Inc. nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*  - The use of this software may or may not infringe the patent rights
*    of one or more patent holders.  This license does not release you
*    from the requirement that you obtain separate licenses from these
*    patent holders to use this software.
*  - Use of the software either in source or binary form, must be run
*    on or directly connected to an Analog Devices Inc. component.
*
* THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT, MERCHANTABILITY
* AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

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
#include <stdlib.h>

/* STC1200 Board Header file */

#include "STC1200.h"
#include "Board.h"
#include "CLITask.h"
#include "AD9837-2.h"

static SPI_Handle  handle;
static uint16_t    configReg;


static void SPIWrite(int16_t value)
{
    uint8_t data[2];
    uint16_t ulReply;
    SPI_Transaction transaction;

    data[0] = (uint8_t)((value & 0xFF00) >> 8);
    data[1] = (uint8_t)((value & 0x00FF) >> 0);

    /* Write AD9837 Control Word Bits */
    transaction.count = 1;
    transaction.txBuf = (Ptr)&data;
    transaction.rxBuf = (Ptr)&ulReply;

    GPIO_write(Board_AD9732_FSYNC, PIN_LOW);
    SPI_transfer(handle, &transaction);
    GPIO_write(Board_AD9732_FSYNC, PIN_HIGH);
}


int32_t AD9837_init(void)
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

    handle = SPI_open(Board_SPI_EXPIO_SIO3, &spiParams);

    if (handle == NULL)
        System_abort("Error initializing SPI0\n");

	return 0;
}

// reset the AD part. This will disable all function generation and set the
//  output to approximately mid-level, constant voltage. Since we're resetting,
//  we can also forego worrying about maintaining the state of the other bits
//  in the config register.

void AD9837_reset()
{
  uint32_t defaultFreq = AD9837_freqCalc(100.0);

  AD9837_adjustFreqMode32(FREQ0, FULL, defaultFreq);
  AD9837_adjustFreqMode32(FREQ1, FULL, defaultFreq);

  AD9837_adjustPhaseShift(PHASE0, 0x0000);
  AD9837_adjustPhaseShift(PHASE1, 0x0000);

  SPIWrite(0x0100);
  SPIWrite(0x0000);
}

// Set the mode of the part. The mode (trinagle, sine, or square) is set by
//  three bits in the status register: D5 (OPBITEN), D3 (DIV2), and D1 (MODE).
//  Here's a nice truth table for those settings:
//  D5 D1 D3
//  0  0  x   Sine wave output
//  0  1  x   Triangle wave output
//  1  0  0   Square wave @ 1/2 frequency
//  1  0  1   Square wave @ frequency
//  1  1  x   Not allowed

void AD9837_setMode(enum MODE newMode)
{
    // We want to adjust the three bits in the config register that we're
    //  interested in without screwing up anything else. Unfortunately, this
    //  part is write-only, so we need to maintain a local shadow, adjust that,
    //  then write it.

    configReg &= ~0x002A; // Clear D5, D3, and D1.

    switch(newMode)
    {
    case TRIANGLE:
        configReg |= 0x0002;
        break;

    case SQUARE_2:
        configReg |= 0x0020;
        break;

    case SQUARE:
        configReg |= 0x0028;
        break;

    case SINE:
        configReg |= 0x0000;
        break;
    }

    // Now write our shadow copy to the part.
    SPIWrite(configReg);
}

// The AD9837 has two frequency registers that can be independently adjusted.
//  This allows us to fiddle with the value in one without affecting the output
//  of the device. The register used for calculating the output is selected by
//  toggling bit 11 of the config register.

void AD9837_selectFreqReg(enum FREQREG reg)
{
    // For register FREQ0, we want to clear bit 11.
    if (reg == FREQ0)
        configReg &= ~0x0800;
    else
        configReg |= 0x0800;

    SPIWrite(configReg);
}

// Similarly, there are two phase registers, selected by bit 10 of the config
//  register.

void AD9837_selectPhaseReg(enum PHASEREG reg)
{
    if (reg == PHASE0)
        configReg &= ~0x0400;
    else
        configReg |= 0x0400;

    SPIWrite(configReg);
}

// The frequency registers are 28 bits in size (combining the lower 14 bits of
//  two 16 bit writes; the upper 2 bits are the register address to write).
//  Bits 13 and 12 of the config register select how these writes are handled:
//  13 12
//  0  0   Any write to a frequency register is treated as a write to the lower
//          14 bits; this allows for fast fine adjustment.
//  0  1   Writes are send to upper 14 bits, allowing for fast coarse adjust.
//  1  x   First write of a pair goes to LSBs, second to MSBs. Note that the
//          user must, in this case, be certain to write in pairs, to avoid
//          unexpected results!

void AD9837_setFreqAdjustMode(enum FREQADJUSTMODE newMode)
{
  // Start by clearing the bits in question.
  configReg &= ~0x3000;
  // Now, adjust the bits to match the truth table above.
  switch(newMode)
  {
    case COARSE:  // D13:12 = 01
      configReg |= 0x1000;
    break;
    case FINE:    // D13:12 = 00
    break;
    case FULL:    // D13:12 = 1x (we use 10)
      configReg |= 0x2000;
    break;
  }
  SPIWrite(configReg);
}

// The phase shift value is 12 bits long; it gets routed to the proper phase
//  register based on the value of the 3 MSBs (4th MSB is ignored).

void AD9837_adjustPhaseShift(enum PHASEREG reg, uint16_t newPhase)
{
  // First, let's blank the top four bits. Just because it's the right thing
  //  to do, you know?
  newPhase &= ~0xF000;
  // Now, we need to set the top three bits to properly route the data.
  //  D15:D13 = 110 for PHASE0...
  if (reg == PHASE0) newPhase |= 0xC000;
  // ... and D15:D13 = 111 for PHASE1.
  else               newPhase |= 0xE000;
  SPIWrite(newPhase);
}

// Okay, now we're going to handle frequency adjustments. This is a little
//  trickier than a phase adjust, because in addition to properly routing the
//  data, we need to know whether we're writing all 32 bits or just 16. I've
//  overloaded this function call for three cases: write with a mode change (if
//  one is needed), and write with the existing mode.

// Adjust the contents of the given register, and, if necessary, switch mode
//  to do so. This is probably the slowest method of updating a register.

void AD9837_adjustFreqMode32(enum FREQREG reg, enum FREQADJUSTMODE mode, uint32_t newFreq)
{
    AD9837_setFreqAdjustMode(mode);
    // Now, we can just call the normal 32-bit write.
    AD9837_adjustFreq32(reg, newFreq);
}

// Fine or coarse update of the given register; change modes if necessary to
//  do this.

void AD9837_adjustFreqMode16(enum FREQREG reg, enum FREQADJUSTMODE mode, uint16_t newFreq)
{
    AD9837_setFreqAdjustMode(mode);  // Set the mode
    AD9837_adjustFreq16(reg, newFreq); // Call the known-mode write.
}

// Adjust the contents of the register, but assume that the write mode is
//  already set to full. Note that if it is NOT set to full, bad things will
//  happen- the coarse or fine register will be updated with the contents of
//  the upper 14 bits of the 28 bits you *meant* to send.

void AD9837_adjustFreq32(enum FREQREG reg, uint32_t newFreq)
{
  // We need to split the 32-bit input into two 16-bit values, blank the top
  //  two bits of those values, and set the top two bits according to the
  //  value of reg.
  // Start by acquiring the low 16-bits...
  uint16_t temp = (uint16_t)newFreq;
  // ...and blanking the first two bits.
  temp &= ~0xC000;
  // Now, set the top two bits according to the reg parameter.
  if (reg==FREQ0) temp |= 0x4000;
  else            temp |= 0x8000;
  // Now, we can write temp out to the device.
  SPIWrite(temp);
  // Okay, that's the lower 14 bits. Now let's grab the upper 14.
  temp = (uint16_t)(newFreq>>14);
  // ...and now, we can just repeat the process.
  temp &= ~0xC000;
  // Now, set the top two bits according to the reg parameter.
  if (reg==FREQ0) temp |= 0x4000;
  else            temp |= 0x8000;
  // Now, we can write temp out to the device.
  SPIWrite(temp);
}

// Adjust the coarse or fine register, depending on the current mode. Note that
//  if the current adjust mode is FULL, this is going to cause undefined
//  behavior, as it will leave one transfer hanging. Maybe that means only
//  half the register gets loaded? Maybe nothing happens until another write
//  to that register? Either way, it's not going to be good.

void AD9837_adjustFreq16(enum FREQREG reg, uint16_t newFreq)
{
    // We need to blank the first two bits...
    newFreq &= ~0xC000;

    // Now, set the top two bits according to the reg parameter.
    if (reg == FREQ0)
        newFreq |= 0x4000;
    else
        newFreq |= 0x8000;

    // Now, we can write newFreq out to the device.
    SPIWrite(newFreq);
}

// Helper function, used to calculate the integer value to be written to a
//  freq register for a desired output frequency.
// The output frequency is fclk/2^28 * FREQREG. For us, fclk is 16MHz. We can
//  save processor time by specifying a constant for fclk/2^28- .0596. That is,
//  in Hz, the smallest step size for adjusting the output frequency.

uint32_t AD9837_freqCalc(float desiredFrequency)
{
    return (uint32_t) (desiredFrequency/.0596);
}

/* End-Of-File */
