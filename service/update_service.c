/**
  远程升级服务
 */
#include "kk_os_header.h"
#include "update_service.h"
#include "update_file.h"
#include "config_service.h"
#include "socket.h"
#include "system_state.h"
#include "stmflash.h"
#include "utility.h"
update_info_struct g_devinfo[DEV_INVALID] = {
    {0,"SYS",0,{0}},
    {0,"EXT",0,{0}}
};


static SocketType s_update_socket= {-1,"",SOCKET_STATUS_MAX,};

typedef struct
{
    u8 getting_data;    //是否正在请求数据文件
    u32 last_ok_time;   //上一次执行升级检测的时间
    u32 wait;           //执行升级检测的间隔
    u8  dev_type;
}SocketTypeUpdateExtend;

static SocketTypeUpdateExtend s_update_socket_extend;

static KK_ERRCODE update_service_transfer_status(u8 new_status);
static void update_service_init_proc(void);
static void update_service_connecting_proc(void);
static void update_service_work_proc(void);
static void update_service_close(void);


static KK_ERRCODE update_msg_send_request(void);
static void update_service_data_finish_proc(void);



static KK_ERRCODE update_service_transfer_status(u8 new_status)
{
    u8 old_status = (u8)s_update_socket.status;
    KK_ERRCODE ret = KK_PARAM_ERROR;
    switch(s_update_socket.status)
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
        default:
            break;
    }


    if(KK_SUCCESS == ret)
    {
        s_update_socket.status = new_status;
        s_update_socket.status_fail_count = 0;
        LOG(LEVEL_INFO,"update_service_transfer_status from %s to %s success", 
            kk_socket_status_string((SocketStatus)old_status), kk_socket_status_string((SocketStatus)new_status));
    }
    else
    {
        LOG(LEVEL_INFO,"update_service_transfer_status from %s to %s failed", 
            kk_socket_status_string((SocketStatus)old_status), kk_socket_status_string((SocketStatus)new_status));
    }

    return ret;

}

u32 update_file_get_cur_file_checksum(void)
{
    u32 offset,addr,checksum = 0;

    u32 flag[4] = {0};

    u8 *pdata = NULL;

    u16 len = 0;

    u8 result = 0;

    addr = SYS_FILE_START_ADDR;

    offset = 0;
        
    do
    {
        STMFLASH_Read(addr + offset,(u16 *)flag,sizeof(flag)/2);

        if(flag[0] != 0xFFFFFFFF && flag[1] != 0xFFFFFFFF &&\
           flag[2] != 0xFFFFFFFF && flag[3] != 0xFFFFFFFF)
        {
            pdata = (u8 *)flag;

            for(len  = 0 ; len < sizeof(flag) ; len++)
            {
                checksum += pdata[len];
            }

            result = 0;

            offset += sizeof(flag);

            
        }
        else if(flag[0] == 0xFFFFFFFF && flag[1] == 0xFFFFFFFF &&\
                flag[2] == 0xFFFFFFFF && flag[3] == 0xFFFFFFFF)
        {
       
            result = 1;
        }
        else
        {   
            pdata = (u8 *)flag;

            checksum += pdata[0];

            checksum += pdata[1];

            checksum += pdata[2];

            checksum += pdata[3];

            result = 0;

            offset += 4;

        }
    }
    while (!result);


    return checksum;
}

u8 *update_file_get_version_addr(void)
{
    update_info_struct *info = &g_devinfo[s_update_socket_extend.dev_type];
    
    if(info->isready)
    {
        return info->version;
    }

    return NULL;
}

u32 update_file_get_checksum(void)
{
    update_info_struct *info = &g_devinfo[s_update_socket_extend.dev_type];
    
    if(info->isready)
    {
        return info->checksum;
    }

    return 0;
}

u32 system_file_checksum(void)
{
    update_info_struct *info = &g_devinfo[0];
    
    if(info->isready)
    {
        return info->checksum;
    }

    return 0;
}

u32 ext_file_checksum(void)
{
    update_info_struct *info = &g_devinfo[1];
    
    if(info->isready)
    {
        return info->checksum;
    }

    return 0;
}



u8 *update_file_get_fs_name(void)
{
    update_info_struct *info = &g_devinfo[s_update_socket_extend.dev_type];
    
    if(info->isready)
    {
        return (u8 *)info->dev_name;
    }

    return NULL;
}

u8 *update_file_get_device_type(void)
{
    update_info_struct *info = &g_devinfo[s_update_socket_extend.dev_type];
    
    if(info->isready)
    {
        return (u8 *)info->dev_name;
    }

    return NULL;
}

DeviceTypeEnum update_file_device_id(void)
{
    return (DeviceTypeEnum)s_update_socket_extend.dev_type;
}

void update_file_set_device_id(DeviceTypeEnum type)
{
    s_update_socket_extend.dev_type = type;
}

void update_dev_info_init(DeviceTypeEnum dev,u8 *ver,u32 checksum)
{
    //u32 size  = 0;
    
    update_info_struct *info;

    switch(dev)
    {
        case DEV_SYS:
            
            info = &g_devinfo[DEV_SYS];

            memset(info->version,0,sizeof(info->version));
            
            memcpy(info->version,VERSION_NUMBER,strlen(VERSION_NUMBER));
            //memcpy(info->version,"2.0.92",strlen("2.0.92"));

            if(!info->checksum)
            {
                info->checksum = update_file_get_cur_file_checksum();

                LOG(LEVEL_INFO,"CheckSum %x",info->checksum);
            }

            info->isready = 1;
            
            break;

       case DEV_EXT:

            info = &g_devinfo[DEV_EXT];

            memset(info->version,0,sizeof(info->version));
            
            memcpy(info->version,ver,strlen((const char *)ver));

            info->checksum = checksum;

            info->isready = 1;
            
            break;
    }

}

KK_ERRCODE update_service_create(bool first_create)
{
    u8 addr[2*KK_DNS_MAX_LENTH+1];
    u8 IP[4] = {0};
    u32 port = 0;
    u8 idx = 0;

    if(!s_update_socket.fifo.base_addr)
    {
        kk_socket_init(&s_update_socket, SOCKET_INDEX_UPDATE);
    }
       

    memset(addr, 0x00, sizeof(addr));
    idx = sscanf((const char *)config_service_get_pointer(CFG_UPDATESERVERADDR), "%[^:]:%d", addr, &port);
    if (idx != 2)
    {
        LOG(LEVEL_WARN,"update_service_create assert(idx ==2) failed.", util_clock());
        return KK_PARAM_ERROR;
    }
    
    if(KK_SUCCESS != util_convertipaddr(addr, IP))
    {
        if(util_is_valid_dns(addr, strlen((const char *)addr)))
        {
            kk_socket_set_addr(&s_update_socket, addr, strlen((const char *)addr), port, config_service_update_socket_type());
        }
        else
        {
            LOG(LEVEL_WARN,"update_service_create assert(dns(%s)) failed.", addr);
            return KK_PARAM_ERROR;
        }
    }
    else
    {
        kk_socket_set_ip_port(&s_update_socket, IP, port, config_service_update_socket_type());
        system_state_set_ip_cache(SOCKET_INDEX_UPDATE, IP);
    }

    s_update_socket_extend.getting_data = 0;
    s_update_socket_extend.last_ok_time = 0;
    s_update_socket_extend.wait = UPDATE_PING_TIME;

    LOG(LEVEL_INFO,"update_service_create access_id(%d) fifo(%p).", s_update_socket.access_id, &s_update_socket.fifo);

    init_file_extend(true);

    update_dev_info_init(DEV_SYS,NULL,0);

    s_update_socket_extend.dev_type = DEV_SYS;

	return KK_SUCCESS;
}

KK_ERRCODE update_service_destroy(void)
{
    if(SOCKET_STATUS_ERROR == s_update_socket.status)
    {
        return KK_SUCCESS;
    }
    
    update_service_close();

    //update socket only reconnect, not recreated.
    fifo_reset(&s_update_socket.fifo);
    
    update_service_transfer_status(SOCKET_STATUS_ERROR);

	return KK_SUCCESS;
}

SocketStatus update_service_get_status(void)
{
	return (SocketStatus)(s_update_socket.status);
}

bool update_service_is_waiting_reboot(void)
{
    if(update_service_get_status() != SOCKET_STATUS_DATA_FINISH)
    {
        return false;
    }
    
    if(get_file_extend()->result == REPORT_RESULT_UPDATED)
    {
        return true;
    }
    
    return false;
}

KK_ERRCODE update_service_timer_proc(void)
{
    if(!s_update_socket.fifo.base_addr)
    {
        return KK_SUCCESS;
    }

    switch(s_update_socket.status)
    {
    case SOCKET_STATUS_INIT:
        update_service_init_proc();
        break;
    case SOCKET_STATUS_CONNECTING:
        update_service_connecting_proc();
        break;
    case SOCKET_STATUS_WORK:
        update_service_work_proc();
        break;
    case SOCKET_STATUS_DATA_FINISH:
        update_service_data_finish_proc();
        break;
    case SOCKET_STATUS_ERROR:
        update_service_data_finish_proc();
        break;
    default:
        LOG(LEVEL_WARN,"update_service_timer_proc assert(s_update_socket.status(%d)) unknown.", util_clock(),s_update_socket.status);
        return KK_ERROR_STATUS;
    }

    return KK_SUCCESS;
}


static void update_service_init_proc(void)
{
    u8 IP[4];
    kk_socket_get_host_by_name_trigger(&s_update_socket);
    system_state_get_ip_cache(SOCKET_INDEX_UPDATE, IP);
    if(KK_SUCCESS == kk_is_valid_ip(IP))
    {
        memcpy( s_update_socket.ip , IP, sizeof(IP));
        update_service_transfer_status(SOCKET_STATUS_CONNECTING);
        if(KK_SUCCESS == kk_socket_connect(&s_update_socket))
        {
        }
        // else do nothing .   connecting_proc will deal.
    }
    else if((!s_update_socket.excuted_get_host) && (KK_SUCCESS == kk_is_valid_ip(s_update_socket.ip)))
    {
        update_service_transfer_status(SOCKET_STATUS_CONNECTING);
        if(KK_SUCCESS == kk_socket_connect(&s_update_socket))
        {
        }
        // else do nothing .   connecting_proc will deal.
    }
}

void update_service_connection_failed(void)
{
    update_service_close();
    
    if(s_update_socket.status_fail_count >= MAX_CONNECT_REPEAT)
    {
        // if excuted get_host transfer to error statu, else get_host.
        if(s_update_socket.excuted_get_host || (s_update_socket.addr[0] == 0))
        {
            update_service_finish(UPDATE_PING_TIME);
        }
        else
        {
            update_service_transfer_status(SOCKET_STATUS_INIT);
        }
    }
    // else do nothing . wait connecting proc to deal.
}

void update_service_connection_ok(void)
{
    update_service_transfer_status(SOCKET_STATUS_WORK);

    update_msg_send_request();
}

void update_service_finish(u32 wait)
{
    update_service_close();

    s_update_socket_extend.last_ok_time=util_clock();
    s_update_socket_extend.wait=wait;
    
    update_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
}


static void update_service_close(void)
{
    if(s_update_socket.id >=0)
    {
        SocketClose(s_update_socket.id);
        s_update_socket.id=-1;
    }
}


static void update_service_connecting_proc(void)
{
    u32 current_time = util_clock();

    if((current_time - s_update_socket.send_time) > CONNECT_TIME_OUT)
    {
        s_update_socket.status_fail_count ++;
        update_service_connection_failed();

        if(s_update_socket.status == SOCKET_STATUS_CONNECTING && 
            s_update_socket.status_fail_count < MAX_CONNECT_REPEAT)
        {
            if(KK_SUCCESS == kk_socket_connect(&s_update_socket))
            {
                //do nothing. wait callback
            }
        }
        
    }
}


static void update_service_work_proc(void)
{
    u32 current_time = util_clock();
    u8 one_send = 1;

    one_send = (STREAM_TYPE_DGRAM == config_service_update_socket_type())? UPDATE_MAX_PACK_ONE_SEND: UPDATE_MAX_PACK_ONE_SEND;
    if(! s_update_socket_extend.getting_data )
    {
        //发送请求阶段 
        if((current_time - s_update_socket.send_time) > MESSAGE_TIME_OUT)
        {
            s_update_socket.status_fail_count ++;
            if(s_update_socket.status_fail_count >= MAX_MESSAGE_REPEAT)
            {
                LOG(LEVEL_INFO,"update_service_work_proc failed:%d", s_update_socket.status_fail_count);
                update_service_finish(UPDATE_RETRY_TIME);
            }
            else
            {
                update_msg_send_request();
            }
        }
    }
    else
    {
        //请求数据阶段 
        if(! (get_file_extend()->use_new_socket))
        {
            if((current_time - s_update_socket.send_time) > MESSAGE_TIME_OUT)
            {
                s_update_socket.status_fail_count ++;
                if(s_update_socket.status_fail_count >= (MAX_MESSAGE_REPEAT * one_send))
                {
                    LOG(LEVEL_INFO,"update_filemod_work_proc failed:%d", s_update_socket.status_fail_count);
                    get_file_extend()->result = REPORT_RESULT_FAILED;
                    strncpy((char *)get_file_extend()->result_info, "request blocks fail(work_proc).", sizeof(get_file_extend()->result_info));
                    update_service_result_to_server();
                }
                else
                {
                    update_msg_send_data_block_request(&s_update_socket);
                }
            }
            
        }
        else
        {
            update_filemod_timer_proc();
        }
    }

    update_msg_receive(&s_update_socket);
}



static void update_service_data_finish_proc(void)
{
    u32 current_time = util_clock();

    if((current_time - s_update_socket_extend.last_ok_time) > s_update_socket_extend.wait)
    {
        LOG(LEVEL_DEBUG,"update_service_data_finish_proc cur(%d) - lastok(%d) > wait(%d).",
            current_time, s_update_socket_extend.last_ok_time,s_update_socket_extend.wait);
        init_file_extend(false);
        if(s_update_socket.id >=0)
        SocketClose(s_update_socket.id);
        s_update_socket_extend.getting_data = 0;
        s_update_socket_extend.last_ok_time = 0;
        s_update_socket_extend.wait = UPDATE_PING_TIME;
        update_service_transfer_status(SOCKET_STATUS_INIT);
    }
}




static KK_ERRCODE update_msg_send_request(void)
{
    u8 buff[100];
    u16 len = sizeof(buff);
    u16 idx = 0;  //current place

    update_msg_pack_head(buff, &idx, len);  //13 bytes
    update_msg_pack_request(buff, &idx, len);  //53 bytes
    update_msg_pack_id_len(buff, PROTOCCOL_UPDATE_REQUEST, idx);

    len=idx+2;  // 1byte checksum , 1byte 0xD

    
    if(KK_SUCCESS == kk_socket_send(&s_update_socket, buff, len))
    {
        s_update_socket.send_time = util_clock();
        LOG(LEVEL_DEBUG,"update_msg_send_request msglen:%d success", len);
        return KK_SUCCESS;
    }
    else
    {
        LOG(LEVEL_DEBUG,"update_msg_send_request msglen:%d failed.", len);
        update_service_finish(UPDATE_PING_TIME);
    }
    return KK_ERROR_STATUS;
}




void update_service_after_response(bool newsocket, u32 total_len)
{
    //已处于获取数据状态了, 多余的消息不处理.
    if(s_update_socket_extend.getting_data)
    {
        return;
    }
    
    if (total_len <= UPDATAE_FILE_MAX_SIZE)
    {
        //准备获取文件
        s_update_socket_extend.getting_data = 1;
        
        if(!get_file_extend()->use_new_socket)
        {
            update_msg_start_data_block_request(&s_update_socket);
        }
        //else  update_filemod_timer_proc() will send request
    }
    else
    {
        LOG(LEVEL_INFO,"update_service_after_response assert(file_total_len(%d)) failed.", total_len);
        update_service_finish(UPDATE_PING_TIME);
    }
}


void update_service_result_to_server(void)
{
    if(get_file_extend()->result == REPORT_RESULT_UPDATED)
    {
       //TODO LCL
    }
    
    update_msg_send_result_to_server(&s_update_socket);
}

void update_service_after_blocks_finish(void)
{
    s_update_socket.status_fail_count = 0;
    update_msg_send_data_block_request(&s_update_socket);
}


