
#include "agps_service.h"
#include "kk_os_header.h"
#include "socket_service.h"
#include "socket.h"
#include "fs_api.h"
#define CODE_EDIT_TIME 1589705673ul
#define INVALID_TIME   2524579200ul

#define AGPS_URL        "sz.fibocom.com"
#define AGPS_ACCOUNT    "ally_agnss"
#define AGPS_PSW        "123456"
#define AGPS_PORT       9051

typedef enum
{
    AGPS_INVALID,
    AGPS_OPEN,
    AGPS_CWDIR,
    AGPS_FILE_LIST,
    AGPS_READ_FILE,
    AGPS_DL_FILE,
    AGPS_SAVE_FILE,
    AGPS_CLOSE,
    AGPS_DATA_INJECT,
    AGPS_WAIT,
    
    
}AgpsSeviceState;

typedef struct
{
    AgpsSeviceState state;
    u32 timsec;
    u32 start_time;
    //u32 last_req_time;
    u16 change_time;
    u16 data_len;
    u32 file_time;
    u8  *file;
    
}AgpsFlowControlStruct;

AgpsFlowControlStruct s_agps;
void trans_agps_state(AgpsSeviceState state);

void agps_service_ftp_notifiy(FtpNotfiyEnum id , u8 *data , u16 len)
{
	u32 time_sec = 0;
   
    switch(id)
    {
        case FTP_OPEN:
            if(data[0] == 1)
            {
                trans_agps_state(AGPS_CWDIR);
            }
            else
            {
                trans_agps_state(AGPS_OPEN);
            }
            break;
        case FTP_CD:
            trans_agps_state(AGPS_FILE_LIST);
            break;
        case FTP_LIST:
					
            //snprintf(s_agps.file_name,sizeof(s_agps.file_name),"%s",(char *)data);
            memcpy(&s_agps.file_time,data,4);

            time_sec = ((util_get_utc_time() + 28800ul)/(86400ul))*86400ul;

            LOG(LEVEL_INFO,"time sec %x , %x",time_sec,s_agps.file_time);

            s_agps.file_time += time_sec;

            s_agps.file_time -= 28800ul;


            LOG(LEVEL_INFO,"agps file time sec %x",s_agps.file_time);
        
            trans_agps_state(AGPS_DL_FILE);
            
            break;
        case FTP_DL:
            trans_agps_state(AGPS_READ_FILE);
            s_agps.data_len = 0;
            break;
        case FTP_RECV:
            if(len > 0)
            {
                memcpy(&s_agps.file[s_agps.data_len],data,len);

                trans_agps_state(AGPS_READ_FILE);

                s_agps.data_len += len;
            }
            else
            {
                trans_agps_state(AGPS_SAVE_FILE);
            }
            break;
        default:
            break;
    }
    
}

KK_ERRCODE agps_service_create(void)
{
    if(s_agps.file != NULL)
    {
        free(s_agps.file);

        s_agps.file = NULL;
    }

    memset(&s_agps,0,sizeof(AgpsFlowControlStruct));
		
    return KK_SUCCESS;
}

void trans_agps_state(AgpsSeviceState state)
{
    s_agps.state = state;

    s_agps.start_time = clock();
}

KK_ERRCODE agps_service_save_file(void)
{
    int handle = -1;

    u32 wlen;
    
    if(FS_Delete(AGPS1_FILE_NAME) == UFS_SUCCESS)
    {
        handle = FS_Open(AGPS1_FILE_NAME,0);

        if(handle >= 0)
        {
            if(FS_Write(handle,(u8 *)&s_agps.file_time,4,&wlen) > 0)
            {
               FS_Seek(handle,4,0);

               if(FS_Write(handle,s_agps.file,s_agps.data_len,&wlen) > 0)
               {
                    FS_Close(handle);
                    
                    return KK_SUCCESS;
               }
            }

            FS_Close(handle);
        }
    }
    else
    {
        LOG(LEVEL_WARN,"agps file delete fail!");
    }

    return KK_ERROR_STATUS;
}

KK_ERRCODE agps_service_file_check(void)
{
    int handle = -1;

    u32 rlen,len,time;

    KK_ERRCODE ret = KK_UNKNOWN;

    handle = FS_Open(AGPS1_FILE_NAME,0);

    if(handle >= 0)
    {
        FS_GetFileSize(AGPS1_FILE_NAME,&len);

        if((s32)len >= 4)
        {
            FS_Read(handle,&time,4,&rlen);

            if(util_get_utc_time() - (s32)time < 30*60)
            {
                s_agps.file_time = time;

                FS_Read(handle,s_agps.file,len-4,&rlen);

                if(rlen == len-4)
                {
                    s_agps.data_len = rlen;
                    
                    ret = KK_SUCCESS;
                }
            }

            LOG(LEVEL_DEBUG,"agps file time %x,len(%d)",time,rlen);
        }

        FS_Close(handle);
    }

    return ret;
}


void agps_service_timer_proc(void)
{
    u32 cur_time = util_get_utc_time();

    switch(s_agps.state)
    {
        case AGPS_INVALID:
            ModelLong = 0.0;
            ModelLat  = 0.0;
            SocketLbsReq();
            trans_agps_state(AGPS_OPEN);
            if(s_agps.file == NULL)s_agps.file = malloc(4096);
            break;
        case AGPS_OPEN:
            if(is_agps_disable()) return;
            if(cur_time <= CODE_EDIT_TIME || cur_time >= INVALID_TIME ||\
                    ModelLong <= 0.0 && ModelLat <= 0.0)
            {
                if(clock() - s_agps.start_time > 120)
                {
                    trans_agps_state(AGPS_INVALID);
                }               
            }
            else
            {
                if(agps_service_file_check() == KK_SUCCESS)
                {
                    agps_service_close();

                    s_agps.change_time = 30;

                    trans_agps_state(AGPS_DATA_INJECT);
                }
                else
                {
                    if(SocketFtpOpen(AGPS_URL,AGPS_ACCOUNT,AGPS_PSW,AGPS_PORT,agps_service_ftp_notifiy) == KK_SUCCESS)
                    {
                        trans_agps_state(AGPS_WAIT);

                        s_agps.change_time = 60;
                    }
                    else
                    {
                       LOG(LEVEL_FATAL,"agps service open socket fail!!");
                        
                       trans_agps_state(AGPS_CLOSE); 
                    }
                }
                
            }

            break;
        case AGPS_CWDIR:
            if(SocketFtpCw("www/eph") == KK_SUCCESS)
            {
                trans_agps_state(AGPS_WAIT);

                s_agps.change_time = 60;
            }
            else
            {
                LOG(LEVEL_FATAL,"agps service cw dir fail!!");
                    
                trans_agps_state(AGPS_CLOSE); 
            }
            break;
        case AGPS_FILE_LIST:
            if(SocketFtpLis() == KK_SUCCESS)
            {
                trans_agps_state(AGPS_WAIT);

                s_agps.change_time = 60;
            }
            else
            {
                LOG(LEVEL_FATAL,"agps service read list fail!!");
                    
                trans_agps_state(AGPS_CLOSE); 
            }
            break;
        case AGPS_DL_FILE:            
            if(SocketFtpDl("HD_GPS_BDS.hdb") == KK_SUCCESS)
            {
                trans_agps_state(AGPS_WAIT);

                s_agps.change_time = 60;
            }
            else
            {
                LOG(LEVEL_FATAL,"agps service download file fail!!");
                    
                trans_agps_state(AGPS_CLOSE); 
            }
            break;
        case AGPS_READ_FILE:
            if(SocketFtpRec(512) != KK_SUCCESS)
            {
                LOG(LEVEL_FATAL,"agps service recv file fail!!");
                    
                trans_agps_state(AGPS_CLOSE); 
            }
            break;
        case AGPS_SAVE_FILE:
            trans_agps_state(AGPS_WAIT);

            LOG(LEVEL_INFO,"recv agps file len %d",s_agps.data_len);

            s_agps.change_time = 30;

            agps_service_save_file();

            agps_service_close();

            trans_agps_state(AGPS_DATA_INJECT);
            break;
        case AGPS_DATA_INJECT:
            s_agps.change_time = s_agps.file_time + 30*60 - util_get_utc_time();
            if(!gps_is_fixed())
            {
                gps_cold_start();
                gps_inject_start();
            }
            trans_agps_state(AGPS_WAIT);
            LOG(LEVEL_INFO,"AGPS file next recv interval %d",s_agps.change_time);
            break;
        case AGPS_CLOSE:
            agps_service_close();
            break;
        case AGPS_WAIT:
            {
                if(clock() - s_agps.start_time >= s_agps.change_time)
                {
                    trans_agps_state(AGPS_INVALID);
                }
            }
            break;
        default:
            break;
    }
        
}

KK_ERRCODE agps_service_close(void)
{
    SocketFtpClose();

    trans_agps_state(AGPS_WAIT);
    
    s_agps.change_time = 30;
    		
    return KK_SUCCESS;
}

u8 *agps_file_base(void)
{
    return s_agps.file;
}

u16 agps_file_len(void)
{
    return s_agps.data_len;
}

KK_ERRCODE agps_service_destory(void)
{
    memset(&s_agps,0,sizeof(AgpsFlowControlStruct));
		
    return KK_SUCCESS;
}


