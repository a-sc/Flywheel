#include <stdio.h>
#include <sys/stat.h>

#include "stm32f4xx.h"
#include "usart.h"

static volatile usart_fifo_t tx_fifo, rx_fifo;

extern int __end__; /* From linker script */

void usart_init()
{
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3; // PA.2 USART2_TX, PA.3 USART2_RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* Connect USART pins to AF */
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

  USART_InitTypeDef USART_InitStructure;

  /* USARTx configuration ------------------------------------------------------*/
  /* USARTx configured as follow:
        - BaudRate = 115200 baud
        - Word Length = 8 Bits
        - One Stop Bit
        - No parity
        - Hardware flow control disabled (RTS and CTS signals)
        - Receive and transmit enabled
  */
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

  USART_Init(USART2, &USART_InitStructure);

  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F; //lowest priority
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  usart_fifo_init(&tx_fifo);
  usart_fifo_init(&rx_fifo);

  USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

  USART_Cmd(USART2, ENABLE);
}

void USART2_IRQHandler(void)
{
  uint8_t rx_data;
  
  if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
    rx_data = USART_ReceiveData(USART2) & 0xff;
    usart_fifo_push(&rx_fifo, rx_data);
    usart_send(&rx_data, 1);
  }
  if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET)
  {
  
  if (!usart_fifo_empty(&tx_fifo))
    USART_SendData(USART2, usart_fifo_pop(&tx_fifo));
  else
    USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
    }
}

void usart_send(const uint8_t *buf, int count)
{
  int irq_enabled = 0;

  while (count--)
  {
    while (usart_fifo_full(&tx_fifo))
    {
      if (!irq_enabled)
      {
        USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
        irq_enabled = 1;
      }
    }

    NVIC_DisableIRQ(USART2_IRQn);
    usart_fifo_push(&tx_fifo, *buf++);
    NVIC_EnableIRQ(USART2_IRQn);
  }

  USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

int usart_recv(uint8_t *buf, int count, int blocking)
{
  int n = 0;
  uint8_t char_read;
  // printf("usart_recv called with count=%d\n", count);
  while (n < count)
  {
    if (rx_fifo.count)
    {
      NVIC_DisableIRQ(USART2_IRQn);
      char_read = usart_fifo_pop(&rx_fifo);
      if (char_read == '\r')
	buf[n] = '\n';
      else
        buf[n] = char_read;
      NVIC_EnableIRQ(USART2_IRQn);
      n++;
      // printf("One char read: 0x%x\n", char_read);
      if (char_read == '\r')
	 break;
    }
    else if (!blocking)
      break;
  }
  // printf("usart_recv returning %d\n", n);
  return n;
}

/* Functions to support the use of printf and scanf in newlib-nano       */
/* See https://interrupt.memfault.com/blog/boostrapping-libc-with-newlib */
/* for details                                                           */

int _write(int handle, char *data, int size ) 
{
    int count ;

    for( count = 0; count < size; count++) 
    {
      usart_send(data++, 1) ;  // Your low-level output function here.
    }

    return count;
}

int _fstat(int file, struct stat *st) {
  st->st_mode = S_IFCHR;

  return 0;
}

int _close(int file) {
  return -1;
}

int _lseek(int file, int ptr, int dir) {
  return 0;
}

int _read (int file, char * ptr, int len) {
  int read = 0;

  read = usart_recv(ptr, len, 1);
  return read;
}

int _isatty(int file) {
  return 1;
}


void *_sbrk(int incr) {
  static unsigned char *heap = NULL;
  unsigned char *prev_heap;

  if (heap == NULL) {
    heap = (unsigned char *)&__end__;
    /* See https://interrupt.memfault.com/blog/how-to-write-linker-scripts-for-firmware */
    /* for an explanation of where the __end__ symbol comes from */
  }
  prev_heap = heap;

  heap += incr;

  return prev_heap;
}
