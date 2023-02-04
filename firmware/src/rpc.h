#ifndef __RPC_H
#define __RPC_H

#define RPC_BUFFER_SIZE 2048

#define RPC_ID_ADC_TEST 0x1
#define RPC_ID_SET_ESC_SPEED 0x2
#define RPC_ID_GET_ESC_SPEED 0x3
#define RPC_ID_SERVO_RESPONSE 0x4
#define RPC_ID_PROFILE_HEIGHT 0x5
#define RPC_ID_LDRIVE_STEP 0x6
#define RPC_ID_LDRIVE_READ_ENCODER 0x7
#define RPC_ID_LDRIVE_CHECK_IDLE 0x8
#define RPC_ID_LDRIVE_GO_HOME 0x9
#define RPC_ID_SERVO_SET_HEIGHTMAP 0xa
#define RPC_ID_SERVO_ENABLE_HEIGHTMAP 0xb
#define RPC_ID_ETCH_SINGLE_SPOT 0xc
#define RPC_ID_PROBE_AND_ETCH 0xd

struct rpc_request {
    int id;
    int size;
    int pos;
    void *data;
};

void rpc_init();
int rpc_request_get(struct rpc_request *rq);
void rpc_answer_push(void *data, int size);
void rpc_answer_commit(int id);
void rpc_answer_send(int id, int size, void *data);
void rpc_test();

static inline int rpc_pop_int32 ( struct rpc_request *rq )
{
    uint8_t *buf = rq->data + rq->pos;
    int x = 0;

    x|=((int)buf[0])<<24;
    x|=((int)buf[1])<<16;
    x|=((int)buf[2])<<8;
    x|=((int)buf[3])<<0;

    rq->pos += 4;
    return x;
}


#endif

