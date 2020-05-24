
#ifndef __AT_CLIENT_H__
#define __AT_CLIENT_H__

#include "kk_os_header.h"
#include "seril_driver.h"
#include <stdarg.h>


#define  ONE_SENTENCE_MAX_LEN 256

typedef struct
{
    char *buf;

    u16  buf_size;

    u16 line_num;

    u16 line_counts;

    u16 timeout;

    
}ATRespStruct;

typedef enum 
{
     AT_RESP_OK = 0,                   /* AT response end is OK */
     AT_RESP_ERROR = -1,               /* AT response end is ERROR */
     AT_RESP_TIMEOUT = -2,             /* AT response is timeout */
     AT_RESP_BUFF_FULL= -3,            /* AT response buffer is full */
}ATRespStatusEnum;



typedef enum
{
    AT_STATUS_UNINITIALIZED = 0,
    AT_STATUS_INITIALIZED,
    AT_STATUS_CLI,
}ATStatusEnum;


/* URC(Unsolicited Result Code) object, such as: 'RING', 'READY' request by AT server */
typedef struct
{
    const char *cmd_prefix;
    const char *cmd_suffix;
    void (*func)(const char *data, u16 size);
}ATUrcStruct;


typedef struct
{
    DeviceInfo* pdevice;

    ATStatusEnum status;
    char end_sign;

    char *recv_buffer;
    u16 recv_bufsz;
    u16 cur_recv_len;
    s32 rx_notice;
    s32 lock;

    ATRespStruct *presp;
    s32 resp_notice;
    ATRespStatusEnum resp_status;

    ATUrcStruct *purc_table;
    u16 urc_table_size;

    //rt_thread_t parser;
}AtClientStruct;



//KK_ERRCODE at_client_service_create(void);

AtClientStruct *at_client_get(void);

KK_ERRCODE at_client_obj_peek_resp_str(AtClientStruct* client,char *str , u16 len);

int at_obj_exec_cmd(AtClientStruct* client, ATRespStruct* resp, const char *cmd_expr, ...);

int at_client_obj_wait_connect(AtClientStruct* client, u32 timeout);

u16 at_client_obj_send(AtClientStruct* client,ATRespStruct* resp, const char *buf, u16 size);

u16 at_client_obj_recv(AtClientStruct* client, char *buf, u16 size, u32 timeout);

void at_obj_set_end_sign(AtClientStruct* client, char ch);

void at_obj_set_urc_table(AtClientStruct* client,  ATUrcStruct *urc_table, u16 table_sz);

KK_ERRCODE at_client_init(void);

ATRespStruct* at_create_resp(u16 buf_size, u16 line_num, u32 timeout);

void at_delete_resp(ATRespStruct* resp);

ATRespStruct* at_resp_set_info(ATRespStruct* resp, u16 buf_size, u16 line_num, u32 timeout);

int at_resp_parse_line_args_by_kw(ATRespStruct* resp, const char *keyword, const char *resp_expr, ...);

const char *at_resp_get_line(ATRespStruct* resp, u16 resp_line);

const char *at_resp_get_line_by_kw(ATRespStruct* resp, const char *keyword);

KK_ERRCODE at_client_get_resp_chr(AtClientStruct* client,u8 *chr);

FifoType *at_client_resp_fifo(AtClientStruct* client);


void at_resp_service_proc(void *arg);



#define at_exec_cmd(resp, ...)                   at_obj_exec_cmd(at_client_get(), resp, __VA_ARGS__)
#define at_client_wait_connect(timeout)          at_client_obj_wait_connect(at_client_get(), timeout)
#define at_client_send(resp,buf, size)           at_client_obj_send(at_client_get(),resp,buf, size)
#define at_client_recv(buf, size, timeout)       at_client_obj_recv(at_client_get(), buf, size, timeout)
#define at_set_end_sign(ch)                      at_obj_set_end_sign(at_client_get(), ch)
#define at_set_urc_table(urc_table, table_sz)    at_obj_set_urc_table(at_client_get(), urc_table, table_sz)
#define at_client_peek_resp(buf,size)            at_client_obj_peek_resp_str(at_client_get(),buf,size)    
#define at_get_resp_chr(chr)                     at_client_get_resp_chr(at_client_get(),chr)
#define at_resp_fifo()                           at_client_resp_fifo(at_client_get())

#endif

