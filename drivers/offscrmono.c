//*****************************************************************************
//
// offscrmono.c - 1 BPP monochrome vertical off-screen display buffer driver.
//
// Copyright (c) 2008-2014 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.0.12573 of the Tiva Graphics Library.
//
//*****************************************************************************
//
// Modifications and ported by Robert E. Starr, Jr.
//
// This driver is for use with the vertical 8 pixels per byte memory
// format as used by some mono display controllers like the SSD1309.

//*****************************************************************************

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>

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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <driverlib/debug.h>
#include <grlib/grlib.h>

#include <driverlib/sysctl.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>

#include "offscrmono.h"
#include "../STC1200.h"
#include "../RAMPServer.h"

extern SYSDAT g_sys;

/* Global context for drawing */
tContext g_context;

/* FEMA OLED Display buffer context for grlib */
tDisplay g_FEMA128x64;

/* Display buffer memory */
unsigned char g_ucScreenBuffer[SCREEN_BUFSIZE+16];

//*****************************************************************************
//
//! \addtogroup primitives_api
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Translates a 24-bit RGB color to a display driver-specific color.
//
// \param c is the 24-bit RGB color.  The least-significant byte is the blue
// channel, the next byte is the green channel, and the third byte is the red
// channel.
//
// This macro translates a 24-bit RGB color into a value that can be written
// into the display's frame buffer in order to reproduce that color, or the
// closest possible approximation of that color.
//
// \return Returns the display-driver specific color.
//
//*****************************************************************************
#define DPYCOLORTRANSLATE(c)    ((((((c) & 0x00ff0000) >> 16) * 19661) + \
                                  ((((c) & 0x0000ff00) >> 8) * 38666) +  \
                                  (((c) & 0x000000ff) * 7209)) /         \
                                 (65536 * 128))

//*****************************************************************************
//
//! Draws a pixel on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param i32X is the X coordinate of the pixel.
//! \param i32Y is the Y coordinate of the pixel.
//! \param ui32Value is the color of the pixel.
//!
//! This function sets the given pixel to a particular color.  The coordinates
//! of the pixel are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
GrOffScreenMonoPixelDraw(void *pvDisplayData, int32_t i32X, int32_t i32Y,
                           uint32_t ui32Value)
{
    uint8_t *pui8Data;
    uint16_t i32Width;

    // Check the arguments.
    ASSERT(pvDisplayData);

    // Create a character pointer for the display-specific data (which points
    // to the image buffer). The first 5 bytes of the buffer contain the
    // image format, width & height.
    pui8Data = (uint8_t *)pvDisplayData;

    // Get the display width info from the video buffer.
    i32Width = *(uint16_t *)(pui8Data + 1);

    // Calculate the page/column offset to the byte of interest. We skip
    // over the first 5 bytes of display info to the start of image buffer.
    pui8Data += (((i32Y/8) * i32Width) + i32X) + 5;

    // Turn the bit of interest off and or in the new bit color, if any */
    *pui8Data = (*pui8Data & ~(1 << (i32Y & 0x07))) | (ui32Value << (i32Y & 0x07));
}

//*****************************************************************************
//
//! Draws a horizontal sequence of pixels on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param i32X is the X coordinate of the first pixel.
//! \param i32Y is the Y coordinate of the first pixel.
//! \param i32X0 is sub-pixel offset within the pixel data, which is valid for
//! 1 or 4 bit per pixel formats.
//! \param i32Count is the number of pixels to draw.
//! \param i32BPP is the number of bits per pixel ORed with a flag indicating
//! whether or not this run represents the start of a new image.
//! \param pui8Data is a pointer to the pixel data.  For 1 and 4 bit per pixel
//! formats, the most significant bit(s) represent the left-most pixel.
//! \param pui8Palette is a pointer to the palette used to draw the pixels.
//!
//! This function draws a horizontal sequence of pixels on the screen, using
//! the supplied palette.  For 1 bit per pixel format, the palette contains
//! pre-translated colors; for 4 and 8 bit per pixel formats, the palette
//! contains 24-bit RGB values that must be translated before being written to
//! the display.
//!
//! The \e i32BPP parameter will take the value 1, 4 or 8 and may be ORed with
//! \b GRLIB_DRIVER_FLAG_NEW_IMAGE to indicate that this run represents the
//! start of a new image.  Drivers which make use of lookup tables to convert
//! from the source to destination pixel values should rebuild their lookup
//! table when \b GRLIB_DRIVER_FLAG_NEW_IMAGE is set.
//!
//! \return None.
//
//*****************************************************************************
static void
GrOffScreenMonoPixelDrawMultiple(void *pvDisplayData, int32_t i32X,
                                   int32_t i32Y, int32_t i32X0,
                                   int32_t i32Count, int32_t i32BPP,
                                   const uint8_t *pui8Data,
                                   const uint8_t *pui8Palette)
{
	// NOT IMPLEMENTED!!
}

//*****************************************************************************
//
//! Draws a horizontal line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param i32X1 is the X coordinate of the start of the line.
//! \param i32X2 is the X coordinate of the end of the line.
//! \param i32Y is the Y coordinate of the line.
//! \param ui32Value is the color of the line.
//!
//! This function draws a horizontal line on the display.  The coordinates of
//! the line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
GrOffScreenMonoLineDrawH(void *pvDisplayData, int32_t i32X1, int32_t i32X2,
                         int32_t i32Y, uint32_t ui32Value)
{
	int32_t x;

	for (x=i32X1; x <= i32X2; x++)
	{
		GrOffScreenMonoPixelDraw(pvDisplayData, x, i32Y, ui32Value);
	}
}

//*****************************************************************************
//
//! Draws a vertical line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param i32X is the X coordinate of the line.
//! \param i32Y1 is the Y coordinate of the start of the line.
//! \param i32Y2 is the Y coordinate of the end of the line.
//! \param ui32Value is the color of the line.
//!
//! This function draws a vertical line on the display.  The coordinates of the
//! line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
GrOffScreenMonoLineDrawV(void *pvDisplayData, int32_t i32X, int32_t i32Y1,
                         int32_t i32Y2, uint32_t ui32Value)
{
	int32_t y;

	for (y=i32Y1; y <= i32Y2; y++)
	{
		GrOffScreenMonoPixelDraw(pvDisplayData, i32X, y, ui32Value);
	}
}

//*****************************************************************************
//
//! Fills a rectangle.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param pRect is a pointer to the structure describing the rectangle.
//! \param ui32Value is the color of the rectangle.
//!
//! This function fills a rectangle on the display.  The coordinates of the
//! rectangle are assumed to be within the extents of the display, and the
//! rectangle specification is fully inclusive (in other words, both i16XMin
//! and i16XMax are drawn, along with i16YMin and i16YMax).
//!
//! \return None.
//
//*****************************************************************************
static void
GrOffScreenMonoRectFill(void *pvDisplayData, const tRectangle *pRect,
                        uint32_t ui32Value)
{
	int32_t x, y;

	for(y=pRect->i16YMin; y <= pRect->i16YMax; y++)
    {
	    for(x=pRect->i16XMin; x <= pRect->i16XMax; x++)
		{
	    	GrOffScreenMonoPixelDraw(pvDisplayData, x, y, ui32Value);
		}
    }
}

//*****************************************************************************
//
//! Translates a 24-bit RGB color to a display driver-specific color.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param ui32Value is the 24-bit RGB color.  The least-significant byte is
//! the blue channel, the next byte is the green channel, and the third byte is
//! the red channel.
//!
//! This function translates a 24-bit RGB color into a value that can be
//! written into the display's frame buffer in order to reproduce that color,
//! or the closest possible approximation of that color.
//!
//! \return Returns the display-driver specific color.
//
//*****************************************************************************
static uint32_t
GrOffScreenMonoColorTranslate(void *pvDisplayData, uint32_t ui32Value)
{
    //
    // Check the arguments.
    //
    ASSERT(pvDisplayData);

    //
    // Translate from a 24-bit RGB color to black or white.
    //
    return(DPYCOLORTRANSLATE(ui32Value));
}

//*****************************************************************************
// GLOBAL GRAPHICS INTERFACE FUNCTIONS
//*****************************************************************************

//*****************************************************************************
//
//! Get the size of the offscreen video buffer memory.
//!
//! \return Buffer size in bytes.
//
//*****************************************************************************

int GrGetScreenBufferSize(void)
{
    return SCREEN_BUFSIZE;
}

//*****************************************************************************
//
//! Get the size of the offscreen video buffer memory.
//!
//! \return Buffer size in bytes.
//
//*****************************************************************************

unsigned char* GrGetScreenBuffer(size_t offset)
{
    return &g_ucScreenBuffer[offset];
}

//*****************************************************************************
//
//! Flushes any cached drawing operations.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//! This functions flushes any cached drawing operations to the display.  This
//! is useful when a local frame buffer is used for drawing operations, and the
//! flush would copy the local frame buffer to the display.  For the off-screen
//! display buffer driver, the flush is a no operation.
//!
//! \return None.
//
//*****************************************************************************
static void
GrOffScreenMonoFlush(void *pvDisplayData)
{
    /* Two extra words of the display buffer at the end contain
     * the LED/lamp state bits for all the button LED's and the
     * second word contains the current transport mode.
     */
    uint32_t *p = (uint32_t*)(&g_ucScreenBuffer[5] + 1024);

    /* 24-bits of the led mask. The transport lamp bits came from the
     * DTC via LED status IPC notifications. We're just passing these
     * along to the DRC remote.
     */
    uint32_t ledmask = (g_sys.ledMaskRemote << 8) | (g_sys.ledMaskTransport & 0xFF);

    *p++ = ledmask;
    *p++ = g_sys.transportMode;

    /* Flush the screen buffer to the DRC remote display via RS-422! */
    RAMP_Send_Display(1000);
}

//*****************************************************************************
//
//! Initializes a 1 BPP off-screen buffer.
//!
//! \param psDisplay is a pointer to the display structure to be configured for
//! the 1 BPP off-screen buffer.
//! \param pui8Image is a pointer to the image buffer to be used for the
//! off-screen buffer.
//! \param i32Width is the width of the image buffer in pixels.
//! \param i32Height is the height of the image buffer in pixels.
//!
//! This function initializes a display structure, preparing it to draw into
//! the supplied image buffer.  The image buffer is assumed to be large enough
//! to hold an image of the specified geometry.
//!
//! \return None.
//
//*****************************************************************************
void
GrOffScreenMonoInit()
{
	int32_t i32Width  = SCREEN_WIDTH;
	int32_t i32Height = SCREEN_HEIGHT;

	uint8_t *pui8Image = g_ucScreenBuffer;

	tDisplay *psDisplay = &g_FEMA128x64

    // Check the arguments.
    ASSERT(psDisplay);

    // Initialize the display structure.
    psDisplay->i32Size              = sizeof(tDisplay);
    psDisplay->pvDisplayData        = pui8Image;
    psDisplay->ui16Width            = i32Width;
    psDisplay->ui16Height           = i32Height;
    psDisplay->pfnPixelDraw         = GrOffScreenMonoPixelDraw;
    psDisplay->pfnPixelDrawMultiple = GrOffScreenMonoPixelDrawMultiple;
    psDisplay->pfnLineDrawH         = GrOffScreenMonoLineDrawH;
    psDisplay->pfnLineDrawV         = GrOffScreenMonoLineDrawV;
    psDisplay->pfnRectFill          = GrOffScreenMonoRectFill;
    psDisplay->pfnColorTranslate    = GrOffScreenMonoColorTranslate;
    psDisplay->pfnFlush             = GrOffScreenMonoFlush;

    // Initialize the image buffer.
    pui8Image[0] = IMAGE_FMT_1BPP_UNCOMP;
    *(uint16_t *)(pui8Image + 1) = i32Width;
    *(uint16_t *)(pui8Image + 3) = i32Height;

    /* Initialize the graphics context */
    GrContextInit(&g_context, &g_FEMA128x64);
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
