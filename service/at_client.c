
#include "service_log.h"
#include "at_client.h"
#include <stdarg.h>
#include "command.h"

#define AT_RESP_END_OK                 "OK"
#define AT_RESP_END_ERROR              "ERROR"
#define AT_RESP_END_FAIL               "FAIL"
#define AT_END_CR_LF                   "\r\n"

#define AT_CMD_MAX_LEN 512

static char send_buf[AT_CMD_MAX_LEN];

static u16 last_cmd_len = 0;

AtClientStruct at_client;

AtClientStruct *at_client_get(void)
{
    return &at_client;
}


u16 at_device_write(DeviceInfo* device, char *buff , u16 len)
{
    u16 i = 0;

    for(i = 0 ; i < len ; i++)
    {
        device->putc(buff[i]);
    }

    return i;
}

const char *at_get_last_cmd(u16 *cmd_size)
{
    *cmd_size = last_cmd_len;
    return send_buf;
}


u16 at_vprintf(DeviceInfo* device, const char *format, va_list args)
{
    last_cmd_len = vsnprintf(send_buf, sizeof(send_buf), format, args);

    send_buf[last_cmd_len] = 0;

    if(is_at_log_enable())
    LOG(LEVEL_TRANS,"%s",send_buf);

    return at_device_write(device,send_buf, last_cmd_len);
}


u16 at_vprintfln(DeviceInfo* device, const char *format, va_list args)
{
    u16 len;

    len = at_vprintf(device, format, args);

    at_device_write(device,"\r\n", 2);

    return len + 2;
}


void at_client_service_sem_pend(void)
{
    //kk_os_semaphore_request(s_at_service_sem, 0);
}

void at_client_service_sem_release(void)
{
    kk_os_semaphore_release(at_client.rx_notice);
}

ATRespStruct* at_create_resp(u16 buf_size, u16 line_num, u32 timeout)
{
    ATRespStruct *resp = NULL;

    resp = (ATRespStruct *)malloc(sizeof(ATRespStruct));
    if (resp == NULL)
    {
        LOG(LEVEL_ERROR,"AT create response object failed! No memory for response object!");
        return NULL;
    }

    resp->buf = (char *) malloc(buf_size);
    if (resp->buf == NULL)
    {
        LOG(LEVEL_ERROR,"AT create response object failed! No memory for response buffer!");
        free(resp);
        return NULL;
    }

    resp->buf_size = buf_size;
    resp->line_num = line_num;
    resp->line_counts = 0;
    resp->timeout = timeout;

    return resp;
}

/**
 * Delete and free response object.
 *
 * @param resp response object
 */
void at_delete_resp(ATRespStruct* resp)
{
    if (resp && resp->buf)
    {
        free(resp->buf);
    }

    if (resp)
    {
        free(resp);
        resp = NULL;
    }
}

/**
 * Set response object information
 *
 * @param resp response object
 * @param buf_size the maximum response buffer size
 * @param line_num the number of setting response lines
 *         = 0: the response data will auto return when received 'OK' or 'ERROR'
 *        != 0: the response data will return when received setting lines number data
 * @param timeout the maximum response time
 *
 * @return  != NULL: response object
 *           = NULL: no memory
 */
ATRespStruct* at_resp_set_info(ATRespStruct* resp, u16 buf_size, u16 line_num, u32 timeout)
{
    KK_ASSERT(resp);

    if (resp->buf_size != buf_size)
    {
        resp->buf_size = buf_size;

        resp->buf = (char *) realloc(resp->buf, buf_size);
        if (!resp->buf)
        {
            LOG(LEVEL_DEBUG,"No memory for realloc response buffer size(%d).", buf_size);
            return NULL;
        }
    }

    resp->line_num = line_num;
    resp->timeout = timeout;

    return resp;
}

/**
 * Get one line AT response buffer by line number.
 *
 * @param resp response object
 * @param resp_line line number, start from '1'
 *
 * @return != NULL: response line buffer
 *          = NULL: input response line error
 */
const char *at_resp_get_line(ATRespStruct* resp, u16 resp_line)
{
    char *resp_buf = resp->buf;
    char *resp_line_buf = NULL;
    u16 line_num = 1;

    KK_ASSERT(resp);

    if (resp_line > resp->line_counts || resp_line <= 0)
    {
        LOG(LEVEL_ERROR,"AT response get line failed! Input response line(%d) error!", resp_line);
        return NULL;
    }

    for (line_num = 1; line_num <= resp->line_counts; line_num++)
    {
        if (resp_line == line_num)
        {
            resp_line_buf = resp_buf;

            return resp_line_buf;
        }

        resp_buf += strlen(resp_buf) + 1;
    }

    return NULL;
}

/**
 * Get one line AT response buffer by keyword
 *
 * @param resp response object
 * @param keyword query keyword
 *
 * @return != NULL: response line buffer
 *          = NULL: no matching data
 */
const char *at_resp_get_line_by_kw(ATRespStruct* resp, const char *keyword)
{
    char *resp_buf = resp->buf;
    char *resp_line_buf = NULL;
    u16 line_num = 1;

    KK_ASSERT(resp);
    KK_ASSERT(keyword);

    for (line_num = 1; line_num <= resp->line_counts; line_num++)
    {
        if (strstr(resp_buf, keyword))
        {
            resp_line_buf = resp_buf;

            return resp_line_buf;
        }

        resp_buf += strlen(resp_buf) + 1;
    }

    return NULL;
}

/**
 * Get and parse AT response buffer arguments by line number.
 *
 * @param resp response object
 * @param resp_line line number, start from '1'
 * @param resp_expr response buffer expression
 *
 * @return -1 : input response line number error or get line buffer error
 *          0 : parsed without match
 *         >0 : the number of arguments successfully parsed
 */
int at_resp_parse_line_args(ATRespStruct* resp, u16 resp_line, const char *resp_expr, ...)
{
    va_list args;
    int resp_args_num = 0;
    const char *resp_line_buf = NULL;

    KK_ASSERT(resp);
    KK_ASSERT(resp_expr);

    if ((resp_line_buf = at_resp_get_line(resp, resp_line)) == NULL)
    {
        return -1;
    }

    va_start(args, resp_expr);

    resp_args_num = sscanf(resp_line_buf, resp_expr, args);

    va_end(args);

    return resp_args_num;
}

/**
 * Get and parse AT response buffer arguments by keyword.
 *
 * @param resp response object
 * @param keyword query keyword
 * @param resp_expr response buffer expression
 *
 * @return -1 : input keyword error or get line buffer error
 *          0 : parsed without match
 *         >0 : the number of arguments successfully parsed
 */
int at_resp_parse_line_args_by_kw(ATRespStruct* resp, const char *keyword, const char *resp_expr, ...)
{
    va_list args;
    int resp_args_num = 0;
    const char *resp_line_buf = NULL;

    KK_ASSERT(resp);
    KK_ASSERT(resp_expr);

    if ((resp_line_buf = at_resp_get_line_by_kw(resp, keyword)) == NULL)
    {
        return -1;
    }

    va_start(args, resp_expr);

    resp_args_num = vsscanf(resp_line_buf, resp_expr, args);

    va_end(args);

    return resp_args_num;
}

/**
 * Send commands to AT server and wait response.
 *
 * @param client current AT client object
 * @param resp AT response object, using NULL when you don't care response
 * @param cmd_expr AT commands expression
 *
 * @return 0 : success
 *        -1 : response status error
 *        -10 : wait timeout
 *        -2 : enter AT CLI mode
 */
int at_obj_exec_cmd(AtClientStruct* client, ATRespStruct* resp, const char *cmd_expr, ...)
{
    va_list args;
    u16 cmd_size = 0;
    KK_ERRCODE result = KK_SUCCESS;
    const char *cmd = NULL;

    KK_ASSERT(cmd_expr);

    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return KK_PARAM_ERROR;
    }

    /* check AT CLI mode */
    if (client->status == AT_STATUS_CLI && resp)
    {
        return KK_NOT_INIT;
    }

    //kk_os_mutex_request(client->lock);

    client->resp_status = AT_RESP_OK;
    client->presp = resp;

    va_start(args, cmd_expr);
    at_vprintfln(client->pdevice, cmd_expr, args);
    va_end(args);

    if (resp != NULL)
    {
        resp->line_counts = 0;
        //kk_os_semaphore_clear(client->resp_notice);
        if (kk_os_semaphore_request(client->resp_notice, resp->timeout) != KK_SUCCESS)
        {
            cmd = at_get_last_cmd(&cmd_size);
            LOG(LEVEL_ERROR,"execute command (%.*s) timeout (%d ticks),err = %d!", cmd_size, cmd, resp->timeout,event_err);
            client->resp_status = AT_RESP_TIMEOUT;
            result = KK_TIMOUT_ERROR;
            goto __exit;
        }
        if (client->resp_status != AT_RESP_OK)
        {
            cmd = at_get_last_cmd(&cmd_size);
            LOG(LEVEL_ERROR,"execute command (%.*s) failed!", cmd_size, cmd);
            result = KK_UNKNOWN;
            goto __exit;
        }
    }

__exit:
    client->presp = NULL;

    //kk_os_mutex_release(client->lock);

    return result;
}

/**
 * Waiting for connection to external devices.
 *
 * @param client current AT client object
 * @param timeout millisecond for timeout
 *
 * @return 0  : success
 *        -11 : timeout
 *        -3  : no memory
 */
int at_client_obj_wait_connect(AtClientStruct* client, u32 timeout)
{
    KK_ERRCODE result = KK_SUCCESS;
    ATRespStruct* resp = NULL;
    u32 start_time = 0;

    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return KK_PARAM_ERROR;
    }

    resp = at_create_resp(16, 0, 500);
    if (resp == NULL)
    {
        LOG(LEVEL_ERROR,"No memory for response object!");
        return KK_MEM_NOT_ENOUGH;
    }

    kk_os_mutex_request(client->lock);
    client->presp = resp;

    start_time = kk_os_ticks_get();

    while (1)
    {
        /* Check whether it is timeout */
        if (kk_os_ticks_clac_timeout(start_time,timeout))
        {
            LOG(LEVEL_ERROR,"wait connect timeout (%d millisecond)!", timeout);
            result = KK_TIMOUT_ERROR;
            break;
        }

        /* Check whether it is already connected */
        resp->line_counts = 0;
        at_device_write(client->pdevice,"AT\r\n", 4);

        if (kk_os_semaphore_request(client->resp_notice, resp->timeout) != KK_SUCCESS)
            continue;
        else
            break;
    }

    at_delete_resp(resp);

    client->presp = NULL;

    kk_os_mutex_release(client->lock);

    return result;
}

/**
 * Send data to AT server, send data don't have end sign(eg: \r\n).
 *
 * @param client current AT client object
 * @param buf   send data buffer
 * @param size  send fixed data size
 *
 * @return >0: send data size
 *         =0: send failed
 */
#if 0
u16 at_client_obj_send(AtClientStruct* client, const char *buf, u16 size)
{
    KK_ASSERT(buf);

    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("send", buf, size);
#endif

    return at_device_write(client->pdevice, (char *)buf, size);
}
#else
u16 at_client_obj_send(AtClientStruct* client, ATRespStruct* resp, const char *buf, u16 size)
{
    u16 result = size;
    
    KK_ASSERT(buf);

    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

    /* check AT CLI mode */
    if (client->status == AT_STATUS_CLI && resp)
    {
        return 0;
    }

    kk_os_mutex_request(client->lock);

    client->resp_status = AT_RESP_OK;
    client->presp = resp;

    at_device_write(client->pdevice, (char *)buf, size);

    if (resp != NULL)
    {
        resp->line_counts = 0;
        //kk_os_semaphore_clear(client->resp_notice);
        if (kk_os_semaphore_request(client->resp_notice, resp->timeout) != KK_SUCCESS)
        {
            //cmd = at_get_last_cmd(&cmd_size);
            LOG(LEVEL_ERROR,"client transfrom data timeout (%d ticks)!",resp->timeout);
            client->resp_status = AT_RESP_TIMEOUT;
            result = 0;
            goto __exit;
        }
        if (client->resp_status != AT_RESP_OK)
        {
            //cmd = at_get_last_cmd(&cmd_size);
            LOG(LEVEL_ERROR,"client transfrom data(%d) failed!",size);
            result = 0;
            goto __exit;
        }
    }

__exit:
    client->presp = NULL;

    kk_os_mutex_release(client->lock);

    return result;

}

#endif

KK_ERRCODE at_client_get_resp_chr(AtClientStruct* client,u8 *chr)
{
    FifoType *fifo = NULL;

    fifo = client->pdevice->rx;
    
    if(fifo->read_idx != fifo->write_idx)
    {
        *chr = fifo->base_addr[fifo->read_idx];

        fifo->read_idx = (fifo->read_idx + 1)%fifo->size;

        return KK_SUCCESS;
    }

    return KK_EMPTY_BUF;
}


FifoType *at_client_resp_fifo(AtClientStruct* client)
{
    return client->pdevice->rx;
}


KK_ERRCODE at_client_obj_peek_resp_str(AtClientStruct* client,char *str , u16 len)
{
    return fifo_peek(client->pdevice->rx,(u8 *)str,len);
}

/**
 * AT client receive fixed-length data.
 *
 * @param client current AT client object
 * @param buf   receive data buffer
 * @param size  receive fixed data size
 * @param timeout  receive data timeout (ms)
 *
 * @note this function can only be used in execution function of URC data
 *
 * @return >0: receive data size
 *         =0: receive failed
 */
u16 at_client_obj_recv(AtClientStruct* client, char *buf, u16 size, u32 timeout)
{
    u32 read_idx = 0;
    KK_ERRCODE result = KK_SUCCESS;
    u8 ch;

    KK_ASSERT(buf);

    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

    while (1)
    {
        if (read_idx < size)
        {
            result = at_client_get_resp_chr(client,&ch);
            if (result != KK_SUCCESS)
            {
                LOG(LEVEL_ERROR,"AT Client receive failed, uart device get data error(%d)", result);
                return 0;
            }

            buf[read_idx++] = (char)ch;
        }
        else
        {
            break;
        }
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("urc_recv", buf, size);
#endif

    return read_idx;
}

/**
 *  AT client set end sign.
 *
 * @param client current AT client object
 * @param ch the end sign, can not be used when it is '\0'
 */
void at_obj_set_end_sign(AtClientStruct* client, char ch)
{
    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return;
    }

    client->end_sign = ch;
}

/**
 * set URC(Unsolicited Result Code) table
 *
 * @param client current AT client object
 * @param table URC table
 * @param size table size
 */
void at_obj_set_urc_table(AtClientStruct* client,  ATUrcStruct *urc_table, u16 table_sz)
{
    u16 idx;

    if (client == NULL)
    {
        LOG(LEVEL_ERROR,"input AT Client object is NULL, please create or get AT Client object!");
        return;
    }

    for (idx = 0; idx < table_sz; idx++)
    {
        KK_ASSERT(urc_table[idx].cmd_prefix);
        KK_ASSERT(urc_table[idx].cmd_suffix);
    }

    client->purc_table = urc_table;
    client->urc_table_size = table_sz;
}



static ATUrcStruct *get_urc_obj(AtClientStruct* client)
{
    u8 i, prefix_len, suffix_len;
    u8 buf_sz;
    char *buffer = NULL;

    if (client->purc_table == NULL)
    {
        return NULL;
    }

    buffer = client->recv_buffer;
    buf_sz = client->cur_recv_len;

    for (i = 0; i < client->urc_table_size; i++)
    {
        prefix_len = strlen(client->purc_table[i].cmd_prefix);
        suffix_len = strlen(client->purc_table[i].cmd_suffix);
        if (buf_sz < prefix_len + suffix_len)
        {
            continue;
        }
        if ((prefix_len ? !strncmp(buffer, client->purc_table[i].cmd_prefix, prefix_len) : 1)
                && (suffix_len ? !strncmp(buffer + buf_sz - suffix_len, client->purc_table[i].cmd_suffix, suffix_len) : 1))
        {
            return &client->purc_table[i];
        }
    }

    return NULL;
}



static int recv_one_sentence(AtClientStruct *client)
{
    u16 read_len = 0;
    u8 ch = 0, last_ch = 0;
    bool is_full = false;

    memset(client->recv_buffer,0,client->recv_bufsz);
    
    while(at_client_get_resp_chr(client,&ch) == KK_SUCCESS)
    {
        if (read_len < client->recv_bufsz)
        {
            client->recv_buffer[read_len++] = (char)ch;
            client->cur_recv_len = read_len;
        }
        else
        {
            is_full = true;
        }

        /* is newline or URC data */
        if ((ch == '\n' && last_ch == '\r')/* || (client->end_sign != 0 && ch == client->end_sign)*/ || get_urc_obj(client))
        {
            if (read_len > 2)
            {
                
                if (is_full)
                {
                    LOG(LEVEL_ERROR,"read line failed. The line data length is out of buffer size(%d)!", client->recv_bufsz);
                    memset(client->recv_buffer, 0x00, client->recv_bufsz);
                    client->cur_recv_len = 0;
                    return 0;
                }
                if(is_at_log_enable())
                LOG(LEVEL_TRANS,"%s",client->recv_buffer);

                break;
            }
            else
            {
                read_len = 0;
            }
        }
        last_ch = ch;
    }

    return read_len;
}

void at_resp_service_proc(void *arg)
{
    u16 resp_buf_len = 0;
    u16 line_counts  = 0;
    ATUrcStruct *urc = NULL;
    AtClientStruct *client = &at_client;
    
    for(;;)
    {
        kk_os_semaphore_request(client->rx_notice, 0);
        
        kk_os_sched_lock();

        while(recv_one_sentence(client) > 0)
        {
            if ((urc = get_urc_obj(client)) != NULL)
            {
                /* current receive is request, try to execute related operations */
                if (urc->func != NULL)
                {
                    urc->func(client->recv_buffer, client->cur_recv_len);
                }
            }
            else if (client->presp != NULL)
            {
                /* current receive is response */
                client->recv_buffer[client->cur_recv_len - 1] = '\0';
                if (resp_buf_len + client->cur_recv_len < client->presp->buf_size)
                {
                    /* copy response lines, separated by '\0' */
                    memcpy(client->presp->buf + resp_buf_len, client->recv_buffer, client->cur_recv_len);
                    resp_buf_len += client->cur_recv_len;

                    line_counts++;
                }
                else
                {
                    client->resp_status = AT_RESP_BUFF_FULL;
                    LOG(LEVEL_ERROR,"Read response buffer failed. The Response buffer size is out of buffer size(%d)!", client->presp->buf_size);
                }
                /* check response result */
                if (memcmp(client->recv_buffer, AT_RESP_END_OK, strlen(AT_RESP_END_OK)) == 0
                        && client->presp->line_num == 0)
                {
                    /* get the end data by response result, return response state END_OK. */
                    client->resp_status = AT_RESP_OK;
                }
                else if (strstr(client->recv_buffer, AT_RESP_END_ERROR)
                        || (memcmp(client->recv_buffer, AT_RESP_END_FAIL, strlen(AT_RESP_END_FAIL)) == 0))
                {
                    client->resp_status = AT_RESP_ERROR;
                }
                else if (line_counts == client->presp->line_num && client->presp->line_num)
                {
                    /* get the end data by response line, return response state END_OK.*/
                    client->resp_status = AT_RESP_OK;
                }
                else if(client->end_sign != 0 && client->recv_buffer[0] == client->end_sign)
                {
                    client->resp_status = AT_RESP_OK;
                }
                else
                {
                    continue;
                }
                client->presp->line_counts = line_counts;

                client->presp = NULL;
                kk_os_semaphore_release(client->resp_notice);
                resp_buf_len = 0, line_counts = 0;
            }
            else
            {
                LOG(LEVEL_WARN,"unrecognized line: %.*s", client->cur_recv_len, client->recv_buffer);
            }

        }

        kk_os_sched_unlock();
    }
}

KK_ERRCODE at_client_init(void)
{
    if(at_client.recv_buffer)free(at_client.recv_buffer);

    memset((u8 *)&at_client,0,sizeof(at_client));
    
    at_client.pdevice   = uart_board_init("uart1",DMA_NUL,4096);

    at_client.rx_notice = kk_os_semaphore_create();

    at_client.lock      = kk_os_mutex_create();

    at_client.resp_notice = kk_os_semaphore_create();

    if(!at_client.pdevice || at_client.rx_notice < 0 || at_client.lock < 0)
    {
        LOG(LEVEL_ERROR,"AT channel create fail!");
    }

    at_client.recv_buffer = malloc(ONE_SENTENCE_MAX_LEN);

    at_client.recv_bufsz  = ONE_SENTENCE_MAX_LEN;

    at_client.pdevice->read_cb = at_client_service_sem_release;

    return KK_SUCCESS;
}


