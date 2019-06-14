/***************************************************************************//**
*   @file   AD9837.h
*   @brief  Header file of AD9837 Driver for Microblaze processor.
*   @author Lucian Sin (Lucian.Sin@analog.com)
*
********************************************************************************
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
*******************************************************************************/

#ifndef _AD9837_H_
#define _AD9837_H_

/******************************************************************************/
/********************* Macros and Constants Definitions ***********************/
/******************************************************************************/

#define AD9837_CTRLB28          (1 << 13)
#define AD9837_CTRLHLB          (1 << 12)
#define AD9837_CTRLFSEL         (1 << 11)
#define AD9837_CTRLPSEL         (1 << 10)
#define AD9834_CTRLPINSW        (1 << 9)
#define AD9837_CTRLRESET        (1 << 8)
#define AD9837_CTRLSLEEP1       (1 << 7)
#define AD9837_CTRLSLEEP12      (1 << 6)
#define AD9837_CTRLOPBITEN      (1 << 5)
#define AD9834_CTRLSIGNPIB      (1 << 4)
#define AD9837_CTRLDIV2         (1 << 3)
#define AD9837_CTRLMODE         (1 << 1)

#define BIT_F0ADDRESS           0x4000      // Frequency Register 0 address.
#define BIT_F1ADDRESS           0x8000      // Frequency Register 1 address.

#define BIT_P0ADDRESS           0xC000      // Phase Register 0 address.
#define BIT_P1ADDRESS           0xE000      // Phase Register 1 address.

/******************************************************************************/
/*************************** Types Declarations *******************************/
/******************************************************************************/

enum MODE {TRIANGLE, SINE, SQUARE, SQUARE_2};
enum FREQREG {FREQ0, FREQ1};
enum PHASEREG {PHASE0, PHASE1};
enum FREQADJUSTMODE {FULL, COARSE, FINE};

typedef struct _AD9837_DEVICE {
    SPI_Handle  handle;
	uint16_t    configReg;
} AD9837_DEVICE;

/******************************************************************************/
/************************** Functions Declarations ****************************/
/******************************************************************************/

int32_t AD9837_init(void);
void AD9837_reset(void);
void AD9837_setMode(enum MODE newMode);
void AD9837_selectFreqReg(enum FREQREG reg);
void AD9837_selectPhaseReg(enum PHASEREG reg);
void AD9837_setFreqAdjustMode(enum FREQADJUSTMODE newMode);
void AD9837_adjustPhaseShift(enum PHASEREG reg, uint16_t newPhase);
void AD9837_adjustFreqMode32(enum FREQREG reg, enum FREQADJUSTMODE mode, uint32_t newFreq);
void AD9837_adjustFreqMode16(enum FREQREG reg, enum FREQADJUSTMODE mode, uint16_t newFreq);
void AD9837_adjustFreq32(enum FREQREG reg, uint32_t newFreq);
void AD9837_adjustFreq16(enum FREQREG reg, uint16_t newFreq);
uint32_t AD9837_freqCalc(float desiredFrequency);

#endif  /* _AD9837_H_ */
