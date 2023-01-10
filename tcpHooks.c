/***************************************************************************
 *
 * STC-1200 Digital Search Timer Cue Controller for Ampex MM-1200.
 *
 * Copyright (C) 2016-2019, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************
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
 *
 ***************************************************************************/

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

/* NDK BSD support */
#include <sys/socket.h>
//#include <ti/ndk/inc/usertype.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>
#include <RAMPServer.h>
#include "drivers/offscrmono.h"

/* PMX42 Board Header file */
#include "Board.h"
#include "STC1200.h"
#include "STC1200TCP.h"
#include "IPCToDTC.h"
#include "IPCCommands.h"
#include "IPCMessage.h"
#include "RemoteTask.h"
#include "CLITask.h"
#include "SMPTE.h"
#include "Utils.h"

/* Configuration Constants and Definitions */
#define NUMTCPWORKERS       4

#ifdef CYASSL_TIRTOS
#define TCPHANDLERSTACK     8704
#else
#define TCPHANDLERSTACK     1024
#endif

/* Static Function Prototypes */
void netOpenHook(void);
void netIPUpdate(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd);
Void tcpStateHandler(UArg arg0, UArg arg1);
Void tcpStateWorker(UArg arg0, UArg arg1);
Void tcpCommandHandler(UArg arg0, UArg arg1);
Void tcpCommandWorker(UArg arg0, UArg arg1);

static int ReadData(int fd, void *pbuf, int size, int flags);
static int WriteData(int fd, void *pbuf, int size, int flags);

static uint16_t HandleVersionGet(int fd, STC_COMMAND_VERSION_GET* cmd);
static uint16_t HandleStop(int fd, STC_COMMAND_STOP* cmd);
static uint16_t HandleRew(int fd, STC_COMMAND_REW* cmd);
static uint16_t HandleFwd(int fd, STC_COMMAND_FWD* cmd);
static uint16_t HandlePlay(int fd, STC_COMMAND_PLAY* cmd);
static uint16_t HandleLifter(int fd, STC_COMMAND_LIFTER* cmd);
static uint16_t HandleLocateModeSet(int fd, STC_COMMAND_LOCATE_MODE_SET* cmd);
static uint16_t HandleLocate(int fd, STC_COMMAND_LOCATE* cmd);
static uint16_t HandleCuePointStore(int fd, STC_COMMAND_CUEPOINT_STORE* cmd);
static uint16_t HandleCuePointSet(int fd, STC_COMMAND_CUEPOINT_SET* cmd);
static uint16_t HandleCuePointGet(int fd, STC_COMMAND_CUEPOINT_GET* cmd);
static uint16_t HandleCuePointClear(int fd, STC_COMMAND_CUEPOINT_CLEAR* cmd);
static uint16_t HandleLocateAutoLoop(int fd, STC_COMMAND_LOCATE_AUTO_LOOP* cmd);
static uint16_t HandleAutoPunchSet(int fd, STC_COMMAND_AUTO_PUNCH_SET* cmd);
static uint16_t HandleAutoPunchGet(int fd, STC_COMMAND_AUTO_PUNCH_GET* cmd);
static uint16_t HandleTrackGetCount(int fd, STC_COMMAND_TRACK_GET_COUNT* cmd);
static uint16_t HandleTrackSetState(int fd, STC_COMMAND_TRACK_SET_STATE* cmd);
static uint16_t HandleTrackGetState(int fd, STC_COMMAND_TRACK_GET_STATE* cmd);
static uint16_t HandleTrackMaskAll(int fd, STC_COMMAND_TRACK_MASK_ALL* cmd);
static uint16_t HandleTrackModeAll(int fd, STC_COMMAND_TRACK_MODE_ALL* cmd);
static uint16_t HandleTrackToggleAll(int fd, STC_COMMAND_TRACK_TOGGLE_ALL* cmd);
static uint16_t HandleMonitor(int fd, STC_COMMAND_MONITOR* cmd);
static uint16_t HandleZeroReset(int fd, STC_COMMAND_ZERO_RESET* cmd);
static uint16_t HandleCancel(int fd, STC_COMMAND_CANCEL* cmd);
static uint16_t HandleTapeSpeedSet(int fd, STC_COMMAND_TAPE_SPEED_SET* cmd);
static uint16_t HandleConfigEPROM(int fd, STC_COMMAND_CONFIG_EPROM* cmd);
static uint16_t HandleMachineConfig(int fd, STC_COMMAND_MACHINE_CONFIG* cmd);
static uint16_t HandleMachineConfigGet(int fd, STC_COMMAND_MACHINE_CONFIG_GET* cmd);
static uint16_t HandleMachineConfigSet(int fd, STC_COMMAND_MACHINE_CONFIG_SET* cmd);
static uint16_t HandleSMPTEMasterCtrl(int fd, STC_COMMAND_SMPTE_MASTER_CTRL* cmd);
static uint16_t HandleRTCTimeDateGet(int fd, STC_COMMAND_RTC_TIMEDATE_GET* cmd);
static uint16_t HandleRTCTimeDateSet(int fd, STC_COMMAND_RTC_TIMEDATE_SET* cmd);
static uint16_t HandleMACAddrGet(int fd, STC_COMMAND_MACADDR_GET* cmd);

/* External Function Prototypes */
extern void NtIPN2Str(uint32_t IPAddr, char *str);

//*****************************************************************************
// Helper Functions
//*****************************************************************************

/* This function performs a blocked read for 'size' number of bytes. It will
 * continue to read until all bytes are read, or return if an error occurs.
 */

int ReadData(int fd, void *pbuf, int size, int flags)
{
    int bytesRcvd = 0;
    int bytesToRecv = size;

    uint8_t* buf = (uint8_t*)pbuf;

    do {

        if ((bytesRcvd = recv(fd, buf, bytesToRecv, 0)) <= 0)
        {
            System_printf("Error: TCP recv failed %d.\n", bytesRcvd);
            break;
        }

        bytesToRecv -= bytesRcvd;

        buf += bytesRcvd;

    } while(bytesToRecv > 0);

    return bytesRcvd;
}

/* This function performs a blocked write for 'size' number of bytes. It will
 * continue to write until all bytes are sent, or return if an error occurs.
 */

int WriteData(int fd, void *pbuf, int size, int flags)
{
    int bytesSent = 0;
    int bytesToSend = size;

    uint8_t* buf = (uint8_t*)pbuf;

    do {

        if ((bytesSent = send(fd, buf, bytesToSend, 0)) <= 0)
        {
            System_printf("Error: TCP send failed %d.\n", bytesSent);
            break;
        }

        bytesToSend -= bytesSent;

        buf += bytesSent;

    } while (bytesToSend > 0);

    return bytesSent;
}

//*****************************************************************************
// NDK network open hook used to initialize IPv6
//*****************************************************************************

void netOpenHook(void)
{
    Task_Handle taskHandle;
    Task_Params taskParams;
    Error_Block eb;

    /* Make sure Error_Block is initialized */
    Error_init(&eb);

    /* Create the task that listens for incoming TCP connections
     * to handle streaming transport state info. The parameter arg0
     * will be the port that this task listens on.
     */

    Task_Params_init(&taskParams);

    taskParams.stackSize = TCPHANDLERSTACK;
    taskParams.priority  = 1;
    taskParams.arg0      = STC_PORT_STATE;

    taskHandle = Task_create((Task_FuncPtr)tcpStateHandler, &taskParams, &eb);

    if (taskHandle == NULL)
    {
        System_printf("netOpenHook: Failed to create tcpStateHandler Task\n");
        System_flush();
    }

    /* Create the Task that listens for incoming TCP connections
     * to handle command/response requests. The parameter arg0 will
     * be the port that this task listens on.
     */

    Task_Params_init(&taskParams);

    taskParams.stackSize = TCPHANDLERSTACK;
    taskParams.priority  = 1;
    taskParams.arg0      = STC_PORT_COMMAND;

    taskHandle = Task_create((Task_FuncPtr)tcpCommandHandler, &taskParams, &eb);

    if (taskHandle == NULL)
    {
        System_printf("netOpenHook: Failed to create tcpCommandHandler Task\n");
        System_flush();
    }

    System_flush();
}

// This handler is called when the DHCP client is assigned an
// address from a DHCP server. We store this in our runtime data
// structure for use later.

void netIPUpdate(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{
    if (fAdd)
        NtIPN2Str(IPAddr, g_sys.ipAddr);
    else
        NtIPN2Str(0, g_sys.ipAddr);

    //System_printf("netIPUpdate() dhcp->%s\n", g_sysData.ipAddr);
}

//*****************************************************************************
// LISTENER CREATES TRANSPORT STATE STREAMING WORKER TASK FOR NEW CONNECTIONS.
//*****************************************************************************

Void tcpStateHandler(UArg arg0, UArg arg1)
{
    int                status;
    int                clientfd;
    int                server;
    struct sockaddr_in localAddr;
    struct sockaddr_in clientAddr;
    int                optval;
    int                optlen = sizeof(optval);
    socklen_t          addrlen = sizeof(clientAddr);
    Task_Handle        taskHandle;
    Task_Params        taskParams;
    Error_Block        eb;

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server == -1)
    {
        System_printf("Error: socket not created.\n");
        System_flush();
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));

    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port        = htons(arg0);

    status = bind(server, (struct sockaddr *)&localAddr, sizeof(localAddr));

    if (status == -1)
    {
        System_printf("Error: bind failed.\n");
        System_flush();
        goto shutdown;
    }

    status = listen(server, NUMTCPWORKERS);

    if (status == -1)
    {
        System_printf("Error: listen failed.\n");
        System_flush();
        goto shutdown;
    }

    optval = 100;

    if (setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        System_printf("Error: setsockopt failed\n");
        System_flush();
        goto shutdown;
    }

    while ((clientfd = accept(server, (struct sockaddr *)&clientAddr, &addrlen)) != -1)
    {
        //System_printf("tcpStateHandler: Creating thread clientfd = %d\n", clientfd);
        //System_flush();

        /* Init the Error_Block */
        Error_init(&eb);

        /* Initialize the defaults and set the parameters. */
        Task_Params_init(&taskParams);
        taskParams.arg0      = (UArg)clientfd;
        taskParams.stackSize = 1280;
        taskParams.priority  = 5;

        taskHandle = Task_create((Task_FuncPtr)tcpStateWorker, &taskParams, &eb);

        if (taskHandle == NULL)
        {
            System_printf("Error: Failed to create new Task\n");
            System_flush();
            close(clientfd);
        }

        /* addrlen is a value-result param, must reset for next accept call */
        addrlen = sizeof(clientAddr);
    }

    System_printf("Exiting TCP state listener task\n");
    System_flush();

shutdown:

    System_flush();

    if (server > 0) {
        close(server);
    }
}

//*****************************************************************************
// STREAMS TRANSPORT STATE CHANGE INFO TO CLIENT. THERE CAN BE MULTIPLE
// STREAMING STATE WORKER THREADS RUNNING.
//*****************************************************************************

Void tcpStateWorker(UArg arg0, UArg arg1)
{
    int         clientfd = (int)arg0;
    int         bytesSent;
    int         bytesToSend;
    uint8_t*    buf;
    size_t      i;
    bool        connected = true;
    STC_STATE_MSG stateMsg;

    System_printf("tcpStateWorker: CONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    /* Set initially to send tape time update packet
     * since the client just connected.
     */
    Event_post(g_eventTransport, Event_Id_00);

    const UInt EVENT_MASK = Event_Id_00|Event_Id_01|Event_Id_02|Event_Id_03|Event_Id_04;

    while (connected)
    {
        /* Wait for a position change event from the tape roller position task */
        UInt events = Event_pend(g_eventTransport, Event_Id_NONE, EVENT_MASK, 2500);

        /* Get the tape time member values */
        PositionToTapeTime(g_sys.tapePosition, &stateMsg.tapeTime);

        int textlen = sizeof(STC_STATE_MSG);

        uint32_t transportMode = g_sys.transportMode;

        /* Test for search mode active */
        if (g_sys.searching)
            transportMode |= STC_M_SEARCH;
        /* Test for loop mode active */
        if (g_sys.autoLoop)
            transportMode |= STC_M_LOOP;
        /* Test for auto-punch mode */
        if (g_sys.autoPunch)
            transportMode |= STC_M_PUNCH;

        int8_t tapedir = 0;

        if (g_sys.tapeTach > 0.0f)
            tapedir = (g_sys.tapeDirection > 0) ?  1 : -1;

        uint32_t maskTransport = g_sys.ledMaskTransport;

        /* Simulate tape lifter button LED active flag */
        if (g_sys.transportMode & M_LIFTER)
            maskTransport |= STC_L_LDEF;

        /* Determine hardware status bit flags */
        uint8_t hardwareFlags = 0;

        /* SMPTE controller found */
        if (g_sys.smpteFound)
            hardwareFlags |= STC_HF_SMPTE;
        /* DCS channel switcher found */
        if (g_sys.dcsFound)
            hardwareFlags |= STC_HF_DCS;
        /* External RTC clock found */
        if (g_sys.rtcFound)
            hardwareFlags |= STC_HF_RTC;

        stateMsg.length             = textlen;
        stateMsg.errorCount         = g_sys.qei_error_cnt;
        stateMsg.ledMaskButton      = g_sys.ledMaskRemote;
        stateMsg.ledMaskTransport   = maskTransport;
        stateMsg.tapePosition       = g_sys.tapePosition;
        stateMsg.tapeVelocity       = (uint32_t)g_sys.tapeTach;
        stateMsg.transportMode      = (uint16_t)transportMode;
        stateMsg.tapeDirection      = tapedir;
        stateMsg.tapeSpeed          = (uint8_t)g_sys.tapeSpeed;
        stateMsg.tapeSize           = (uint8_t)2;
        stateMsg.searchProgress     = (uint8_t)g_sys.searchProgress;
        stateMsg.searching          = g_sys.searching;
        stateMsg.monitorFlags       = (uint8_t)g_sys.standbyMonitor;
        stateMsg.trackCount         = (uint8_t)g_sys.trackCount;
        stateMsg.hardwareFlags      = hardwareFlags;
        stateMsg.reserved2          = 0;
        stateMsg.reserved3          = 0;

        /* Copy the track state info */
        for (i=0; i < STC_MAX_TRACKS; i++)
            stateMsg.trackState[i] = g_sys.trackState[i];

        /* Copy the cue memory status bits */
        for (i=0; i < STC_MAX_CUE_POINTS; i++)
            stateMsg.cueState[i] = (uint8_t)g_sys.cuePoint[i].flags;

        /* Prepare to start sending state message buffer */

        bytesToSend = textlen;

        buf = (uint8_t*)&stateMsg;

        if ((bytesSent = WriteData(clientfd, buf, bytesToSend, 0)) <= 0)
        {
            connected = false;
            break;
        }

        (void)bytesSent;
    }

    System_printf("tcpStateWorker DISCONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    close(clientfd);
}

//*****************************************************************************
// LISTENER CREATES COMMAND/RESPONSE WORKER TASK FOR NEW CONNECTIONS.
//*****************************************************************************

Void tcpCommandHandler(UArg arg0, UArg arg1)
{
    int                status;
    int                clientfd;
    int                server;
    struct sockaddr_in localAddr;
    struct sockaddr_in clientAddr;
    int                optval;
    int                optlen = sizeof(optval);
    socklen_t          addrlen = sizeof(clientAddr);
    Task_Handle        taskHandle;
    Task_Params        taskParams;
    Error_Block        eb;

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server == -1)
    {
        System_printf("Error: socket not created.\n");
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));

    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port        = htons(arg0);

    status = bind(server, (struct sockaddr *)&localAddr, sizeof(localAddr));

    if (status == -1)
    {
        System_printf("Error: bind failed.\n");
        goto shutdown;
    }

    status = listen(server, NUMTCPWORKERS);

    if (status == -1)
    {
        System_printf("Error: listen failed.\n");
        goto shutdown;
    }

    optval = 100;

    if (setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        System_printf("Error: setsockopt failed\n");
        goto shutdown;
    }

    while ((clientfd = accept(server, (struct sockaddr *)&clientAddr, &addrlen)) != -1)
    {
        //System_printf("tcpStateHandler: Creating thread clientfd = %d\n", clientfd);
        //System_flush();

        /* Init the Error_Block */
        Error_init(&eb);

        /* Initialize the defaults and set the parameters. */
        Task_Params_init(&taskParams);
        taskParams.arg0      = (UArg)clientfd;
        taskParams.stackSize = 1280;
        taskParams.priority  = 5;

        taskHandle = Task_create((Task_FuncPtr)tcpCommandWorker, &taskParams, &eb);

        if (taskHandle == NULL)
        {
            //System_printf("Error: Failed to create new Task\n");
            //System_flush();
            close(clientfd);
        }

        /* addrlen is a value-result param, must reset for next accept call */
        addrlen = sizeof(clientAddr);
    }

    System_printf("Exiting TCP command listener task\n");
    System_flush();

shutdown:

    System_flush();

    if (server > 0)
    {
        close(server);
    }
}

//*****************************************************************************
// HANDLES COMMAND/RESPONSE REQUESTS FROM CLIENT. THERE CAN BE MULTIPLE
// COMMAND HANDLER WORKER THREADS RUNNING.
//*****************************************************************************

#define RXBUF_SIZE  512

Void tcpCommandWorker(UArg arg0, UArg arg1)
{
    bool        connected = true;
    bool        notify;
    int         clientfd = (int)arg0;
    int         bytesSent;
    int         bytesRcvd;
    uint16_t    status;
    uint8_t*    buf;
    Error_Block eb;
    STC_COMMAND_HDR* hdr;

    Error_init(&eb);

    if ((buf = Memory_alloc(NULL, RXBUF_SIZE, NULL, &eb)) == NULL)
    {
        System_printf("tcpCommandWorker: OUT OF MEMORY 0x%x\n", clientfd);
        System_flush();
        return;
    }

    System_printf("tcpCommandWorker: CONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    while (connected)
    {
        /* Attempt to read a message header */
        bytesRcvd = ReadData(clientfd, buf, sizeof(STC_COMMAND_HDR), 0);

        if (bytesRcvd <= 0)
        {
            System_printf("Error: TCP read error %d.\n", bytesRcvd);
            System_flush();
            connected = FALSE;
            break;
        }

        /* First part of message is always the header struct data */
        hdr = (STC_COMMAND_HDR*)buf;

        /* Make sure our buffer can hold the message */
        if (hdr->length >= RXBUF_SIZE)
        {
            System_printf("Error: buffer size too small to handle %d bytes.\n", hdr->length);
            System_flush();
            continue;
        }

        /* Attempt to read the rest of the message following the header */
        bytesRcvd = ReadData(
                clientfd,
                buf + sizeof(STC_COMMAND_HDR),
                hdr->length - sizeof(STC_COMMAND_HDR),
                0);

        if (bytesRcvd <= 0)
        {
            System_printf("Error: TCP read error %d.\n", bytesRcvd);
            System_flush();
            connected = FALSE;
            break;
        }

        /*
         * Determine which command to process from the client
         */

        notify = false;

        switch(hdr->command)
        {

        case STC_CMD_VERSION_GET:
            status = HandleVersionGet(clientfd, (STC_COMMAND_VERSION_GET*)buf);
            break;

        case STC_CMD_STOP:
            status = HandleStop(clientfd, (STC_COMMAND_STOP*)buf);
            break;

        case STC_CMD_PLAY:
            status = HandlePlay(clientfd, (STC_COMMAND_PLAY*)buf);
            break;

        case STC_CMD_REW:
            status = HandleRew(clientfd, (STC_COMMAND_REW*)buf);
            break;

        case STC_CMD_FWD:
            status = HandleFwd(clientfd, (STC_COMMAND_FWD*)buf);
            break;

        case STC_CMD_LIFTER:
            status = HandleLifter(clientfd, (STC_COMMAND_LIFTER*)buf);
            break;

        case STC_CMD_LOCATE:
            status = HandleLocate(clientfd, (STC_COMMAND_LOCATE*)buf);
            break;

        case STC_CMD_LOCATE_AUTO_LOOP:
            status = HandleLocateAutoLoop(clientfd, (STC_COMMAND_LOCATE_AUTO_LOOP*)buf);
            break;

        case STC_CMD_LOCATE_MODE_SET:
            status = HandleLocateModeSet(clientfd, (STC_COMMAND_LOCATE_MODE_SET*)buf);
            break;

        case STC_CMD_AUTO_PUNCH_SET:
            status = HandleAutoPunchSet(clientfd, (STC_COMMAND_AUTO_PUNCH_SET*)buf);
            notify = TRUE;
            break;

        case STC_CMD_AUTO_PUNCH_GET:
            status = HandleAutoPunchGet(clientfd, (STC_COMMAND_AUTO_PUNCH_GET*)buf);
            break;

        case STC_CMD_CUEPOINT_CLEAR:
            status = HandleCuePointClear(clientfd, (STC_COMMAND_CUEPOINT_CLEAR*)buf);
            notify = true;
            break;

        case STC_CMD_CUEPOINT_STORE:
            status = HandleCuePointStore(clientfd, (STC_COMMAND_CUEPOINT_STORE*)buf);
            notify = true;
            break;

        case STC_CMD_CUEPOINT_SET:
            status = HandleCuePointSet(clientfd, (STC_COMMAND_CUEPOINT_SET*)buf);
            notify = true;
            break;

        case STC_CMD_CUEPOINT_GET:
            status = HandleCuePointGet(clientfd, (STC_COMMAND_CUEPOINT_GET*)buf);
            break;

        case STC_CMD_TRACK_TOGGLE_ALL:
            status = HandleTrackToggleAll(clientfd, (STC_COMMAND_TRACK_TOGGLE_ALL*)buf);
            notify = true;
            break;

        case STC_CMD_TRACK_SET_STATE:
            status = HandleTrackSetState(clientfd, (STC_COMMAND_TRACK_SET_STATE*)buf);
            notify = true;
            break;

        case STC_CMD_TRACK_GET_STATE:
            status = HandleTrackGetState(clientfd, (STC_COMMAND_TRACK_GET_STATE*)buf);
            break;

        case STC_CMD_TRACK_MASK_ALL:
            status = HandleTrackMaskAll(clientfd, (STC_COMMAND_TRACK_MASK_ALL*)buf);
            notify = true;
            break;

        case STC_CMD_TRACK_MODE_ALL:
            status = HandleTrackModeAll(clientfd, (STC_COMMAND_TRACK_MODE_ALL*)buf);
            notify = true;
            break;

        case STC_CMD_ZERO_RESET:
            status = HandleZeroReset(clientfd, (STC_COMMAND_ZERO_RESET*)buf);
            notify = true;
            break;

        case STC_CMD_CANCEL:
            status = HandleCancel(clientfd, (STC_COMMAND_CANCEL*)buf);
            break;

        case STC_CMD_TAPE_SPEED_SET:
            status = HandleTapeSpeedSet(clientfd, (STC_COMMAND_TAPE_SPEED_SET*)buf);
            notify = true;
            break;

        case STC_CMD_CONFIG_EPROM:
            status = HandleConfigEPROM(clientfd, (STC_COMMAND_CONFIG_EPROM*)buf);
            notify = true;
            break;

        case STC_CMD_MONITOR:
            status = HandleMonitor(clientfd, (STC_COMMAND_MONITOR*)buf);
            notify = true;
            break;

        case STC_CMD_TRACK_GET_COUNT:
            status = HandleTrackGetCount(clientfd, (STC_COMMAND_TRACK_GET_COUNT*)buf);
            break;

        case STC_CMD_MACHINE_CONFIG:
            status = HandleMachineConfig(clientfd, (STC_COMMAND_MACHINE_CONFIG*)buf);
            notify = true;
            break;

        case STC_CMD_MACHINE_CONFIG_GET:
            status = HandleMachineConfigGet(clientfd, (STC_COMMAND_MACHINE_CONFIG_GET*)buf);
            break;

        case STC_CMD_MACHINE_CONFIG_SET:
            status = HandleMachineConfigSet(clientfd, (STC_COMMAND_MACHINE_CONFIG_SET*)buf);
            notify = true;
            break;

        case STC_CMD_SMPTE_MASTER_CTRL:
            status = HandleSMPTEMasterCtrl(clientfd, (STC_COMMAND_SMPTE_MASTER_CTRL*)buf);
            break;

        case STC_CMD_RTC_TIMEDATE_GET:
            status = HandleRTCTimeDateGet(clientfd, (STC_COMMAND_RTC_TIMEDATE_GET*)buf);
            break;

        case STC_CMD_RTC_TIMEDATE_SET:
            status = HandleRTCTimeDateSet(clientfd, (STC_COMMAND_RTC_TIMEDATE_SET*)buf);
            break;

        case STC_CMD_MACADDR_GET:
            status = HandleMACAddrGet(clientfd, (STC_COMMAND_MACADDR_GET*)buf);
            break;

        default:
            break;
        }

        /* Send the response packet */

        bytesSent =  WriteData(clientfd, buf, hdr->length, 0);

        if (bytesSent <= 0)
        {
            System_printf("Error: TCP write error %d.\n", bytesSent);
            System_flush();
            connected = false;
            break;
        }

        /* Refresh transport state change to DRC1200 wired remote */
        if (notify)
            Event_post(g_eventTransport, Event_Id_03);

        (void)status;
    }

    /* Close the TCP handle */
    close(clientfd);

    /* Free rx buffer memory allocated */
    Memory_free(NULL, buf, RXBUF_SIZE);

    /* Debugger Message */
    System_printf("tcpCommandWorker DISCONNECT clientfd = 0x%x\n", clientfd);
    System_flush();
}

//*****************************************************************************
// COMMAND MESSAGE HANDLERS
//*****************************************************************************

uint16_t HandleVersionGet(int fd, STC_COMMAND_VERSION_GET* cmd)
{
#if 0
    int rc;
    uint32_t dtc_version;
    uint32_t dtc_build;

    /* Make an attempt to get the DTC version. If it's not there it will
     * timeout with an error and we just set the DTC version to zero.
     */
    rc = IPCToDTC_VersionGet(g_sys.ipcToDTC, &dtc_version, &dtc_build, NULL);

    if (rc != IPC_ERR_SUCCESS)
    {
        dtc_version = dtc_build = 0;
    }
#endif

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_VERSION_GET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = (uint16_t)0;

    /* Reply Message Data */
    cmd->arg.param1.U = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);    /* STC version */
    cmd->arg.param2.U = g_sys.dtcVersion;   //dtc_version;      /* DTC version */
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleStop(int fd, STC_COMMAND_STOP* cmd)
{
    /* Cancel an locate in progress */
    if (IsLocating())
        LocateCancel();

    /* Send STOP button press */
    Transport_PostButtonPress(S_STOP);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_STOP);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleRew(int fd, STC_COMMAND_REW* cmd)
{
    uint32_t mask = S_REW;

    /* param1: flags STC_M_LIBWIND
     * param2: not used, zero
     */
    if (IsLocating())
        LocateCancel();

    /* simulate REC+FWD for lib wind mode */
    if (cmd->arg.param1.U & STC_M_LIBWIND)
        mask |= S_REC;

    Transport_PostButtonPress(mask);
    //Transport_Rew(0, msg.param1);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_REW);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleFwd(int fd, STC_COMMAND_FWD* cmd)
{
    uint32_t mask = S_FWD;

    /* param1: flags STC_M_LIBWIND
     * param2: not used, zero
     */
    if (IsLocating())
        LocateCancel();

    /* simulate REC+FWD for lib wind mode */
    if (cmd->arg.param1.U & STC_M_LIBWIND)
        mask |= S_REC;

    Transport_PostButtonPress(mask);
    //Transport_Fwd(0, msg.param1);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_FWD);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandlePlay(int fd, STC_COMMAND_PLAY* cmd)
{
    /* param1: 1=play+record, 0=play mode
     * param2: not used, zero
     */
    if (IsLocating())
        LocateCancel();

    if (cmd->arg.param1.U == 1)
        Transport_PostButtonPress(S_PLAY|S_REC);
    else
        Transport_PostButtonPress(S_PLAY);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_PLAY);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleLifter(int fd, STC_COMMAND_LIFTER* cmd)
{
    /* param1: not used, zero
     * param2: not used, zero
     */
    Transport_PostButtonPress(S_LDEF);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_LIFTER);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleLocateModeSet(int fd, STC_COMMAND_LOCATE_MODE_SET* cmd)
{
    /* param1: 1=store-mode, 0=cue-mode
     * param2: not used, zero
     */
    Remote_PostSwitchPress((cmd->arg.param1.U == 1) ? SW_STORE : SW_CUE, 0);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_LOCATE_MODE_SET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleLocate(int fd, STC_COMMAND_LOCATE* cmd)
{
    size_t      index;
    uint32_t    flags;

    // Start the auto-locator for the cue point index given
    /* param1: cue point index (0-9)
     * param2: cue flags, STC_CF_AUTO_PLAY or STC_CF_AUTO_REC
     */
    if (IsLocating())
        LocateCancel();

    index = 0;

    flags = cmd->arg.param2.U;

    switch(cmd->arg.param1.U)
    {
    case  STC_CUE_POINT_HOME:
        index = CUE_POINT_HOME;
        break;

    case STC_CUE_POINT_MARK_IN:
        index = CUE_POINT_MARK_IN;
        break;

    case STC_CUE_POINT_MARK_OUT:
        index = CUE_POINT_MARK_OUT;
        break;

    case STC_CUE_POINT_PUNCH_IN:
        index = CUE_POINT_PUNCH_IN;
        break;

    case STC_CUE_POINT_PUNCH_OUT:
        index = CUE_POINT_PUNCH_OUT;
        break;

    default:
        index = (cmd->arg.param1.U <= 9) ? cmd->arg.param1.U : 0;
        break;
    }

    //SetLocateButtonLED(index);
    LocateSearch(index, flags);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_LOCATE);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleCuePointStore(int fd, STC_COMMAND_CUEPOINT_STORE* cmd)
{
    int         ipos;
    size_t      index;
    uint32_t    flags;

    /* Index into cue table or track table */
    index = (size_t)cmd->hdr.index;

    /* param1: tape position
     * param2: cue flags (CF_ACTIVE, etc)
     */
    if (cmd->arg.param1.I == -1)
        ipos = g_sys.tapePosition;
    else
        ipos = cmd->arg.param1.I;

    flags = cmd->arg.param2.U;

    if (index == STC_CUE_POINT_MARK_IN)
    {
        SetButtonLedMask(STC_L_MARK_IN, 0);
        CuePointSet(index, ipos, flags);
    }
    else if (index == STC_CUE_POINT_MARK_OUT)
    {
        SetButtonLedMask(STC_L_MARK_OUT, 0);
        CuePointSet(index, ipos, flags);
    }
    else if (index == STC_CUE_POINT_PUNCH_IN)
    {
        SetButtonLedMask(STC_L_PUNCH_IN, 0);
        CuePointSet(index, ipos, flags);
    }
    else if (index == STC_CUE_POINT_PUNCH_OUT)
    {
        SetButtonLedMask(STC_L_PUNCH_OUT, 0);
        CuePointSet(index, ipos, flags);
    }
    else if (index <= 9)
    {
        SetLocateButtonLED(index);
        //SetButtonLedMask(smask[index] , 0);
        CuePointSet(index, ipos, flags);
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CUEPOINT_STORE);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleCuePointSet(int fd, STC_COMMAND_CUEPOINT_SET* cmd)
{
    /* Index into cue table or track table */
    size_t index = (size_t)cmd->hdr.index;

    /* param1: tape position
     * param2: cue flags (CF_ACTIVE, etc)
     */
    CuePointSet(index, cmd->arg.param1.I, cmd->arg.param2.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CUEPOINT_SET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleCuePointGet(int fd, STC_COMMAND_CUEPOINT_GET* cmd)
{
    int ipos;
    uint32_t flags;

    /* param1: cue point index
     * param2: not used, zero
     */
    CuePointGet((size_t)cmd->arg.param1.U, &ipos, &flags);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CUEPOINT_GET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.I = ipos;       /* return position in param1 */
    cmd->arg.param2.U = flags;      /* return flags in param2    */
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleCuePointClear(int fd, STC_COMMAND_CUEPOINT_CLEAR* cmd)
{
    /* param1: cue point index, or -1 for all cue points
     * param2: not used, zero
     */
    if (cmd->arg.param1.U == STC_ALL_CUEPOINTS)
    {
        /* Clear all points 0-9 */
        CuePointClearAll();

        /* Clear loop mark in/out points */
        CuePointClear(CUE_POINT_MARK_IN);
        CuePointClear(CUE_POINT_MARK_OUT);

        /* Clear punch in/out points */
        CuePointClear(CUE_POINT_PUNCH_IN);
        CuePointClear(CUE_POINT_PUNCH_OUT);

        SetButtonLedMask(0, STC_L_LOC_MASK |
                            STC_L_MARK_IN  | STC_L_MARK_OUT |
                            STC_L_PUNCH_IN | STC_L_PUNCH_OUT);
    }
    else
    {
        CuePointClear((size_t)cmd->arg.param1.U);
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CUEPOINT_CLEAR);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleLocateAutoLoop(int fd, STC_COMMAND_LOCATE_AUTO_LOOP* cmd)
{
    bool status;

    /* param1: cue flags, CF_AUTO_PLAY, etc
     * param2: not used, zero
     */
    if (IsLocating())
        LocateCancel();

    status = LocateLoop((uint32_t)cmd->arg.param1.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CUEPOINT_CLEAR);
    cmd->hdr.index  = 0;
    cmd->hdr.status = (uint16_t)status;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleAutoPunchSet(int fd, STC_COMMAND_AUTO_PUNCH_SET* cmd)
{
    if (cmd->arg.param1.U)
    {
        SetButtonLedMask(STC_L_AUTO_PUNCH, 0);

        g_sys.autoPunch = TRUE;
    }
    else
    {
        SetButtonLedMask(0, STC_L_AUTO_PUNCH);

        g_sys.autoPunch = FALSE;
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_AUTO_PUNCH_SET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleAutoPunchGet(int fd, STC_COMMAND_AUTO_PUNCH_GET* cmd)
{
    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_AUTO_PUNCH_GET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = g_sys.autoPunch;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTrackSetState(int fd, STC_COMMAND_TRACK_SET_STATE* cmd)
{
    /* param1: index or -1 for all tracks
     * param2: track state bits
     */
    if (cmd->arg.param1.U == STC_ALL_TRACKS)
        Track_SetAll((uint8_t)cmd->arg.param2.U, 0);
    else
        Track_SetState((size_t)cmd->arg.param1.U, (uint8_t)cmd->arg.param2.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TRACK_SET_STATE);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTrackGetState(int fd, STC_COMMAND_TRACK_GET_STATE* cmd)
{
    size_t track;
    uint8_t trackState;

    track = (size_t)cmd->arg.param1.U;

    Track_GetState(track, &trackState);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TRACK_GET_STATE);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = track;
    cmd->arg.param2.U = trackState;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTrackMaskAll(int fd, STC_COMMAND_TRACK_MASK_ALL* cmd)
{
    /* param1 = set mask,
     * param2 = clear mask
     */
    Track_MaskAll((uint8_t)cmd->arg.param1.U, (uint8_t)cmd->arg.param2.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TRACK_MASK_ALL);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTrackModeAll(int fd, STC_COMMAND_TRACK_MODE_ALL* cmd)
{
    /* param1 = new transport mode
     * param2 = 0
     */
    Track_SetModeAll((uint8_t)cmd->arg.param1.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TRACK_MODE_ALL);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTrackToggleAll(int fd, STC_COMMAND_TRACK_TOGGLE_ALL* cmd)
{
    /* param1 = bit flags to toggle
     * param2 = 0
     */
    Track_ToggleMaskAll((uint8_t)cmd->arg.param1.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TRACK_TOGGLE_ALL);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleZeroReset(int fd, STC_COMMAND_ZERO_RESET* cmd)
{
    PositionZeroReset();

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_ZERO_RESET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleCancel(int fd, STC_COMMAND_CANCEL* cmd)
{
    /* Cancel any locate request active */
    LocateCancel();

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CANCEL);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTapeSpeedSet(int fd, STC_COMMAND_TAPE_SPEED_SET* cmd)
{
    /* Set transport tape speed */
    Track_SetTapeSpeed((int)cmd->arg.param1.U);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TAPE_SPEED_SET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleConfigEPROM(int fd, STC_COMMAND_CONFIG_EPROM* cmd)
{
    /* param1: 0=load, 1=store, 3=reset
     * param2 = 0
     */
    switch(cmd->arg.param1.U)
    {
    case 0:
        // load STC config data to EPROM */
        ConfigLoad(1);
        break;

    case 1:
        // save STC config data to EPROM */
        ConfigSave(1);
        break;

    case 3:
        // reset STC config data in memory to defaults */
        ConfigReset(1);
        break;

    default:
        break;
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_CONFIG_EPROM);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleMonitor(int fd, STC_COMMAND_MONITOR* cmd)
{
    /* Enable standby monitor mode for all tracks */
    g_sys.standbyMonitor = (cmd->arg.param1.U) ? true : false;

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_MONITOR);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleTrackGetCount(int fd, STC_COMMAND_TRACK_GET_COUNT* cmd)
{
    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_TRACK_GET_COUNT);
    cmd->hdr.index  = 0;
    cmd->hdr.status = 0;

    /* Reply Message Data */
    cmd->arg.param1.U = g_sys.trackCount;
    cmd->arg.param2.U = g_sys.dcsFound;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleMachineConfig(int fd, STC_COMMAND_MACHINE_CONFIG* cmd)
{
    int rc;

    /* This allows the STC to tell the DTC to load, store or
     * reset it configuration parameters to/from EPROM
     */
    switch(cmd->arg.param1.U)
    {
    case 0:
        // load the STC config data
        ConfigLoad(1);
        /* load DTC config */
        rc = IPCToDTC_ConfigEPROM(g_sys.ipcToDTC, 0);
        break;

    case 1:
        // save the STC config data
        ConfigSave(1);
        // save DTC config
        rc = IPCToDTC_ConfigEPROM(g_sys.ipcToDTC, 1);
        break;

    case 2:         /* reset DTC config */
        // reset STC config data to defaults
        ConfigReset(1);
        // reset DTC config data to defaults
        rc = IPCToDTC_ConfigEPROM(g_sys.ipcToDTC, 2);
        break;

    default:
        break;
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_MACHINE_CONFIG);
    cmd->hdr.index  = 0;
    cmd->hdr.status = (uint16_t)rc;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return 0;
}


uint16_t HandleMachineConfigGet(int fd, STC_COMMAND_MACHINE_CONFIG_GET* cmd)
{
    int rc = IPC_ERR_SUCCESS;

    /* Gets the STC and DTC configuration parameters struct in memory */
    memset(&(cmd->stc), 0, sizeof(STC_CONFIG_DATA));
    memset(&(cmd->dtc), 0, sizeof(DTC_CONFIG_DATA));

    if (sizeof(STC_CONFIG_DATA) == sizeof(SYSCFG))
    {
        /* Return global STC system config data in the message buffer */
        memcpy(&(cmd->stc), &g_cfg, sizeof(STC_CONFIG_DATA));

        /* Get the DTC config data via IPC from DTC */
        rc = IPCToDTC_ConfigGet(g_sys.ipcToDTC, &cmd->dtc);
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_MACHINE_CONFIG_GET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = (uint16_t)rc;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return rc;
}


uint16_t HandleMachineConfigSet(int fd, STC_COMMAND_MACHINE_CONFIG_SET* cmd)
{
    int rc = IPC_ERR_SUCCESS;

    /* Sets the STC and DTC configuration parameters in memory */

    if (sizeof(STC_CONFIG_DATA) == sizeof(SYSCFG))
    {
        /* Copy STC config data into the STC config buffer */
        memcpy(&g_cfg, &(cmd->stc), sizeof(STC_CONFIG_DATA));

        /* Send the DTC new config data via IPC */
        rc = IPCToDTC_ConfigSet(g_sys.ipcToDTC, &cmd->dtc);
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_HDR) + sizeof(STC_COMMAND_ARG);
    cmd->hdr.index  = 0;
    cmd->hdr.status = (uint16_t)rc;

    /* Reply Message Data */
    cmd->arg.param1.U = 0;
    cmd->arg.param2.U = 0;
    cmd->arg.bitflags = 0;

    return rc;
}


uint16_t HandleSMPTEMasterCtrl(int fd, STC_COMMAND_SMPTE_MASTER_CTRL* cmd)
{
    uint16_t status = 0;

    if (!g_sys.smpteFound)
    {
        status = 0xFFFF;
    }
    else
    {
        switch(cmd->ctrl)
        {
        case 0:
            /* stop the SMPTE generator */
            SMPTE_generator_stop();
            break;

        case 1:
            /* start the SMPTE generator */
            SMPTE_generator_start();
            break;

        case 2:
            /* resume the SMPTE generator */
            SMPTE_generator_resume();
            break;

        default:
            status = 0xFFFF;
            break;
        }
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_SMPTE_MASTER_CTRL);
    cmd->hdr.index  = 0;
    cmd->hdr.status = status;

    return status;
}


uint16_t HandleRTCTimeDateGet(int fd, STC_COMMAND_RTC_TIMEDATE_GET* cmd)
{
    uint16_t status = 0;
    RTCC_Struct ts;

    /* Read the current RTC time/date */

    if (RTC_GetDateTime(&ts))
    {
        /* Fill in the return members */
        cmd->datetime.sec     = ts.sec;         /* seconds 0-59      */
        cmd->datetime.min     = ts.min;         /* minutes 0-59      */
        cmd->datetime.hour    = ts.hour;        /* 24-hour 0-23      */
        cmd->datetime.weekday = ts.weekday;     /* weekday 1-7       */
        cmd->datetime.date    = ts.date;        /* day of month 0-30 */
        cmd->datetime.month   = ts.month;       /* month 0-11        */
        cmd->datetime.year    = ts.year;        /* year 0-128 (+2000)*/
    }
    else
    {
        /* Error reading time/date */
        status = 1;
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_RTC_TIMEDATE_GET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = status;

    return status;
}


uint16_t HandleRTCTimeDateSet(int fd, STC_COMMAND_RTC_TIMEDATE_SET* cmd)
{
    uint16_t status = 0;
    RTCC_Struct ts;

    /* Fill in time/date with values received */

    ts.sec     = cmd->datetime.sec;         /* seconds 0-59      */
    ts.min     = cmd->datetime.min;         /* minutes 0-59      */
    ts.hour    = cmd->datetime.hour;        /* 24-hour 0-23      */
    ts.weekday = cmd->datetime.weekday;     /* weekday 1-7       */
    ts.date    = cmd->datetime.date;        /* day of month 0-30 */
    ts.month   = cmd->datetime.month;       /* month 0-11        */
    ts.year    = cmd->datetime.year;        /* year 0-128 (+2000)*/

    /* Check that the time/date parameters are all valid */
    if (RTC_IsValidTime(&ts))
    {
        /* Attempt to set the RTC time/date */
        if (!RTC_SetDateTime(&ts))
        {
            /* Error setting time/date */
            status = 1;
        }
    }
    else
    {
        /* Invalid time or date */
        status = 2;
    }

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_RTC_TIMEDATE_SET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = status;

    return status;
}


uint16_t HandleMACAddrGet(int fd, STC_COMMAND_MACADDR_GET* cmd)
{
    uint16_t status = 0;

    /* Copy the MAC address */
    memcpy(&(cmd->macaddr), &g_sys.ui8MAC, 6);

    /* Copy the STC serial number */
    memcpy(&(cmd->sernum_stc), &g_sys.ui8SerialNumberSTC, 16);

    /* Copy the DTC serial number*/
    memcpy(&(cmd->sernum_dtc), &g_sys.ui8SerialNumberDTC, 16);

    /* Reply Header Data */
    cmd->hdr.length = sizeof(STC_COMMAND_MACADDR_GET);
    cmd->hdr.index  = 0;
    cmd->hdr.status = status;

    return status;
}


// End-Of-File
