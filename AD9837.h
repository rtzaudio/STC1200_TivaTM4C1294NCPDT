/*
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 */

#ifndef _AD9837_H_
#define _AD9837_H_

/* SG-310SEF Clock module on SMPTE daughter card 12.0 Mhz */
#define AD9837_MCLK_FREQ        12000000

/* Registers */

#define AD9837_REG_CMD          (0 << 14)
#define AD9837_REG_FREQ0        (1 << 14)
#define AD9837_REG_FREQ1        (2 << 14)
#define AD9837_REG_PHASE0       (6 << 13)
#define AD9837_REG_PHASE1       (7 << 13)

/* Command Control Bits */

#define AD9837_B28              (1 << 13)
#define AD9837_HLB              (1 << 12)
#define AD9837_FSEL0            (0 << 11)
#define AD9837_FSEL1            (1 << 11)
#define AD9837_PSEL0            (0 << 10)
#define AD9837_PSEL1            (1 << 10)
#define AD9837_PIN_SW           (1 << 9)
#define AD9837_RESET            (1 << 8)
#define AD9837_SLEEP1           (1 << 7)
#define AD9837_SLEEP12          (1 << 6)
#define AD9837_OPBITEN          (1 << 5)
#define AD9837_SIGN_PIB         (1 << 4)
#define AD9837_DIV2             (1 << 3)
#define AD9837_MODE             (1 << 1)

#define AD9837_OUT_SINUS        ((0 << 5) | (0 << 1) | (0 << 3))
#define AD9837_OUT_TRIANGLE     ((0 << 5) | (1 << 1) | (0 << 3))
#define AD9837_OUT_MSB          ((1 << 5) | (0 << 1) | (1 << 3))
#define AD9837_OUT_MSB2         ((1 << 5) | (0 << 1) | (0 << 3))

/* Register Select Bits */

#define BIT_F0ADDRESS           0x4000      // Frequency Register 0 address.
#define BIT_F1ADDRESS           0x8000      // Frequency Register 1 address.

#define BIT_P0ADDRESS           0xC000      // Phase Register 0 address.
#define BIT_P1ADDRESS           0xE000      // Phase Register 1 address.

/******************************************************************************/
/* Functions Prototypes                                                       */
/******************************************************************************/

/* calculate frequency register value for a desired output frequency */
uint32_t AD9837_freqCalc(uint32_t freq);

/* Initializes the SPI communication peripheral and resets the part. */
void AD9837_Init(void);

/* Sets the Reset bit of the AD9837. */
void AD9837_Reset(void);

/* Clears the Reset bit of the AD9837. */
void AD9837_ClearReset(void);

/* Writes the value to a register. */
void AD9837_SetRegisterValue(uint16_t regValue);

/* Writes to the frequency registers. */
void AD9837_SetFrequency(uint16_t reg, uint32_t val);

/* Writes to the phase registers. */
void AD9837_SetPhase(uint16_t reg, uint16_t val);

/* Selects the Frequency,Phase and Waveform type. */
void AD9837_Setup(uint16_t freq, uint16_t phase, uint16_t type);

#endif // _AD9837_H_
