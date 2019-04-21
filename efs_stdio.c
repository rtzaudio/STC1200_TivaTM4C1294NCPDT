
/*
 * Copyright (c) 2013, Texas Instruments Incorporated
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
 * */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/knl/Task.h>

#include <ti/ndk/inc/netmain.h>

#define PREFIX "fat:0:"

struct FxnTable {
    const char *name;
    EFSFUN fxn;
};

#define CGITABLECOUNT  20

#define READBUFSIZE     512

static struct FxnTable cgiTable[CGITABLECOUNT];

/****************************************************************************/
/*                                                                          */
/****************************************************************************/

static int findCGI(char *name)
{
    int i;
    int status = -1;

    for (i = 0; cgiTable[i].name; i++) {
        if (strcmp(cgiTable[i].name, name) == 0) {
            status = i;
            break;
        }
    }

    return status;
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/

void efs_createfilecb(char *name, INT32 length, UINT8 *pData,
                      EFSFUN pllDestroyFun, UINT32 MemMgrArg)
{
}

int efs_fclose(EFS_FILE *stream)
{
    return fclose(stream);
}

INT32 efs_getfilesize(EFS_FILE *f)
{
    long cur;
    long size;

    cur = ftell(f);
    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    fseek(f, cur, SEEK_SET);

    return size;
}

void efs_createfile(char *name, INT32 length, UINT8 *pData)
{
    UInt key;
    int i;

    key = Task_disable();

    for (i = 0; cgiTable[i].name && i < CGITABLECOUNT; i++) {
        ;
    }

    if (i < CGITABLECOUNT) {
        cgiTable[i].name = name;
        cgiTable[i].fxn = (EFSFUN)pData;
    }

    Task_restore(key);
}

EFS_FILE *efs_fopen(char *name, char *mode)
{
    char *prefixName;
    EFS_FILE *file = NULL;

    if ((prefixName = malloc(sizeof(PREFIX) + strlen(name) + 1)) == NULL) {
        System_abort("efs_open() out of memory!\n");
        return NULL;
    }

    strcat(strcpy(prefixName, PREFIX), name);
    file = fopen(prefixName, mode);
    free(prefixName);

    return file;
}

size_t efs_filesend(EFS_FILE *stream, size_t size, SOCKET s)
{
    char *buf;
    size_t total = size;
    int cnt;

    if ((buf = malloc(READBUFSIZE)) == NULL)
    {
        System_abort("efs_filesend() out of memory!\n");
    }
    else
    {
        for (total = size; total > 0; total -= cnt)
        {
            cnt = fread(buf, 1, READBUFSIZE, stream);
            if (cnt == 0) {
                break;
            }
            cnt = send(s, buf, cnt, 0);
            if (cnt < 0) {
                break;
            }
        }

        free(buf);
    }

    return (size - total);
}

int efs_filecheck(char *name, char *user, char *pass, int *prealm)
{
    EFS_FILE *file;

    if (findCGI(name) >= 0) {
        return (EFS_FC_EXECUTE);
    }

    if ((file = efs_fopen(name, "rb")) == NULL) {
        return (EFS_FC_NOTFOUND);
    }

    efs_fclose(file);

    return (0);
}

EFSFUN efs_loadfunction(char *name)
{
    EFSFUN fxn;
    int index;

    if ((index = findCGI(name)) < 0) {
        fxn = NULL;
    }
    else {
        fxn = cgiTable[index].fxn;
    }

    return (fxn);
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/

/* 
 * The following functions are not used but need to be defined to 
 * avoid the NDK's efs.c functions from being pulled in.
 */ 
void efs_destroyfile(char *name)
{
}
int efs_feof(EFS_FILE *stream)
{
    return(0);
}
size_t efs_fread(void *ptr, size_t size, size_t nobj, EFS_FILE *stream)
{
    return(0);
}
size_t efs_fwrite(void *ptr, size_t size, size_t nobj, EFS_FILE *stream)
{
    return(0);
}
INT32 efs_fseek(EFS_FILE *stream, INT32 offset, int origin)
{
    return(0);
}
INT32 efs_ftell(EFS_FILE *stream)
{
    return(0);
}
void efs_rewind(EFS_FILE *stream)
{
}
