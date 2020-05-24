#include "socket_service.h"
#ifdef L610_MODLE
#include "l610_socket_service.h"
#endif


s32 g_socket_msg = -1;

enum
{
    PS_CB_DNS_PRASE,
        
    PS_CB_SOCKET,

    //NET_STATUS,

    MAX_SOCKET_CB_FUN,
};


SocketCallbackFun s_pscallback[MAX_SOCKET_CB_FUN] ={NULL,NULL};

#define MAX_SOCKET_EVENT 50

SocNotifyIndStruct socket_event[MAX_SOCKET_EVENT] = {0};



NetDevInfoStruct netdev;

const char *s_sockk_msg_string[SOC_MSG_MAX] = {
                                                    "SOC_MSG_READ"                ,  /* Notify for read */
                                                    "SOC_MSG_WRITE"               ,  /* Notify for write */
                                                    "SOC_MSG_ACCEPT"              ,  /* Notify for accept */
                                                    "SOC_MSG_CONNECT"             ,  /* Notify for connect */
                                                    "SOC_MSG_CLOSE"               ,  /* Notify for close */
                                                    "SOC_MSG_DNS"                 ,
                                                    "SOC_MSG_SMS"                 ,
                                                    "SOC_MSG_PDP_STATE_CHANGE"    ,
                                                    "SOC_MSG_NET_STATUS"          ,
                                                    "SOC_MSG_NET_DESTORY"         ,
                                                    "SOC_MSG_NET_WORK"            ,
                                                    "SOC_MSG_SIMDROP"             ,
                                                    "SOC_MSG_SLEEP"               ,
                                                };

ModleInfoStruct *model_addr(void)
{
    return &netdev.model;
}

s32 dns_prase_register_callback(SocketCallbackFun cb_fun)
{
    s_pscallback[PS_CB_DNS_PRASE] = cb_fun;
   
    return 0;
}

void socket_register_callback(SocketCallbackFun func)
{
    s_pscallback[PS_CB_SOCKET] = func;
}

SocNotifyIndStruct *socket_event_malloc(void)
{
    u8 i = 0;

    for(i = 0 ; i < MAX_SOCKET_EVENT ; i++)
    {
        if(!socket_event[i].inuse)
        {
            socket_event[i].inuse = 1; 
            LOG(LEVEL_INFO,"evenet buff id %d addr %x",i,&socket_event[i]);
            return &socket_event[i];
        }
    }

    return NULL;
}

void socket_event_free(SocNotifyIndStruct *soc)
{
    //soc->inuse = 0;

    if(soc->event_type != SOC_MSG_DNS && soc->data != 0)
    {
        free((u8 *)soc->data);
    }

    memset((u8 *)soc,0,sizeof(SocNotifyIndStruct));
}

KK_ERRCODE socket_event_send(SocEnvEnum env,u8 *data, u16 len,u8 soc_id , SocErrorEnum err)
{
    SocNotifyIndStruct *soc = NULL;

    soc = socket_event_malloc();

    if(soc)
    {
        
        soc->socket_id  = soc_id;
        soc->error_cause= err;
        soc->event_type = env;
        soc->msg_len    = len;
        if(env == SOC_MSG_DNS)
        {
            soc->data = MKDWORD(data[3],data[2], data[1], data[0]);
            //memcpy((u8 *)&soc->data,data,4);
        }
        else
        {
            soc->data = (u32)data;
        }
        

        if(kk_os_send_msg(g_socket_msg,(void *)soc) != KK_SUCCESS)
        {
            LOG(LEVEL_FATAL,"os send msg error!");
        }

        LOG(LEVEL_INFO,"kk_os_send_msg id(%s) soc_id(%d)",s_sockk_msg_string[env],soc_id);

        return KK_SUCCESS;
    }
    else
    {
        LOG(LEVEL_FATAL,"socket_event_send malloc error(env %d sock %d errcode %d msg_len %d)",env,soc_id,err,len);
    }

    return KK_MEM_NOT_ENOUGH;
}

s32  socket_pdp_state_change(PDPActiveInfoStrct *pdp)
{
    if(pdp->state && (pdp->ip[0] != 0 || pdp->ip[1] != 0 || pdp->ip[2] != 0 || pdp->ip[3] != 0))
    {
        netdev.model.pdp_rdy = 1;

        LOG(LEVEL_INFO,"pdp active success!");

        return KK_SUCCESS;
    }
    else
    {
        if(netdev.model.pdp_active) 
        return netdev.model.pdp_active();
    }

    LOG(LEVEL_ERROR,"pdp active opration NULL!");

    return KK_UNKNOWN;
}

void socket_register_query(void *prg , void *ptg)
{
    static u32 count1 = 0;
    LOG(LEVEL_INFO,"socket_register_query %d",(++count1));
    socket_event_send(SOC_MSG_NET_STATUS,NULL,0,0,SOC_SUCCESS);
}

void socket_work_timer(void *prg , void *ptg)
{
    static u32 count2 = 0;
    LOG(LEVEL_INFO,"socket_work_timer %d",(++count2));
    socket_event_send(SOC_MSG_NET_WORK,NULL,0,0,SOC_SUCCESS);
}


void socket_task(void *arg)
{
    #define NET_STA_QUERY_TIM 100 //20S
    
    KK_ERRCODE res = KK_SUCCESS;
    
    SocNotifyIndStruct *msg = NULL;
	
    u32 count  = 0;
    
    g_socket_msg = kk_os_msg_queue_create(MAX_SOCKET_EVENT);


    for(;;)
    {
       res = at_socket_init(&netdev);

       while(res == KK_SUCCESS)
       {
           msg = (SocNotifyIndStruct *)kk_os_recive_msg(g_socket_msg,KK_OS_MSG_PEND);
           //msg = (SocNotifyIndStruct *)kk_os_recive_msg(g_socket_msg,KK_OS_MSG_QUERY);

           if(msg)
           {
                if(msg->event_type < SOC_MSG_MAX && msg->event_type != SOC_MSG_NET_WORK)
                LOG(LEVEL_DEBUG,"Scoket Task MSG(%s)",s_sockk_msg_string[msg->event_type]);
                switch(msg->event_type)
                {
                    case SOC_MSG_READ:
                    case SOC_MSG_WRITE:
                    case SOC_MSG_CONNECT:
                    case SOC_MSG_CLOSE:
                        if(s_pscallback[PS_CB_SOCKET])s_pscallback[PS_CB_SOCKET]((void *)msg);
                        break;
                    case SOC_MSG_SMS:
                        break;
                    case SOC_MSG_DNS:
                        if(s_pscallback[PS_CB_DNS_PRASE])s_pscallback[PS_CB_DNS_PRASE]((void *)msg->data);
                        break;
                    case SOC_MSG_PDP_STATE_CHANGE:
                        if(socket_pdp_state_change((PDPActiveInfoStrct *)msg->data) < 0)
                        {
                            LOG(LEVEL_ERROR,"PDP active error!");
                        }
                        break;
                    case SOC_MSG_NET_STATUS:
						count++;
                        if(netdev.model.regsiter_query)
                        {
                            if(netdev.model.regsiter_query())
                            {
                                //socket_event_send(, u8 * data, u16 len, u8 soc_id, SocErrorEnum err)
                                LOG(LEVEL_WARN,"Net Error csq %d cgreg %d,total conun %d",netdev.model.csq,netdev.model.cgreg_rdy,count);

                                res = KK_NET_ERROR;
                            }
                        }
                        
                        break;
                   case SOC_MSG_NET_WORK:
                        //gprs_timer_proc();
                        break;
                   case SOC_MSG_NET_DESTORY:
                        res = KK_NET_ERROR;
                        break;
                   case SOC_MSG_SIMDROP:
                        gprs_destroy();
                        break;
                   case SOC_MSG_SLEEP:
                        break;
                    default:
                        LOG(LEVEL_WARN,"Socket get Unkown msg(%d)",msg->event_type);
                        break;
                }
                //if(msg->data) free(msg->data);
                socket_event_free(msg);
           }
		   gprs_timer_proc();
           if(count > 0)count--;
           if(netdev.model.regsiter_query && count == 0)
           {
			  count = NET_STA_QUERY_TIM; 
               if(netdev.model.regsiter_query())
               {
                   //socket_event_send(, u8 * data, u16 len, u8 soc_id, SocErrorEnum err)
                   LOG(LEVEL_WARN,"Net Error csq %d cgreg %d,total conun %d",netdev.model.csq,netdev.model.cgreg_rdy,count);
   
                   res = KK_NET_ERROR;
               }
           }
           if(res != KK_SUCCESS)
           {
                //kk_os_timer_stop(TIMER_REGISER_QUERY);

                //kk_os_timer_stop(TIMER_NET_WORK);
                led_service_net_set(LED_OFF);
           }

           //res = KK_NET_ERROR;
           //kk_os_sleep(10);
       }
    }
}

void socket_service_suspend(void)
{
    
}


