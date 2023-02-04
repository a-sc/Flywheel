#ifndef __USART_H
#define __USART_H

#include <stdint.h>

#define USART_FIFO_SIZE 0x40

typedef struct {
    int wr_ptr, rd_ptr;
    int count;
    uint8_t buf[USART_FIFO_SIZE];
} usart_fifo_t;

extern volatile usart_fifo_t usart_tx_fifo, usart_rx_fifo;

static inline int usart_fifo_empty(volatile usart_fifo_t *fifo)
{
    return fifo->count == 0;
}

static inline int usart_fifo_full(volatile usart_fifo_t *fifo)
{
    return fifo->count == USART_FIFO_SIZE - 1;
}

static inline void usart_fifo_push(volatile usart_fifo_t *fifo, uint8_t c)
{
    if(fifo->count == USART_FIFO_SIZE - 1)
	return;

    fifo->buf[fifo->wr_ptr++] = c;
    if(fifo->wr_ptr == USART_FIFO_SIZE)
	    fifo->wr_ptr = 0;
    fifo->count++;
}

static inline uint8_t usart_fifo_pop(volatile usart_fifo_t *fifo)
{


    uint8_t c = fifo->buf[fifo->rd_ptr++];

    if(fifo->rd_ptr == USART_FIFO_SIZE)
	fifo->rd_ptr = 0;

    fifo->count--;

    return c;
}

static inline void usart_fifo_init(volatile usart_fifo_t *fifo)
{
    fifo->rd_ptr = 0;
    fifo->wr_ptr = 0;
    fifo->count = 0;
}

void usart_init();
void usart_send ( const uint8_t *buf, int count );
int usart_poll();
int usart_recv ( uint8_t *buf, int count, int blocking );
int usart_rx_char();
void usart_tx_char (int c);


#endif
