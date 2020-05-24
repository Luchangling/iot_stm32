
#ifndef __SOCKET_SERVICE_H__
#define __SOCKET_SERVICE_H__

#include "kk_os_header.h"

typedef enum
{
    FTP_OPEN,
    FTP_CD,
    FTP_LIST,
    FTP_DL,
    FTP_RECV,
    FTP_CLOSE,
    FTP_ERROR,
    FTP_INVALID
    
}FtpNotfiyEnum;

typedef void (*ftp_notify_cb)(FtpNotfiyEnum id , u8 *data , u16 len);

typedef struct
{
    u8 opened;

    //void (*ftp_notify)(FtpNotfiyEnum id , u8 *data , u16 len);
    ftp_notify_cb notfiy_cb;

}SocketFtpStruct;

typedef struct
{
    int state;  //1:active 0:deactive;

    int ip[4];  //IP
}PDPActiveInfoStrct;

#pragma anon_unions 
typedef struct
{
    union
    {
        u16 state;
        struct
        {
            u16 at_rdy      : 1;
            u16 sim_rdy     : 1;
            u16 sms_rdy     : 1;
            u16 creg_rdy    : 1;
            u16 cgreg_rdy   : 1;
            u16 pdp_rdy     : 3;
            u16 csq         : 8;
        };
    };

    double latitude;
    double longitude;
    char sw[40];
    char imsi[50];
    char imei[50];
    char iccid[50];
    s32 (*regsiter_query)(void);
    s32 (*pdp_active)(void);
    s32 (*pdp_deactive)(void);
    
}ModleInfoStruct;

/* event */
typedef enum
{
    SOC_MSG_READ                ,  /* Notify for read */
    SOC_MSG_WRITE               ,  /* Notify for write */
    SOC_MSG_ACCEPT              ,  /* Notify for accept */
    SOC_MSG_CONNECT             ,  /* Notify for connect */
    SOC_MSG_CLOSE               ,  /* Notify for close */
    SOC_MSG_DNS                 ,
    SOC_MSG_SMS                 ,
    SOC_MSG_PDP_STATE_CHANGE    ,
    SOC_MSG_NET_STATUS          ,
    SOC_MSG_NET_DESTORY         ,
    SOC_MSG_NET_WORK            ,
    SOC_MSG_SIMDROP             ,
    SOC_MSG_SLEEP               ,
    SOC_MSG_MAX                 ,
} SocEnvEnum;

/* Socket return codes, negative values stand for errors */
typedef enum
{
    SOC_SUCCESS           = 0,     /* success */
    SOC_ERROR             = -1,    /* error */
    SOC_WOULDBLOCK        = -2,    /* not done yet */
    SOC_LIMIT_RESOURCE    = -3,    /* limited resource */
    SOC_INVALID_SOCKET    = -4,    /* invalid socket */
    SOC_INVALID_ACCOUNT   = -5,    /* invalid account id */
    SOC_NAMETOOLONG       = -6,    /* address too long */
    SOC_ALREADY           = -7,    /* operation already in progress */
    SOC_OPNOTSUPP         = -8,    /* operation not support */
    SOC_CONNABORTED       = -9,    /* Software caused connection abort */
    SOC_INVAL             = -10,   /* invalid argument */
    SOC_PIPE              = -11,   /* broken pipe */
    SOC_NOTCONN           = -12,   /* socket is not connected */
    SOC_MSGSIZE           = -13,   /* msg is too long */
    SOC_BEARER_FAIL       = -14,   /* bearer is broken */
    SOC_CONNRESET         = -15,   /* TCP half-write close, i.e., FINED */
    SOC_DHCP_ERROR        = -16,   /* DHCP error */
    SOC_IP_CHANGED        = -17,   /* IP has changed */
    SOC_ADDRINUSE         = -18,   /* address already in use */
    SOC_CANCEL_ACT_BEARER = -19,    /* cancel the activation of bearer */
    SOC_BROKEN_PROTOCOL   = -20,    /*broken protocol stack*/
    LOG_LOGIN_FAIL = -115,  // Dec-08-2016  登录无应答       5 
    LOG_CONFIG_PARAM = -116,  // 4  参数配置                4
    LOG_ACK_ERR = -117,  // 3 +120                         3
    LOG_HEART_ERROR = -118, // 2,
    LOG_SIM_DROP = -119, //  1,
} SocErrorEnum;


#pragma anon_unions 
typedef struct
{
   u8           inuse;
   u16          msg_len;	
   u8           socket_id;    /* socket ID */
   SocEnvEnum   event_type;   /* soc_event_enum */
   SocErrorEnum error_cause;  /* used only when EVENT is close/connect */
   u32 data;
} SocNotifyIndStruct;


typedef struct 
{
    
     s32 (*create)(int bears ,u8 accesid, StreamType type);
     s32 (*connect)(s32 sock_id, char *ip, u16 port, s32 bears,StreamType type);
     s32 (*send)(s32 sock_id , u8 *data , u16 len);
     s32 (*recv)(s32 sock_id , u8 *recv_buff , u16 max_len);
     s32 (*close)(s32 sock_id);
     s32 (*dns_prase)(char *addr, s32 bears, s32 *result);
     s32 (*ftp_open)(char *url, char *name ,char *psw,u16 port,ftp_notify_cb cb);
     s32 (*ftp_cd)(char *dir);
     s32 (*ftp_dl)(char *file);
     s32 (*ftp_recv)(u16 len);
     s32 (*ftp_close)(void);
     s32 (*ftp_list)(void);
     s32 (*lbs_req)(void);
     //void (*dns_cb) (void *msg);
     void (*socket_notify)(void *msg);

}SocketFuncStruct;

typedef void (*SocketNotifyFun)(void *msg);

typedef struct
{
    ModleInfoStruct model;

    SocketFuncStruct socket;

    SocketNotifyFun socket_notify;
    
}NetDevInfoStruct;

extern NetDevInfoStruct netdev;


void socket_task(void *arg);

KK_ERRCODE socket_event_send(SocEnvEnum env,u8 *data, u16 len,u8 soc_id , SocErrorEnum err);

ModleInfoStruct *model_addr(void);

typedef void (*SocketCallbackFun) (void *);

s32 dns_prase_register_callback(SocketCallbackFun cb_fun);
void socket_register_callback(SocketCallbackFun func);



#endif

