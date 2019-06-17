/* Copyright (c) 2014 David Hubbard github.com/davidhubbard
 *
 * libti2cit: an improvement over the tiva i2c driverlib. Use driverlib to initialize hardware, then call these
 * functions for data I/O.
 *
 * TODO: slave mode, interrupt-driven instead of polled, i2c FIFO, and uDMA
 */

/* libti2cit_m_sync_send(): i2c send a buffer and do not return until the send is complete.
 *   addr bit 0 == 0 for write, == 1 for read
 *   if write && len == 0 then libti2cit_m_send() does a "quick_command": i2c start, 8-bit address, i2c stop
 *   if write && len > 0 then libti2cit_m_send() does an i2c write:       i2c start, 8-bit address, data bytes, i2c stop
 *   if addr bit 0 == 1 for read: do not send the i2c stop:               i2c start, 8-bit address, send any data bytes as specified by len
 *      call libti2cit_m_recv() to read data bytes and send i2c stop
 *      it is not possible to do a "quick_command" recv(), i.e. do NOT call recv(len == 0)
 *      the correct way: send(addr bit 0 == 1, len >= 0) followed by a recv(len > 0) -- does a start, send, repeated start, recv, stop
 *
 * returns 0=ack, or > 0 for error
 */
extern uint8_t libti2cit_m_sync_send(uint32_t base, uint8_t addr, uint32_t len, const uint8_t * buf);

/* libti2cit_m_sync_recv(): i2c receive a buffer and do not return until the receive is complete
 *   must be preceded by libti2cit_m_send() with addr bit 0 == 1 for read
 *   len must be non-zero
 *
 * i2c does not support an ack or nack from the slave during a read
 * received data may be all ff's if the slave froze because the i2c bus is open-drain
 */
extern uint8_t libti2cit_m_sync_recv(uint32_t base, uint32_t len, uint8_t * buf);

/* libti2cit_m_sync_recvpart(): i2c receive a buffer but do not send i2c STOP -- for when the length varies based on the data
 *   must be preceded by libti2cit_m_send() with addr bit 0 == 1 for read
 *   call this repeatedly to continue receiving
 *   finally, call with len == 0 to send i2c STOP
 *   you MUST NOT call with len == 0 immediately after the libti2cit_m_send() which sets up the i2c bus (first receive some bytes!)
 *   you SHOULD always call with len == 0 to complete the bus transaction, and MUST NOT call any other functions until you do
 */
extern uint8_t libti2cit_m_sync_recvpart(uint32_t base, uint32_t len, uint8_t * buf);
