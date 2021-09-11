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

=== VERSION 2.01 (09/11/2021) ==============================================

This build fixes a bug that caused the STC to hang when the standby monitor
was enabled and no DCS-1200 controller is present/found.

=== VERSION 2.00 (07/05/2021) ==============================================

This build includes changes for DRCWIN enhancements and upgrades. This
release requires DRCWIN 2.0 or higher, previous versions of DRCWIN are not
compatible with STC-1200 v2.0. The command line interface has been greatly
enhanced and includes commands to view files and directories on the
SD drive. Support for the upcoming DCS (digital channel switcher) with
DRCWIN has also been added.
 
A new version of the bootloader that supports flashing firmware updates 
directly from the SD drive is also included, but a JTAG programmer is 
required to flash the bootloader. Please contact us for options if needed.
THe BOOT and RESET button sequence initiates the firmware update from SD.

New commands are also included in the command line interface to download 
files directly to the SD drive via TeraTerm using xmodem. Or, you can 
remove the SD card and copy the new firmware image file to the SD drive 
on another computer. 

=== VERSION 1.14 (02/27/2021) ==============================================

This build includes a number of fixes and minor changes to support DRCWIN
and the DRC1200 wired remote simultaneously. The locator now supports
AUTO-PLAY with record by holding the REC button and pressing a LOC button
with AUTO-PLAY enabled. The machine will then search to the LOC point 
requested and then enter PLAY+REC more when REC button on the DRC1200 is
held prior to initiating a LOC or LOOP function. In DRCWIN the LOOP 
function also supports auto record by clicking the REC button prior
to initiating a LOC or LOOP function. The AUTO-PUNCH function has still
not been implimented yet.

=== VERSION 1.13 (01/26/2021) ==============================================

Improved cue point memory editing feature. Now time is entered starting
with the tens units first. If the user enters all 6 digits it completes the
data entry on the sixth digit. Otherwise the user can enter 'n' digits and
then CLICK the JOG WHEEL to accept the digits entered so far.

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
