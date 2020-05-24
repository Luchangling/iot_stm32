
#include "l610_socket_service.h"
#include "time.h"
#include "board.h"
static s32 at_event_lock  = -1;

static s32 at_socket_event = -1;

SocketFtpStruct l610_ftp;  //= {
//                .opened     = 0,
//                .notfiy_cb  = NULL,
//};


#define L610_SUPPORT_MAX_SOCKET 6

static u8 l610_socket_id[L610_SUPPORT_MAX_SOCKET + 2] = {0};  //just for align

#define L610_MAX_SEND_LEN  1446

#define ASSERTSOCKET(id) do                                                                                                          \
                         {                                                                                                           \
                             if (id == 0 || id > L610_SUPPORT_MAX_SOCKET)    \
                             {                                                                                                       \
                                 return SOC_INVALID_SOCKET;                                                                               \
                             }                                                                                                       \
                         } while(0);

const char *CGDCONT_CHINA_MOBILE  = "AT+CGDCONT=1,\"IP\",\"CMNET\"";
const char *CGDCONT_CHINA_UNICOM  = "AT+CGDCONT=1,\"IP\",\"UNINET\"";
const char *CGDCONT_CHINA_TELECOM = "AT+CGDCONT=1,\"IP\",\"CTNET\"";
const char *NTP_SERVER            = "ntp5.aliyun.com";
static char sresp_buf[128] = {0};
static ATRespStruct sresp = {
                                sresp_buf,
                                128,
                                0,0,300
                            };  //just for fast AT commad



#define L610_SOCKET_CONNECT_OK   1<<0
#define L610_SOCKET_CONNECT_FAIL 1<<1
#define L610_SOCKET_SEND_OK      1<<2
#define L610_SOCKET_SEND_FAIL    1<<3


static void l610_socket_mamger(s32 socket_id)
{
    if(socket_id > 0 && socket_id <= L610_SUPPORT_MAX_SOCKET)
    {
        l610_socket_id[socket_id] = 0;
    }
}

static void l610_urc_mippush(const char *data, u16 size)
{
    #if 0
    s32 sock_id = 0;
    s32 errocde = 0;
    
    sscanf(data,"+MIPPUSH: %d,%d",&sock_id,&errocde);

    if(sock_id > 0 && sock_id < L610_MAX_SEND_LEN && errocde == 0)
    {        
        kk_os_event_send(at_socket_event, L610_SOCKET_SEND_OK);

        errocde = SOC_SUCCESS;
    }
    else
    {
        errocde = SOC_ERROR;
        
        kk_os_event_send(at_socket_event, L610_SOCKET_SEND_FAIL);
    }

    LOG(LEVEL_DEBUG,"Push socket %d , errocde %d",sock_id,errocde);
    #endif
}

static void l610_urc_miprtcp(const char *data, u16 size)
{
    //+MIPRTCP: 1,20,hex
    s32 sock_id = -1;

    u8 *d = NULL;

    u16 i,count = 0;

    u8 chr,str[10];

    if(sscanf(data,"+MIPRTCP: %d",&sock_id) == 1)
    {
        for(i = 0 ; i < 10 ; i++)
        {
            if(at_get_resp_chr(&chr) == KK_SUCCESS)
            {
                if(chr == ',')
                {
                    str[i] = '\0';

                    count  = (u16)atoi((const char *)str);

                    break;
                }
                else
                {
                    str[i] = chr;
                }
            }
            else
               break;
        }

        if(count > 0 && count < L610_MAX_SEND_LEN)
        {
            d = (u8 *)malloc(count);

            if(!d) 
            {
                LOG(LEVEL_FATAL,"urc miptcp malloc error");
                return;
            }
            
            for(i = 0 ; i < count ; i++)
            {
                if(at_get_resp_chr(&chr) == KK_SUCCESS)
                {
                    d[i] = chr;
                }
                else
                    break;
            }

            LOG(LEVEL_DEBUG,"TCP incoming socket id %d len %d",sock_id,count);

            //LOG_HEX(LEVEL_DEBUG,d,count,"Socket(%d) recv len(%d)<-",sock_id,count);

            socket_event_send(SOC_MSG_READ,d,count,sock_id,SOC_SUCCESS);

        }
    }
    
}

static void l610_urc_miprudp(const char *data, u16 size)
{
    //+MIPRUDP: 120.78.176.175,10009,2,10,<RZ*ACK*3>
    u8 *d = NULL;

    u8 str[16] = {0};

    s32 sock_id,count = 0;

    u16 len = 16;
    
    FifoType *fifo = at_resp_fifo();

    if(fifo_peek_until(fifo, str,&len, ',') == KK_SUCCESS)
    {
        fifo_pop_len(fifo, len);

        len = 16;

        if(fifo_peek_until(fifo, str,&len, ',') == KK_SUCCESS)
        {
            sock_id = atoi((const char*)str);

            fifo_pop_len(fifo,len);

            len  = 16;

            if(fifo_peek_until(fifo, str,&len, ',') == KK_SUCCESS)
            {
                count = atoi((char *)str);
                
                fifo_pop_len(fifo, len);

                d = (u8 *)malloc(count);

                if(d)
                {
                    fifo_peek(fifo,d,count);

                    fifo_pop_len(fifo,count);

                    LOG(LEVEL_DEBUG,"UDP incoming socket id %d len %d",sock_id,count);

                    //LOG_HEX(LEVEL_DEBUG,d,count,"Socket(%d) recv len(%d)<-",sock_id,count);

                    socket_event_send(SOC_MSG_READ,d,count,sock_id,SOC_SUCCESS);
                }
            }
        }
    }

    
}

static void l610_urc_mipopen(const char *data, u16 size)
{
    //+MIPOPEN: <socket_id>,<STATE> state = 1 active
    s32 socket_id,state = -1;
    SocErrorEnum ret = SOC_SUCCESS;

    if(sscanf(data,"+MIPOPEN: %d,%d",&socket_id,&state) == 2)
    {
        if(socket_id <= L610_SUPPORT_MAX_SOCKET)
        {
            if(state != 1) ret = SOC_ERROR;

            LOG(LEVEL_DEBUG,"Connect Socket id %d statue %d",socket_id,ret);
            
            socket_event_send(SOC_MSG_CONNECT,NULL,0,(u8)socket_id,ret);

            //kk_os_sleep(10);
        }
    }
}

static void l610_urc_mipstat(const char *data, u16 size)
{
    s32 sock_id,state = -1;

    //state  0:ACK indication 1:broken protocol stack 2:connection closed automatically
    
    if(sscanf(data,"+MIPSTAT: %d,%d",&sock_id,&state) == 2)
    {
        if(state == 0)
        {
            return;
        }
        else if(state == 1)
        {
            state = SOC_BROKEN_PROTOCOL;
        }
        else if(state == 2)
        {
            state = SOC_CONNRESET;
        }
        else
        {
            state = SOC_ERROR;
        }

        l610_socket_mamger(sock_id);

        socket_event_send(SOC_MSG_CLOSE,NULL,0,sock_id, (SocErrorEnum)state);
    }
}


static void l610_urc_mipcall(const char *data, u16 size)
{
    PDPActiveInfoStrct *pdp = NULL;

    pdp = malloc(sizeof(PDPActiveInfoStrct));

    memset((u8 *)pdp,0,sizeof(PDPActiveInfoStrct));

    if(pdp)
    {
      //AT+MIPCALL? respose +MIPCALL: 1,10.112.241.69
      if(sscanf(data,"+MIPCALL: %d,%d.%d.%d.%d",&pdp->state,pdp->ip,pdp->ip+1,pdp->ip+2,pdp->ip+3)  != 5)
      {
        //URC +MIPCALL: 10.112.241.69
        if(sscanf(data,"+MIPCALL: %d.%d.%d.%d",pdp->ip,pdp->ip+1,pdp->ip+2,pdp->ip+3) == 4)
        {
            pdp->state = 1;
        }
        else if(sscanf(data,"+MIPCALL: %d",&pdp->state) == 1)//AT+MIPCALL=0 response +MIPCALL: 0
        {
            pdp->ip[0] = pdp->ip[1] = pdp->ip[2] = pdp->ip[3] = 0;
            
        }
        else
        {
            free(pdp);
            pdp = NULL;
        }
      }
       
    }

    if(pdp)
    {
        LOG(LEVEL_DEBUG,"URC pdp state %d",pdp->state);
    }

    if(pdp)socket_event_send(SOC_MSG_PDP_STATE_CHANGE,(u8 *)pdp,sizeof(PDPActiveInfoStrct),0,SOC_SUCCESS);
    
}

static void l610_urc_mipclose(const char *data, u16 size)
{
    s32 sock_id,state = -1;
    
    if(sscanf(data,"+MIPCLOSE: %d,%d",&sock_id,&state) == 2)
    {
        l610_socket_mamger(sock_id);
        
        socket_event_send(SOC_MSG_CLOSE,NULL,0,sock_id,state == 0?SOC_SUCCESS:SOC_ERROR);

    }
}

static void l610_urc_gtgisfunc(const char *data, u16 size)
{
    double lat ,lnt = 0;

    if(sscanf(data,"+GTGIS: %*[\"]%lf,%lf",&lnt,&lat) == 2)
    {
        model_addr()->latitude  = lat;
        model_addr()->longitude = lnt;

        LOG(LEVEL_INFO,"device address %.7f,%.7f",lat,lnt);
    }
}

static void l610_urc_mipsend(const char *data, u16 size)
{
    #if 0
    s32 sock_id = 0;
    s32 errocde = 2;
    
    sscanf(data,"+MIPSEND: %d,%d",&sock_id,&errocde);

    if(sock_id > 0 && sock_id < L610_MAX_SEND_LEN && errocde == 0)
    {
        kk_os_event_send(at_socket_event, L610_SOCKET_SEND_OK);

        errocde = SOC_SUCCESS;
    }
    else
    {
        errocde = SOC_ERROR;
        
        kk_os_event_send(at_socket_event, L610_SOCKET_SEND_FAIL);
    }

    #endif
    
}

static void l610_urc_mipdns(const char *data, u16 size)
{
    s32 ip[4] = {0};
    u8 ip_addr[4];

    //+MIPDNS: "yj03.yunjiwulian.com",120.78.176.175
    if(sscanf(strstr(data,",")+1,"%d.%d.%d.%d",ip,ip+1,ip+2,ip+3) == 4)
    {
        if(ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0)
        {
            LOG(LEVEL_INFO,"dns prase result %s",strstr(data," "));

            //ip_addr = (u8 *)malloc(4);

            if(ip_addr)
            {
                ip_addr[0] = (u8)ip[0];
                ip_addr[1] = (u8)ip[1];
                ip_addr[2] = (u8)ip[2];
                ip_addr[3] = (u8)ip[3];
                socket_event_send(SOC_MSG_DNS, ip_addr,4,0,SOC_SUCCESS);
            }
        }
    }
}

static void l610_sim_dropfunc(const char *data, u16 size)
{
    socket_event_send(SOC_MSG_SIMDROP, NULL,0,0,SOC_ERROR);
}

static void l610_urc_mipfunc(const char *data, u16 size)
{    
    switch(*(data + 5))
    {    
        case 'N' :  l610_urc_mipdns(data,size); break;  //+MIPDNS
        case 'U' : l610_urc_mippush(data,size); break;  //+MIPPUSH
        case 'E' : l610_urc_mipsend(data,size); break;  //+MIPSEND
        case 'P' : l610_urc_mipopen(data,size); break;  //+MIPOPEN
        case 'T' : l610_urc_mipstat(data,size); break;  //+MIPSTAT
        case 'A' : l610_urc_mipcall(data,size); break;  //+MIPCALL
        case 'L' : l610_urc_mipclose(data,size);break;  //+MIPCLOSE
	    default  : LOG(LEVEL_INFO,"MIP URC data:%d : %s",size,data);break;
    }
}

static void l610_urc_ftpopen(const char *data, u16 size)
{
    s32 state = -1;

    u8 d = 0;
    
    if(sscanf(data,"+FTPOPEN: %d",&state) == 1)
    {
        if(state == 1 && l610_ftp.notfiy_cb)
        {
            l610_ftp.opened = 1;

            d = 1;

            l610_ftp.notfiy_cb(FTP_OPEN,&d,1);
        }
    }
}

static void l610_urc_ftpcwd(const char *data, u16 size)
{
    s32 state = -1;
    
    if(sscanf(data,"+FTPCWD: %d",&state) == 1)
    {
        if(state == 1 && l610_ftp.notfiy_cb && l610_ftp.opened)
        {

            l610_ftp.notfiy_cb(FTP_CD,NULL,0);
        }
    }
}

static void l610_urc_ftpget(const char *data, u16 size)
{
    s32 state = -1;
    
    if(sscanf(data,"\n+FTPGET: %d",&state) == 1)
    {
        if(state == 1 && l610_ftp.notfiy_cb && l610_ftp.opened)
        {
            l610_ftp.notfiy_cb(FTP_DL,NULL,0);
        }
    }
}

static void l610_urc_ftplist(const char *data, u16 size)
{
    s32 state = -1;
    s32 ret = 0;
    u16 len_p  = 0;
    char file[100] = {0};
    char time1[36] = {0};
    char time2[36] = {0};
    // sec;
    //char *file_list;
    FifoType *fifo = at_resp_fifo();
    
    if(sscanf(data,"\n+FTPLIST: %d",&state) == 1)
    {
        if(state == 2 && l610_ftp.notfiy_cb && l610_ftp.opened)
        {
            //file_list = malloc(4096);

            state = 0;

            //if(file_list)
            {
                while(ret == 0)
                {
                    len_p  = 100;
                    
                    if(fifo_peek_until(fifo,(U8 *)file,&len_p,'\n') == KK_SUCCESS)
                    {
                        fifo_pop_len(fifo,len_p);

                        ret = (s32)strstr(file,"FTPLIST");

                        if(0 == ret)
                        {
                            memset(time1,0,sizeof(time1));

                            memset(time2,0,sizeof(time2));
                            
                            if(sscanf(file,"%[^:]:%[^ ]",time1,time2) == 2)
                            {
                                len_p = strlen(time1);

                                state = 3600 * atoi(&time1[len_p-2]);

                                state += atoi(time2);
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    
                    
                }
                
                l610_ftp.notfiy_cb(FTP_LIST,(u8 *)&state,4);

                //free(file_list);
            }
            
        }
    }
}

static l610_urc_ftprecv(const char *data, u16 size)
{
    s32 len = -1;
    u8 *recv = NULL;
    FifoType *fifo = at_resp_fifo();
    
    if(sscanf(data,"+FTPRECV: %d",&len) == 1)
    {
        if(l610_ftp.notfiy_cb && l610_ftp.opened)
        {
            if(len  > 0)
            recv = malloc(len + 4);

            if(!recv && len > 0)
            {
                LOG(LEVEL_FATAL,"l610_urc_ftprecv malloc(%d) error",len + 4);

                l610_ftp.notfiy_cb(FTP_ERROR,NULL,0);
            }
            else
            {
                if(len  > 0)
                {
                    fifo_peek(fifo,recv,len);
                    fifo_pop_len(fifo,len);
                }

                l610_ftp.notfiy_cb(FTP_RECV,recv,len);

                free(recv);
            }

        }
    }
}

static void l610_urc_ftpclose(const char *data, u16 size)
{
    l610_ftp.notfiy_cb = NULL;

    l610_ftp.opened = 0;
}


static void l610_urc_ftpfunc(const char *data, u16 size)
{
    switch(*(data + 6))
    {
        
        case 'D' : l610_urc_ftpcwd(data,size) ; break;  //+FTPCWD
        //case 'T' : l610_urc_ftpget(data,size) ; break;  //+FTPGET
        //case 'S' : l610_urc_ftplist(data,size); break;  //+FTPLIST
        case 'E' : l610_urc_ftpopen(data,size); break;  //+FTPOPEN
        case 'C' : l610_urc_ftprecv(data,size); break;  //+FTPRECV
        case 'O' : l610_urc_ftpclose(data,size);break;  //+FTPCLOSE
        default  : LOG(LEVEL_INFO,"FTP URC data:%d : %s",size,data);break;
    }
}

const ATUrcStruct urc_table[] = {

    {"+MIPRTCP" , ","    ,l610_urc_miprtcp},
    {"+MIPRUDP" , ","    ,l610_urc_miprudp},
    {"\n+FTPLIST", "\r\n" ,l610_urc_ftplist},
    {"\n+FTPGET" , "\r\n" ,l610_urc_ftpget},
    {"+FTP"     , "\r\n" ,l610_urc_ftpfunc},
    //{"+MIPPUSH" , "\r\n" ,l610_urc_mippush},
    {"+MIP"     , "\r\n" ,l610_urc_mipfunc},
    {"+GTGIS"   , "\r\n" ,l610_urc_gtgisfunc},
    {"+SIM DROP", "\r\n" ,l610_sim_dropfunc}
};


void l610_net_service_create(char *device_name , bool is_reset)
{
    
    if(!is_reset)
    {
        //POWER ON
        //at_client_service_create();
    }
    
    //if(at_client_wait_connect(10000) == SUCCESS)
    //{
    //    LOG(LEVEL_INFO,"L610 at device ready!");
    //}
}

#define AT_SEND_CMD(resp, resp_line, timeout, cmd)                                                              \
    do                                                                                                          \
    {                                                                                                           \
        if (at_exec_cmd(at_resp_set_info(resp, 128, resp_line, timeout), cmd) < 0)    \
        {                                                                                                       \
            result = KK_AT_ERROR;                                                                                 \
            goto __exit;                                                                                        \
        }                                                                                                       \
    } while(0);                                                                                                 \


s32 l610_at_socket_dns_prase(char *addr, s32 bears, s32 *result)
{
    //s32 result = SOC_SUCCESS; 
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    sresp.timeout  = 5000;

    if (at_exec_cmd(&sresp, "AT+MIPDNS=\"%s\"",addr) < 0)
    {
        *result = SOC_ERROR;
    }
    
    kk_os_mutex_release(at_event_lock);

    *result = SOC_SUCCESS;

    sresp.timeout = 300;

    return SOC_SUCCESS;
}

S32 l610_at_socket_creat(int bears ,u8 accesid, StreamType type)
{
    //u8 i = 0;

    if(accesid > 0 && accesid < L610_SUPPORT_MAX_SOCKET)
    {
        if(!l610_socket_id[accesid])return accesid;
    }
    
    return SOC_INVALID_SOCKET;
}

s32 l610_at_socket_close(s32 sock_id)
{
    s32 result = SOC_SUCCESS;

    ASSERTSOCKET(sock_id); 

    if(l610_socket_id[sock_id] == 0) return result;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+MIPCLOSE=%d,0",sock_id) < 0)
    {
        result = SOC_ERROR;
    }

    l610_socket_id[sock_id] = 0;
    
    kk_os_mutex_release(at_event_lock);

    return result;
}


s32 l610_at_socket_recv(s32 sock_id , u8 *recv_buff , u16 max_len)
{
    s32 result = SOC_SUCCESS;

    ASSERTSOCKET(sock_id);

    kk_os_mutex_request(at_event_lock);

    if (at_exec_cmd(&sresp, "AT+MIPREAD=%d,%d",sock_id,max_len) < 0)
    {
        result = SOC_ERROR;
    }

    //l610_socket_id[sock_id] = 0;
    
    kk_os_mutex_release(at_event_lock);
    
    return result;
}

s32 l610_at_socket_send(s32 sock_id , u8 *data , u16 len)
{
    s32 result = SOC_SUCCESS;

    //s32 event_flg = 0;

    ASSERTSOCKET(sock_id);

    if(len > L610_MAX_SEND_LEN) return SOC_ERROR;

    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 3;
    sresp.timeout  = 5000;

    at_set_end_sign('>');

    kk_os_event_clear(at_socket_event,L610_SOCKET_SEND_OK|L610_SOCKET_CONNECT_FAIL);

    /* send the "AT+MIPSEND" commands to AT server than receive the '>' response on the first line. */
    if (at_exec_cmd(&sresp, "AT+MIPSEND=%d,%d", sock_id, len) < 0)
    {
				LOG(LEVEL_ERROR,"AT+MIPSEND %d,%d ERROR",sock_id,len);
        result = SOC_ERROR;
        goto __EXIT;
    }

    at_set_end_sign(0);

    sresp.line_num = 0;
    sresp.timeout  = 5000;
    /* send the real data to server or client */
    result = (int) at_client_send(&sresp,(const char *)data, len);
    if (result == 0)
    {
	    LOG(LEVEL_ERROR,"at_client_send ERROR");
        result = SOC_ERROR;
        goto __EXIT;
    }
    else
    {
        result = SOC_SUCCESS;
    }
    #if 0
    if((event_flg = kk_os_event_recv(at_socket_event,L610_SOCKET_SEND_OK|L610_SOCKET_CONNECT_FAIL, 1000)) < 0)
    {
        result = SOC_ERROR;

        goto __EXIT;
    }

    if(event_flg&L610_SOCKET_CONNECT_FAIL)
    {
        result = SOC_ERROR;
    }
    #endif

__EXIT:

    at_set_end_sign(0);

    kk_os_mutex_release(at_event_lock);

    if(result == SOC_SUCCESS) result = len;
    
    return result;
}

s32 l610_at_socket_connect(s32 sock_id, char *ip, u16 port, s32 bears,StreamType type)
{
    s32 result = SOC_SUCCESS;

    KK_ASSERT(ip);
    KK_ASSERT((port != 0));
    ASSERTSOCKET(sock_id);

    /* lock AT socket connect */
    kk_os_mutex_request(at_event_lock);

    switch (type)
    {
        case STREAM_TYPE_STREAM:
            /* send AT commands(AT+MIPOPEN=<socket_id>,[Source Port],<Remote IP>,<Remote_port>,<Porocol>) to connect TCP server */
            if (at_exec_cmd(&sresp, "AT+MIPOPEN=%d,,\"%s\",%d,0", sock_id, ip, port) < 0)
            {
                result = SOC_ERROR;
                //goto __exit;
            }
            break;

        case STREAM_TYPE_DGRAM:
            if (at_exec_cmd(&sresp, "AT+MIPOPEN=%d,,\"%s\",%d,1", sock_id, ip, port) < 0)
            {
                result = SOC_ERROR;
                //goto __exit;
            }
            break;

        default:
            LOG(LEVEL_ERROR,"Not supported connect type : %d.", type);
            //kk_os_mutex_release(at_event_lock);
            result = SOC_ERROR;
            break;
    }

    /* unlock AT socket connect */
    kk_os_mutex_release(at_event_lock);

    return result;
}

s32 l610_pdp_active(void)
{
    s32 result = KK_SUCCESS;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;
    
    if (at_exec_cmd(&sresp, "AT+MIPCALL=1") < 0)
    {
        result = KK_AT_ERROR;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}

s32 l610_pdp_deactive(void)
{
    s32 result = KK_SUCCESS;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+MIPCALL=0") < 0)
    {
        result = KK_AT_ERROR;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}

s32 l610_ftp_open(char *url, char *name ,char *psw,u16 port , ftp_notify_cb notfiy)
{
    s32 result = KK_SUCCESS;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    l610_ftp.notfiy_cb = notfiy;

    //l610_ftp.opened = 1;

    //at_exec_cmd(&sresp,"AT+FTPOPEN?");

    sresp.line_num = 0;

    sresp.timeout  = 1000;

    if (at_exec_cmd(&sresp, "AT+FTPOPEN=\"%s\",\"%s\",\"%s\",\"\",,%d,",url,name,psw,port) < 0)
    {
        result = KK_AT_ERROR;

        l610_ftp.opened = 0;

        l610_ftp.notfiy_cb = NULL;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}


s32 l610_ftp_cd(char *dir)
{
    s32 result = KK_SUCCESS;

    if(!l610_ftp.opened || !l610_ftp.notfiy_cb) return KK_NET_ERROR;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+FTPCWD=\"%s\"",dir) < 0)
    {
        result = KK_AT_ERROR;

        l610_ftp.opened = 0;

        l610_ftp.notfiy_cb = NULL;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}

s32 l610_ftp_list(void)
{
    s32 result = KK_SUCCESS;

    if(!l610_ftp.opened || !l610_ftp.notfiy_cb) return KK_NET_ERROR;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+FTPLIST") < 0)
    {
        result = KK_AT_ERROR;

        l610_ftp.opened = 0;

        l610_ftp.notfiy_cb = NULL;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}


s32 l610_ftp_dl(char *file)
{
    s32 result = KK_SUCCESS;

    if(!l610_ftp.opened || !l610_ftp.notfiy_cb) return KK_NET_ERROR;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+FTPGET=\"%s\",1",file) < 0)
    {
        result = KK_AT_ERROR;

        l610_ftp.opened = 0;

        l610_ftp.notfiy_cb = NULL;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}


s32 l610_ftp_recv(u16 len)
{
    s32 result = KK_SUCCESS;

    FifoType *fifo = at_resp_fifo();

    if(!l610_ftp.opened || !l610_ftp.notfiy_cb) return KK_NET_ERROR;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+FTPRECV=%d",len) < 0)
    {
        result = KK_AT_ERROR;

        l610_ftp.opened = 0;

        l610_ftp.notfiy_cb = NULL;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}

s32 l610_ftp_close(void)
{
    s32 result = KK_SUCCESS;

    if(!l610_ftp.opened || !l610_ftp.notfiy_cb) return KK_NET_ERROR;
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+FTPCLOSE") < 0)
    {
        result = KK_AT_ERROR;

        l610_ftp.opened = 0;

        l610_ftp.notfiy_cb = NULL;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}


void l610_power_up(void)
{
    //static u8 not_first = 0;

    //if(!not_first)
    {
        gpio_type_set(PA0,GPIO_Mode_Out_PP); //4G_EN
        gpio_out(PA0,1);
        //not_first = 1;
        //kk_os_sleep(200);
    }
    gpio_type_set(PB9,GPIO_Mode_Out_PP); //Reset
    gpio_out(PB9,0);
    gpio_type_set(PB8,GPIO_Mode_Out_PP); //PowerKey
    gpio_out(PB8,0);
    kk_os_sleep(200);
    gpio_out(PB8,1);
    kk_os_sleep(2100);
    gpio_out(PB8,0);
}

void l610_power_down(void)
{
    gpio_out(PB8,1);
    kk_os_sleep(3100);
    gpio_out(PB8,0);
    kk_os_sleep(100);
    gpio_out(PA0,1);
}

s32 l610_regsiter_query(void)
{
    s32 result = KK_SUCCESS;

    s32 qi_arg[2];

    struct tm t_tm;

    int t_gmt = 0;

    u32 sec = 0;

    ModleInfoStruct *model = model_addr();
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+CSQ") < 0)
    {
        result = KK_AT_ERROR;
    }

    at_resp_parse_line_args_by_kw(&sresp, "+CSQ:", "+CSQ: %d,%d", &qi_arg[0], &qi_arg[1]);

    if(qi_arg[0] != 99)
    {
        model->csq = (u8 )qi_arg[0];
    }
    else
    {
        kk_os_mutex_release(at_event_lock);
        
        return KK_NET_ERROR;
    }

    sresp.line_num = 0;

    if (at_exec_cmd(&sresp, "AT+CEREG?") < 0)
    {
        result = KK_AT_ERROR;
    }

    at_resp_parse_line_args_by_kw(&sresp, "+CEREG:", "+CEREG: %d,%d", &qi_arg[0], &qi_arg[1]);

    if(qi_arg[1] == 1 || qi_arg[1] == 5)model->cgreg_rdy = 1;
    else
    {
        model->cgreg_rdy = 0;

        result = KK_NET_ERROR;
        
    }

    if(result != KK_SUCCESS)
    {
        sresp.line_num = 0;

        if (at_exec_cmd(&sresp, "AT+CGREG?") < 0)
        {
            return KK_AT_ERROR;
        }

        at_resp_parse_line_args_by_kw(&sresp, "+CGREG:", "+CGREG: %d,%d", &qi_arg[0], &qi_arg[1]);

        if(qi_arg[1] == 1 || qi_arg[1] == 5)model->cgreg_rdy = 1;
        else
        {
            model->cgreg_rdy = 0;

            return KK_NET_ERROR;
            
        }
    }

    sresp.line_num = 0;

    //if ( get_local_utc_sec() < 1587481331ul) //specil value when local time late than code time.
    {
        if (at_exec_cmd(&sresp, "AT+CCLK?") < 0)
        {
            result = KK_AT_ERROR;
        }

        if (at_resp_parse_line_args_by_kw(&sresp, "+CCLK:", "+CCLK: \"%d/%d/%d,%d:%d:%d+%d",&t_tm.tm_year,&t_tm.tm_mon,&t_tm.tm_mday,&t_tm.tm_hour,\
                &t_tm.tm_min,&t_tm.tm_sec,&t_gmt) == 7)
        {
                t_tm.tm_year = (t_tm.tm_year + 2000 - 1900);

                t_tm.tm_mon  = t_tm.tm_mon - 1;

                LOG(LEVEL_INFO,"get net work time %d/%d/%d,%d:%d:%d+%d",t_tm.tm_year + 1900,t_tm.tm_mon+1,t_tm.tm_mday,t_tm.tm_hour,\
                t_tm.tm_min,t_tm.tm_sec,t_gmt);

                sec = mktime(&t_tm);

                if(t_gmt > 0)
                {
                    sec = sec - (t_gmt * 15 * 60);
                }

                LOG(LEVEL_INFO,"clock(%d) calac utc sec %x",clock(),sec);

                if(sec > 0 && sec < (u32)2840112000ul) //filter invalid timesec
                {
                    set_local_time((u32)sec);
                }
         }

         sresp.line_num = 0;
                
         //at_exec_cmd(&sresp,"AT+MIPNTP=\"%s\",123",NTP_SERVER);
    }
    #if 0
    if(model_addr()->latitude == 0 || model_addr()->longitude == 0)
    {
        sresp.line_num = 0;
        
        at_exec_cmd(&sresp, "AT+GTGIS=6");
    }
    #endif
    kk_os_mutex_release(at_event_lock);

    return result;
}

s32 l610_lbs_info_req(void)
{
    s32 result = KK_SUCCESS;

    ModleInfoStruct *model = model_addr();
    
    kk_os_mutex_request(at_event_lock);

    sresp.line_num = 0;

    if(at_exec_cmd(&sresp,"AT+MIPNTP=\"%s\",123",NTP_SERVER) != KK_SUCCESS)
    {
        result = KK_AT_ERROR;
    }
    

    sresp.line_num = 0;
        
    if(at_exec_cmd(&sresp, "AT+GTGIS=6") != KK_SUCCESS)
    {
        result = KK_AT_ERROR;
    }
    
    kk_os_mutex_release(at_event_lock);

    return result;
}


KK_ERRCODE at_socket_init(NetDevInfoStruct *net_dev)
{
    #define SIM_RETRY                      10
    #define IMEI_RETRY                     10
    #define CIMI_RETRY                     10
    #define CSQ_RETRY                      20
    #define CREG_RETRY                     10
    #define CGREG_RETRY                    20
    ATRespStruct* resp = NULL;
    ModleInfoStruct *model = &net_dev->model;
    SocketFuncStruct*socket = &net_dev->socket;

    KK_ERRCODE result = KK_SUCCESS;

    int i,qi_arg[3];

    char parsed_data[20];

    l610_power_up();

    if(at_socket_event < 0)
    {
        at_socket_event = kk_os_event_create();
    }

    //model->state = 0;
    if(strlen(model->imei) == 15)
    {
        memcpy(parsed_data,model->imei,sizeof(parsed_data));
    }
    
    memset((u8 *)net_dev,0,sizeof(NetDevInfoStruct));

    memcpy(model->imei,parsed_data,sizeof(parsed_data));

    if(at_event_lock < 0)at_event_lock = kk_os_mutex_create();
    
    //base at command max exec time 300ms
    resp = at_create_resp(128, 0, 300);

    if(resp == NULL)
    {
        LOG(LEVEL_ERROR,"No memory for reap struct!");

        result = KK_MEM_NOT_ENOUGH;

        goto __exit;
    }

    model->at_rdy = 1;
    
    if(at_client_wait_connect(10000) != KK_SUCCESS)
    {
        result = KK_TIMOUT_ERROR;
        
        goto __exit;
    }

    led_service_net_set(LED_SLOW_FLASH);

    LOG(LEVEL_INFO,"L610 at device ready!");

    at_set_urc_table((ATUrcStruct *)urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    /* disable echo */
    AT_SEND_CMD(resp, 0, 300, "ATE0");
    
    for(i = 0 ; i < IMEI_RETRY ; i++)
    {
        AT_SEND_CMD(resp,0,300,"AT+GSN");

        memset(model->imei,0,sizeof(model->imei));
        sscanf(at_resp_get_line(resp,1),"%s",model->imei);

        if(strlen((const char *)model->imei) > 0)
        {
            break;
        }
    }

    if (i == IMEI_RETRY)
    {
        LOG(LEVEL_ERROR,"IMEI get error!");
        result = KK_AT_ERROR;
        goto __exit;
    }
    /* Get the baudrate */
    AT_SEND_CMD(resp, 0, 300, "AT+IPR?");
    
    at_resp_parse_line_args_by_kw(resp, "+IPR:", "+IPR: %d", &i);

    LOG(LEVEL_DEBUG,"Baudrate %d", i);
    /* get module version */
    AT_SEND_CMD(resp, 0, 300, "ATI");

    /* show module version */
    sscanf(at_resp_get_line(resp,3),"%s",model->sw);

    /* check SIM card */
    for(i = 0 ; i < SIM_RETRY ; i++)
    {
        //AT_SEND_CMD(resp, 2, 3 * 1000, "AT+CPIN?");
        if(at_exec_cmd(at_resp_set_info(resp, 128, 2, 3*1000), "AT+CPIN?") < 0)
        {
            kk_os_sleep(500);
        }
        else 
        {
            if (at_resp_get_line_by_kw(resp, "READY"))
            {
                model->sim_rdy = 1;
                break;
            }
            else
            {
                kk_os_sleep(500);
            }
        }
    }

    if(i == SIM_RETRY)
    {
        result = KK_AT_ERROR;
        
        LOG(LEVEL_ERROR,"SIM card dected fail");

        goto __exit;
    }

    i = 0;
    while(at_exec_cmd(at_resp_set_info(resp,128,0,300),"AT+CIMI") < 0)
    {
        i++;
        LOG(LEVEL_DEBUG,"AT+CIMI retry %d",i);

        if(i > CIMI_RETRY)
        {
            LOG(LEVEL_ERROR,"Read CIMI failed");

            result = KK_AT_ERROR;

            goto __exit;
        }

        kk_os_sleep(1000);
    }

    memset(model->imsi,0,sizeof(model->imsi));
    sscanf(at_resp_get_line(resp,1),"%s",model->imsi);

    AT_SEND_CMD(resp,0,300,"AT+CCID");
    
    memset(model->iccid,0,sizeof(model->iccid));
    at_resp_parse_line_args_by_kw(resp, "+CCID:", "+CCID: %s", model->iccid);

    for (i = 0; i < CSQ_RETRY; i++)
    {
        AT_SEND_CMD(resp, 0, 300, "AT+CSQ");
        at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %d,%d", &qi_arg[0], &qi_arg[1]);
        if (qi_arg[0] != 99)
        {
            model->csq = (u8)qi_arg[0];
            LOG(LEVEL_DEBUG,"Signal strength: %d  Channel bit error rate: %d", qi_arg[0], qi_arg[1]);
            break;
        }
        kk_os_sleep(1000);
    }
    if (i == CSQ_RETRY)
    {
        LOG(LEVEL_ERROR,"Signal strength check failed");
        result = KK_AT_ERROR;
        goto __exit;
    }

    LOG(LEVEL_INFO,"Model Info ver[%s],ccid[%s],imsi[%s],imei[%s],csq[%d]",model->sw,model->iccid,model->imsi, model->imei,model->csq);
    #if 0
    /* check the GSM network is registered */
    for (i = 0; i < CREG_RETRY; i++)
    {
        AT_SEND_CMD(resp, 0, 300, "AT+CREG?");
        at_resp_parse_line_args_by_kw(resp, "+CREG:", "+CREG: %s", parsed_data);
        if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
        {
            LOG(LEVEL_DEBUG,"CS is registered (%s)", parsed_data);
            break;
        }
        kk_os_sleep(1000);
    }
    if (i == CREG_RETRY)
    {
        LOG(LEVEL_DEBUG,"The CS is register failed (%s)", parsed_data);
        result = KK_AT_ERROR;
        goto __exit;
    }
    #endif
    model->creg_rdy = 1;
    
    /* check the GPRS network is registered */
    for (i = 0; i < CGREG_RETRY; i++)
    {
        AT_SEND_CMD(resp, 0, 5*1000, "AT+CEREG?");
        at_resp_parse_line_args_by_kw(resp, "+CEREG:", "+CEREG: %s", parsed_data);
        if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
        {
            LOG(LEVEL_DEBUG,"LTE is registered (%s)", parsed_data);
            break;
        }
        
        AT_SEND_CMD(resp, 0, 300, "AT+CGREG?");
        at_resp_parse_line_args_by_kw(resp, "+CGREG:", "+CGREG: %s", parsed_data);
        if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
        {
            LOG(LEVEL_DEBUG,"GPRS is registered (%s)", parsed_data);
            break;
        }
        kk_os_sleep(1000);
    }
    if (i == CGREG_RETRY)
    {
        LOG(LEVEL_ERROR,"The PS is register failed (%s)", parsed_data);
        result = KK_AT_ERROR;
        goto __exit;
    }

    model->cgreg_rdy = 1;

    AT_SEND_CMD(resp, 0, 300, "AT+CMEE=2");

    AT_SEND_CMD(resp, 0, 300, "AT+COPS?");

    at_resp_parse_line_args_by_kw(resp, "+COPS:", "+COPS: %*[^\"]\"%[^\"]", parsed_data);
    if(strcmp(parsed_data,"CHINA MOBILE") == 0)
    {
        /* "CMCC" */
        LOG(LEVEL_INFO,"%s", parsed_data);
        AT_SEND_CMD(resp, 0, 300, CGDCONT_CHINA_MOBILE);
    }
    else if(strcmp(parsed_data,"CHN-UNICOM") == 0)
    {
        /* "UNICOM" */
        LOG(LEVEL_INFO,"%s", parsed_data);
        AT_SEND_CMD(resp, 0, 300, CGDCONT_CHINA_UNICOM);
    }
    else if(strcmp(parsed_data,"CHN-CT") == 0)
    {
        AT_SEND_CMD(resp, 0, 300, CGDCONT_CHINA_TELECOM);
        /* "CT" */
        LOG(LEVEL_INFO,"%s", parsed_data);
    }

    AT_SEND_CMD(resp,0,300,"AT+GTSET=\"IPRFMT\",2");

    if (at_exec_cmd(at_resp_set_info(resp, 128, 0, 150*1000),"AT+MIPCALL=0") < 0)  
    {                                                                                                       
        /* Query the status of the context profile */
        AT_SEND_CMD(resp, 0, 150 * 1000, "AT+MIPCALL?");                                                                               
    }

    l610_regsiter_query();

    __exit:

    if(resp)
    {
        at_delete_resp(resp);
    }

    if(result < 0)
    {
        l610_power_down();

        led_service_net_set(LED_OFF);
    }
    else
    {
        socket->create          = l610_at_socket_creat;
        socket->connect         = l610_at_socket_connect;
        socket->send            = l610_at_socket_send;
        socket->recv            = l610_at_socket_recv;
        socket->dns_prase       = l610_at_socket_dns_prase;
        socket->close           = l610_at_socket_close;

        /*ftp*/
        socket->ftp_open        = l610_ftp_open;
        socket->ftp_cd          = l610_ftp_cd;
        socket->ftp_dl          = l610_ftp_dl;
        socket->ftp_list        = l610_ftp_list;
        socket->ftp_recv        = l610_ftp_recv;
        socket->ftp_close       = l610_ftp_close;

        socket->lbs_req         = l610_lbs_info_req;

        model->pdp_active       = l610_pdp_active;
        model->pdp_deactive     = l610_pdp_deactive;
        model->regsiter_query   = l610_regsiter_query;
    }

    return result;
}


