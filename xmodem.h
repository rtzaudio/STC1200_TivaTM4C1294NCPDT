#ifndef _XMODEM_H_
#define _XMODEM_H_


typedef enum XMODEM_ERROR {
    XMODEM_SUCCESS = 0,
    XMODEM_NO_REPLY,
    XMODEM_TIMEOUT,
    XMODEM_CANCEL,
    XMODEM_MAX_RETRIES,
    XMODEM_FILE_WRITE,
    XMODEM_FILE_READ
} XMODEM_ERROR;

/* Interface Functions */

int xmodem_receive(UART_Handle handle, FIL* fp);
int xmodem_send(UART_Handle handle, FIL* fp);

#endif /* _XMODEM_H_ */
