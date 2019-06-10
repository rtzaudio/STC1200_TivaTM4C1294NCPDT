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

/******************************************************************************/
/************************** Constants Definitions *****************************/
/******************************************************************************/
float phase_const = 651.8986469f;
//float freq_const  = 16.777216f;         // mclk = 16000000
float freq_const  = 22.36962133f;         // mclk = 12000000

/******************************************************************************/
/************************** Functions Definitions *****************************/
/******************************************************************************/

/***************************************************************************//**
 * @brief Initialize the SPI communication with the device.
 *
 * @param device     - The device structure.
 * @param init_param - The structure that contains the device initial
 * 		       parameters.
 *
 * @return status - Result of the initialization procedure.
 *                  Example:  0 - if initialization was successful;
 *                           -1 - if initialization was unsuccessful.
*******************************************************************************/

int32_t AD9837_init(struct AD9837_DEVICE **device)
{
	uint16_t spi_data = 0;
    struct AD9837_DEVICE *dev;
    SPI_Params spiParams;

	dev = (struct AD9837_DEVICE *)malloc(sizeof(*dev));

	if (!dev)
		return -1;

	dev->prog_method    = 0;
	dev->ctrl_reg_value = 0;
    dev->test_opbiten   = 0;

    /* Open SPI port to AD9732  on the STC SMPTE daughter card */

    /* 1 Mhz, Moto fmt, polarity 1, phase 0 */
    SPI_Params_init(&spiParams);

    spiParams.transferMode  = SPI_MODE_BLOCKING;
    spiParams.mode          = SPI_MASTER;
    spiParams.frameFormat   = SPI_POL1_PHA0;
    spiParams.bitRate       = 250000;
    spiParams.dataSize      = 16;
    spiParams.transferCallbackFxn = NULL;

    dev->handle = SPI_open(Board_SPI_EXPIO_SIO3, &spiParams);

    if (dev->handle == NULL)
        System_abort("Error initializing SPI0\n");

	/* Initialize board. */
	spi_data |= AD9837_CTRLRESET;
	AD9837_write(dev, spi_data);
	Task_sleep(10);
	spi_data &= ~(AD9837_CTRLRESET);
	AD9837_write(dev, spi_data);

	/* Sine Output */
	AD9837_out_mode(dev, 0);

	AD9837_set_freq(dev, 0, 9600);
	AD9837_set_freq(dev, 1, 9600);

	AD9837_set_phase(dev, 0, 0.0f);
	AD9837_set_phase(dev, 1, 0.0f);

	*device = dev;

	return 0;
}

/***************************************************************************//**
 * @brief Free the resources allocated by AD9837_init().
 *
 * @param dev - The device structure.
 *
 * @return SUCCESS in case of success, negative error code otherwise.
*******************************************************************************/
int32_t AD9837_remove(struct AD9837_DEVICE *dev)
{
	int32_t ret = -1;

	if (dev->handle)
	{
	    SPI_close(dev->handle);
	    dev->handle = NULL;
	    ret = 0;
	}

	free(dev);

	return ret;
}

/**************************************************************************//**
 * @brief Transmits 16 bits on SPI.
 *
 * @param dev   - The device structure.
 * @param value - Data which will be transmitted.
 *
 * @return none.
******************************************************************************/

void AD9837_write(struct AD9837_DEVICE *dev, int16_t value)
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

    SPI_transfer(dev->handle, &transaction);
}

/**************************************************************************//**
 * @brief Selects the type of output.
 *
 * @param dev      - The device structure.
 * @param out_mode - type of output
 *                   Example AD9837&AD9837: 0 - Sinusoid.
 *                                          1 - Triangle.
 *                                          2 - DAC Data MSB/2.
 *                                          3 - DAC Data MSB.
 *
 * @return status - output type could / couldn't be selected.
 *                  Example: 0 - output type possible for the given configuration.
 *                          -1 - output type not possible for the given configuration.
******************************************************************************/

int32_t AD9837_out_mode(struct AD9837_DEVICE *dev, uint8_t out_mode)
{
	uint16_t spi_data = 0;
	int8_t status = 0;

	spi_data = (dev->ctrl_reg_value & ~(AD9837_CTRLMODE | AD9837_CTRLOPBITEN | AD9837_CTRLDIV2));

	switch(out_mode)
	{
		case 1:     // Triangle
			spi_data += AD9837_CTRLMODE;
			break;
		case 2:     // DAC Data MSB/2
			spi_data += AD9837_CTRLOPBITEN;
			break;
		case 3:     // DAC Data MSB
			spi_data += AD9837_CTRLOPBITEN + AD9837_CTRLDIV2;
			break;
		default:    // Sinusoid
			break;
	}

	AD9837_write(dev, spi_data);

	dev->ctrl_reg_value = spi_data;

	return status;
}

/**************************************************************************//**
 * @brief Enable / Disable the sleep function.
 *
 * @param dev        - The device structure.
 * @param sleep_mode - type of sleep
 *                    Example soft method(all devices):
 *                              0 - No power-down.
 *                              1 - DAC powered down.
 *                              2 - Internal clock disabled.
 *                              3 - DAC powered down and Internal
 *                                  clock disabled.
 *                    Example hard method(AD9834 & AD9838):
 *                              0 - No power-down.
 *                              1 - DAC powered down.
 *
 * @return None.
******************************************************************************/
void AD9837_sleep_mode(struct AD9837_DEVICE *dev,
		       uint8_t sleep_mode)
{
	uint16_t spi_data = 0;

	spi_data = (dev->ctrl_reg_value & ~(AD9837_CTRLSLEEP12 | AD9837_CTRLSLEEP1));

	switch(sleep_mode)
	{
		case 1:     // DAC powered down
			spi_data += AD9837_CTRLSLEEP12;
			break;
		case 2:     // Internal clock disabled
			spi_data += AD9837_CTRLSLEEP1;
			break;
		case 3:     // DAC powered down and Internal clock disabled
			spi_data += AD9837_CTRLSLEEP1 + AD9837_CTRLSLEEP12;
			break;
		default:    // No power-down
			break;
	}

	AD9837_write(dev, spi_data);

	dev->ctrl_reg_value = spi_data;
}

/**************************************************************************//**
 * @brief Loads a frequency value in a register.
 *
 * @param dev             - The device structure.
 * @param register_number - Number of the register (0 / 1).
 * @param frequency_value - Frequency value.
 *
 * @return None.
******************************************************************************/
void AD9837_set_freq(struct AD9837_DEVICE *dev,
		     uint8_t register_number,
		     uint32_t frequency_value)
{
	uint32_t ul_freq_register;
	uint16_t i_freq_lsb;
	uint16_t i_freq_msb;

	ul_freq_register = (uint32_t)(frequency_value * freq_const);

	i_freq_lsb = (ul_freq_register & 0x0003FFF);
	i_freq_msb = ((ul_freq_register & 0xFFFC000) >> 14);

	dev->ctrl_reg_value |= AD9837_CTRLB28;

	AD9837_write(dev, dev->ctrl_reg_value);

	if (register_number == 0)
	{
		AD9837_write(dev, BIT_F0ADDRESS + i_freq_lsb);
		AD9837_write(dev, BIT_F0ADDRESS + i_freq_msb);
	}
	else
	{
		AD9837_write(dev, BIT_F1ADDRESS + i_freq_lsb);
		AD9837_write(dev, BIT_F1ADDRESS + i_freq_msb);
	}
}

/**************************************************************************//**
 * @brief Loads a phase value in a register.
 *
 * @param dev             - The device structure.
 * @param register_number - Number of the register (0 / 1).
 * @param phase_value     - Phase value.
 *
 * @return none
******************************************************************************/
void AD9837_set_phase(struct AD9837_DEVICE *dev,
		      uint8_t register_number,
		      float phase_value)
{
	uint16_t phase_calc;

	phase_calc = (uint16_t)(phase_value * phase_const);

	if (register_number == 0)
		AD9837_write(dev, BIT_P0ADDRESS + phase_calc);
	else
		AD9837_write(dev, BIT_P1ADDRESS + phase_calc);
}

/**************************************************************************//**
 * @brief Select the frequency register to be used.
 *
 * @param dev      - The device structure.
 * @param freq_reg - Number of frequency register. (0 / 1)
 *
 * @return None.
******************************************************************************/
void AD9837_select_freq_reg(struct AD9837_DEVICE *dev,
			    uint8_t freq_reg)
{
	uint16_t spi_data = 0;

	spi_data = (dev->ctrl_reg_value & ~AD9837_CTRLFSEL);

	// Select soft the working frequency register according to parameter
	if (freq_reg == 1)
		spi_data += AD9837_CTRLFSEL;

	AD9837_write(dev, spi_data);

	dev->ctrl_reg_value = spi_data;
}

/**************************************************************************//**
 * @brief Select the phase register to be used.
 *
 * @param dev       - The device structure.
 * @param phase_reg - Number of phase register. (0 / 1)
 *
 * @return None.
******************************************************************************/
void AD9837_select_phase_reg(struct AD9837_DEVICE *dev,
			     uint8_t phase_reg)
{
	uint16_t spi_data = 0;

	spi_data = (dev->ctrl_reg_value & ~AD9837_CTRLPSEL);

	// Select soft the working phase register according to parameter
	if (phase_reg == 1)
	    spi_data += AD9837_CTRLPSEL;

	AD9837_write(dev, spi_data);

	dev->ctrl_reg_value = spi_data;
}

/* End-Of-File */
