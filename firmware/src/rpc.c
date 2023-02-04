#include <stdint.h>
#include <string.h>

#include "usart.h"
#include "rpc.h"
#include "pp-printf.h"

#define RPC_MAGIC_CHAR 0x01

enum rpc_rx_state {
    IDLE = 0,
    SIZE0,
    SIZE1,
    ID0,
    ID1,
    DATA
};

static uint8_t rpc_buf[RPC_BUFFER_SIZE];
static int rpc_size, rpc_n, rpc_id;
static int rpc_state;

void rpc_init()
{
    rpc_state = IDLE;
}

int rpc_request_get(struct rpc_request *rq)
{
    if(!usart_poll())
        return 0;

    int c = usart_rx_char();

    switch(rpc_state)
    {
        case IDLE:
            if (c == RPC_MAGIC_CHAR)
                rpc_state = SIZE0;
            break;
        case SIZE0:
            rpc_size = (c << 8) & 0xff00;
            rpc_state = SIZE1;
            break;
        case SIZE1:
            rpc_size |= (c << 0) & 0xff;
            rpc_n = 0;
            rpc_state = ID0;
            break;
        case ID0:
            rpc_id = (c << 8) & 0xff00;
            rpc_state = ID1;
            break;
        case ID1:
            rpc_id |= (c << 0) & 0xff;
            rpc_state = DATA;
            break;
        case DATA:
            rpc_buf[rpc_n++] = c;
            if(rpc_n == rpc_size)
            {
                rq->id = rpc_id;
                rq->size = rpc_size;
                rq->data = &rpc_buf;
                rq->pos = 0;
                rpc_n = 0;
                rpc_state = IDLE;
                return 1;
            }
        default:
            break;
    }

    return 0;
}

void rpc_answer_push(void *data, int size)
{
    if (rpc_n + size < RPC_BUFFER_SIZE)
    {
        memcpy(rpc_buf + rpc_n, data, size);
        rpc_n += size;
    }
}

void rpc_answer_commit(int id)
{
    usart_tx_char(RPC_MAGIC_CHAR);
    usart_tx_char((rpc_n >> 8) & 0xff);
    usart_tx_char((rpc_n >> 0) & 0xff);
    usart_tx_char((id >> 8) & 0xff);
    usart_tx_char((id >> 0) & 0xff);
    usart_send(rpc_buf, rpc_n);
    rpc_n = 0;
}

void rpc_answer_send(int id, int size, void *data)
{
    usart_tx_char(RPC_MAGIC_CHAR);
    usart_tx_char((size >> 8) & 0xff);
    usart_tx_char((size >> 0) & 0xff);
    usart_tx_char((id >> 8) & 0xff);
    usart_tx_char((id >> 0) & 0xff);
    usart_send(data, size);
}

void rpc_test()
{
    for(;;)
    {
        struct rpc_request rq;

        if(rpc_request_get(&rq))
        {
            char *hello = "Hello, world!";
            pp_printf("Got RPC request %x, size %d\n\r", rq.id, rq.size);

            rpc_answer_push(hello, strlen(hello));
            rpc_answer_commit(0x5678);
        }


    }
}