
#ifndef __GPRS_H__
#define __GPRS_H__

#include "kk_os_header.h"


#define MAX_GPRS_USER_NAME_LEN 32
#define MAX_GPRS_PASSWORD_LEN  32
#define MAX_GPRS_APN_LEN       100
#define MAX_GPRS_MESSAGE_LEN       1500
#define MAX_GPRS_PART_MESSAGE_TIMEOUT    3

#define  SW_DNS_REQUEST_ID	         0x055A5121

typedef struct {
    u8 apnName[MAX_GPRS_APN_LEN];
    u8 apnUserId[MAX_GPRS_USER_NAME_LEN]; 
    u8 apnPasswd[MAX_GPRS_PASSWORD_LEN]; 
    u8 authtype; // pap or chap

} ST_GprsConfig;



/**
 * Function:   创建gprs模块
 * Description:创建gprs模块
 * Input:	   无
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   使用前必须调用,否则调用其它接口返回失败错误码
 */
KK_ERRCODE gprs_create(void);

/**
 * Function:   销毁gprs模块
 * Description:销毁gprs模块
 * Input:	   无
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
KK_ERRCODE gprs_destroy(void);

/**
 * Function:   gprs模块定时处理入口
 * Description:gprs模块定时处理入口
 * Input:	   无
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   1秒钟调用1次
 */
KK_ERRCODE gprs_timer_proc(void);

/**
 * Function:   判断GPRS是否正常
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   true——正常；false——不正常
 * Others:	   
 */
bool gprs_is_ok(void);


/**
 * Function:   判断是否需要重启
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   true——重启；false——不重启
 * Others:	   
 */
bool gprs_check_need_reboot(u32 check_time);

u32 gprs_get_last_good_time(void);
u32 gprs_get_call_ok_count(void);

void gprs_config_apn(void);


//#define 

#endif


