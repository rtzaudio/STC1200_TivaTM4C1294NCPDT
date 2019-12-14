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
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
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
#include "IPCCommands.h"
#include "IPCMessage.h"
#include "RemoteTask.h"
#include "CLITask.h"

/* Configuration Constants and Definitions */
#define NUMTCPWORKERS       4

#ifdef CYASSL_TIRTOS
#define TCPHANDLERSTACK     8704
#else
#define TCPHANDLERSTACK     1024
#endif

/* Global STC-1200 System data */
extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

/* Static Function Prototypes */
void netOpenHook(void);
void netIPUpdate(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd);
Void tcpStateHandler(UArg arg0, UArg arg1);
Void tcpStateWorker(UArg arg0, UArg arg1);
Void tcpCommandHandler(UArg arg0, UArg arg1);
Void tcpCommandWorker(UArg arg0, UArg arg1);

static int ReadData(int fd, void *pbuf, int size, int flags);
static int WriteData(int fd, void *pbuf, int size, int flags);

/* External Function Prototypes */
extern void NtIPN2Str(uint32_t IPAddr, char *str);

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

    if (taskHandle == NULL) {
        System_printf("netOpenHook: Failed to create tcpStateHandler Task\n");
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

    if (taskHandle == NULL) {
        System_printf("netOpenHook: Failed to create tcpCommandHandler Task\n");
    }

    System_flush();
}

// This handler is called when the DHCP client is assigned an
// address from a DHCP server. We store this in our runtime data
// structure for use later.

void netIPUpdate(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{
    if (fAdd)
        NtIPN2Str(IPAddr, g_sysData.ipAddr);
    else
        NtIPN2Str(0, g_sysData.ipAddr);

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

    if (server == -1) {
        System_printf("Error: socket not created.\n");
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));

    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port        = htons(arg0);

    status = bind(server, (struct sockaddr *)&localAddr, sizeof(localAddr));

    if (status == -1) {
        System_printf("Error: bind failed.\n");
            goto shutdown;
    }

    status = listen(server, NUMTCPWORKERS);

    if (status == -1) {
        System_printf("Error: listen failed.\n");
            goto shutdown;
    }

    optval = 100;

    if (setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
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

        taskHandle = Task_create((Task_FuncPtr)tcpStateWorker, &taskParams, &eb);

        if (taskHandle == NULL) {
            //System_printf("Error: Failed to create new Task\n");
            //System_flush();
            close(clientfd);
        }

        /* addrlen is a value-result param, must reset for next accept call */
        addrlen = sizeof(clientAddr);
    }

    System_printf("Error: accept failed.\n");
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

    static STC_STATE_MSG stateMsg;

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
        PositionToTapeTime(g_sysData.tapePosition, &stateMsg.tapeTime);

        int textlen = sizeof(STC_STATE_MSG);

        uint32_t transportMode = g_sysData.transportMode;

        /* If locate is active, set search bit flag */
        if (g_sysData.searching)
            transportMode |= STC_M_SEARCH;

        /* If loop mode is active, set loop mode bit flag */
        if (g_sysData.autoLoop)
            transportMode |= STC_M_LOOP;

        if (g_sysData.autoPunch)
            transportMode |= STC_M_PUNCH;

        int8_t tapedir = 0;

        if (g_sysData.tapeSpeed)
            tapedir = (g_sysData.tapeDirection > 0) ?  1 : -1;

        stateMsg.length             = textlen;
        stateMsg.errorCount         = g_sysData.qei_error_cnt;
        stateMsg.ledMaskButton      = g_sysData.ledMaskButton;
        stateMsg.ledMaskTransport   = g_sysData.ledMaskTransport;
        stateMsg.tapePosition       = g_sysData.tapePosition;
        stateMsg.tapeVelocity       = (uint32_t)g_sysData.tapeTach;
        stateMsg.transportMode      = (uint16_t)transportMode;
        stateMsg.tapeDirection      = tapedir;
        stateMsg.tapeSpeed          = (uint8_t)g_sysData.tapeSpeed;
        stateMsg.tapeSize           = (uint8_t)2;
        stateMsg.searchProgress     = (uint8_t)g_sysData.searchProgress;
        stateMsg.searching          = g_sysData.searching;
        stateMsg.monitorFlags       = 0;

        /* Copy the track state info */
        for (i=0; i < STC_MAX_TRACKS; i++)
            stateMsg.trackState[i] = g_sysData.trackState[i];

        /* Copy the cue memory status bits */
        for (i=0; i < STC_MAX_CUE_POINTS; i++)
            stateMsg.cueState[i] = (uint8_t)g_sysData.cuePoint[i].flags;

        /* Prepare to start sending state message buffer */

        bytesToSend = textlen;

        buf = (uint8_t*)&stateMsg;

        do {

            if ((bytesSent = send(clientfd, buf, bytesToSend, 0)) <= 0)
            {
                connected = false;
                break;
            }

            bytesToSend -= bytesSent;

            buf += bytesSent;

        } while (bytesToSend > 0);
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

    if (server == -1) {
        System_printf("Error: socket not created.\n");
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));

    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port        = htons(arg0);

    status = bind(server, (struct sockaddr *)&localAddr, sizeof(localAddr));

    if (status == -1) {
        System_printf("Error: bind failed.\n");
            goto shutdown;
    }

    status = listen(server, NUMTCPWORKERS);

    if (status == -1) {
        System_printf("Error: listen failed.\n");
            goto shutdown;
    }

    optval = 100;

    if (setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
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

        taskHandle = Task_create((Task_FuncPtr)tcpCommandWorker, &taskParams, &eb);

        if (taskHandle == NULL) {
            //System_printf("Error: Failed to create new Task\n");
            //System_flush();
            close(clientfd);
        }

        /* addrlen is a value-result param, must reset for next accept call */
        addrlen = sizeof(clientAddr);
    }

    System_printf("Error: accept failed.\n");
    System_flush();

shutdown:

    System_flush();

    if (server > 0) {
        close(server);
    }
}

//*****************************************************************************
// HANDLES COMMAND/RESPONSE REQUESTS FROM CLIENT. THERE CAN BE MULTIPLE
// COMMAND HANDLER WORKER THREADS RUNNING.
//*****************************************************************************

Void tcpCommandWorker(UArg arg0, UArg arg1)
{
    bool        connected = true;
    bool        notify;
    int         clientfd = (int)arg0;
    int         bytesSent;
    int         bytesRcvd;
    int         ipos;
    size_t      index;
    uint16_t    status;
    uint32_t    mask;
    uint32_t    flags;

    STC_COMMAND_HDR msg;

    static const uint32_t smask[10] = {
        SW_LOC0, SW_LOC1, SW_LOC2, SW_LOC3, SW_LOC4,
        SW_LOC5, SW_LOC6, SW_LOC7, SW_LOC8, SW_LOC9
    };

    System_printf("tcpCommandWorker: CONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    while (connected)
    {
        /* Attempt to read a message header */
        bytesRcvd = ReadData(clientfd, &msg, sizeof(STC_COMMAND_HDR), 0);

        if (bytesRcvd < 0)
        {
            System_printf("Error: TCP read error %d.\n", bytesRcvd);
            connected = FALSE;
            break;
        }

        msg.status = status = 0;

        /* Index into cue table or track table */
        index = (size_t)msg.index;

        /*
         * Determine which command to process from the client
         */

        notify = false;

        switch(msg.command)
        {
        case STC_CMD_STOP:
            /* param1: not used, zero
             * param2: not used, zero
             */
            if (IsLocating())
                LocateCancel();
            Transport_PostButtonPress(S_STOP);
            break;

        case STC_CMD_REW:
            /* param1: flags STC_M_LIBWIND
             * param2: not used, zero
             */
            if (IsLocating())
                LocateCancel();
            mask = S_REW;
            /* simulate REC+FWD for lib wind mode */
            if (msg.param1.U & STC_M_LIBWIND)
                mask |= S_REC;
            Transport_PostButtonPress(mask);
            //Transport_Rew(0, msg.param1);
            break;

        case STC_CMD_FWD:
            /* param1: flags STC_M_LIBWIND
             * param2: not used, zero
             */
            if (IsLocating())
                LocateCancel();
            mask = S_FWD;
            /* simulate REC+FWD for lib wind mode */
            if (msg.param1.U & STC_M_LIBWIND)
                mask |= S_REC;
            Transport_PostButtonPress(mask);
            //Transport_Fwd(0, msg.param1);
            break;

        case STC_CMD_PLAY:
            /* param1: 1=play+record, 0=play mode
             * param2: not used, zero
             */
            if (IsLocating())
                LocateCancel();
            if (msg.param1.U == 1)
                Transport_PostButtonPress(S_PLAY|S_REC);
            else
                Transport_PostButtonPress(S_PLAY);
            break;

        case STC_CMD_LIFTER:
            /* param1: not used, zero
             * param2: not used, zero
             */
            Transport_PostButtonPress(S_LDEF);
            break;

        case STC_CMD_LOCATE_MODE_SET:
            /* param1: 1=store-mode, 0=cue-mode
             * param2: not used, zero
             */
            Remote_PostSwitchPress((msg.param1.U == 1) ? SW_STORE : SW_CUE, 0);
            break;

        case STC_CMD_LOCATE:
            // Start the auto-locator for the cue point index given
            /* param1: cue point index (0-9)
             * param2: cue flags, STC_CF_AUTO_PLAY or STC_CF_AUTO_REC
             */

            if (IsLocating())
                LocateCancel();

            if (msg.param1.U <= 9)
            {
                Remote_PostSwitchPress(smask[msg.param1.U], msg.param2.U);
            }
            else if (msg.param1.U == STC_CUE_POINT_HOME)
            {
                LocateSearch(CUE_POINT_HOME,  msg.param2.U);
            }
            else if (msg.param1.U == STC_CUE_POINT_MARK_IN)
            {
                LocateSearch(CUE_POINT_MARK_IN,  msg.param2.U);
            }
            else if (msg.param1.U == STC_CUE_POINT_MARK_OUT)
            {
                LocateSearch(CUE_POINT_MARK_OUT,  msg.param2.U);
            }
            else if (msg.param1.U == STC_CUE_POINT_PUNCH_IN)
            {
                LocateSearch(CUE_POINT_PUNCH_IN,  msg.param2.U);
            }
            else if (msg.param1.U == STC_CUE_POINT_PUNCH_OUT)
            {
                LocateSearch(CUE_POINT_PUNCH_OUT,  msg.param2.U);
            }
            break;

        case STC_CMD_LOCATE_AUTO_LOOP:
            /* param1: cue flags, CF_AUTO_PLAY, etc
             * param2: not used, zero
             */
            if (IsLocating())
                LocateCancel();

            status = LocateLoop((uint32_t)msg.param1.U);
            break;

        case STC_CMD_AUTO_PUNCH_SET:
            if (msg.param1.U)
            {
                SetButtonLedMask(STC_L_AUTO_PUNCH, 0);
                g_sysData.autoPunch = TRUE;
            }
            else
            {
                SetButtonLedMask(0, STC_L_AUTO_PUNCH);
                g_sysData.autoPunch = FALSE;
            }
            notify = TRUE;
            break;

        case STC_CMD_CUEPOINT_STORE:
            /* param1: tape position
             * param2: cue flags (CF_ACTIVE, etc)
             */
            if (msg.param1.I == -1)
                ipos = g_sysData.tapePosition;
            else
                ipos = msg.param1.I;

            flags = msg.param2.U;

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
                SetButtonLedMask(smask[index] , 0);
                CuePointSet(index, ipos, flags);
            }
            notify = true;
            break;

        case STC_CMD_CUEPOINT_SET:
            /* param1: tape position
             * param2: cue flags (CF_ACTIVE, etc)
             */
            CuePointSet(index, msg.param1.I, msg.param2.U);
            notify = true;
            break;

        case STC_CMD_CUEPOINT_GET:
            /* param1: cue point index
             * param2: not used, zero
             */
            CuePointGet((size_t)msg.param1.U, &ipos, &flags);
            /* return position in param1, flags in param2 */
            msg.param1.I = ipos;
            msg.param2.U = flags;
            break;

        case STC_CMD_CUEPOINT_CLEAR:
            /* param1: cue point index, or -1 for all cue points
             * param2: not used, zero
             */
            if (msg.param1.U == STC_ALL_CUEPOINTS)
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
                CuePointClear((size_t)msg.param1.U);
            }
            notify = true;
            break;

        case STC_CMD_TRACK_SET_STATE:
            /* param1: index or -1 for all tracks
             * param2: track state bits
             */
            if (msg.param1.U == STC_ALL_TRACKS)
                Track_SetAll((uint8_t)msg.param2.U, 0);
            else
                Track_SetState((size_t)msg.param1.U, (uint8_t)msg.param2.U, 0);
            notify = true;
            break;

        case STC_CMD_TRACK_MASK_ALL:
            /* param1 = set mask,
             * param2 = clear mask
             */
            Track_MaskAll((uint8_t)msg.param1.U, (uint8_t)msg.param2.U);
            notify = true;
            break;

        case STC_CMD_TRACK_MODE_ALL:
            /* param1 = new transport mode
             * param2 = 0
             */
            Track_ModeAll((uint8_t)msg.param1.U);
            notify = true;
            break;

        case STC_CMD_ZERO_RESET:
            /* param1 = 0
             * param2 = 0
             */
            PositionZeroReset();
            notify = true;
            break;

        case STC_CMD_CANCEL:
            /* param1 = 0
             * param2 = 0
             */
            LocateCancel();
            break;
        }

        /* Now send the response packet */

        msg.status  = status;
        msg.datalen = 0;
        msg.index   = index;

        bytesSent =  WriteData(clientfd, &msg, sizeof(STC_COMMAND_HDR), 0);

        if (bytesSent < 0)
        {
            System_printf("Error: TCP write error %d.\n", bytesSent);
            connected = false;
            break;
        }

        /* Notify refresh of current transport state change */

        if (notify)
            Event_post(g_eventTransport, Event_Id_03);
    }

    System_printf("tcpCommandWorker DISCONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    close(clientfd);
}

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

// End-Of-File
