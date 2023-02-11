#include "pp-printf.h"
#include "stm32f4xx.h"
#include "usart.h"

static volatile usart_fifo_t tx_fifo, rx_fifo;

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

int puts(const char *s)
{
  int n = 0;
  while (*s)
  {
    usart_send(s, 1);
    s++;
    n++;
  }
  return n;
}

void USART2_IRQHandler(void)
{
  if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    usart_fifo_push(&rx_fifo, USART_ReceiveData(USART2) & 0xff);

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

void usart_tx_char(int c)
{
  while (usart_fifo_full(&tx_fifo))
    ;

  NVIC_DisableIRQ(USART2_IRQn);
  usart_fifo_push(&tx_fifo, c);
  NVIC_EnableIRQ(USART2_IRQn);
}

int usart_poll()
{
  return rx_fifo.count;
}

int usart_recv(uint8_t *buf, int count, int blocking)
{
  int n = 0;
  while (n < count)
  {
    if (rx_fifo.count)
    {
      NVIC_DisableIRQ(USART2_IRQn);
      buf[n] = usart_fifo_pop(&rx_fifo);
      NVIC_EnableIRQ(USART2_IRQn);
      n++;
    }
    else if (!blocking)
      break;
  }
  return n;
}

int usart_rx_char()
{
  if (!rx_fifo.count)
    return -1;

  return usart_fifo_pop(&rx_fifo);
}
