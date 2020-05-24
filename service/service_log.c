
#include "service_log.h"
#include <stdarg.h>
#include "kk_os_config_api.h"
#include "socket.h"
#include "command.h"
#define LOG_BUFF_MAX_LEN 1024

UartOptionStruct *g_logHanle;

u8 g_log_level = LEVEL_INFO;

static char s_log_buff[LOG_BUFF_MAX_LEN] = {0};

const char *log_level_string[LEVEL_FATAL + 1] = {
    "[DEBUG]",
    "[INFO]",
    "[WARN]",
    "[ERROR]",
    "[FATAL]",
};

s32 s_log_sem  = -1;
s32 s_log_lock = -1;


void sem_service_log_pend(void)
{
    kk_os_semaphore_request(s_log_sem, 2000);
}

void sem_service_log_release(void)
{
    kk_os_semaphore_release(s_log_sem);
}

void service_log_lock(void)
{
    kk_os_mutex_request(s_log_lock);
}

void service_log_unlock(void)
{
    kk_os_mutex_release(s_log_lock);
}

void service_log_init(void)
{
    s_log_sem  = kk_os_semaphore_create();

    s_log_lock = kk_os_mutex_create();
    
    g_logHanle = uart_board_init("uart3",(DMAInfoStruct)(DMA_TX),256);

    g_logHanle->read_cb = sem_service_log_release;
}

u8 s_uart_command[256] = {0};
u8 s_uart_resp[256] = {0};

void service_log_timer_proc(void)
{
    u16 len  = 256;
    
    if(g_logHanle && g_logHanle->puts)
    g_logHanle->puts();

    if(g_logHanle)
    {
        if(fifo_peek_until(g_logHanle->rx,s_uart_command, &len, '\n') == KK_SUCCESS)
        {
            if(command_on_receive_data(COMMAND_UART,s_uart_command,strlen(s_uart_command),s_uart_resp,NULL) == KK_SUCCESS)
            {
                LOG(LEVEL_TRANS,"%s",s_uart_resp);
            }

            fifo_pop_len(g_logHanle->rx,len);
        }
        else
        {
            if(len  == (sizeof(s_uart_command) - 1))
            {
                fifo_pop_len(g_logHanle->rx,len);
            }
        }
        
    }
}


void log_print(LogLevelEnum level, const char * format, ... )
{  
    
    u16 i,jval = 0; 
    
    va_list args;

    service_log_lock();

    if(level < g_log_level)
    {
        service_log_unlock();
        return;
    }


    va_start (args, format);

    if(level != LEVEL_TRANS)
    jval += sprintf(&s_log_buff[jval],"%s(%d)",log_level_string[level],clock());

    jval += vsnprintf(&s_log_buff[jval],LOG_BUFF_MAX_LEN,format, args);

    if(s_log_buff[jval - 1] != '\n')
    {
        jval += sprintf(&s_log_buff[jval],"\r\n");
    }
    
    va_end (args);

    if(g_logHanle->tx->size != 0)
    {
        fifo_insert(g_logHanle->tx,(u8 *)s_log_buff, jval);
    }
    else
    {
        for(i = 0 ; i < jval ; i++)
        g_logHanle->putc(s_log_buff[i]);
    }
    

    service_log_unlock();

    //for(i = 0 ; i < jval ; i++)
    //g_logHanle->putc(s_log_buff[i]);

}


void log_print_hex(LogLevelEnum level, u8 *data, u16 len, const char * format, ... )
{  
    
    u16 i,jval = 0; 
    
    va_list args;

    service_log_lock();

    if(level < g_log_level)
    {
        service_log_unlock();
        return;
    }


    va_start (args, format);

    jval += sprintf(&s_log_buff[jval],"%s(%d)",log_level_string[level],clock());

    jval += vsprintf(&s_log_buff[jval],format, args);

    for(i = 0 ; i < len; i++)
    {
        jval += sprintf(&s_log_buff[jval],"%02X",data[i]);
    }

    if(s_log_buff[jval - 1] != '\n')
    {
        jval += sprintf(&s_log_buff[jval],"\r\n");
    }
    
    va_end (args);

    if(g_logHanle->tx->size != 0)
    {
        fifo_insert(g_logHanle->tx,(u8 *)s_log_buff, jval);
    }
    else
    {
        for(i = 0 ; i < jval ; i++)
        g_logHanle->putc(s_log_buff[i]);
    }
    

    service_log_unlock();

    //for(i = 0 ; i < jval ; i++)
    //g_logHanle->putc(s_log_buff[i]);

}

#define LOG_PING_TIME 60

typedef struct
{
    U8* buf;
    u32 len;
}LogSaveData;


/*它是个fifo*/
typedef struct
{
    LogSaveData *his;
    u16 size;  /*总空间*/
    u16 read_idx;  /*当前读到哪一条, read==write 表示队列空*/
    u16 write_idx;  /*当前写到哪一条, 新消息往write处写*/
}LogCurData;

typedef struct
{
    u32 data_finish_time;
	U32 log_seq;
}LogServiceExtend;


static FifoType s_log_data_fifo = {0,0,0,0,0};

static LogServiceExtend s_log_socket_extend = {0,0};

#define KK_LOG_MAX_LEN 1536


static SocketType s_log_socket = {-1,"",SOCKET_STATUS_ERROR,};



static KK_ERRCODE log_service_transfer_status(u8 new_status);
static void log_service_init_proc(void);
static void log_service_connecting_proc(void);
static void log_service_work_proc(void);
static void log_msg_receive(void);
static void log_service_close(void);
static void log_service_data_finish_proc(void);
static KK_ERRCODE log_data_insert_one(U8* data, u16 len);
static void log_data_release(LogSaveData *one);

static KK_ERRCODE log_data_insert_one(U8* data, u16 len)
{
    KK_ERRCODE ret = KK_SUCCESS;
    LogSaveData log_data;
    
	//多申请一个字节，放字符串结束符
    log_data.buf = (U8* )malloc(len + 1);
    if(NULL == log_data.buf)
    {
        //这里不适合再调用 log_service_print，出错会导致递归, 打印只求速死
        LOG(LEVEL_WARN,"log_data_insert_one assert(malloc(%d) failed",  len);
        return KK_SYSTEM_ERROR;
    }
    log_data.len = len;
    memcpy(log_data.buf, data, len);
	log_data.buf[len] = '\0';


    if(KK_SUCCESS != fifo_insert(&s_log_data_fifo, (u8*)&log_data, sizeof(LogSaveData)))
    {
        ret = KK_MEM_NOT_ENOUGH;
        //这里不适合再调用 log_service_print，出错会导致递归
        free(log_data.buf);
    }
    return ret;
}

static void log_data_release(LogSaveData *one)
{
    if(one && one->buf)
    {
        free(one->buf);
        one->buf = NULL;
        one->len = 0;
    }
}

static KK_ERRCODE log_service_transfer_status(u8 new_status)
{
    u8 old_status = (u8)s_log_socket.status;
    KK_ERRCODE ret = KK_PARAM_ERROR;
    switch(s_log_socket.status)
    {
        case SOCKET_STATUS_INIT:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_WORK:
                    break;
                case SOCKET_STATUS_DATA_FINISH:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_ERROR:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case SOCKET_STATUS_GET_HOST:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    break;
                case SOCKET_STATUS_GET_HOST:
                    break;
                case SOCKET_STATUS_CONNECTING:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_WORK:
                    break;
                case SOCKET_STATUS_DATA_FINISH:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_ERROR:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case SOCKET_STATUS_CONNECTING:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_WORK:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_DATA_FINISH:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_ERROR:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case SOCKET_STATUS_WORK:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_WORK:
                    break;
                case SOCKET_STATUS_DATA_FINISH:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_ERROR:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case SOCKET_STATUS_DATA_FINISH:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_WORK:
                    break;
                case SOCKET_STATUS_DATA_FINISH:
                    break;
                case SOCKET_STATUS_ERROR:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case SOCKET_STATUS_ERROR:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_WORK:
                    break;
                case SOCKET_STATUS_DATA_FINISH:
                    break;
                case SOCKET_STATUS_ERROR:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }


    if(KK_SUCCESS == ret)
    {
        s_log_socket.status = new_status;
        s_log_socket.status_fail_count = 0;
        LOG(LEVEL_INFO,"log_service_transfer_status from %s to %s success",  
            kk_socket_status_string((SocketStatus)old_status), kk_socket_status_string((SocketStatus)new_status));
    }
    else
    {
        LOG(LEVEL_INFO,"log_service_transfer_status from %s to %s failed",  
            kk_socket_status_string((SocketStatus)old_status), kk_socket_status_string((SocketStatus)new_status));
    }

    return ret;

}


KK_ERRCODE log_service_create(void)
{
    u8 addr[KK_OS_DNS_MAX_LENTH+1];
    u8 IP[4] = {0};
    u32 port = 0;
    u8 idx = 0;

    if(!s_log_socket.fifo.base_addr)kk_socket_init(&s_log_socket, SOCKET_INDEX_LOG);

    s_log_socket_extend.data_finish_time = 0;

    memset(addr, 0x00, sizeof(addr));
    idx = sscanf(/*(const char *)config_service_get_pointer(CFG_LOGSERVERADDR)*/"yj03.yunjiwulian.com:10009", "%[^:]:%d", addr, &port);
    if (idx != 2)
    {
        LOG(LEVEL_WARN,"log_service_create assert(idx ==2) failed.");
        return KK_PARAM_ERROR;
    }
    
    if(KK_SUCCESS != util_convertipaddr(addr, IP))
    {
        if(util_is_valid_dns(addr, strlen((const char *)addr)))
        {
            kk_socket_set_addr(&s_log_socket, addr, strlen((const char *)addr), port, STREAM_TYPE_DGRAM);
        }
        else
        {
            LOG(LEVEL_WARN,"log_service_create assert(dns(%s)) failed.",  addr);
            return KK_PARAM_ERROR;
        }
    }
    else
    {
        kk_socket_set_ip_port(&s_log_socket, IP, port, STREAM_TYPE_DGRAM);
        system_state_set_ip_cache(SOCKET_INDEX_LOG, IP);
    }
    
	if(!s_log_data_fifo.base_addr)fifo_init(&s_log_data_fifo, LOG_BUFFER_NUM * sizeof(LogSaveData));
    LOG(LEVEL_INFO,"log_service_create access_id(%d) fifo(%p).",  s_log_socket.access_id, &s_log_socket.fifo);
	return KK_SUCCESS;
}

KK_ERRCODE log_service_destroy(void)
{
    if(SOCKET_STATUS_ERROR == s_log_socket.status)
    {
        return KK_SUCCESS;
    }
    
    log_service_close();

    //log_service是在gprs_create中创建, 后面 重建连接, 所以保留fifo
    log_service_transfer_status(SOCKET_STATUS_ERROR);
	return KK_SUCCESS;
}


static void log_service_close(void)
{
    if(s_log_socket.id >=0)
    {
        SocketClose(s_log_socket.id);
        s_log_socket.id=-1;
    }
}


static void log_service_data_finish_proc(void)
{
    u32 current_time = 0;
    
    if(!IsModelPdpOK)
    {
        return;
    }
    
    current_time = util_clock();
    if((current_time - s_log_socket_extend.data_finish_time) > LOG_PING_TIME)
    {
        LOG(LEVEL_DEBUG,"log_service_data_finish_proc cur(%d) - fin(%d) > LOG_PING_TIME(%d).",
             current_time, s_log_socket_extend.data_finish_time,LOG_PING_TIME);
        // 可以重建连接
        log_service_transfer_status(SOCKET_STATUS_INIT);
    }
}


KK_ERRCODE log_service_timer_proc(void)
{
    if(!s_log_socket.fifo.base_addr)
    {
        return KK_SUCCESS;
    }

    switch(s_log_socket.status)
    {
    case SOCKET_STATUS_INIT:
        log_service_init_proc();
        break;
    case SOCKET_STATUS_CONNECTING:
        log_service_connecting_proc();
        break;
    case SOCKET_STATUS_WORK:
        log_service_work_proc();
        break;
    case SOCKET_STATUS_DATA_FINISH:  
        log_service_data_finish_proc();
        break;
    case SOCKET_STATUS_ERROR:
        log_service_data_finish_proc();
        break;
    default:
        LOG(LEVEL_WARN,"log_service_timer_proc assert(s_log_socket.status(%d)) unknown.", s_log_socket.status);
        return KK_ERROR_STATUS;
    }

    return KK_SUCCESS;
}


static void log_service_init_proc(void)
{
    u8 IP[4]={0};
    kk_socket_get_host_by_name_trigger(&s_log_socket);
    system_state_get_ip_cache(SOCKET_INDEX_LOG, IP);
    if(KK_SUCCESS == kk_is_valid_ip(IP))
    {
        memcpy( s_log_socket.ip , IP, sizeof(IP));
        log_service_transfer_status(SOCKET_STATUS_CONNECTING);
        if(KK_SUCCESS == kk_socket_connect(&s_log_socket))
        {
        }
        // else do nothing .   connecting_proc will deal.
    }
    else if((!s_log_socket.excuted_get_host) && (KK_SUCCESS == kk_is_valid_ip(s_log_socket.ip)))
    {
        log_service_transfer_status(SOCKET_STATUS_CONNECTING);
        if(KK_SUCCESS == kk_socket_connect(&s_log_socket))
        {
        }
        // else do nothing .   connecting_proc will deal.
    }
}

void log_service_connection_failed(void)
{
    log_service_close();
    
    if(s_log_socket.status_fail_count >= MAX_CONNECT_REPEAT)
    {
        // if excuted get_host transfer to error statu, else get_host.
        if(s_log_socket.excuted_get_host || (s_log_socket.addr[0] == 0))
        {
            s_log_socket_extend.data_finish_time = util_clock();
            log_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
        }
        else
        {
            log_service_transfer_status(SOCKET_STATUS_INIT);
        }
    }
    // else do nothing . wait connecting proc to deal.
}


void log_service_connection_ok(void)
{
    log_service_transfer_status(SOCKET_STATUS_WORK);
}

void log_service_close_for_reconnect(void)
{
    log_service_close();
    s_log_socket_extend.data_finish_time = util_clock();
    log_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
}


static void log_service_connecting_proc(void)
{
    u32 current_time = util_clock();

    if((current_time - s_log_socket.send_time) > CONNECT_TIME_OUT)
    {
        s_log_socket.status_fail_count ++;
        log_service_connection_failed();

        if(s_log_socket.status == SOCKET_STATUS_CONNECTING && 
            s_log_socket.status_fail_count < MAX_CONNECT_REPEAT)
        {
            if(KK_SUCCESS == kk_socket_connect(&s_log_socket))
            {
                //do nothing. wait callback
            }
        }
        
    }
}


#define LOG_PKT_HEAD "<RZ"
#define LOG_PKT_SPLIT '*'
#define LOG_PKT_TAIL '>'

void log_service_send_msg(SocketType *socket)
{
    LogSaveData log_data;
	//u8 i = 0;
	U8 imei[17+ 1] = {0};
    #if 0
    u8 *ptr = NULL;
	ptr = config_service_get_pointer(CFG_JT_CEL_NUM);

    if(!ptr)
    {
        LOG(LEVEL_INFO," log_service_send_msg can not get imei, ret:%d.", util_clock());
    }
    
    if (0 == strlen((const char *)ptr))
    {
        memset(ptr, 0, sizeof(ptr));
    }
    #if 1
    memset(imei,0x30,CONFIG_MAX_CELL_NUM_LEN);

    if(strlen((const char*)ptr) >= 15)
    {
        i = 0;
    }
    else
    {
        i = 15 - strlen((const char*)ptr);
    }

    memcpy(imei + i,ptr , 15);

    imei[15] = 0;
    #endif
    #else
    if(strlen((const char *)ModelImei) == 0)
    {
        LOG(LEVEL_INFO," log_service_send_msg can not get imei.");
    }

    memcpy(imei,ModelImei, 15);
    #endif

    if(KK_SUCCESS == fifo_peek(&s_log_data_fifo, (U8*)&log_data, sizeof(LogSaveData)))
    {
    	char* p_send_buf = NULL;
		U16 send_buf_len = 0;

		if (log_data.len > KK_LOG_MAX_LEN)
		{
			fifo_pop_len(&s_log_data_fifo, sizeof(LogSaveData));
			LOG(LEVEL_ERROR,"log_service_send_msg data is too long(%d)",  log_data.len);
			log_data_release(&log_data);
			return;
		}
		
		//一条日志<RZ*862964022280089*xxxxxx>,6个字节：LOG_PKT_HEAD——3个字节，2个分隔符，1和结束符
		//日志回复<RZ*ACK>
		send_buf_len = 6 + strlen((const char*)imei) + 10 + log_data.len;
		p_send_buf = malloc(send_buf_len + 1);
		memset(p_send_buf, 0, send_buf_len + 1);
		
		snprintf(p_send_buf, send_buf_len + 1, "%s%c%s%c%d%c%s%c", 
													LOG_PKT_HEAD,
													LOG_PKT_SPLIT,
													(const char*)imei,
													LOG_PKT_SPLIT,
													s_log_socket_extend.log_seq
													,LOG_PKT_SPLIT,
													(const char*)log_data.buf,
	        										LOG_PKT_TAIL);
        s_log_socket_extend.log_seq++;
		send_buf_len = strlen(p_send_buf);
        if(KK_SUCCESS == kk_socket_send(socket, (U8*)p_send_buf,send_buf_len))
        {
            fifo_pop_len(&s_log_data_fifo, sizeof(LogSaveData));
			LOG(LEVEL_DEBUG,"log_service_send_msg msglen(%d):%s",  send_buf_len,p_send_buf);
			log_data_release(&log_data);
        }
        else
        {
        	LOG(LEVEL_ERROR,"Failed to send log data!");
            log_service_close();
            s_log_socket_extend.data_finish_time = util_clock();
            log_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
        }
		free(p_send_buf);
    }
    
    return;
}

static void log_service_work_proc(void)
{
    u32 current_time = util_clock();

	//每秒钟最多一条日志，防止发送过快
    if((current_time - s_log_socket.send_time) >= 1)
    {
        log_service_send_msg(&s_log_socket);
		s_log_socket.send_time = current_time;
    }
    log_msg_receive();
}



/*当前服务器是只收不发响应的. 下面这个函数正常来説不会工作 */
static void log_msg_receive(void)
{
    // parse buf msg
    // if OK, after creating other socket, transfer to finish
    // not ok, ignore msg.
    u8 msg[100] = {0};

	u16 len = sizeof(msg);

    if(SOCKET_STATUS_WORK != s_log_socket.status)
    {
        return;
    }
    
    //get head then delete
    if(fifo_peek_until(&s_log_socket.fifo, msg, &len,'>') != KK_SUCCESS)
    {
        // no msg
        return;
    }

    fifo_pop_len(&s_log_socket.fifo, len);

	LOG(LEVEL_DEBUG,"log_msg_receive msg:%s, len:%d",  msg,len);

    //do nothing. just read and clear msgs

}

char s_log_str[KK_LOG_MAX_LEN] = {0};

void log_service_upload(LogLevelEnum level,JsonObject* p_root)
{
	//s8 zone = 0;
  //  char date[50] = {0};
	
    memset(s_log_str,0,KK_LOG_MAX_LEN);

    json_add_int(p_root,"clock", util_clock());

	if(json_print_to_buffer(p_root, s_log_str, KK_LOG_MAX_LEN - 1))
	{
		log_data_insert_one((U8*)s_log_str, strlen(s_log_str));
	}

    
    //debug级肯定不再回调log_service_upload
    LOG(LEVEL_DEBUG, s_log_str);
	json_destroy(p_root);
	p_root = NULL;
}



