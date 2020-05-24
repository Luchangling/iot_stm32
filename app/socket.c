
#include "socket.h"
#include "stdio.h"
#include "gps_service.h"
#include "update_service.h"
#include "update_file.h"
#include "service_log.h"
/*returned by apn register, used when creating socket. */
static int s_account_id = -1;
static SocketType *s_all_sockets[MAX_SOCKET_SUPPORTED];

#define SOCKET_STATUS_STRING_MAX_LEN 32
const char s_socket_status_string[SOCKET_STATUS_MAX][SOCKET_STATUS_STRING_MAX_LEN] = 
{
    "SOCKET_STATUS_INIT",
    "SOCKET_STATUS_GET_HOST",
    "SOCKET_STATUS_CONNECTING",
    "SOCKET_STATUS_LOGIN",
    "SOCKET_STATUS_WORK",
    "SOCKET_STATUS_DATA_FINISH",
    "SOCKET_STATUS_ERROR"
};


/*
这里将dns解析单独出来, 各模块只管去取ip地址 并设置 requested= true., 
没取到就等着, 
取到了就继续 
*/
typedef struct
{
    bool requested;         /* 是否触发了要请求host_name */
    u8 fail_count;  /* fail count of current status */
    u32 send_time;   /* record time of recent get host, connect, send msg etc*/
}GetHostItem;

GetHostItem s_get_host_list[SOCKET_INDEX_MAX];

typedef struct 
{
    SocketType *socket;
    u8 count; 
    CurrentGetHostByNameStatus status;
    char addr[KK_OS_DNS_MAX_LENTH];    /*dns name*/
}CurrentGetHostByName;

static CurrentGetHostByName current_get_host;
static  KK_ERRCODE kk_socket_get_host_by_name(SocketType *socket, GetHostItem *item);
static CurrentGetHostByNameStatus kk_socket_current_get_host_status(void);

/*由于gethostbyname同时只能有一个, 所以下面的函数让各service模块判断是否自已的域名解析操作*/
static u8 kk_socket_same_as_current_get_host(SocketType *socket);


static void init_all_sockets(void);
static  KK_ERRCODE add_to_all_sockets(SocketType *p);
static void current_get_host_start(SocketType *socket);
static void kk_socket_connect_failed(SocketType *socket);





static void init_all_sockets(void)
{
    int i;
    for(i =0; i< MAX_SOCKET_SUPPORTED; ++i)
    {
        s_all_sockets[i] = NULL;
    }
    
    for(i = 0; i<SOCKET_INDEX_MAX; ++i )
    {
        s_get_host_list[i].requested = false;
        s_get_host_list[i].fail_count = 0;
        s_get_host_list[i].send_time = 0;
    }
    
    return;
}

static  KK_ERRCODE add_to_all_sockets(SocketType *p)
{
    int i;

    //先确定是否已经加入了
    for(i =0; i< MAX_SOCKET_SUPPORTED; ++i)
    {
        if(s_all_sockets[i]->access_id == p->access_id)
        {
            s_all_sockets[i] = p;
            return  KK_SUCCESS;
        }
    }

    //没加入的情况, 找一个空位加入
    for(i =0; i< MAX_SOCKET_SUPPORTED; ++i)
    {
        if(s_all_sockets[i] == NULL)
        {
            s_all_sockets[i] = p;
            return  KK_SUCCESS;
        }
    }

    return  KK_MEM_NOT_ENOUGH;
}


SocketType * get_socket_by_id(int id)
{
    int i;
    for(i =0; i< MAX_SOCKET_SUPPORTED; ++i)
    {
        if(s_all_sockets[i] && s_all_sockets[i]->id == id)
        {
            return s_all_sockets[i];
        }
    }
    return NULL;
}

SocketType * get_socket_by_accessid(int access_id)
{
    int i;
    for(i =0; i< MAX_SOCKET_SUPPORTED; ++i)
    {
        if(s_all_sockets[i] && s_all_sockets[i]->access_id == access_id)
        {
            return s_all_sockets[i];
        }
    }
    return NULL;
}


 KK_ERRCODE kk_socket_global_init(void)
{
    init_all_sockets();
    current_get_host_init();
    return  KK_SUCCESS;
}


/* init socket*/
 KK_ERRCODE kk_socket_init(SocketType *socket, SocketIndexEnum access_id)
{
    u32 size = SIZE_OF_SOCK_FIFO;
    socket->id=-1;
    socket->status = SOCKET_STATUS_INIT;
    socket->send_time = 0;
    socket->status_fail_count = 0;
    socket->excuted_get_host = 0;
    socket->last_ack_seq = 0;
    socket->addr[0] = 0;
    socket->ip[0] = socket->ip[1] = socket->ip[2] = socket->ip[3] = 0;
    socket->port = 0;
    socket->access_id = access_id;

    //socket->fifo.baseAddr = malloc( SIZE_OF_SOCK_FIFO);
    if(access_id == SOCKET_INDEX_LOG ) size = 128;
    else if(access_id == SOCKET_INDEX_MAIN) size = 512;
    
    if(fifo_init(&(socket->fifo), size) != KK_SUCCESS)
    {
        LOG(LEVEL_FATAL,"scoket(id %d) fifo creat fail!",access_id);
    }
    
    add_to_all_sockets(socket);
    return  KK_SUCCESS;
}


 KK_ERRCODE kk_socket_set_addr(SocketType *socket, u8 *addr, u8 addr_len, u16 port, StreamType type)
{
    if(addr_len >= sizeof(socket->addr))
    {
        return  KK_PARAM_ERROR;
    }
    memcpy(socket->addr, addr , addr_len);
    socket->addr[addr_len] = 0;
    socket->port = port;
    socket->type = type;
    return  KK_SUCCESS;
}

void kk_socket_set_account_id(int account_id)
{
    s_account_id = account_id;
}

 KK_ERRCODE kk_socket_set_ip_port(SocketType *socket, u8 ip[4], u16 port, StreamType type)
{
    socket->ip[0] = ip[0];
    socket->ip[1] = ip[1];
    socket->ip[2] = ip[2];
    socket->ip[3] = ip[3];
    socket->port = port;
    socket->type = type;
    return  KK_SUCCESS;
}

void kk_socket_destroy(SocketType * socket)
{
	LOG(LEVEL_INFO,"kk_socket_destroy socket->access_id(%d) .", socket->access_id);
    switch(socket->access_id)
    {
    case SOCKET_INDEX_MAIN:
        gps_service_destroy();
        break;
    case SOCKET_INDEX_EPO: //SOCKET_INDEX_AGPS
        //agps_service_destroy();
        break;
    case SOCKET_INDEX_LOG:
        log_service_destroy();
        break;
    case SOCKET_INDEX_UPDATE:
        update_service_destroy();
        update_filemod_destroy();
        break;
    case SOCKET_INDEX_CONFIG:
        //config_service_destroy();
        break;
    default:
        LOG(LEVEL_WARN,"kk_socket_destroy assert(socket->access_id(%d) unknown.", 
			socket->access_id);
        break;
    }
}

void kk_socket_global_destroy(void)
{
    int i;
    
    for(i =0; i< MAX_SOCKET_SUPPORTED; ++i)
    {
        if(s_all_sockets[i] == NULL)
        {
            continue;
        }
        kk_socket_destroy(s_all_sockets[i]);
    }
}


void current_get_host_init(void)
{
    current_get_host.socket = NULL;
    current_get_host.count = 0;
    current_get_host.status = CURRENT_GET_HOST_INIT;
    current_get_host.addr[0] = 0;
}

/* get host can only start one by one .*/
static void current_get_host_start(SocketType *socket)
{
    current_get_host.socket = socket;
    current_get_host.count = 0;
    current_get_host.status = CURRENT_GET_HOST_CONTINUE;
    strncpy(current_get_host.addr,socket->addr,sizeof(current_get_host.addr));
}


void socket_get_host_by_name_callback(void *msg)
{
    u8 ip[4] = {0};
    
    if(current_get_host.socket == NULL)
    {
        LOG(LEVEL_ERROR,"socket_get_host_by_name_callback assert(current_get_host.socket != NULL) failed.");
        current_get_host.status = CURRENT_GET_HOST_INIT;
        return;
    }
    
    if(strcmp(current_get_host.addr, current_get_host.socket->addr) != 0)
    {
        LOG(LEVEL_INFO,"socket_get_host_by_name_callback (%s != %s).", 
			current_get_host.addr, current_get_host.socket->addr);
        current_get_host.status = CURRENT_GET_HOST_INIT;
        return;
    }

    ip[0] = (u32)msg&0x0FF;

    ip[1] = ((u32)msg>>8)&0xff;

    ip[2] = ((u32)msg>>16)&0xff;

    ip[3] = ((u32)msg>>24)&0xff;

    if( KK_SUCCESS != kk_is_valid_ip((const u8 *)ip))
    {
        LOG(LEVEL_INFO," socket_get_host_by_name_callback(%s) return error msg %s.", 
			current_get_host.socket->addr, (const char *)ip);
        current_get_host.status = CURRENT_GET_HOST_FAILED;
        return;
    }

    system_state_set_ip_cache(current_get_host.socket->access_id, (const U8*)ip);
    current_get_host.status = CURRENT_GET_HOST_SUCCESS;

	LOG(LEVEL_INFO,"socket_get_host_by_name_callback (%s)(%d.%d.%d.%d).", 
		current_get_host.socket->addr, ((const u8*)ip)[0], 
		((const u8*)ip)[1], ((const u8*)ip)[2], ((const u8*)ip)[3]);
}





static CurrentGetHostByNameStatus kk_socket_current_get_host_status()
{
    return current_get_host.status;
}

u8 kk_socket_same_as_current_get_host(SocketType *socket)
{
    if(current_get_host.socket == socket)
    {
        return 1;
    }
    return 0;
}

 KK_ERRCODE kk_socket_get_host_by_name(SocketType *socket, GetHostItem *item)
{
	  s32 result = 0;

    if(socket->addr[0] == 0)
    {
        LOG(LEVEL_INFO,"kk_socket_get_host_by_name socket(%d)->addr is empty.", socket->access_id );
        return  KK_PARAM_ERROR;
    }
    
    if(kk_socket_current_get_host_status() != CURRENT_GET_HOST_INIT)
    {
        //LOG(DEBUG,"kk_socket_get_host_by_name socket not init.");
        return  KK_NOT_INIT;
    }

    item->send_time = util_clock();
    socket->excuted_get_host=1;
    current_get_host_start(socket);
    
    GetHostByName(socket->addr, s_account_id, &result);
	LOG(LEVEL_INFO,"kk_socket_get_host_by_name socket->addr(%s) result(%d).", 
		socket->addr, result);
    return  KK_SUCCESS;
}


 KK_ERRCODE kk_is_valid_ip(const u8*data)
{
    if ((data[0]==0) && (data[1]==0)
        && (data[2]==0) && (data[3]==0))
    {
        return  KK_PARAM_ERROR;
    }
        
    if ((data[0]==255) && (data[1]==255)
    && (data[2]==255) && (data[3]==255))
    {
        return  KK_PARAM_ERROR;
    }
    
    return  KK_SUCCESS;
}



void kk_socket_connect_ok(SocketType *socket)
{
    switch(socket->access_id)
    {
    case SOCKET_INDEX_MAIN:
        gps_service_connection_ok();
        break;
    case SOCKET_INDEX_EPO: //SOCKET_INDEX_AGPS
        // udp not need callback
        //agps_service_connection_ok();
        break;
    case SOCKET_INDEX_LOG:
        log_service_connection_ok();
        break;
    case SOCKET_INDEX_UPDATE:
        update_service_connection_ok();
        update_filemod_connection_ok();
        break;
    case SOCKET_INDEX_CONFIG:
        //config_service_connection_ok();
        break;
    }
}

void kk_socket_close_for_reconnect(SocketType *socket)
{
    switch(socket->access_id)
    {
    case SOCKET_INDEX_MAIN:
        gps_service_close_for_reconnect();
        break;
    case SOCKET_INDEX_EPO: //SOCKET_INDEX_AGPS
        // udp not need callback
        //agps_service_close_for_reconnect();
        break;
    case SOCKET_INDEX_LOG:
        log_service_close_for_reconnect();
        break;
    case SOCKET_INDEX_UPDATE:
        update_service_finish(UPDATE_PING_TIME);
        update_filemod_close_for_reconnect();
        break;
    case SOCKET_INDEX_CONFIG:
        //config_service_close_for_reconnect();
        break;
    }
}


static void kk_socket_connect_failed(SocketType *socket)
{
	/*	different socket have different dealing
	gps_service retry N times should reinit gprs
	other services ignore failue */
    switch(socket->access_id)
    {
    case SOCKET_INDEX_MAIN:
        gps_service_connection_failed();
        break;
    case SOCKET_INDEX_EPO: //SOCKET_INDEX_AGPS
        //agps_service_connection_failed();
        break;
    case SOCKET_INDEX_LOG:
        log_service_connection_failed();
        break;
    case SOCKET_INDEX_UPDATE:
        update_service_connection_failed();
        update_filemod_connection_failed();
        break;
    case SOCKET_INDEX_CONFIG:
        //config_service_connection_failed();
        break;
    }
}


 KK_ERRCODE kk_socket_connect(SocketType *socket)
{
	char ip_addr[16];
	
    if (socket->id >= 0)
    {
        return  KK_SUCCESS;
    }
    
    LOG(LEVEL_INFO,"kk_socket_connect type(%d)(%d.%d.%d.%d:%d).", 
        socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], socket->port);
    if( KK_SUCCESS != kk_is_valid_ip(socket->ip))
    {
        return  KK_PARAM_ERROR;
    }

    socket->send_time = util_clock();
    socket->id = SocketCreate(s_account_id,socket->access_id, socket->type);
    if (socket->id >= 0)
    {
		snprintf(ip_addr, sizeof(ip_addr), "%d.%d.%d.%d", socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3]);
        if (SocketConnect(socket->id, ip_addr, socket->port, s_account_id,socket->type) == KK_SUCCESS)
        {
            if(socket->type == STREAM_TYPE_STREAM)
            {
                // wait call back
                //kk_socket_connect_ok(socket);
            }
            else if(socket->type == STREAM_TYPE_DGRAM)
            {
                //LOG(LEVEL_INFO,"kk_socket_connect udp(%d.%d.%d.%d:%d) id(%d) access_id(%d) success.", 
                //    socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], socket->port, socket->id, socket->access_id);
                //kk_socket_connect_ok(socket);
            }
            else
            {
                LOG(LEVEL_ERROR,"kk_socket_connect unknown type(%d)(%d.%d.%d.%d:%d).", 
                    socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], socket->port);
				return  KK_UNKNOWN;
            }
        }
        else
        {
            LOG(LEVEL_INFO,"kk_socket_connect type(%d)(%d.%d.%d.%d:%d) failed.", 
               socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], socket->port);

            kk_socket_connect_failed(socket);
            return  KK_UNKNOWN;
        }
    }
    else
    {
        LOG(LEVEL_INFO,"kk_socket_connect type(%d)(%d.%d.%d.%d:%d) create failed.", 
            socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], socket->port);
        return  KK_UNKNOWN;
    }
    
    return  KK_SUCCESS;
}



 KK_ERRCODE kk_socket_send(SocketType *socket, u8 *data, u16 len)
{  
    s32 ret;

	if(socket->status <= SOCKET_STATUS_CONNECTING || socket->status >= SOCKET_STATUS_DATA_FINISH)
    {
        LOG(LEVEL_ERROR,"kk_socket_send status(%d) error. type(%d)(%d.%d.%d.%d:%d) id(%d) len(%d).", 
             socket->status, socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], 
            socket->port,socket->id ,len);
        return  KK_ERROR_STATUS;
    }


    if(len > MAX_SOCKET_SEND_MSG_LEN)
    {
        LOG(LEVEL_ERROR,"kk_socket_send type(%d)(%d.%d.%d.%d:%d) id(%d) len(%d) too long.", 
             socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], 
            socket->port,socket->id,len);
        return  KK_PARAM_ERROR;
    }
    if(len <= 0)
    {
        return  KK_PARAM_ERROR;
    }
    
    //发一部分的没有出现过,发送失败会返回负数
    //kk_socket_get_ackseq(socket, &socket->last_ack_seq);
    ret = SocketSend(socket->id, (u8 *)data, len);
    if (ret != len)
    {
        LOG(LEVEL_INFO,"socket_send type(%d)(%d.%d.%d.%d:%d) id(%d) ret(%d)!=len(%d).", 
             socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], 
            socket->port,socket->id,ret,len);
        
        return  KK_UNKNOWN;
        
    }
    //日志的是字符串 无需打印
    if(socket->id != SOCKET_INDEX_LOG)
    LOG_HEX(LEVEL_DEBUG,data,len,"send to %s:%d->",socket->addr,socket->port);
    

    return  KK_SUCCESS;

}



 KK_ERRCODE kk_socket_recv(SocketType *socket ,int recvlen ,u8 *recv_buff)
{
    //u8 *recv_buff = NULL;
    //int recvlen=0;
	u32 ret;

	
	if(socket->status <= SOCKET_STATUS_CONNECTING || socket->status >= SOCKET_STATUS_DATA_FINISH)
    {
        LOG(LEVEL_ERROR,"kk_socket_recv status(%d) error. type(%d)(%d.%d.%d.%d:%d) id(%d).", 
             socket->status, socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], 
            socket->port,socket->id);
        return  KK_ERROR_STATUS;
    }


    //recv_buff = malloc(MAX_SOCKET_RECV_MSG_LEN);
    //if (NULL == recv_buff)
    //{
    //    LOG(LEVEL_ERROR,"kk_socket_recv type(%d)(%d.%d.%d.%d:%d) id(%d) memory not enough.", 
    //         socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], 
    //        socket->port,socket->id);
    //    return  KK_MEM_NOT_ENOUGH;
    //}

    //do
    //{
        //SocketRecv必须把所有消息接收完， 否则后面会收不了消息，正因为如此， fifo的缓冲要足够大，放得下
    //memset(recv_buff, 0x00, MAX_SOCKET_RECV_MSG_LEN);
    //recvlen = SocketRecv(socket->id, recv_buff, MAX_SOCKET_RECV_MSG_LEN);
    if(recvlen > 0)
    {
        LOG(LEVEL_INFO,"kk_socket_recv type(%d)(%d.%d.%d.%d:%d) id(%d) recvlen(%d).", 
             socket->type, socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], 
            socket->port,socket->id,recvlen);
        LOG_HEX(LEVEL_DEBUG,recv_buff,recvlen,"socket(%d) recv len(%d)<-",socket->id,recvlen);
    }
    
    if (recvlen > 0)
    {
        if((socket->access_id == SOCKET_INDEX_MAIN)&&(socket->type == STREAM_TYPE_STREAM))
        {
            //gps_service_confirm_gps_cache();
        }

        LOG(LEVEL_INFO,"recv from %s len %d:",socket->addr,recvlen);


        ret = fifo_insert(&socket->fifo, (u8 *)recv_buff, recvlen);
		if(ret != KK_SUCCESS)
		{
			LOG(LEVEL_ERROR,"fifo insert failed, return(%d)",  ret );
            if(socket->access_id != SOCKET_INDEX_UPDATE)  //update_service 会重发
            {
                //socket need rebuild.
                fifo_reset(&socket->fifo);
                kk_socket_close_for_reconnect(socket);
                //free(recv_buff);
                //recv_buff = NULL;
                return  KK_MEM_NOT_ENOUGH;
            }
		}
        else
        {
            LOG(LEVEL_INFO,"cur socket fifo len %d",fifo_get_msg_length(&socket->fifo));
        }
    }
    //}while(recvlen > 0);

    //free(recv_buff);
    //recv_buff = NULL;

    //if(recvlen != -2)  //SOC_WOULDBLOCK
    //{
    //    kk_socket_close_for_reconnect(socket);
    //}

    return  KK_SUCCESS;
}


const char * kk_socket_status_string(SocketStatus statu)
{
    return s_socket_status_string[statu];
}




void kk_socket_get_host_by_name_trigger(SocketType *socket)
{
    if(socket->addr[0])
    {
        s_get_host_list[socket->access_id].requested = true;
    }
}

/*
   循环处理所有的get_host_by_name请求
*/
void kk_socket_get_host_timer_proc(void)
{
    int i = 0;
    SocketType * socket = NULL;
    
    for(i = 0; i < SOCKET_INDEX_MAX; ++i)
    {
        if(s_get_host_list[i].requested)
        {
            break;
        }
    }

    
    if(i >= SOCKET_INDEX_MAX)
    {
        // no need get host
        return;
    }

    
    socket = get_socket_by_accessid(i);
    if(!socket)
    {
        s_get_host_list[i].requested = false;
        LOG(LEVEL_ERROR," kk_socket_get_host_timer_proc assert(socket_idx(%d)) failed.",  i );
        return;
    }

    if(current_get_host.socket)
    {
        u32 current_time = util_clock();
        bool same_as_current_get_host = false;
        int current_idx = current_get_host.socket->access_id;
        if(kk_socket_same_as_current_get_host(socket))
        {
            same_as_current_get_host = true;
        }
        
        //或者成功, 或者失败,或者超时
        if((kk_socket_current_get_host_status() == CURRENT_GET_HOST_SUCCESS) || 
            (kk_socket_current_get_host_status() == CURRENT_GET_HOST_FAILED))
        {
            // 释放资源,后面去抢
            s_get_host_list[current_idx].requested = false;
            s_get_host_list[current_idx].fail_count = 0;
            s_get_host_list[current_idx].send_time = 0;
            current_get_host_init();
            if(same_as_current_get_host) return;
        }
        else if((current_time - s_get_host_list[current_idx].send_time) > GET_HOST_TIME_OUT)
        {
            s_get_host_list[current_idx].fail_count ++;
            current_get_host_init(); //clear ，后面 竞争

            if(s_get_host_list[current_idx].fail_count >= MAX_GET_HOST_REPEAT)
            {
                s_get_host_list[current_idx].requested = false;
                s_get_host_list[current_idx].fail_count = 0;
                s_get_host_list[current_idx].send_time = 0;
                if(same_as_current_get_host) return;
            }
        }
        else
        {
            //正在执行, 未超时
            return;
        }
    }

    if( KK_SUCCESS == kk_socket_get_host_by_name(socket, &s_get_host_list[i]))
    {
        LOG(LEVEL_DEBUG," kk_socket_get_host_timer_proc start dns(%s).",  socket->addr );
        //do nothing. wait callback
    }
    // else do nothing .
    

}



