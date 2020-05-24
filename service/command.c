
/*串口短信命令*/
#include "kk_os_header.h"
#include "command.h"
#include <stdarg.h>
#include <stdio.h>
#include "config_service.h"

static const char set_success_rsp_en[] = "Exec Success.\r\n";
static const char set_fail_rsp_en[] = "Exec failure!\r\n";

#pragma anon_unions  
union logprintunion
{
    u32 v;
    struct
    {
        u32 nmea_info : 1;
        u32 at_info   : 1;
        u32 agps_en   : 1;
        u32 reserv   : 29;
    };
};

union logprintunion s_log_switch;


#define CMD_NAME_MAX_LEN 11

#define COMMNADFUNCDEF(cmd) command_##cmd##_prase(u8 * p_cmd_content,u8* cmd_name, u8 * p_rsp)
#define CMDEXEFUNC(cmd) command_##cmd##_prase

typedef u16 (*command_exe_func)(u8 *p_cmd_content, u8* cmd_name, u8 *p_rsp);


static char command_scan(const char* p_command, const char* p_format, ...)
{
    bool para_num = false;
    bool optional = false;
	const char* p_field = p_command;
	char type = 0;
	s32 value_32 = 0;
	char* p_buf = NULL;
    char switch_text[8] = {0};
	
	u8 index = 0;
	
	
    va_list ap;
    va_start(ap, p_format);

    while (*p_format) 
	{
        type = *p_format++;

        if (type == ';') 
		{
            
            optional = true;
            continue;
        }

        if (!p_field && !optional) 
		{
            goto parse_error;
        }

        switch (type) 
		{
			case 'c': 
			{ 
				char value = 0;
                if (p_field && util_isprint((U8)*p_field) && *p_field != ',' && *p_field != '#')
                {
                    value = *p_field;
					para_num++;
                }
                *va_arg(ap, char*) = value;
            } 
			break;
			
            case 'w': 
			{ 
                index = 0;
                if (p_field) 
				{
                    while (util_isprint((U8)*p_field) && *p_field != ',' && *p_field != '#')
                    {
                        switch_text[index++] = *p_field++;
                    }
                }
				switch_text[index] = 0;

				util_string_upper((U8*)switch_text,strlen(switch_text));

				if (!strcmp(switch_text, "ON") || !strcmp(switch_text, "1"))
				{
					*va_arg(ap, char*) = true;
					para_num++;
				}
				else if (!strcmp(switch_text, "OFF") || !strcmp(switch_text, "0"))
				{
					*va_arg(ap, char*) = false;
					para_num++;
				}
				else
				{
				}
				
            } 
			break;
			
			// Integer value, default 0 (S32).
            case 'i': 
			{ 
                value_32 = 0;

                if (p_field && util_isdigit(*p_field)) 
				{	
                    char *endptr;
                    value_32 = util_strtol(p_field, &endptr);
                    if (util_isprint((u8)*endptr) && *endptr != ',' && *endptr != '#')
                    {
                    	
                        goto parse_error;
                    }
					para_num++;
                }
                *va_arg(ap, S32*) = value_32;
				
            } 
			break;

			// String value (char *).
            case 's': 
			{ 
                p_buf = va_arg(ap, char*);

                if (p_field && util_isprint((u8)*p_field) && *p_field != ',' && *p_field != '#') 
				{
                    while (util_isprint((u8)*p_field) && *p_field != ',' && *p_field != '#')
                    {
                        *p_buf++ = *p_field++;
                    }
					para_num++;
                }
                *p_buf = '\0';
            } 
			break;

            default:
			{ 
                goto parse_error;
            }
        }

        /* Progress to the next p_field. */
    	while (util_isprint((U8)*p_command) && *p_command != ',' && *p_command != '#')
    	{
        	p_command++;
    	}
    	/* Make sure there is a p_field there. */ 
    	if (*p_command == ',') 
		{ 
        	p_command++;
        	p_field = p_command;
    	} 
		else 
		{
        	p_field = NULL;
    	}
    }

parse_error:
    va_end(ap);
    return para_num;
}


u16 COMMNADFUNCDEF(apn)
{   
    char apn_name[CONFIG_MAX_APN_LEN] = {0};
    char user_name[CONFIG_MAX_APN_USR_LEN] = {0};
    char pass_word[CONFIG_MAX_APN_PSW_LEN] = {0};
    u8 para_num;
    u16 len;
    para_num = command_scan((char*)p_cmd_content, "s;sss", cmd_name,apn_name,user_name,pass_word);

    if (para_num == 1)
    {
        len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "APN name:%s,user:%s,passpord:%s", (char*)config_service_get_pointer(CFG_APN_NAME),(char*)config_service_get_pointer(CFG_APN_USER),(char*)config_service_get_pointer(CFG_APN_PWD));
    }
    else if(para_num == 2)
    {
        
        if(0 == strlen(apn_name))
        {
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_fail_rsp_en);
        }
        else
        {
            config_service_set(CFG_APN_NAME, TYPE_STRING, apn_name, strlen(apn_name));
            config_service_save_to_local();
            //memcpy(p_rsp, set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_success_rsp_en);
            system_state_set_gpss_reboot_reason("cmd apn param2");
            gprs_destroy();
        }
    
    }
    else if(para_num == 3)
    {
        //璁剧疆APN
        config_service_set(CFG_APN_NAME, TYPE_STRING, apn_name, strlen(apn_name));
        config_service_set(CFG_APN_USER, TYPE_STRING, user_name,strlen(user_name));
        if(KK_SUCCESS != config_service_save_to_local())
        {   
            //memcpy(p_rsp, set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_fail_rsp_en);
        }
        else
        {
            //memcpy(p_rsp, set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_success_rsp_en);
            system_state_set_gpss_reboot_reason("cmd apn param3");
            gprs_destroy();
        }
    }
    else if(para_num == 4)
    {
        //
        config_service_set(CFG_APN_NAME, TYPE_STRING, apn_name, strlen(apn_name));
        config_service_set(CFG_APN_USER, TYPE_STRING, user_name, strlen(user_name));
        config_service_set(CFG_APN_PWD, TYPE_STRING, pass_word, strlen(pass_word));
        if(KK_SUCCESS != config_service_save_to_local())
        {   
            //memcpy(p_rsp,set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_fail_rsp_en);
        }
        else
        {
            //memcpy(p_rsp,set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_success_rsp_en);
            system_state_set_gpss_reboot_reason("cmd apn param4");
            gprs_destroy();
        }   
    }
    else
    {
        //memcpy(p_rsp,set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
        len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_fail_rsp_en);
    }

    return len;
}

u16 COMMNADFUNCDEF(deviceno)
{
    char deviceno[17] = {0};
    char pre_deviceno[17] = {0};
    u8 para_num;
    u8 is_reg = 0;
    u8 reg_code[MAX_JT_AUTH_CODE_LEN] = {0};
    u16 len = 0;
    
	para_num = command_scan((char*)p_cmd_content, "s;s", cmd_name,deviceno);
    config_service_get(CFG_JT_CEL_NUM,TYPE_STRING,pre_deviceno,sizeof(pre_deviceno));
	if(para_num == 2)
	{
        if(strlen(deviceno) < 17 && strlen(deviceno) > 0)
        {
            config_service_set(CFG_JT_CEL_NUM,TYPE_STRING,deviceno,strlen(deviceno));

            //memcpy(p_rsp, set_success_rsp_en, COMMAND_MAX_RESP_LEN);
            len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_success_rsp_en);

            LOG(LEVEL_INFO,"set no:%s,pre no:%s",deviceno,pre_deviceno);

            if(strcmp(deviceno,pre_deviceno) != 0)
            {
                LOG(LEVEL_WARN,"Device number not compared,clear auth code...");

                is_reg = 0;

                config_service_set(CFG_JT_ISREGISTERED,TYPE_BYTE,&is_reg,sizeof(is_reg));

                config_service_set(CFG_JT_AUTH_CODE,TYPE_STRING,reg_code,sizeof(reg_code));

                config_service_save_to_local();
            }
        }
		else
		{
			//memcpy(p_rsp, set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
			len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"%s",set_fail_rsp_en);
		}
	}
	else
	{
        len = snprintf((char *)p_rsp,COMMAND_MAX_RESP_LEN,"device number:%s\r\n%s",pre_deviceno,set_success_rsp_en);
	}

    return len;
}

u16 COMMNADFUNCDEF(server)
{
    char addr[KK_DNS_MAX_LENTH] = {0};
    u32 port = 0;
    u8 is_udp = false;
    u8 para_num = 0;
    u16 len = 0;
    
    para_num = command_scan((char*)p_cmd_content, "s;siw", cmd_name,addr,&port,&is_udp);

    if (para_num == 1)
    {
        len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "Server:%s", (char*)config_service_get_pointer(CFG_SERVERADDR));
    }
    else if(para_num == 3 || para_num == 4)
    {
        char server_addr[64] = {0};
        
        snprintf(server_addr, 31, "%s:%d", addr,port);
        LOG(LEVEL_INFO,"Set server addr:%s:%d",addr,port);
        config_service_set(CFG_SERVERADDR, TYPE_STRING, server_addr, strlen(server_addr));
        if(KK_SUCCESS != config_service_save_to_local())
        {   
            len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", (char*)set_fail_rsp_en);
        }
        else
        {
            len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", (char*)set_success_rsp_en);
        }
        gps_service_change_config();
        //gps_power_on(true);
        //g_sensor_reset_noshake_time();
        system_state_set_reported_gps_since_modify_ip(false);
    }
    else
    {
        len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", (char*)set_fail_rsp_en);
    }

    return len;
}

u16 COMMNADFUNCDEF(timer)
{
    u8 para_num;
    U16 upload_time = 0;
    u16 len  = 0;
    para_num = command_scan((char*)p_cmd_content, "s;i", cmd_name,&upload_time);
    if (para_num == 1)
    {
        config_service_get(CFG_UPLOADTIME, TYPE_SHORT, &upload_time, sizeof(upload_time));
        

        len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "Upload interval:%d seconds", upload_time);
     
    }
    else if(para_num == 2)
    {
        //设置
        if(upload_time < CONFIG_UPLOAD_TIME_MIN || upload_time > CONFIG_UPLOAD_TIME_MAX)
        {   
            //memcpy(p_rsp, set_fail_rsp_en, COMMAND_MAX_RESP_LEN);
            snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_fail_rsp_en);
        }
        else
        {
            config_service_set(CFG_UPLOADTIME, TYPE_SHORT, &upload_time, sizeof(U16));
            config_service_save_to_local();
            //memcpy(p_rsp, set_success_rsp_en, COMMAND_MAX_RESP_LEN);
            snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_success_rsp_en);
        }
    }
    else
    {
        snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_fail_rsp_en);
    }

    return len;
}

    

u16 COMMNADFUNCDEF(version)
{

    u8 para_num;
    u16 len = 0;
    para_num = command_scan((char*)p_cmd_content, "s", cmd_name);

    if(para_num == 1)
    {
        len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "version:%s,%d", (char*)VERSION_NUMBER,gps_service_total_send_counts());
    }
    

    return len;
}

u16 COMMNADFUNCDEF(loglevel)
{
    u8 para_num;
    U16 level = 0;
    u16 len  = 0;
    para_num = command_scan((char*)p_cmd_content, "s;i", cmd_name,&level);
    if(para_num == 2)
    {
        g_log_level = level;
        len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_success_rsp_en);
    }
    else
    {
        len = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_fail_rsp_en);
    }

    return len;
}

u16 COMMNADFUNCDEF(nmealog)
{
    u8 para_num;
    U16 enable = 0;
    u16 len  = 0;
    para_num = command_scan((char*)p_cmd_content, "s;i", cmd_name,&enable);
    if(para_num == 2)
    {
        s_log_switch.nmea_info = enable>0?1:0;
        
        len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_success_rsp_en);
    }
    else
    {
        //len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "neme log state %d \r\n%s",s_log_switch.nmea_info,set_success_rsp_en);
    }

    return len;
}

u16 COMMNADFUNCDEF(atlog)
{
    u8 para_num;
    U16 enable = 0;
    u16 len  = 0;
    para_num = command_scan((char*)p_cmd_content, "s;i", cmd_name,&enable);
    if(para_num == 2)
    {
        s_log_switch.at_info = enable>0?1:0;
        
        len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_success_rsp_en);
    }
    else
    {
        //len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "neme log state %d \r\n%s",s_log_switch.nmea_info,set_success_rsp_en);
    }

    return len;
}


u16 COMMNADFUNCDEF(restart)
{

    u16 len  = 0;

    reset_system();
    //ResetSystem("Instructions");

    len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s",set_success_rsp_en);

    return len;
}

u16 COMMNADFUNCDEF(agpsdisable)
{
    u8 para_num;
    U16 enable = 0;
    u16 len  = 0;
    para_num = command_scan((char*)p_cmd_content, "s;i", cmd_name,&enable);
    if(para_num == 2)
    {
        s_log_switch.agps_en = enable>0?1:0;
        
        len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "%s", set_success_rsp_en);
    }
    else
    {
        //len  = snprintf((char*)p_rsp, COMMAND_MAX_RESP_LEN, "neme log state %d \r\n%s",s_log_switch.nmea_info,set_success_rsp_en);
    }

    return len;
}



typedef struct
{
    char *cmd;

    command_exe_func exe;
    
}CommandParseStruct;

u8 is_nmea_log_enable(void)
{
    return (u8)s_log_switch.nmea_info;
}

u8 is_at_log_enable(void)
{
    return (u8)s_log_switch.at_info;
}

u8 is_agps_disable(void)
{
    return (u8)s_log_switch.agps_en; 
}

CommandParseStruct s_command[] = {
    {"APN",CMDEXEFUNC(apn)},
    {"DEVICENO",CMDEXEFUNC(deviceno)},
    {"SERVER",CMDEXEFUNC(server)},
    {"VERSION",CMDEXEFUNC(version)},
    {"TIMER",  CMDEXEFUNC(timer)},
    {"LOGLEVEL",  CMDEXEFUNC(loglevel)},
    {"NMEALOG",  CMDEXEFUNC(nmealog)},
    {"ATLOG",    CMDEXEFUNC(atlog)},
    {"RESET",    CMDEXEFUNC(restart)},
    {"AGPSDISABLE",    CMDEXEFUNC(agpsdisable)},
    {NULL,NULL}
};


KK_ERRCODE command_on_receive_data(CommandReceiveFromEnum from, char* p_cmd, u16 cmd_len, char* p_rsp, void * pmsg)
{   
    char cmd_name[CMD_NAME_MAX_LEN] = {0};
    char* p_cmd_content = NULL;
    //u16 len = 0;
    u8 i = 0;

    if (NULL == p_cmd || 0 == cmd_len || cmd_len > (COMMAND_MAX_RESP_LEN - 1) || NULL == p_rsp)
    {
        return KK_PARAM_ERROR;
    }

    p_cmd_content = (char*)malloc(cmd_len + 1);
    memcpy(p_cmd_content, p_cmd, cmd_len);
    p_cmd_content[cmd_len] = 0;
    LOG(LEVEL_INFO,"From %d received cmd:%s len:%d",from,p_cmd_content, cmd_len);

    util_remove_char((u8*)p_cmd_content,cmd_len,' ');
    util_remove_char((u8*)p_cmd_content,cmd_len,'\r');
    util_remove_char((u8*)p_cmd_content,cmd_len,'\n');
    cmd_len = strlen(p_cmd_content);

    if (p_cmd_content[cmd_len -1] != '#')
    {
        snprintf(p_rsp,COMMAND_MAX_RESP_LEN,"invalid cmd from %d,len=%d:%s",from,cmd_len,p_cmd_content);
        free(p_cmd_content);
        p_cmd_content = NULL;
        return KK_PARAM_ERROR;
    }
        

    command_scan((char*)p_cmd_content, "s", cmd_name);
    util_string_upper((U8*)cmd_name,strlen(cmd_name));

    LOG(LEVEL_INFO,"Cmd name :%s",cmd_name);

    for(i = 0; s_command[i].cmd != NULL ; i++)
    {
        if(strcmp(cmd_name,s_command[i].cmd) == 0)
        {
            s_command[i].exe((u8 *)p_cmd_content,(u8 *)cmd_name,(u8 *)p_rsp);

            //LOG(INFO,"Cmd Result:%s",p_rsp);

            return KK_SUCCESS;
        }
    }

    return KK_UNKNOWN;
 
}

