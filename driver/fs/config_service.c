#include "kk_type.h"
#include "fifo.h"
#include "utility.h"
#include "config_service.h"
//#include "gps_service.h"
#include "gprs.h"
#include "fs_api.h"


#define CONFIG_PING_TIME  43200

#define MAX_CONFIG_PARA_LEN 1024

u8 s_config_para[MAX_CONFIG_PARA_LEN] = {0};

ConfigParamItems s_param_items[CFG_PARAM_MAX] = 
{
    {TYPE_SHORT,sizeof(u16)},               /*MAGIC_CRC*/
    {TYPE_STRING , CONFIG_MAX_DNS_LEN},     /*CFG_SERVERADDR*/
    {TYPE_SHORT, sizeof(u16)},              /*CFG_UPLOADTIME*/
    {TYPE_BYTE, sizeof(u8)},              /*CFG_SPEEDTHR*/
    {TYPE_BYTE,  sizeof(u8)},               /*CFG_PROTOCOL*/
    {TYPE_SHORT, sizeof(u16)},              /*CFG_HEART_INTERVAL*/
    {TYPE_STRING,CONFIG_MAX_APN_LEN},       /*CFG_APN_NAME*/
    {TYPE_STRING,CONFIG_MAX_APN_USR_LEN},   /*CFG_APN_USER*/
    {TYPE_STRING,CONFIG_MAX_APN_PSW_LEN},   /*CFG_APN_PWD*/
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_TIME_ZONE*/
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_JT_ISREGISTERED*/
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_JT_AT_TYPE*/
    {TYPE_STRING,MAX_JT_DEV_ID_LEN},      /*CFG_JT_DEVICE_ID*/
    {TYPE_STRING,MAX_JT_VEHICLE_NUMBER_LEN},/*CFG_JT_VEHICLE_NUMBER*/
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_JT_VEHICLE_COLOR*/
    {TYPE_STRING,MAX_JT_OEM_ID_LEN},          /*CFG_JT_OEM_ID*/
    {TYPE_SHORT,sizeof(u16)},               /*CFG_JT_PROVINCE*/
    {TYPE_SHORT,sizeof(u16)},               /*CFG_JT_CITY*/
    {TYPE_STRING,MAX_JT_AUTH_CODE_LEN},       /*CFG_JT_AUTH_CODE*/
    {TYPE_STRING,CONFIG_MAX_CELL_NUM_LEN},  /*CFG_JT_CELL_NUM*/
    {TYPE_SHORT,sizeof(u16)},               /*CFG_JT_HBINTERVAL*/
    {TYPE_BYTE,sizeof(u8)},                    //CFG_IS_STATIC_UPLOAD
    {TYPE_SHORT,sizeof(u16)},                //CFG_TURN_ANGLE
    {TYPE_BYTE,sizeof(u8)},                 //CFG_SPEED_ALARM_ENABLE
    {TYPE_BYTE,sizeof(u8)},                 //CFG_SPEED_CHECK_TIME
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_IS_TEST_MODE*/
    {TYPE_STRING,CONFIG_MAX_DNS_LEN},       /*CFG_LOGSERVERADDR*/
    {TYPE_STRING,CONFIG_MAX_DNS_LEN},       /*CFG_UPDATESERVERADDR*/
    {TYPE_STRING,CONFIG_MAX_DNS_LEN},       /*CFG_UPDATEFILESERVER*/
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_RELAY_STATE*/
    {TYPE_BYTE,sizeof(u8)},                 /*CFG_AGPS_EN*/
};

#define MAGIC_NUM 0xDEADFEED

ErrorStatus config_service_save_params(u8 *para , const u8 *filename);

KK_ERRCODE config_service_set_factory_default(void)
{
    u16 value_u16;

    u8 value_u8;

    u8 temp[50] = {0};

    s8 value_s8;
    
    memset(s_config_para,0,MAX_CONFIG_PARA_LEN);

    config_service_set(CFG_SERVERADDR,TYPE_STRING,CONFIG_DEFAULT_MAIN_ADDR,strlen(CONFIG_DEFAULT_MAIN_ADDR));

    value_u16 = 10;
    config_service_set(CFG_UPLOADTIME,TYPE_SHORT,&value_u16,sizeof(u16));

    config_service_set(CFG_APN_NAME, TYPE_STRING, APN_DEFAULT, strlen((const char *)APN_DEFAULT));
    config_service_set(CFG_APN_USER, TYPE_STRING, APN_USER_DEFAULT, strlen((const char *)APN_USER_DEFAULT));
    config_service_set(CFG_APN_PWD, TYPE_STRING, APN_USER_DEFAULT, strlen((const char *)APN_USER_DEFAULT));


    value_u8 = (u8)PROTOCOL_JT808;
    config_service_set(CFG_PROTOCOL, TYPE_BYTE, &value_u8, sizeof(value_u8));

    value_u8 = (u8)GB_ATTYPE_STANDARD;
    config_service_set(CFG_JT_AT_TYPE, TYPE_BYTE, &value_u8, sizeof(value_u8));

    //config_service_set(CFG_JT_CEL_NUM, TYPE_STRING, JT_DEFAULT_CELL_NUM,strlen(JT_DEFAULT_CELL_NUM));

    config_service_set(CFG_JT_DEVICE_ID, TYPE_STRING, JT_DEV_ID, strlen(JT_DEV_ID));
    config_service_set(CFG_JT_VEHICLE_NUMBER, TYPE_STRING, JT_VEHICLE_NUMBER, strlen(JT_VEHICLE_NUMBER));
    config_service_set(CFG_JT_OEM_ID, TYPE_STRING, JT_OEM_ID, strlen(JT_OEM_ID));

    //temp[0] = 0;//strlen(CONFIG_DEFAULT_REG_CODE);
    //memcpy(temp + 1 , CONFIG_DEFAULT_REG_CODE ,temp[0]);
    config_service_set(CFG_JT_AUTH_CODE, TYPE_STRING, temp, sizeof(temp));

    value_u8 = 0;
    config_service_set(CFG_JT_ISREGISTERED,TYPE_BYTE,&value_u8,sizeof(value_u8));

    value_u16 = 44;
    config_service_set(CFG_JT_PROVINCE, TYPE_SHORT, &value_u16, sizeof(value_u16));
    
    value_u16 = 303;
    config_service_set(CFG_JT_CITY, TYPE_SHORT, &value_u16, sizeof(value_u16));

    value_u8 = (u8)0x01;
    config_service_set(CFG_JT_VEHICLE_COLOR, TYPE_BYTE, &value_u8, sizeof(value_u8));

    value_s8 = 8;
	config_service_set(CFG_TIME_ZONE, TYPE_BYTE, &value_s8, sizeof(value_s8));

    value_u16 = 180;
    config_service_set(CFG_JT_HBINTERVAL,TYPE_SHORT,&value_u16,sizeof(value_u16));

    value_u16 = 180;
    config_service_set(CFG_HEART_INTERVAL,TYPE_SHORT,&value_u16,sizeof(value_u16));

    value_u8 = 0;
    config_service_set(CFG_IS_TEST_MODE, TYPE_BYTE, &value_u8, sizeof(value_u8));

    value_u16 = applied_math_calc_common_crc16(&s_config_para[2],MAX_CONFIG_PARA_LEN - 2);

    config_service_set(CFG_LOGSERVERADDR, TYPE_STRING, CONFIG_LOG_SERVER_ADDERSS, strlen(CONFIG_LOG_SERVER_ADDERSS));
    config_service_set(CFG_UPDATESERVERADDR, TYPE_STRING, KK_UPDATE_SERVER_DNS, strlen(KK_UPDATE_SERVER_DNS));
    config_service_set(CFG_UPDATEFILESERVER, TYPE_STRING, KK_UPDATE_SERVER_DNS, strlen(KK_UPDATE_SERVER_DNS));

    //value_u16 = 180;
    config_service_set(MAGIC_CRC,TYPE_SHORT,&value_u16,sizeof(value_u16));

    return KK_SUCCESS;
}

KK_ERRCODE config_service_create(void)
{    
    config_service_set_factory_default();

    config_service_read_from_fs();

    config_service_set(CFG_SERVERADDR,TYPE_STRING,CONFIG_DEFAULT_MAIN_ADDR,strlen(CONFIG_DEFAULT_MAIN_ADDR));

    config_service_set(CFG_UPDATESERVERADDR, TYPE_STRING, KK_UPDATE_SERVER_DNS, strlen(KK_UPDATE_SERVER_DNS));
    config_service_set(CFG_UPDATEFILESERVER, TYPE_STRING, KK_UPDATE_SERVER_DNS, strlen(KK_UPDATE_SERVER_DNS));
    config_service_set(CFG_LOGSERVERADDR, TYPE_STRING, CONFIG_LOG_SERVER_ADDERSS, strlen(CONFIG_LOG_SERVER_ADDERSS));

    return KK_SUCCESS;
}


KK_ERRCODE config_service_destroy(void)
{
    return KK_SUCCESS;
}

static s16 config_service_get_items_offset(ConfigParamEnum id)
{
    u16 i = 0 , offset = 0;

    if(id >= CFG_PARAM_MAX)return -1;

    for(i = 0; i < id ; i++)
    {
        offset += s_param_items[i].len;
    }

    return (s16)offset;
}

void config_service_set(ConfigParamEnum id, ConfigParamDataType type, const void* buf, u16 len)
{
	s16 offset = 0;

    u8 *ptr = s_config_para;

    offset = config_service_get_items_offset(id);

    if(offset < 0)
    {
        LOG(LEVEL_FATAL,"config_service_set assert ID(%d) failed.",id);
        
        return;
    }

    if(type != s_param_items[id].type)
    {
        LOG(LEVEL_FATAL,"config_service_set assert(s_param_items[%d].type(%d)== %d) failed.", id,s_param_items[id].type, type);

        return;
    }

    if(len > s_param_items[id].len)
    {
        LOG(LEVEL_FATAL,"config_service_set assert(s_param_items[%d].len(%d)<= %d) failed.", id,s_param_items[id].len, len);

        return;
    }

    if(offset + len > MAX_CONFIG_PARA_LEN)
    {
        LOG(LEVEL_FATAL,"config_service_set overflow offset(%d) , len(%d)",offset,len);
    }
    ptr += offset;

    memset(ptr,0,s_param_items[id].len);

    memcpy(ptr,buf,len);
    
}




/*获取指定id对应的配置        */
KK_ERRCODE config_service_get(ConfigParamEnum id, ConfigParamDataType type, void* buf, u16 len)
{	
    s16 offset = 0;

    u16 copy_len  = 0;

    u8 *ptr = s_config_para;

    offset = config_service_get_items_offset(id);

    if(offset <= 0)
    {
        LOG(LEVEL_FATAL,"config_service_get assert ID(%d) failed.",id);
        
        return KK_PARAM_ERROR;
    }
    
	if(type != s_param_items[id].type)
	{
		LOG(LEVEL_FATAL,"config_service_get assert(s_param_items[%d].type(%d)== %d) failed.", id,s_param_items[id].type, type);
		return KK_PARAM_ERROR;
	}

    ptr += offset;
	
	  copy_len = len;

    if(len > s_param_items[id].len)
    {
        //LOG(LEVEL_FATAL,"config_service_get assert(s_param_items[%d].len(%d) > %d) failed.", id,s_param_items[id].len, len);
        copy_len = s_param_items[id].len;

        //return KK_PARAM_ERROR;
    }

    

    memcpy((u8 *)buf,ptr,copy_len);

	return KK_SUCCESS;
}


u16 config_service_get_length(ConfigParamEnum id, ConfigParamDataType type)
{
    if(id >= CFG_PARAM_MAX )
    {
        LOG(LEVEL_WARN,"config_service_get assert(id(%d)) failed.", id);
        return 0;
    }

	return s_param_items[id].len;
}


void* config_service_get_pointer(ConfigParamEnum id)
{	
    s16 offset = 0;

    u8 *ptr = s_config_para;

    offset = config_service_get_items_offset(id);

    if(offset <= 0)
    {
        LOG(LEVEL_FATAL,"config_service_get assert ID(%d) failed.",id);
        
        return NULL;
    }
    
	ptr += offset;

	return ptr;
}

bool config_service_is_test_mode(void)
{
    u8 is_test;

    config_service_get(CFG_IS_TEST_MODE,TYPE_BYTE,&is_test,sizeof(u8));

    return (is_test > 0)?true:false;
}

bool config_service_is_default_imei(void)
{
    return false;
}

void config_service_change_ip(ConfigParamEnum idx, u8 *buf, u16 len)
{
    u8 dns_str[KK_DNS_MAX_LENTH];
    u8 value_str[CONFIG_STRING_MAX_LEN];
    u8 IP[4];
    u32 value_u32;
	s32 index;

    if(len >=  KK_DNS_MAX_LENTH)
    {
        LOG(LEVEL_WARN,"config_service_change_ip assert(len(%d) of CFG_SERVERADDR failed.", len);
        return;
    }
    
    memset(value_str, 0, sizeof(value_str));
    memcpy(value_str, buf, len);
    if(KK_SUCCESS != ConvertIpAddr(value_str, IP))
    {
        if(!util_is_valid_dns(value_str, len))
        {
            LOG(LEVEL_WARN,"config_service_change_ip assert(valid dns(%s) failed.",  value_str);
            return;
        }
    }

    memset(dns_str, 0x00, sizeof(dns_str));
    index = sscanf((const char*)config_service_get_pointer(idx), "%[^:]:%d", dns_str, &value_u32);
    if ((index != 2) || (strlen((const char*)dns_str) >= KK_DNS_MAX_LENTH) || (value_u32>65535))
    {
        LOG(LEVEL_WARN,"config_service_change_ip assert(idx ==2) of CFG_SERVERADDR failed.", util_clock());
        return;
    }

    strcpy((char *)dns_str,(char *)value_str);
    snprintf((char *)value_str, sizeof(value_str),"%s:%d", dns_str, value_u32);
    config_service_set(idx, TYPE_STRING, value_str, strlen((const char*)value_str));
}


void config_service_change_port(ConfigParamEnum idx, u16 port)
{
    u8 dns_str[KK_DNS_MAX_LENTH];
    u8 value_str[CONFIG_STRING_MAX_LEN];
    u32 value_u32;
	s32 index;

    memset(dns_str, 0x00, sizeof(dns_str));
    index = sscanf((const char*)config_service_get_pointer(idx), "%[^:]:%d", dns_str, &value_u32);
    if ((index != 2) || (strlen((const char*)dns_str) >= KK_DNS_MAX_LENTH) || (value_u32>65535))
    {
        LOG(LEVEL_WARN,"config_service_change_port assert(idx ==2) of CFG_SERVERADDR failed.", util_clock());
        return;
    }

    snprintf((char *)value_str, sizeof(value_str),"%s:%d", dns_str, port);
    config_service_set(idx, TYPE_STRING, value_str, strlen((const char*)value_str));
}


ConfigProtocolEnum config_service_get_app_protocol()
{
    u8 byte_value;
    config_service_get(CFG_PROTOCOL, TYPE_BYTE, &byte_value, sizeof(byte_value));
    switch (byte_value)
    {

        case PROTOCOL_JT808:
            return PROTOCOL_JT808;

        default:
            LOG(LEVEL_DEBUG,"config_service_get_app_protocol (CFG_PROTOCOL(PROTOCOL_NONE).");
            return PROTOCOL_NONE;
    }
}

s8 config_service_get_zone(void)
{
    KK_ERRCODE ret;
    s8 time_zone = 0;
    
	//获取本地时间
	ret = config_service_get(CFG_TIME_ZONE, TYPE_BYTE, &time_zone, sizeof(time_zone));
	if (KK_SUCCESS != ret)
	{
		LOG(LEVEL_INFO,"config_service_get_zone failed,ret=%d", ret);
	}
    return time_zone;
}

ErrorStatus config_service_read_param(u8 *para , const u8 *filename)
{
    s32 handle = -1;

    u32 rlen = 0;

    //u8 *para = NULL;

    u16 save_crc = 0;

    u16 *crc = NULL;

    ErrorStatus res = ERROR;

    handle = FS_Open(filename, 0);

    if(handle < 0)
    {
        LOG(LEVEL_FATAL,"file(%s) open fail %d",(char *)filename,handle);

        return ERROR;
    }


    FS_Read(handle,para,MAX_CONFIG_PARA_LEN,&rlen);

    //LOG_hex(para,MAX_CONFIG_PARA_LEN,"[DATA]Read:");

    if(rlen != MAX_CONFIG_PARA_LEN)
    {
        //config_service_set_factory_default();
        
        //FS_Write(handle,s_config_para,MAX_CONFIG_PARA_LEN,&rlen);

        //memcpy(para,s_config_para);
        LOG(LEVEL_WARN,"file(%s) read LEN %d",(char *)filename,rlen);

        res = ERROR;
    }
    else
    {
        crc = (U16 *)para;

        save_crc = applied_math_calc_common_crc16(para+2,MAX_CONFIG_PARA_LEN-2);

        LOG(LEVEL_DEBUG,"saved crc %X cur crc %X",save_crc,*crc);
        
        if(save_crc == (*crc))
        {
             res = SUCCESS;
        }
        else
        {
            LOG(LEVEL_WARN,"file(%s) crc error",(char *)filename);

            res = ERROR;
        }
        
    }

    FS_Close(handle);

    return res;
}

KK_ERRCODE config_service_read_from_fs(void)
{
    u8 *para = NULL;
    
    para = malloc(MAX_CONFIG_PARA_LEN);

    if(para != NULL)
    {
        if(config_service_read_param(para,MAN_PARAM_NAME) == SUCCESS)
        {
            memcpy(s_config_para,para,MAX_CONFIG_PARA_LEN);
        }
        else if(config_service_read_param(para,BKP_PARAM_NAME) == SUCCESS)
        {
            memcpy(s_config_para,para,MAX_CONFIG_PARA_LEN);

            config_service_save_params(para,MAN_PARAM_NAME);
        }
        else
        {
            config_service_set_factory_default();

            config_service_save_to_local();
        }

        free(para);
           
    }

    return KK_SUCCESS;
}

ErrorStatus config_service_save_params(u8 *para , const u8 *filename)
{
    s32 handle = -1;

    u32 wlen = 0;

    //u8 *para = NULL;

    //u16 *crc = NULL;

    ErrorStatus res = ERROR;

    #if 1

    handle = FS_Open(filename, 0);

    if(handle < 0)
    {
        LOG(LEVEL_FATAL,"config_service_save_params file(%s) open fail %d",(char *)filename,handle);

        return ERROR;
    }

    FS_Write(handle,para,MAX_CONFIG_PARA_LEN,&wlen);

    if(wlen != MAX_CONFIG_PARA_LEN)
    {
        LOG(LEVEL_WARN,"config_service_save_params file(%s) write LEN %d",(char *)filename,wlen);
        
        res = ERROR;
    }
    else
    {
        res = SUCCESS;
    }

    FS_Close(handle);
    #endif

    return res;
}


KK_ERRCODE config_service_save_to_local(void)
{
    u16 value_u16 = 0;

    value_u16 = applied_math_calc_common_crc16(&s_config_para[2],MAX_CONFIG_PARA_LEN - 2);

    config_service_set(MAGIC_CRC,TYPE_SHORT,&value_u16,sizeof(value_u16));

    LOG(LEVEL_INFO,"calc crc %x",value_u16);
    
    config_service_save_params(s_config_para,MAN_PARAM_NAME);

    config_service_save_params(s_config_para,BKP_PARAM_NAME);
    
    return KK_SUCCESS;
}

StreamType config_service_update_socket_type(void)
{
    return STREAM_TYPE_STREAM;
}


