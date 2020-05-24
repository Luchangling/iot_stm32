
#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "socket_service.h"

#define KK_OS_DNS_MAX_LENTH  50


#define MAX_SOCKET_RECV_MSG_LEN 750
#define MAX_SOCKET_SEND_MSG_LEN 1600
#define MAX_SOCKET_ONE_SEND_LEN 500

#define MAX_SOCKET_SUPPORTED 6
#define SIZE_OF_SOCK_FIFO            1500

#define MAX_GET_HOST_REPEAT 3
#define MAX_CONNECT_REPEAT 3
#define MAX_MESSAGE_REPEAT 3

#define CONNECT_TIME_OUT 10
#define GET_HOST_TIME_OUT 10
#define MESSAGE_TIME_OUT 10


/*
标明Socket的通道,在SDK中会根据access_id来记录Socket相关参数（sockaddr_struct??
对应到KK_SocketConnect 的access_id参数
*/
typedef enum
{
    SOCKET_INDEX_MAIN = 1,
    SOCKET_INDEX_EPO = 2,
    SOCKET_INDEX_AGPS = SOCKET_INDEX_EPO,
    SOCKET_INDEX_LOG = 3,
    SOCKET_INDEX_UPDATE = 4,
    SOCKET_INDEX_CONFIG = 5,
    SOCKET_INDEX_UPDATE_FILE = SOCKET_INDEX_UPDATE,
    SOCKET_INDEX_MAX = MAX_SOCKET_SUPPORTED + 1,
}SocketIndexEnum;


typedef struct
{
    int id;         /* socket id */
    char addr[KK_OS_DNS_MAX_LENTH];    /*dns name*/
    u8 status;      /* current status */
    u8 status_fail_count;  /* fail count of current status */
    u8 ip[4];
    u16 port;
    StreamType type;
    SocketIndexEnum access_id;    /* used by sdk. */
    FifoType fifo;
    u8 excuted_get_host;        /* if excuted get host, can not connect then fail.*/
    u32 send_time;   /* record time of recent get host, connect, send msg etc*/
    u32 confirm_time;
    //because gps data have no ack msg. it depends on ack_seq to confirm msg is received.
    u32 last_ack_seq;  //ack_seq before send
   
}SocketType;





typedef enum 
{
    SOCKET_STATUS_INIT = 0,
    SOCKET_STATUS_GET_HOST = 1,
    SOCKET_STATUS_CONNECTING = 2,
    SOCKET_STATUS_LOGIN = 3,          //一些socket不用登录
    SOCKET_STATUS_WORK = 4,
    SOCKET_STATUS_DATA_FINISH = 5,    //配置socket数据ok后才对其它socket初始化
    SOCKET_STATUS_ERROR = 6,          //配置socket数据不ok 也要对其它socket初始化
    SOCKET_STATUS_MAX = 7,
}SocketStatus;

typedef enum 
{
    CURRENT_GET_HOST_INIT = 0,
    CURRENT_GET_HOST_CONTINUE = 1,
    CURRENT_GET_HOST_SUCCESS = 2,
    CURRENT_GET_HOST_FAILED = 3,
}CurrentGetHostByNameStatus;
    

/*
get_host_by_name 独立运行, 更新system_state ip_cache

socket 与gprs模块交互过程:
            系统 初始态  
                gprs_create
            系统 work:    
                gprs_timer_proc 中据状态操作
                init态
                reg态
                cgreg态
                    注册成功
                        kk_socket_global_init()
                        若是开机重启(配置socket处于init态
                            配置socket_create
                            调kk_socket_init    kk_socket_set_ip_port/kk_socket_set_addr
                        切换到工作态
                工作态
                    调配置socket_timer_proc()
                    配置socket_timer_proc据状态操作
                    init态
                        ip有效,直接用ip, 
                        无效等待ip有效
                    连接态
                        超时重建连接, 三次进入error态
                    工作态
                        收发数据,数据完毕更改配置 进数据ok态
                    数据ok态 error态
                        各socket_create
                    各socket _timer_proc据状态操作
                        初始态
                            ip有效,直接用ip建连接
                        gethost态
                             超时Gprs重新初始化
                            三次进入error态
                            拿到ip建连接
                        连接态
                            超时重建连接, 三次进入error态
                            区分主连接与非主连接
                        连接态
                            需login就login
                        工作态
                            收发数据
                        断连态
                              清理数据, 等待重连
                断连态
                    调kk_socket_global_destroy()函数,

*/

/* 在系统初始化时调用, 初始化account_id*/
KK_ERRCODE kk_socket_global_init(void);

/* 在gprs重建连接时调用 清理所有socket */
void kk_socket_global_destroy(void);

SocketType * get_socket_by_id(int id);
SocketType * get_socket_by_accessid(int access_id);



/* 在socket 初始化时调用, 初始化socket对应的id, status, fifo等*/
KK_ERRCODE kk_socket_init(SocketType *socket, SocketIndexEnum access_id);
KK_ERRCODE kk_socket_connect(SocketType *socket);
void kk_socket_get_host_by_name_trigger(SocketType *socket);
void kk_socket_get_host_timer_proc(void);


/*在KK_SocketRegisterCallBack 注册的回调函数 gprs_socket_notify 中调用 */
void kk_socket_connect_ok(SocketType *socket);
void kk_socket_close_for_reconnect(SocketType *socket);
void kk_socket_destroy(SocketType * socket);


/* 在socket 连接建立前调用 重建连接不用调用 */
KK_ERRCODE kk_socket_set_ip_port(SocketType *socket, u8 ip[4], u16 port, StreamType type);
KK_ERRCODE kk_socket_set_addr(SocketType *socket, u8 *addr, u8 addr_len, u16 port, StreamType type);
void kk_socket_set_account_id(int account_id);
KK_ERRCODE kk_is_valid_ip(const u8*data);
KK_ERRCODE kk_socket_send(SocketType *socket, u8 *data, u16 len);
KK_ERRCODE kk_socket_recv(SocketType *socket ,int recvlen ,u8 *recv_buff);
const char * kk_socket_status_string(SocketStatus statu);
KK_ERRCODE kk_socket_get_ackseq(SocketType *socket, u32 *ack_seq);


void current_get_host_init(void);
void socket_get_host_by_name_callback(void *msg);

#define GetHostByName  netdev.socket.dns_prase

#define SocketCreate   netdev.socket.create

#define SocketConnect  netdev.socket.connect

#define SocketRecv     netdev.socket.recv

#define SocketClose    netdev.socket.close

#define SocketSend     netdev.socket.send

#define SocketFtpOpen  netdev.socket.ftp_open

#define SocketFtpCw    netdev.socket.ftp_cd

#define SocketFtpDl    netdev.socket.ftp_dl

#define SocketFtpRec   netdev.socket.ftp_recv

#define SocketFtpLis   netdev.socket.ftp_list

#define SocketFtpClose netdev.socket.ftp_close

#define SocketLbsReq   netdev.socket.lbs_req

#define ModelImei      netdev.model.imei
//#define ModelImei    "668613632849783"

#define ModelIccid     netdev.model.iccid

#define ModelCsq       netdev.model.csq

#define ModelImsi      netdev.model.imsi

#define ModelSw        netdev.model.sw

#define IsModelRegOK   netdev.model.cgreg_rdy

#define IsModelPdpOK   netdev.model.pdp_rdy

#define IsModelSimOK   netdev.model.sim_rdy

#define ModelLat       netdev.model.latitude

#define ModelLong      netdev.model.longitude

#endif



