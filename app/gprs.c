#include "gprs.h"
#include "socket.h"
#include "service_log.h"
#include "gps_service.h"
#include "update_Service.h"
#include "agps_service.h"
typedef enum 
{
    CURRENT_GPRS_INIT = 0,
    CURRENT_GPRS_CALL_OK = 1,  // 语音ok, 可以注册apn了
    CURRENT_GPRS_ACTIVATED = 2,  // 网络准备好了,可以建socket了
    CURRENT_GPRS_TIME_PROC = 3,  //after create services
    CURRENT_GPRS_FAILED = 4,
	CURRENT_GPRS_STATUS_MAX,
}CurrentGetGprsConnectStatusEnum;


typedef struct
{
    u8 status;      /* current status */
    u32 failed_time;  //故障, 准备重新开始注册时间
    u32 start_apn_time;   //开始注册apn时间
    u32 last_good_time;  // 上次网络正常的时间
    u32 call_ok_count;   //CURRENT_GPRS_INIT->CURRENT_GPRS_CALL_OK 次数
}GprsType;
static GprsType s_gprs;

#define GPRS_REREGIST_INTERVALL    5
#define GPRS_REREGIST_NEED_TIME_MAX  90
#define GPRS_STATUS_STRING_MAX_LEN 32
#define GPRS_REBOOT_HEART_FAILEDTIMES  7

const char s_gprs_status_string[CURRENT_GPRS_STATUS_MAX][GPRS_STATUS_STRING_MAX_LEN] = 
{
    "CURRENT_GPRS_INIT",
    "CURRENT_GPRS_CALL_OK",
    "CURRENT_GPRS_ACTIVATED",
    "CURRENT_GPRS_TIME_PROC",
    "CURRENT_GPRS_FAILED",
};

static KK_ERRCODE gprs_transfer_status(u8 new_status);
static void gprs_init_proc(void);
static void gprs_call_ok_proc(void);
static void gprs_failed_proc(void);


const char * gprs_status_string(u8 statu)
{
    return s_gprs_status_string[statu];
}



void gprs_config_apn(void)
{
    kk_socket_set_account_id(1);
}

static KK_ERRCODE gprs_transfer_status(u8 new_status)
{
    u8 old_status = (u8)s_gprs.status;
    KK_ERRCODE ret = KK_PARAM_ERROR;
    switch(s_gprs.status)
    {
        case CURRENT_GPRS_INIT:
            switch(new_status)
            {
                case CURRENT_GPRS_INIT:
                    break;
                case CURRENT_GPRS_CALL_OK:
                    s_gprs.call_ok_count ++;
                    ret = KK_SUCCESS;
                    break;
                case CURRENT_GPRS_ACTIVATED:
                    break;
                case CURRENT_GPRS_TIME_PROC:
                    break;
                case CURRENT_GPRS_FAILED:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case CURRENT_GPRS_CALL_OK:
            switch(new_status)
            {
                case CURRENT_GPRS_INIT:
                    break;
                case CURRENT_GPRS_CALL_OK:
                    break;
                case CURRENT_GPRS_ACTIVATED:
                    ret = KK_SUCCESS;
                    break;
                case CURRENT_GPRS_TIME_PROC:
                    break;
                case CURRENT_GPRS_FAILED:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case CURRENT_GPRS_ACTIVATED:
            switch(new_status)
            {
                case CURRENT_GPRS_INIT:
                    break;
                case CURRENT_GPRS_ACTIVATED:
                    break;
                case CURRENT_GPRS_TIME_PROC:
                    ret = KK_SUCCESS;
                    break;
                case CURRENT_GPRS_FAILED:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case CURRENT_GPRS_TIME_PROC:
            switch(new_status)
            {
                case CURRENT_GPRS_INIT:
                    break;
                case CURRENT_GPRS_ACTIVATED:
                    break;
                case CURRENT_GPRS_TIME_PROC:
                    break;
                case CURRENT_GPRS_FAILED:
                    ret = KK_SUCCESS;
                    break;
                default:
                    break;
            }
            break;
        case CURRENT_GPRS_FAILED:
            switch(new_status)
            {
                case CURRENT_GPRS_INIT:
                    ret = KK_SUCCESS;
                    break;
                case CURRENT_GPRS_ACTIVATED:
                    break;
                case CURRENT_GPRS_TIME_PROC:
                    break;
                case CURRENT_GPRS_FAILED:
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
        s_gprs.status = new_status;
        LOG(LEVEL_INFO,"gprs_transfer_status from %s to %s success",  gprs_status_string(old_status),gprs_status_string(new_status));
    }
    else
    {
        LOG(LEVEL_WARN,"gprs_transfer_status assert(from %s to %s) failed",  gprs_status_string(old_status),gprs_status_string(new_status));
    }

    return ret;

}


void gprs_socket_notify(void* msg_ptr)
{
    SocketType * socket;
    SocNotifyIndStruct *msg = (SocNotifyIndStruct *)msg_ptr;
    
    socket = get_socket_by_id(msg->socket_id);
    if(!socket)
    {
        LOG(LEVEL_ERROR,"gprs_socket_notify assert(socket_id(%d)) failed.",  msg->socket_id);
        SocketClose(msg->socket_id);
        return;
    }
    else
    {
        LOG(LEVEL_DEBUG,"gprs_socket_notify socket->access_id(%d) id(%d) result(%d) event(%d).", 
             socket->access_id, socket->id, msg->error_cause, msg->event_type);
    }
    
    if (msg->error_cause == KK_SUCCESS)
    {
        switch (msg->event_type)
        {
            case SOC_MSG_READ:
                kk_socket_recv(socket,msg->msg_len,(u8 *)msg->data);
                break;
                
            case SOC_MSG_WRITE:          
                LOG(LEVEL_DEBUG,"gprs_socket_notify KK_SOC_WRITE(%d).",  msg->socket_id);
                break;
                
            case SOC_MSG_ACCEPT:
                LOG(LEVEL_DEBUG,"gprs_socket_notify KK_SOC_ACCEPT(%d).",  msg->socket_id);
                break;
                
            case SOC_MSG_CONNECT:
            {
                LOG(LEVEL_INFO,"kk_socket_connect tcp(%d.%d.%d.%d:%d) id(%d) access_id(%d) success.", 
                     socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3], socket->port, socket->id, socket->access_id);
                kk_socket_connect_ok(socket);
                break;
            }
            
            case SOC_MSG_CLOSE:
            {
                LOG(LEVEL_INFO,"gprs_socket_notify id(%d) close. error_cause(%d)",  msg->socket_id,msg->error_cause);
                kk_socket_close_for_reconnect(socket);
                break;
            }
            default:
            {
                LOG(LEVEL_DEBUG,"gprs_socket_notify id(%d) unknown. error_cause(%d).",  msg->socket_id,msg->error_cause);
                break;
            }
        }
    }
    else
    {
        LOG(LEVEL_DEBUG,"gprs_socket_notify socket_id(%d) error_cause(%d).",  msg->socket_id,msg->error_cause);
        if (IsModelSimOK == 0)
        {
            LOG(LEVEL_INFO,"gprs_socket_notify Check Sim Valid failed.");
        }

        if(socket->access_id == SOCKET_INDEX_MAIN)
        {
            gps_service_destroy_gprs();
        }
        else
        {
            kk_socket_destroy(socket);
        }
    }
}



KK_ERRCODE gprs_create(void)
{
    s_gprs.status = CURRENT_GPRS_INIT;

    // 在gprs_init_proc中需要检测
    s_gprs.last_good_time = s_gprs.failed_time = util_clock();
    s_gprs.call_ok_count = 0;
    
    kk_socket_global_init();
	agps_service_create();
    //config_service_create();
    gps_service_create(true);
	update_service_create(true);
	log_service_create();
    dns_prase_register_callback(socket_get_host_by_name_callback);
    
    
    return KK_SUCCESS;
}


static void gprs_init_proc(void)
{
    u32 current_time = 0;
    
    if (IsModelRegOK)
    {
        #if 1
        // 注网成功(包括cg 与creg)
		JsonObject* p_log_root = json_create();
	
		json_add_string(p_log_root, "event", "LTE call ok");
		json_add_string(p_log_root, "iccid", (char*)ModelIccid);
		json_add_int(p_log_root, "csq", ModelCsq);
		#endif
        //gprs_config_apn();
        s_gprs.start_apn_time = util_clock();
        gprs_transfer_status(CURRENT_GPRS_CALL_OK);

		socket_register_callback(gprs_socket_notify);
		
		log_service_upload(LEVEL_INFO,p_log_root);
		
    }
    else
    {
        led_service_net_set(LED_OFF);
        current_time = util_clock();
        if((current_time - s_gprs.failed_time) > GPRS_REREGIST_NEED_TIME_MAX)
        {
            LOG(LEVEL_DEBUG,"gprs_init_proc exceed(%d)",GPRS_REREGIST_NEED_TIME_MAX);
            gprs_destroy();
        }
        // else recheck.
    }
}


static void gprs_call_ok_proc(void)
{
    u32 current_time = 0;
    //s32 bear_status = 0;
    u8 checksum[10] = {0};
    
    if (IsModelPdpOK)
    {
		JsonObject* p_log_root = json_create();
        //此时网络可用了
        led_service_net_set(LED_FAST_FLASH);
        gprs_transfer_status(CURRENT_GPRS_ACTIVATED);

        // 网络可用了
		json_add_string(p_log_root, "event", "net ok");
        json_add_string(p_log_root, "version",VERSION_NUMBER);
        sprintf((char *)checksum,"%4X",system_file_checksum());
        json_add_string(p_log_root, "sys checksum",(const char *)checksum);
        memset(checksum,0,sizeof(checksum));
        sprintf((char *)checksum,"%4X",ext_file_checksum());
        json_add_string(p_log_root, "ext checksum",(const char *)checksum);
		json_add_int(p_log_root, "csq", ModelCsq);
		log_service_upload(LEVEL_INFO,p_log_root);

    }
    else
    {
        current_time = util_clock();
        if((current_time - s_gprs.start_apn_time) > GPRS_REREGIST_NEED_TIME_MAX)
        {
            LOG(LEVEL_INFO,"gprs_call_ok_proc gprs_destroy because bear_status(%d)",IsModelPdpOK);
            gprs_destroy();
        }
    }
}

KK_ERRCODE gprs_destroy(void)
{
    #if 1
    //上传日志
    JsonObject* p_log_root = json_create();
    char ip_str[16] = {0};
	//U8 iccid[30] = {0};

	U8* ip = gps_service_get_current_ip();

	//net_get_iccid(iccid);
	snprintf(ip_str, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	json_add_string(p_log_root, "event", "LTE Failed");
	json_add_string(p_log_root, "addr", config_service_get_pointer(CFG_SERVERADDR));
	json_add_string(p_log_root, "ip", ip_str);
	json_add_string(p_log_root, "iccid", (char*)ModelIccid);
	json_add_int(p_log_root, "csq", ModelCsq);
	log_service_upload(LEVEL_INFO,p_log_root);
	#endif
    kk_socket_global_destroy();

    //关闭gprs网络等待重新注册成功
    socket_event_send(SOC_MSG_NET_DESTORY,NULL,0,0,SOC_SUCCESS);
    
    led_service_net_set(LED_SLOW_FLASH);

    s_gprs.failed_time = util_clock();
    gprs_transfer_status(CURRENT_GPRS_FAILED);
	socket_register_callback(NULL);
    return KK_SUCCESS;
}

static void gprs_failed_proc(void)
{
    u32 current_time = util_clock();

    //卡失效时，要进入飞行模式再退出才可用。但飞行模式用得少，还是重启靠谱
	if(gprs_check_need_reboot(s_gprs.last_good_time))
	{
        return;
	}
	
    //if((current_time - s_gprs.failed_time) >= GPRS_REREGIST_INTERVALL)
    {
        gprs_transfer_status(CURRENT_GPRS_INIT);
    }

}

KK_ERRCODE gprs_timer_proc(void)
{
    switch(s_gprs.status)
    {
        case CURRENT_GPRS_INIT:
            gprs_init_proc();
            break;
        case CURRENT_GPRS_CALL_OK:
            gprs_call_ok_proc();
            break;
        case CURRENT_GPRS_ACTIVATED:
            //本意是在此创建各service, 但目前用不上
            gprs_transfer_status(CURRENT_GPRS_TIME_PROC);
            break;
        case CURRENT_GPRS_TIME_PROC:
            s_gprs.last_good_time = util_clock();
            kk_socket_get_host_timer_proc();
            agps_service_timer_proc();
            //config_service_timer_proc();
            
            // config_service 放前面, 确保第一次能创建service
            update_service_timer_proc();
            gps_service_timer_proc();
            log_service_timer_proc();
            break;
        case CURRENT_GPRS_FAILED:
            gprs_failed_proc();
            break;
    }
    return KK_SUCCESS;
}

bool gprs_is_ok(void)
{
 	return (CURRENT_GPRS_TIME_PROC == s_gprs.status ? true:false);
}


bool gprs_check_need_reboot(u32 check_time)
{
    //保命手段 , 网络一直不好, 重启
    u16 value_u16 = 180; 

	//7次心跳失败重启，考虑时间误差再加5秒
    if((util_clock() - check_time) > (GPRS_REBOOT_HEART_FAILEDTIMES*value_u16 + 5))
    {
        //ResetSystem("gprs_check_need_reboot!");
        //重启函数
        reset_system();
        return true;
    }
    return false;
}

u32 gprs_get_last_good_time(void)
{
    return s_gprs.last_good_time;
}

u32 gprs_get_call_ok_count(void)
{
    return s_gprs.call_ok_count;
}



