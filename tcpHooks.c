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
        for (i=0; i < STC_MAX_CUES; i++)
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
    int         bytesToSend;
    int         bytesRcvd;
    int         bytesToRecv;
    uint8_t*    buf;
    uint32_t    mask;

    STC_COMMAND_HDR msg;

    static const uint32_t smask[10] = {
        SW_LOC0, SW_LOC1, SW_LOC2, SW_LOC3, SW_LOC4,
        SW_LOC5, SW_LOC6, SW_LOC7, SW_LOC8, SW_LOC9
    };

    System_printf("tcpCommandWorker: CONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    while (connected)
    {
        bytesToRecv = sizeof(STC_COMMAND_HDR);

        buf = (uint8_t*)&msg;

        do {

            if ((bytesRcvd = recv(clientfd, buf, bytesToRecv, 0)) <= 0)
            {
                System_printf("Error: tpc recv failed %d.\n", bytesRcvd);
                break;
            }

            bytesToRecv -= bytesRcvd;

            buf += bytesRcvd;

        } while(bytesToRecv > 0);

        // Determine which command to process from the client

        notify = false;

        switch(msg.command)
        {
        case STC_CMD_STOP:
            Transport_PostButtonPress(S_STOP);
            break;

        case STC_CMD_REW:
            mask = S_REW;
            /* simulate REC+FWD for lib wind mode */
            if (msg.param1 & STC_M_LIBWIND)
                mask |= S_REC;
            Transport_PostButtonPress(mask);
            //Transport_Rew(0, msg.param1);
            break;

        case STC_CMD_FWD:
            mask = S_FWD;
            /* simulate REC+FWD for lib wind mode */
            if (msg.param1 & STC_M_LIBWIND)
                mask |= S_REC;
            Transport_PostButtonPress(mask);
            //Transport_Fwd(0, msg.param1);
            break;

        case STC_CMD_PLAY:
            if (msg.param1 == 1)
                Transport_PostButtonPress(S_PLAY|S_REC);
            else
                Transport_PostButtonPress(S_PLAY);
            break;

        case STC_CMD_LIFTER:
            Transport_PostButtonPress(S_LDEF);
            break;

        case STC_CMD_LOCATE:
            // Start the auto-locator for the cue point index given
            if (msg.param1 <= 9)
                Remote_PostSwitchPress(smask[msg.param1], (uint32_t)msg.param2);
            else
                LocateSearch(LAST_CUE_POINT,  (uint32_t)msg.param2);
            break;

        case STC_CMD_LOCATE_MODE:
            /* 1=store-mode, 0=cue-mode */
            Remote_PostSwitchPress((msg.param1 == 1) ? SW_STORE : SW_CUE, 0);
            break;

        case STC_CMD_CUEPOINT_CLEAR:
            if (msg.param1 == 0xFFFF)
                CuePointClearAll();
            else
                CuePointClear((size_t)msg.param1);
            notify = true;
            break;

        case STC_CMD_TRACK_SET_STATE:
            /* param0 0=index, param1=flags */
            if (msg.param1 == 0xFFFF)
                Track_SetAll((uint8_t)msg.param2, 0);
            else
                Track_SetState((size_t)msg.param1, (uint8_t)msg.param2, 0);
            notify = true;
            break;
        }

        // Now send the response packet

        bytesToSend = sizeof(STC_COMMAND_HDR);

        buf = (uint8_t*)&msg;

        do {

            if ((bytesSent = send(clientfd, buf, bytesToSend, 0)) <= 0)
            {
                System_printf("Error: tpc send failed %d.\n", bytesSent);
                connected = false;
                break;
            }

            bytesToSend -= bytesSent;

            buf += bytesSent;

        } while (bytesToSend > 0);

        /* Notify refresh of current transport state change */

        if (notify)
            Event_post(g_eventTransport, Event_Id_03);
    }

    System_printf("tcpCommandWorker DISCONNECT clientfd = 0x%x\n", clientfd);
    System_flush();

    close(clientfd);
}

// End-Of-File
