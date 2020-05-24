#include "gps_service.h"
#include "kk_os_header.h"
#include "protocol.h"
#include "gps_save.h"
#include "gprs.h"
#include "protocol_jt808.h"
#include "system_state.h"
#include "math.h"
#define GPS_PING_TIME 60
typedef struct
{
	bool heart_atonce;     // 立即发送心跳
    u32 heart_send_time;   // 心跳发送时间
    u32 heart_locate_send_time;  // 定位发送时间   超越平台PROTOCOL_JT808只支持定位,不支持心跳,所以用定位代替心跳
    u32 heart_receive_time;  // 心跳收到时间
	u32 receive_heart_counts;  //收到心跳计数
	u32 send_location_counts;  //发送定位数据计数
    u32 data_finish_time;  //进入到SOCKET_STATUS_DATA_FINISH状态的时间
    u32 lbs_send_time;
    u32 connection_time;  //连接建立时间
    u32 saved_socket_ack;
    u32 last_socket_ack;
}SocketTypeExtend;

static SocketType s_gps_socket = {-1,"",SOCKET_STATUS_ERROR,};
static SocketTypeExtend s_gps_socket_extend = {false,0,0,0,0,0,0,0,0,0};

static KK_ERRCODE gps_service_transfer_status(u8 new_status);
static void gps_service_init_proc(void);
static void gps_service_connecting_proc(void);
static void gps_service_login_proc(void);
static void gps_service_work_proc(void);
static void gps_service_close(void);
//void gps_service_send_msg(GpsServiceQueMsg msg , u8 *data , u16 len);


static KK_ERRCODE protocol_pack_gps_msg(GpsDataModeEnum mode, const GPSData *gps, u8 *pdata, u16 *idx, u16 len);
static KK_ERRCODE protocol_pack_lbs_msg(u8 *pdata, u16 *idx, u16 len);
static KK_ERRCODE protocol_pack_alarm_msg(AlarmInfo *alarm, u8 *pdata, u16 *idx, u16 len);
//static KK_ERRCODE protocol_pack_device_state_msg(u8 *pdata, u16 *idx, u16 len);
static KK_ERRCODE protocol_pack_position_request_msg(u8 *mobile_num, u8 num_len, u8 *command, u8 cmd_len,u8 *pdata, u16 *idx, u16 len);
//static KK_ERRCODE protocol_pack_wifi_msg(u8 *pdata, u16 *idx, u16 len);


static KK_ERRCODE gps_service_transfer_status(u8 new_status)
{
    u8 old_status = (u8)s_gps_socket.status;
    KK_ERRCODE ret = KK_PARAM_ERROR;
    switch(s_gps_socket.status)
    {
        case SOCKET_STATUS_INIT:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_LOGIN:
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
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    break;
                case SOCKET_STATUS_CONNECTING:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_LOGIN:
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
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_LOGIN:
                    ret = KK_SUCCESS;
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
        case SOCKET_STATUS_LOGIN:
            switch(new_status)
            {
                case SOCKET_STATUS_INIT:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_LOGIN:
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
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_GET_HOST:
                    ret = KK_SUCCESS;
                    break;
                case SOCKET_STATUS_CONNECTING:
                    break;
                case SOCKET_STATUS_LOGIN:
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
                case SOCKET_STATUS_LOGIN:
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
                case SOCKET_STATUS_LOGIN:
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
        s_gps_socket.status = new_status;
        s_gps_socket.status_fail_count = 0;
        LOG(LEVEL_INFO,"clock(%d) gps_service_transfer_status from %s to %s success", util_clock(), 
            kk_socket_status_string((SocketStatus)old_status), kk_socket_status_string((SocketStatus)new_status));
    }
    else
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_transfer_status assert(from %s to %s) failed", util_clock(), 
            kk_socket_status_string((SocketStatus)old_status), kk_socket_status_string((SocketStatus)new_status));
    }

    return ret;

}


/*
// have ip, use ip to connect
// no ip, call kk_socket_get_host_by_name 
*/
static void gps_service_init_proc(void)
{
    u8 IP[4];
    kk_socket_get_host_by_name_trigger(&s_gps_socket);
    system_state_get_ip_cache(SOCKET_INDEX_MAIN, IP);
    if(KK_SUCCESS == kk_is_valid_ip(IP))
    {
        memcpy( s_gps_socket.ip , IP, sizeof(IP));
        gps_service_transfer_status(SOCKET_STATUS_CONNECTING);
        if(KK_SUCCESS == kk_socket_connect(&s_gps_socket))
        {
        }
        // else do nothing .   connecting_proc will deal.
    }
    else if((!s_gps_socket.excuted_get_host) && (KK_SUCCESS == kk_is_valid_ip(s_gps_socket.ip)))
    {
        gps_service_transfer_status(SOCKET_STATUS_CONNECTING);
        if(KK_SUCCESS == kk_socket_connect(&s_gps_socket))
        {
        }
        // else do nothing .   connecting_proc will deal.
    }
}

static void gps_service_connecting_proc(void)
{
    u32 current_time = util_clock();

    if((current_time - s_gps_socket.send_time) > CONNECT_TIME_OUT)
    {
        s_gps_socket.status_fail_count ++;
        gps_service_connection_failed();

        if(s_gps_socket.status == SOCKET_STATUS_CONNECTING && 
            s_gps_socket.status_fail_count < MAX_CONNECT_REPEAT)
        {
            if(KK_SUCCESS == kk_socket_connect(&s_gps_socket))
            {
                //do nothing. wait callback
            }
        }
        
    }
}



static void gps_service_login_proc(void)
{
    u32 current_time = util_clock();

    if((current_time - s_gps_socket.send_time) >= MESSAGE_TIME_OUT)
    {
        ++s_gps_socket.status_fail_count;
        if(s_gps_socket.status_fail_count >= GPS_LOGIN_MSG_FAIL_MAX)
        {
			JsonObject* p_json_log = json_create();
			json_add_string(p_json_log, "event", "login");
			json_add_string(p_json_log, "state", "failed");
			json_add_int(p_json_log, "fail count", s_gps_socket.status_fail_count);
			log_service_upload(LEVEL_WARN, p_json_log);
   
            system_state_set_gpss_reboot_reason("login failed.");
            gps_service_close();
            s_gps_socket_extend.data_finish_time = util_clock();
            gps_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
        }
        else
        {
            if(KK_SUCCESS != protocol_send_login_msg(&s_gps_socket))
            {
                return;
            }
        }
    }

    protocol_msg_receive(&s_gps_socket);
}

/*
    get msg from fifo
    if have no gps msg, sending heartbeat, else sending gpsmsg
*/
static void gps_service_work_proc(void)
{
    u32 current_time = util_clock();
    u16 value_u16;
    //bool lbs_enable;
    u32 send_counts = 0;
    //u8 work_mode = 0;
    //GpsServiceMsgStruct *msg = NULL;
    //LocationSaveData one;
    //u16 upload_time;
    //u8 key_feature;

	//服务模块工作状态并且不是休眠状态LED常亮
	if (KK_SYSTEM_STATE_WORK == system_state_get_work_state())
	{
		led_service_net_set(LED_ON);
	}
    
	//config_service_get(CFG_UPLOADTIME, TYPE_SHORT, &upload_time, sizeof(upload_time));
    #if 0
    config_service_get(CFG_JT_HBINTERVAL, TYPE_SHORT, &value_u16, sizeof(value_u16));
    
    if(value_u16)  // != 0
    {
        if((current_time - s_gps_socket_extend.heart_locate_send_time) >= value_u16 || s_gps_socket_extend.heart_atonce)
        {
            gps_service_send_one_locate(GPS_MODE_FIX_TIME, false);
            s_gps_socket_extend.heart_locate_send_time = current_time;
			s_gps_socket_extend.heart_atonce = false;
        }
    }
    #endif

    config_service_get(CFG_HEART_INTERVAL, TYPE_SHORT, &value_u16, sizeof(value_u16));
    
    if((current_time - s_gps_socket_extend.heart_send_time) >= value_u16 || s_gps_socket_extend.heart_atonce)
    {
        protocol_send_heartbeat_msg(&s_gps_socket);
        s_gps_socket_extend.heart_send_time = current_time;
		s_gps_socket_extend.heart_atonce = false;
    }


    /*
        连接建立成功2次心跳间隔后才检测心跳有没有超时
    */
    if((current_time - s_gps_socket_extend.connection_time) > (2*value_u16)
        && (current_time - s_gps_socket_extend.heart_receive_time) >= GPS_HEART_MSG_RECEIVE_TIME_OUT)
    {
        LOG(LEVEL_INFO,"clock(%d) gps_service_work_proc heart break. last heart time:%d", util_clock(), s_gps_socket_extend.heart_receive_time);
        system_state_set_gpss_reboot_reason("heart broken.");
        gps_service_close();
        s_gps_socket_extend.data_finish_time = util_clock();
        gps_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
        return;
    }

    //作为立即发送的消息队列
    send_counts = protocol_send_gps_cache_msg(&s_gps_socket);
        
    if(send_counts == 0)
    {
        send_counts = protocol_send_gps_msg(&s_gps_socket);
    }
    
    s_gps_socket_extend.send_location_counts += send_counts;

    protocol_msg_receive(&s_gps_socket);
}

u32 gps_service_total_send_counts(void)
{
    return s_gps_socket_extend.send_location_counts;
}

void gps_service_connection_ok(void)
{
    u32 current_time = util_clock();
    s_gps_socket_extend.connection_time = current_time;
    gps_service_transfer_status(SOCKET_STATUS_LOGIN);

    protocol_send_login_msg(&s_gps_socket);
    
}

void gps_service_close_for_reconnect(void)
{
    gps_service_close();
    s_gps_socket_extend.data_finish_time = util_clock();
    gps_service_transfer_status(SOCKET_STATUS_DATA_FINISH);
}

void gps_service_connection_failed(void)
{
    system_state_set_gpss_reboot_reason("connect failed.");
    gps_service_close();
    
    if(s_gps_socket.status_fail_count >= MAX_CONNECT_REPEAT)
    {
        // if excuted get_host reinit gprs, else get_host.
        if(s_gps_socket.excuted_get_host || (s_gps_socket.addr[0] == 0))
        {
            //  reinit gprs
            LOG(LEVEL_DEBUG,"clock(%d) gps_service_connection_failed excuted_get_host(%d)",util_clock(),s_gps_socket.excuted_get_host);
            gps_service_destroy_gprs();
        }
        else
        {
            gps_service_transfer_status(SOCKET_STATUS_INIT);
        }
    }
    //else do nothing. wait gps_service_connecting_proc to deal
}


KK_ERRCODE gps_service_create(bool first_create)
{
    u8 addr[2*KK_DNS_MAX_LENTH+1] = {0};
    u8 IP[4] = {0};
    u32 port = 0;
    u8 idx = 0;
    //KK_ERRCODE ret = KK_SUCCESS;
    //u8 imei[15 + 1] = {0};
	
    //if(strlen((const char*)ModelImei) == 0)
    //{
    //    return ret;
    //}

    /*because s_gps_socket.fifo.base_addr is not initialized, here depend on first_create
    to check base_addr is null or not*/
    if(!first_create)
    {
        if(s_gps_socket.fifo.base_addr)
        {
            return KK_SUCCESS;
        }
    }
	
    kk_socket_init(&s_gps_socket, SOCKET_INDEX_MAIN);

    memset(addr, 0x00, sizeof(addr));

    idx = sscanf((const char *)config_service_get_pointer(CFG_SERVERADDR), "%[^:]:%d", addr, &port);
    
    if (idx != 2)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_create assert(idx ==2) failed.", util_clock());
        return KK_PARAM_ERROR;
    }
	LOG(LEVEL_INFO,"is not default imei,use main server");

	//是域名
    if(KK_SUCCESS != ConvertIpAddr(addr, IP))
    {
        if(util_is_valid_dns(addr, strlen((const char *)addr)))
        {
            kk_socket_set_addr(&s_gps_socket, addr, strlen((const char *)addr), port, STREAM_TYPE_STREAM);
	        system_state_get_ip_cache(SOCKET_INDEX_MAIN, IP);
            kk_socket_set_ip_port(&s_gps_socket, IP, port, STREAM_TYPE_STREAM);
        }
        else
        {
            LOG(LEVEL_WARN,"clock(%d) gps_service_create assert(dns(%s)) failed.", util_clock(), addr);
            return KK_PARAM_ERROR;
        }
    }
	//是IP
    else
    {
        kk_socket_set_ip_port(&s_gps_socket, IP, port, STREAM_TYPE_STREAM);
        if (!config_service_is_test_mode())
        {
        	system_state_set_ip_cache(SOCKET_INDEX_MAIN, IP);
        }
    }


	s_gps_socket_extend.heart_atonce = false;
    s_gps_socket_extend.heart_send_time = 0;
    s_gps_socket_extend.heart_locate_send_time = 0;
	s_gps_socket_extend.receive_heart_counts = 0;
	s_gps_socket_extend.send_location_counts = 0;
    s_gps_socket_extend.data_finish_time = 0;
    s_gps_socket_extend.lbs_send_time = 0;

    LOG(LEVEL_INFO,"gps_service_create access_id(%d) fifo(%p).",s_gps_socket.access_id, &s_gps_socket.fifo);
    

	return KK_SUCCESS;
}



KK_ERRCODE gps_service_change_config(void)
{
    if (! gps_service_socket_is_same())
    {
        u8 value_u8 = 0;
        system_state_set_gpss_reboot_reason("change config.");
        gps_service_destroy();
        
        // clear previous connection info.
        s_gps_socket.ip[0] = s_gps_socket.ip[1] = s_gps_socket.ip[2] = s_gps_socket.ip[3] = 0;
        s_gps_socket.addr[0] = 0;
        system_state_set_ip_cache(SOCKET_INDEX_MAIN, s_gps_socket.ip);
        config_service_set(CFG_JT_ISREGISTERED, TYPE_BOOL, &value_u8, sizeof(value_u8));
        
        gps_service_create(false);
    }
    return KK_SUCCESS;
}


KK_ERRCODE gps_service_destroy(void)
{
    if(SOCKET_STATUS_ERROR == s_gps_socket.status)
    {
        return KK_SUCCESS;
    }
    
    gps_service_close();
    led_service_net_set(LED_SLOW_FLASH);

    fifo_reset(&s_gps_socket.fifo);

    s_gps_socket_extend.data_finish_time = util_clock();
    gps_service_transfer_status(SOCKET_STATUS_ERROR);
    
	return KK_SUCCESS;
}

static void gps_service_close(void)
{
    gps_service_save_to_history_file();

    if(s_gps_socket.id >=0)
    {
        SocketClose(s_gps_socket.id);
        s_gps_socket.id=-1;
    }
}


static void gps_service_data_finish_proc(void)
{
    u32 current_time = util_clock();

	if(gprs_check_need_reboot(s_gps_socket_extend.heart_receive_time))
	{
        //以下日志如果放开,会打印过多.(10ms一次)
        //LOG(INFO,"clock(%d) gps_service_data_finish_proc heart break. last heart time:%d", util_clock(), s_gps_socket_extend.heart_receive_time);
        return;
	}

    if(! s_gps_socket.fifo.base_addr)
    {
        return;
    }

    if((current_time - s_gps_socket_extend.data_finish_time) > GPS_PING_TIME)
    {
        LOG(LEVEL_DEBUG,"clock(%d) gps_service_data_finish_proc cur(%d) - fin(%d) > GPS_PING_TIME(%d).",
            util_clock(), current_time, s_gps_socket_extend.data_finish_time,GPS_PING_TIME);

        // 可以重建连接
        gps_service_transfer_status(SOCKET_STATUS_INIT);
    }
}


KK_ERRCODE gps_service_timer_proc(void)
{
    // 如果gprs成功, 但socket无法收发数据, 则heart_receive_time 不会更新
	if(gprs_check_need_reboot(s_gps_socket_extend.heart_receive_time))
	{
        // 如果未初始化, 相当于gprs_check_need_reboot(0) 即检测启动了21分钟.
        gprs_destroy();
        return KK_ERROR_STATUS;
	}

    if(!s_gps_socket.fifo.base_addr)
    {
        return KK_SUCCESS;
    }

    switch(s_gps_socket.status)
    {
        case SOCKET_STATUS_INIT:
            gps_service_init_proc();
            break;
        case SOCKET_STATUS_CONNECTING:
            gps_service_connecting_proc();
            break;
        case SOCKET_STATUS_LOGIN:
            gps_service_login_proc();
            break;
        case SOCKET_STATUS_WORK:
            gps_service_work_proc();
            break;
        case SOCKET_STATUS_DATA_FINISH:
            gps_service_data_finish_proc();
            break;
        case SOCKET_STATUS_ERROR:
            gps_service_data_finish_proc();
            break;
        default:
        {
            char reason[30];
            LOG(LEVEL_WARN,"clock(%d) gps_service_timer_proc assert(s_gps_socket.status(%d)) unknown.",util_clock(), s_gps_socket.status);

            //  reinit gprs
            snprintf(reason,sizeof(reason),"unknown status %d", s_gps_socket.status);
            reason[sizeof(reason) - 1] = 0;
            system_state_set_gpss_reboot_reason(reason);
            gps_service_destroy_gprs();
            return KK_ERROR_STATUS;
        }
    }

    return KK_SUCCESS;
}



u8 gps_service_socket_is_same(void)
{
    u8 addr[50+1];
    u8 IP[4];
    u32 port = 0;
    u8 idx = 0;

    memset(addr, 0x00, sizeof(addr));
    idx = sscanf((const char *)config_service_get_pointer(CFG_SERVERADDR), "%[^:]:%d", addr, &port);
    if (idx != 2)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_socket_is_same assert(idx ==2) failed.",util_clock());
        return 1; // not change.
    }

    if(s_gps_socket.port != port)
    {
        return 0;
    }
    
    if(KK_SUCCESS == ConvertIpAddr(addr, IP))
    {
        if(s_gps_socket.ip[0] == IP[0] && s_gps_socket.ip[1] == IP[1] && 
            s_gps_socket.ip[2] == IP[2] && s_gps_socket.ip[3] == IP[3])
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if(0 == strcmp(s_gps_socket.addr, (const char *)addr))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    // return 1;
}




static KK_ERRCODE protocol_pack_gps_msg(GpsDataModeEnum mode, const GPSData *gps, u8 *pdata, u16 *idx, u16 len)
{
    
    protocol_jt_pack_gps_msg(mode, gps, pdata, idx, len);  // 53 bytes
    
    return KK_SUCCESS;
}

KK_ERRCODE gps_service_data_send(u8 *data, u8 len)
{
    //转义后带基站的消息可能超过100字节, 所以在此处转义
    u8 buff[HIS_FILE_FRAME_SIZE*2];
    u16 buf_len = sizeof(buff);
    protocol_jt_pack_escape(data, len, buff, &buf_len);
    if(KK_SUCCESS == kk_socket_send(&s_gps_socket, buff, buf_len))
    {
        s_gps_socket.send_time = util_clock();
        return KK_SUCCESS;
    }
    else
    {
        system_state_set_gpss_reboot_reason("kk_socket_send cache_send");
        gps_service_destroy_gprs();
        return KK_NET_ERROR;
    }
    
    //return KK_MEM_NOT_ENOUGH;
}


void gps_service_confirm_gps_cache(SocketType *socket)
{
    //kk_socket_get_ackseq(socket, &socket->last_ack_seq);
    if(socket->access_id != SOCKET_INDEX_MAIN)
    {
        return;
    }
    
    gps_service_confirm_cache(&s_gps_socket_extend.saved_socket_ack, s_gps_socket.last_ack_seq);
}


KK_ERRCODE gps_service_push_gps(GpsDataModeEnum mode, const GPSData *gps)
{
    u8 buff[HIS_FILE_FRAME_SIZE];
    u16 len = sizeof(buff);
    u16 idx = 0;
    //bool gps_close = false;
   
    if(NULL == gps)
    {
        return KK_PARAM_ERROR;
    }
    #if 0
	LOG(LEVEL_INFO,"Report GPS,LAT:%f,LNG:%f,SPEED:%f,COURSE:%f,SATES:%d,HDOP:%f,SIGNAL:%d",
		gps->lat,
		gps->lng,
		gps->speed,
		gps->course,
		gps->satellites_tracked,
		gps->hdop,
		gps->signal_intensity_grade);
    #endif
    system_state_set_last_gps(gps);
    if(KK_SUCCESS != protocol_pack_gps_msg(mode, gps, buff, &idx, len))
    {
        return KK_SYSTEM_ERROR;
    }

    len=idx;  // idx is msg len

    s_gps_socket_extend.send_location_counts++; 

    return gps_service_push_to_stack(buff, idx);
}

void gps_service_heart_atonce(void)
{
	LOG(LEVEL_INFO,"gps_service_heart_atonce");
	s_gps_socket_extend.heart_atonce = true;
}


KK_ERRCODE gps_service_send_one_locate(GpsDataModeEnum mode, bool use_lbs)
{
    u8 buff[HIS_FILE_FRAME_SIZE];
    u16 len = sizeof(buff);
    u16 idx = 0;
    GPSData gps;

    LOG(LEVEL_DEBUG,"clock(%d) gps_service_send_one_locate mode(%d) use_lbs(%d).",util_clock(), mode, use_lbs);
    if(s_gps_socket.status != SOCKET_STATUS_LOGIN && s_gps_socket.status != SOCKET_STATUS_WORK)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_send_one_locate socket->status(%s) error.", util_clock(), kk_socket_status_string((SocketStatus)s_gps_socket.status));
        return KK_PARAM_ERROR;
    }
    
    //超越平台 不认心跳包,没有GPS包就会显示掉线
    if(use_lbs)
    {
        #if 0
        if(!gps_is_fixed())
        {
            //未定位, 发LBS
            if(KK_SUCCESS != protocol_pack_lbs_msg(buff, &idx, len))
            {
                return KK_SYSTEM_ERROR;
            }
            
            len=idx;  // idx is msg len
            
            if(SOCKET_STATUS_WORK == s_gps_socket.status)
            {
                LOG(LEVEL_DEBUG,"clock(%d) gps_service_send_one_locate lbs msglen(%d)", util_clock(), idx);
                if(KK_SUCCESS == kk_socket_send(&s_gps_socket, buff, idx))
                {
                    s_gps_socket.send_time = util_clock();
                    return KK_SUCCESS;
                }
                else
                {
                    system_state_set_gpss_reboot_reason("kk_socket_send one_locate");
                    gps_service_destroy_gprs();
                }
            }
            
            return KK_SUCCESS;
        }
        #endif
    }

    //已定位,或不用lbs
    memset(&gps,0,sizeof(gps));
    gps_get_last_data(&gps);
	gps_find_optimal_data_by_time_and_hdop(&gps);
    if(gps.gps_time == (time_t)0)
    {
        system_state_get_last_gps(&gps);
    }
    
    if(gps.gps_time == (time_t)0)
    {
        gps.lat = ModelLat;
        gps.lng = ModelLong;

        if(gps.lat < 0.00001f)
        {
            // prevent send not located position.
            return KK_SUCCESS;
        }

    }

    //use current time.
    gps.gps_time = util_get_utc_time();
	//如果静止状态，速度设置为0
	if (VEHICLE_STATE_STATIC == system_state_get_vehicle_state())
	{
		gps.speed = 0;
	}

    if(KK_SUCCESS != protocol_pack_gps_msg(mode, &gps, buff, &idx, len))
    {
        return KK_SYSTEM_ERROR;
    }

    len=idx;  // idx is msg len

    if(SOCKET_STATUS_WORK == s_gps_socket.status)
    {
        LOG(LEVEL_DEBUG,"clock(%d) gps_service_send_one_locate gps msglen(%d)", util_clock(), idx);
        #if 0
        if(KK_SUCCESS == kk_socket_send(&s_gps_socket, buff, idx))
        {
            s_gps_socket.send_time = util_clock();
            return KK_SUCCESS;
        }
        else
        {
            system_state_set_gpss_reboot_reason("kk_socket_send onelocate");
            gps_service_destroy_gprs();
        }
        #else
        gps_service_save_to_cache(buff, idx);
        #endif
    }

    return KK_SUCCESS;
}


static KK_ERRCODE protocol_pack_lbs_msg(u8 *pdata, u16 *idx, u16 len)
{
    protocol_jt_pack_lbs_msg(pdata, idx, len);  // max 97 bytes
    return KK_SUCCESS;
}



KK_ERRCODE gps_service_push_lbs(void)
{
    u8 buff[HIS_FILE_FRAME_SIZE];
    u16 len = sizeof(buff);
    u16 idx = 0;

    s_gps_socket_extend.lbs_send_time = util_clock();
    
    if(KK_SUCCESS != protocol_pack_lbs_msg(buff, &idx, len))
    {
        return KK_SYSTEM_ERROR;
    }

    len=idx;  // idx is msg len

    if(SOCKET_STATUS_WORK == s_gps_socket.status)
    {
        LOG(LEVEL_DEBUG,"gps_service_push_lbs msglen(%d)",idx);

        //gps_service_send_msg(MSG_LBS,buff,idx);
        gps_service_save_to_cache(buff,idx);
    }

    return KK_SUCCESS;
}



static KK_ERRCODE protocol_pack_alarm_msg(AlarmInfo *alarm,u8 *pdata, u16 *idx, u16 len)
{

    protocol_jt_pack_gps_msg2(pdata, idx, len);  // 53 bytes
    
    return KK_SUCCESS;
}

/**
 * 短信报警
*/
static KK_ERRCODE gps_service_push_sms_alarm(AlarmInfo *alarm)
{
    //KK_ERRCODE content;
    u16 lencont = 0;
    //u8 imei[15 + 1] = {0};
    //char *number = 0//config_service_get_pointer(CFG_USER1_NUMBER);
    u8 buff[1024] = {0};
    //u8 zone = 0;
    //u8 bcd_tim[6] = {0};
    //ST_Time current_time;
    GPSData gps;
    //获取本地时间
    //zone = config_service_get_zone();
    //util_get_current_local_time(bcd_tim, &current_time, zone);
    //GPS
    memset(&gps,0,sizeof(gps));
    gps_get_last_data(&gps);
    if(gps.gps_time == (time_t)0)
    {
        system_state_get_last_gps(&gps);
    }
    
    if(gps.gps_time == (time_t)0)
    {
        gps.lat = ModelLat;
        gps.lng = ModelLong;
    }

   // lencont += snprintf((char*)buff, 1023,"IMEI:%s,Time:%d-%02d-%02d %02d:%02d:%02d,AlarmType:", ModelImei,current_time.year,current_time.month,current_time.day,current_time.hour,current_time.minute,current_time.second);
    
    
    switch (alarm->type)
    {
        case ALARM_POWER_OFF:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "POWER OFF");
            break;
        case ALARM_BATTERY_LOW:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "BATTERY LOW");
            break;
        case ALARM_SHOCK:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "SHOCK");
            break;
        case ALARM_MOVE:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "MOVE");
            break;
        case ALARM_SPEED:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "SPEED");
            break;
        case ALARM_FAKE_CELL:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "FAKE CELL");
            break;
        case ALARM_POWER_HIGH:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "POWER HIGH");
            break;
        case ALARM_COLLISION:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "COLLISION");
            break;
        case ALARM_SPEED_UP:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "SPEED UP");
            break;
        case ALARM_SPEED_DOWN:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "SPEED DOWN");
            break;
        case ALARM_TURN_OVER:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "TURN OVER");
            break;
        case ALARM_SHARP_TURN:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "SHARP TURN");
            break;
        default:
            lencont += snprintf((char*)&buff[lencont],1023-lencont, "REMOVE");
            break;
    }

    
    if(fabs(gps.lat) < 0.00001f)
    {
        // prevent send not located position.
        lencont += snprintf((char*)&buff[lencont],1023-lencont, ",Address:null");
    }
    else
    {
        u8 ns = 'N';
        u8 ew = 'E';
        if(gps.lat<0)
        {
            ns ='S';
        }
        if(gps.lng<0)
        {
            ew = 'W';
        }
        //lencont += snprintf((char*)&buff[lencont],1023-lencont, ",Address:<%02d-%02d %02d:%02d>",current_time.month,current_time.day,current_time.hour,current_time.minute);
        lencont += snprintf((char*)&buff[lencont],1023-lencont, "http://maps.google.com/maps?q=%c%.6f,%c%.6f",ns,fabs(gps.lat),ew,fabs(gps.lng));
    }

    LOG(LEVEL_INFO,"SMS send %s",buff);
    
    return KK_SUCCESS;//sms_send((char*)buff,lencont,number,KK_DEFAULT_DCS);
}

KK_ERRCODE gps_service_push_alarm(AlarmInfo *alarm)
{
    u8 buff[HIS_FILE_FRAME_SIZE];
    u16 len = sizeof(buff);
    u16 idx = 0;

    gps_service_push_sms_alarm(alarm);

    //不定位不报警,很可能造成断电报警丢失
	//先报上去,等GPS定位了也会上报位置的

    if(KK_SUCCESS != protocol_pack_alarm_msg(alarm, buff, &idx, len))
    {
    	LOG(LEVEL_ERROR,"Failed to protocol_pack_alarm_msg!");
        return KK_SYSTEM_ERROR;
    }

    len=idx;

    if(SOCKET_STATUS_WORK == s_gps_socket.status)
    {
        LOG(LEVEL_DEBUG,"gps_service_push_alarm msglen(%d) alarm(%d)",len,alarm->type);

        //gps_service_send_msg(MSG_ALARM,buff,idx);
        gps_service_save_to_cache(buff,idx);
    }

    return KK_SUCCESS;//gps_service_push_to_stack(buff, idx);
}


static KK_ERRCODE protocol_pack_device_state_msg(u8 *pdata, u16 *idx, u16 len)
{
   
    return KK_SUCCESS;
}



KK_ERRCODE gps_service_report_device_state(void)
{
	u8 buff[HIS_FILE_FRAME_SIZE];
	u16 len = sizeof(buff);
	u16 idx = 0;

	if(KK_SUCCESS != protocol_pack_device_state_msg(buff, &idx, len))
	{
		LOG(LEVEL_ERROR,"Failed to gps_service_report_device_state!");
		return KK_SYSTEM_ERROR;
	}

	len=idx;

	if(SOCKET_STATUS_WORK == s_gps_socket.status)
	{
		LOG(LEVEL_DEBUG,"gps_service_report_device_state msglen(%d)",len);

        //gps_service_send_msg(MSG_DEV_STA,buff,idx);
        gps_service_save_to_cache(buff,idx);
	}

	return KK_SUCCESS;//gps_service_push_to_stack(buff, idx);
}



static KK_ERRCODE protocol_pack_position_request_msg(u8 *mobile_num, u8 num_len, u8 *command, u8 cmd_len,u8 *pdata, u16 *idx, u16 len)
{
    
    return KK_SUCCESS;
}




KK_ERRCODE gps_service_push_position_request(u8 *mobile_num, u8 num_len, u8 *command, u8 cmd_len)
{
    u8 buff[HIS_FILE_FRAME_SIZE];
    u16 len = sizeof(buff);
    u16 idx = 0;

    if(KK_SUCCESS != protocol_pack_position_request_msg(mobile_num,num_len,command,cmd_len, buff, &idx, len))
    {
        LOG(LEVEL_ERROR,"Failed to gps_service_push_position_request!");
        return KK_SYSTEM_ERROR;
    }

    len=idx;

    if(SOCKET_STATUS_WORK == s_gps_socket.status)
    {
        LOG(LEVEL_DEBUG,"gps_service_push_position_request msglen(%d) mobile_num(%s)",len,mobile_num);
        if(KK_SUCCESS == kk_socket_send(&s_gps_socket, buff, len))
        {
            s_gps_socket.send_time = util_clock();
            return KK_SUCCESS;
        }
        else
        {
            LOG(LEVEL_WARN,"Failed to send data by socket!");
            system_state_set_gpss_reboot_reason("kk_socket_send position_req");
            gps_service_destroy_gprs();
        }
    }

    return gps_service_push_to_stack(buff, idx);
}


void gps_service_after_register_response(void)
{
    if(SOCKET_STATUS_LOGIN != s_gps_socket.status)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_after_register_response assert(socket->status(%d) == SOCKET_STATUS_LOGIN) failed.", util_clock(), s_gps_socket.status);
        return;
    }
    protocol_send_login_msg(&s_gps_socket);
}


void gps_service_after_login_response(void)
{	
	JsonObject* p_json_log = json_create();
	//u8 work_mode;
	
    char ip_str[16] = {0};
	U8* ip = gps_service_get_current_ip();
	snprintf(ip_str, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	
    if(SOCKET_STATUS_LOGIN != s_gps_socket.status)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_after_login_response assert(socket->status(%d) == SOCKET_STATUS_LOGIN) failed.", util_clock(), s_gps_socket.status);
        return;
    }
    
    if(KK_SUCCESS != protocol_send_device_msg(&s_gps_socket))
    {
        return;
    }
    
    protocol_send_heartbeat_msg(&s_gps_socket);
    s_gps_socket_extend.heart_locate_send_time = s_gps_socket_extend.heart_send_time = util_clock();
	
    gps_service_send_one_locate(GPS_MODE_FIX_TIME, false);
    
    gps_service_transfer_status(SOCKET_STATUS_WORK);
	
	json_add_string(p_json_log, "event", "login");
	json_add_string(p_json_log, "state", "ok");
	json_add_string(p_json_log, "protocol","JT808");
	json_add_string(p_json_log, "ip", ip_str);
	log_service_upload(LEVEL_INFO, p_json_log);
}

void gps_service_after_receive_heartbeat(void)
{
    if(SOCKET_STATUS_LOGIN != s_gps_socket.status && SOCKET_STATUS_WORK != s_gps_socket.status)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_after_receive_heartbeat assert(socket->status(%d)) failed.", util_clock(), s_gps_socket.status);
        return;
    }
    
    LOG(LEVEL_INFO,"clock(%d) gps_service_after_receive_heartbeat.", util_clock());
    s_gps_socket_extend.heart_receive_time = util_clock();
	s_gps_socket_extend.receive_heart_counts++;
}

U32 gps_service_get_heart_counts(void)
{
	return s_gps_socket_extend.receive_heart_counts;
}

U32 gps_service_get_location_counts(void)
{
	return s_gps_socket_extend.send_location_counts;
}

void gps_service_after_server_req(void)
{
    if(SOCKET_STATUS_WORK != s_gps_socket.status)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_after_server_req assert(socket->status(%d)) failed.", util_clock(), s_gps_socket.status);
        return;
    }
    protocol_send_general_ack(&s_gps_socket);
}


void gps_service_after_server_locate_req(void)
{
    if(SOCKET_STATUS_WORK != s_gps_socket.status)
    {
        LOG(LEVEL_WARN,"clock(%d) gps_service_after_server_locate_req assert(socket->status(%d)) failed.", util_clock(), s_gps_socket.status);
        return;
    }

    //发送一条gps数据
    gps_service_send_one_locate(GPS_MODE_FIX_TIME, true);
}


void gps_service_after_receive_remote_msg(u8 *pRet, u16 len)
{
    protocol_send_remote_ack(&s_gps_socket, pRet, len);
}

void gps_service_after_param_get(void)
{
    protocol_send_param_get_ack(&s_gps_socket);
}

void gps_service_destroy_gprs(void)
{
    #if 0
    if(SOCKET_STATUS_DATA_FINISH == update_service_get_status() || 
        SOCKET_STATUS_ERROR == update_service_get_status())
    {
        LOG(LEVEL_DEBUG,"clock(%d) gps_service_destroy_gprs update_service_status(%d)",util_clock(),update_service_get_status());
        gprs_destroy();
    }
    else
    {
        //wait gprs reinit
        gps_service_destroy();
    }
    #else
    gps_service_destroy();
    #endif
}

bool gps_service_is_login_ok(void)
{
	return (bool)(SOCKET_STATUS_WORK == s_gps_socket.status);
}

U8* gps_service_get_current_ip(void)
{
	return s_gps_socket.ip;
}



