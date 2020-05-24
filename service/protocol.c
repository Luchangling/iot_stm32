#include "kk_os_header.h"
#include "protocol.h"
#include "protocol_jt808.h"
#include "gps_save.h"
#include "gprs.h"
static bool protocol_msg_skiped_wrong(SocketType *socket, u8 *check, u16 check_len, u8 *head, u16 head_len)
{
    u16 start_idx = 0;
    u16 idx = 0;
    while(start_idx + check_len <= head_len)
    {
        bool match = true;
        for(idx = 0; idx < check_len; ++idx )
        {
            if(head[start_idx + idx] != check[idx])
            {
                match = false;
                break;
            }
        }

        if(match)
        {
            if(start_idx == 0)
            {
                //继续后面的执行
                return true;  
            }
            else
            {
                 //跳掉错误的消息， 等下次循环读取消息头
                LOG(LEVEL_WARN,"protocol_msg_skiped_wrong skiped(%d).", start_idx);
                LOG_HEX(LEVEL_DEBUG,head, head_len,"ErrFmt::");
                fifo_pop_len(&socket->fifo, start_idx);
                return false;
            }
        }
        else
        {
            start_idx++;
        }
    }

    //跳掉错误的消息， 等下次循环读取消息头
    LOG(LEVEL_WARN,"protocol_msg_skiped_wrong skiped(%d).", start_idx);
    LOG_HEX(LEVEL_DEBUG,head, head_len,"ErrHd::");
    fifo_pop_len(&socket->fifo, start_idx);
    return false;  
}

static bool alloc_memory_for_socket_msg(u8 **ppdata, u16 msg_len, SocketType *socket)
{
    if(*ppdata)
    {
        return true;
    }
    
    *ppdata = malloc(msg_len);
    if(*ppdata == NULL)
    {
        LOG(LEVEL_INFO,"alloc_memory_for_socket_msg alloc buf failed. len:%d", msg_len);

        //clear fifo and restart socket.
        fifo_reset(&socket->fifo);
        system_state_set_gpss_reboot_reason("malloc error.");
        kk_socket_close_for_reconnect(socket);
        return false;
    }
    return true;
}



static bool get_jt808_message(u8 *pdata, u16 *len_p)
{
    u8 *pend;
    if(*pdata != PROTOCOL_HEADER_JT808)
    {
        LOG(LEVEL_WARN,"get_jt808_message assert first byte failed.", util_clock());
        return false;
    }
    
    pend = pdata + 1;
    while((pend - pdata + 1) <= (*len_p))
    {
        if(*pend == PROTOCOL_HEADER_JT808)
        {
            *len_p = (pend - pdata + 1);
            return true;
        }
        pend ++;
    }
    return false;
}

void protocol_msg_receive(SocketType *socket)
{
    u8 head[7];
    u32 len = sizeof(head);
    u16 msg_len = 0;
    u8 *pdata = NULL;
    u8 check[1] = {0x7e};
     u32 msglen;
    static u32 packet_error_start = 0;

    if(socket->status != SOCKET_STATUS_LOGIN && socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_msg_receive socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return;
    }
    
    /*
    jt808协议 最短15
        标志位1(0x7e) 消息头12/14(ID 2 属性2(10位长度) 终端号6 流水2 封装0/4(属性 位13决定)) 消息体N 校验1 标志位1
        注意0x7e转码 0x7e->0x7d+02 0x7d->0x7d+01
    */

    if(KK_SUCCESS != fifo_peek(&socket->fifo, head, len))
    {
        // no msg
        return;
    }

    
    if(!protocol_msg_skiped_wrong(socket,check,sizeof(check),head,sizeof(head)))
    {
        return;
    }

    /*
        jt808 要以0x7e至0x7e 作为消息.
        直接取消息中的长度,会导致消息在有转义时取不全
    */
    msglen = MAX_JT808_MSG_LEN;
    if(!alloc_memory_for_socket_msg(&pdata, msglen, socket))
    {
        return;
    }
    if(KK_SUCCESS != fifo_peek_and_get_len(&socket->fifo, pdata, &msglen))
    {
        free(pdata);
        return;
    }
    msg_len = msglen;
    if(!get_jt808_message(pdata, &msg_len))
    {
        free(pdata);
        return;
    }

    

    if(msg_len > MAX_GPRS_MESSAGE_LEN)
    {
        LOG(LEVEL_WARN,"protocol_msg_receive assert(msg_len(%d)) failed.",msg_len);
        //clear fifo and restart socket.
        fifo_reset(&socket->fifo);
        system_state_set_gpss_reboot_reason("msg_len error.");
        kk_socket_close_for_reconnect(socket);

        //actually，for jt808 it is less than MAX_JT808_MSG_LEN，so, will not in this branch
        if(pdata) free(pdata);  
        return;
    }

    if(!alloc_memory_for_socket_msg(&pdata, msg_len, socket))
    {
        return;
    }

    if(KK_SUCCESS != fifo_peek(&socket->fifo, pdata, msg_len))
    {
        // KK_EMPTY_BUF
        free(pdata);
        
        if(packet_error_start == 0)
        {
            LOG(LEVEL_DEBUG,"protocol_msg_receive get msg failed. len:%d", msg_len);
            LOG_HEX(LEVEL_DEBUG,head, sizeof(head),"ErrFrame::");
            packet_error_start = util_clock();
        }
        else
        {
            if((util_clock() - packet_error_start) > MAX_GPRS_PART_MESSAGE_TIMEOUT)
            {
                LOG(LEVEL_WARN,"protocol_msg_receive MAX_GPRS_PART_MESSAGE_TIMEOUT.",util_clock());
                //clear fifo and restart socket.
                fifo_reset(&socket->fifo);
                system_state_set_gpss_reboot_reason("msg only part.");
                kk_socket_close_for_reconnect(socket);
                packet_error_start = 0;
            }
        }
        return;
    }
    fifo_pop_len(&socket->fifo, msg_len);
	LOG(LEVEL_DEBUG,"protocol_msg_receive msg len(%d) head(%02x)", msg_len, head[0]);

    protocol_jt_parse_msg(pdata,  msg_len);

    free(pdata);
}






KK_ERRCODE protocol_send_login_msg(SocketType *socket)
{
    u8 buff[200];
    u16 len = sizeof(buff);
    u16 idx = 0;
    u8 value_u8 = 0;
    
    memset(buff, 0x00, sizeof(buff));

    config_service_get(CFG_JT_ISREGISTERED, TYPE_BYTE, &value_u8, sizeof(value_u8));
    if(value_u8)
    {
        protocol_jt_pack_auth_msg(buff, &idx, len); // max 115 bytes
        LOG(LEVEL_DEBUG,"protocol_send_login_msg PROTOCOL_JT808 auth msg len(%d)).", idx);
    }
    else
    {
        protocol_jt_pack_regist_msg(buff, &idx, len);   // max 76 byes
        LOG(LEVEL_DEBUG,"protocol_send_login_msg PROTOCOL_JT808 regist msg len(%d)).", idx);
    }
    
    len=idx;  // idx is msg len

    socket->send_time = util_clock();
    LOG(LEVEL_DEBUG,"protocol_send_login_msg len(%d) protocol(%d).", len, config_service_get_app_protocol());
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send login");
        gps_service_destroy_gprs();
        return KK_NET_ERROR;
    }
    
    return KK_SUCCESS;
}


KK_ERRCODE protocol_send_device_msg(SocketType *socket)
{
    u8 buff[50];
    u16 len = sizeof(buff);
    u16 idx = 0;

    if(socket->status != SOCKET_STATUS_LOGIN && socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_device_msg socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return KK_PARAM_ERROR;
    }
    
    protocol_jt_pack_iccid_msg(buff, &idx, len);  // max 39 bytes

    len=idx;  // idx is msg len

    socket->send_time = util_clock();
    LOG(LEVEL_DEBUG,"protocol_send_device_msg len(%d) protocol(%d).", len, config_service_get_app_protocol());
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send devmsg");
        gps_service_destroy_gprs();
        return KK_NET_ERROR;
    }
    
    return KK_SUCCESS;
}


KK_ERRCODE protocol_send_heartbeat_msg(SocketType *socket)
{
    u8 buff[20];
    u16 len = sizeof(buff);
    u16 idx = 0;

    if(socket->status != SOCKET_STATUS_LOGIN && socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_heartbeat_msg socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return KK_PARAM_ERROR;
    }
    
    protocol_jt_pack_heartbeat_msg(buff, &idx, len);  // max 19 bytes

    len=idx;  // idx is msg len

    LOG(LEVEL_DEBUG,"protocol_send_heartbeat_msg len(%d) protocol(%d).", len, config_service_get_app_protocol());
    socket->send_time = util_clock();
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send heart");
        gps_service_destroy_gprs();
        return KK_NET_ERROR;
    }
    return KK_SUCCESS;
}


U32 protocol_send_gps_cache_msg(SocketType *socket)
{

    LocationSaveData one;
	U32 send_counts = 0;

    if(socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_gps_msg socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return 0;
    }

    
    //先发送最新的数据
    if(KK_SUCCESS == gps_service_cache_peek_one(&one, false))
    {
        LOG(LEVEL_DEBUG,"protocol_send_gps_cache_msg len(%d).", one.len);
        if(one.len == 0)
        {
            gps_service_cache_commit_peek(true);
            //continue;
            return 0;
        }

        if(KK_SUCCESS == gps_service_data_send(one.buf, one.len))
        {
            gps_service_cache_commit_peek(true);
            
			send_counts++;
        }
    }
    return send_counts;
}



U32 protocol_send_gps_msg(SocketType *socket)
{

    LocationSaveData one;
	U32 send_counts = 0;

    if(socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_gps_msg socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return 0;
    }

    
    //先发送最新的数据
    if(KK_SUCCESS == gps_service_peek_one(&one, false))
    {
        LOG(LEVEL_DEBUG,"protocol_send_gps_msg len(%d),seril %d.", one.len,MKWORD(one.buf[11], one.buf[12]));
        if(one.len == 0)
        {
            gps_service_commit_peek(true);
            //continue;
            return 0;
        }

        if(KK_SUCCESS == gps_service_data_send(one.buf, one.len))
        {
            gps_service_commit_peek(true);
			send_counts++;
        }
    }
    return send_counts;
}


KK_ERRCODE protocol_send_logout_msg(SocketType *socket)
{
    u8 buff[20];
    u16 len = sizeof(buff);
    u16 idx = 0;


    if(socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_logout_msg socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return KK_PARAM_ERROR;
    }
    
    protocol_jt_pack_logout_msg(buff, &idx, len);  // 15 bytes

    len=idx;  // idx is msg len
    
    LOG(LEVEL_DEBUG,"protocol_send_logout_msg len(%d) protocol(%d).", len, config_service_get_app_protocol());

    socket->send_time = util_clock();
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send logout");
        gps_service_destroy_gprs();
        return KK_NET_ERROR;
    }
    
    return KK_SUCCESS;
}


KK_ERRCODE protocol_send_remote_ack(SocketType *socket, u8 *pRet, u16 retlen)
{
    u8 *buff;
    u16 len = 30 + retlen;  //jt808 need 22 bytes
    u16 idx = 0;

    if(socket->status != SOCKET_STATUS_LOGIN && socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_remote_ack socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return KK_PARAM_ERROR;
    }

    buff = malloc(len);
    if (NULL == buff)
    {
        LOG(LEVEL_WARN,"protocol_send_remote_ack assert(KK_MemoryAlloc(%d)) failed.", len);
        return KK_MEM_NOT_ENOUGH;
    }
    
    protocol_jt_pack_remote_ack(buff, &idx, len, pRet, retlen);  //22|26 +retlen bytes

    len=idx;  // idx is msg len

    socket->send_time = util_clock();
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send remote_ack");
        gps_service_destroy_gprs();
        free(buff);
        return KK_NET_ERROR;
    }
    free(buff);
    return KK_SUCCESS;
}



KK_ERRCODE protocol_send_general_ack(SocketType *socket)
{
    u8 buff[50];
    u16 len = sizeof(buff); 
    u16 idx = 0;

    if(socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_remote_ack socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return KK_PARAM_ERROR;
    }
    
    protocol_jt_pack_general_ack(buff, &idx, len);  //max 24 bytes

    len=idx;  // idx is msg len
    LOG(LEVEL_DEBUG,"protocol_send_general_ack len(%d).", len);

    socket->send_time = util_clock();
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send geneal_ack");
        gps_service_destroy_gprs();
        return KK_NET_ERROR;
    }
    return KK_SUCCESS;
}



KK_ERRCODE protocol_send_param_get_ack(SocketType *socket)
{
    u8 *buff;
    u16 len = 1000; 
    u16 idx = 0;

    if(socket->status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"protocol_send_remote_ack socket->status(%s) error.", kk_socket_status_string((SocketStatus)socket->status));
        return KK_PARAM_ERROR;
    }
    

    buff = malloc(len);
    if (NULL == buff)
    {
        LOG(LEVEL_WARN,"protocol_send_param_get_ack assert(KK_MemoryAlloc(%d)) failed.", len);
        return KK_MEM_NOT_ENOUGH;
    }
    
    protocol_jt_pack_param_ack(buff, &idx, len); 

    len=idx;  // idx is msg len
    LOG(LEVEL_DEBUG,"protocol_send_param_get_ack len(%d) protocol(%d).", len, config_service_get_app_protocol());

    socket->send_time = util_clock();
    if(KK_SUCCESS != kk_socket_send(socket, buff, idx))
    {
        system_state_set_gpss_reboot_reason("kk_socket_send param_ack");
        gps_service_destroy_gprs();
        free(buff);
        return KK_NET_ERROR;
    }
    free(buff);
    return KK_SUCCESS;
}



