# STC1200_TivaTM4C1294NCPDT
Digital Search /Timer/Cue Controller for the Ampex MM1200 Professional Studio Recorder

By [Bob Starr](http://www.rtzaudio.com).

## Description
The **STC-1200** project contains the source code for the RTZ Audio STC-1200 
digitial search/timer/cue card for vintage Ampex MM-1200 professional studio
recorders.

## Building the Firmware

This firmware builds with with Texas Instruments Code Composer Studio (CCS) v9.3.0.00012
and uses the TI-RTOS v2.16.0.08 real-time kernel add on package. You must install CCS v9,
the TivaWare libraries, and the add on TI-RTOS package, see the "View->Resource Explorer"
menu option in CCS.

The SD disk driver provided with TI-RTOS must be configured and rebuilt to include
all the interface functions needed from the FATFS file system driver. By default, the
FATFS driver shipped is built for minimal footprint in the TI-RTOS install package. 

You must edit the file <ffconf.h> file to include all basic functions and rebuild 
the TI-RTOS driver libraries. The TI-RTOS configuration file for FATFS can be 
found at:
   
C:\ti\tirtos_tivac_2_16_00_08\products\tidrivers_tivac_2_16_00_08\packages\ti\mw\fatfs

Note a copy of the header <ffconv.h> is included in the STC-1200 project source code for
reference. You can copy this file over the existing file in the directory above, then 
open a command line window in the following directory:

C:\ti\tirtos_tivac_2_16_00_08

Execute the following commands at the prompt to rebuild TI-RTOS driver libraries
with the additional features enabled in the FATFS driver library.

..\ccs930\xdctools_3_32_00_06_core\gmake -f tirtos.mak clean-drivers

..\ccs930\xdctools_3_32_00_06_core\gmake -f tirtos.mak drivers

Once the driver libs have been successfully rebuilt with the new FATFS configuration
changes, you can then build the STC-1200_TivaTM4C1294NCPDT project in CSS and flash
the firmware built from the CCS menu option "Run->Load".

We use the XDS200 and XDS110 JTAG probes for debugging our ARM systems. You will need
to set the debugger type in the STC1200_TivaTM4C1294NCPDT.ccxml file if you use a pod
other than the XDS110 debug interface pod. The XDS110 pod is the most affordable and
offers decent performance, so we recommend using this for programming and flashing
the Tiva ARM processors.

## Authors

* Bob Starr (https://github.com/rtzaudio)


## License

Copyright (C) 2016-2021, RTZ Professional Audio, LLC

All Rights Reserved

RTZ is registered trademark of RTZ Professional Audio, LLC

 