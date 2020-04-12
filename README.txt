============================================================================

 STC-1200 Search Timer Controller for Ampex MM-1200 Tape Machines

 Copyright (C) 2016-2019, RTZ Professional Audio, LLC

 All Rights Reserved

 RTZ is registered trademark of RTZ Professional Audio, LLC

 Visit us on the web at http://www.rtzaudio.com
 
============================================================================

This archive contains firmware for the RTZ Audio STC-1200 digital transport
controller based on the Tiva STC1200_TM4C1294NCPDT M4 ARM processor. 

The file "STC1200_TivaTM4C1294NCPDT.bin" contains the binary image and is
programmed to the part using the LMFLASH utility and the STC RS232 serial
port boot loader. Please read the STC owners manual instructions using 
the LMFLASH utility for firmware updates over RS-232.

The file "STC1200_TivaTM4C1294NCPDT.out" contains the object format version
of the firmware image for use with the XDS JTAG programming pods. These
are generally used for development systems or recovery only.

Download and install the LMFLASH utility. Follow the STC bootloader firmware
update instructions in the owners manual for instructions. You will need to
download the LMFLASH utility from the link on the RTZ page, or search the
web and download free from TI.

=== VERSION 1.12 (02/01/2020) ==============================================

Added new 'stat' command to command line interface. Also fixed minor
issue with SD drive that resolved potential random access failures
occasionally. Some SD cards would exhibit problems, others would not, due
to pullup resistors not being enabled on the I/O pins. Should work with
all SD cards properly now.

=== VERSION 1.11 (01/14/2020) ==============================================

Minor changes diagnostic and debugging checks of SD drive.

=== VERSION 1.10 (01/10/2020) ==============================================

Minor changes for debugging checks of SD drive. Internal house build.

=== VERSION 1.09 (12/14/2019) ==============================================

Added new CLI commands for cue, store and rtz. The locator can now be used
from the command line interface. Other minor bug fixes.
 
=== VERSION 1.08 (12/11/2019) ==============================================

Additional build changes for bug checks. 

=== VERSION 1.07 (11/16/2019) ==============================================

Fixed bugs with locator introduced with auto-looping. Hopefully locator 
and looping issues are resolved with this release.

=== VERSION 1.06 (11/12/2019) ==============================================

Enhancements for DRCWIN software remote. Added velocity graph and ability
to store/load cue point memories to disk. This version of the STC requires
DTC firmware 2.35 or greater. Also requires DRCWin v1.04 or greater.

=== VERSION 1.05 (11/01/2019) ==============================================

Enhancements for DRCWIN software remotes. Now includes basic support for
auto-loop play mode. The MARK-IN and MARK-OUT butts define the start/stop 
points for looping. The CAN button will exit loop/search mode and leaves
the machine running in whatever current mode it's operating in. Note this
version of the STC requires DTC with firmware 2.35 or greater. Also requires
DRCWin v1.03 or greater.

=== VERSION 1.04 (10/27/2019) ==============================================

Support for DRCWIN TCP/IP interface software remote control added. Minor bug
fixes in the locator and other tasks. Software interface added to support
future channel switching logic.

=== VERSION 1.03 (09/10/2019) ==============================================

First beta relase of STC-1200 firmware.  
