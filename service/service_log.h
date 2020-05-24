
#ifndef __SERVICE_LOG_H__
#define __SERVICE_LOG_H__

#include "board.h"
#include "json.h"
typedef enum
{
    LEVEL_DEBUG,
    LEVEL_INFO,
    LEVEL_WARN,
    LEVEL_ERROR,
    LEVEL_FATAL,
    LEVEL_TRANS,
}LogLevelEnum;



void service_log_init(void);


void service_log_timer_proc(void);


void log_print(LogLevelEnum level, const char * format, ... );


void sem_service_log_pend(void);


void sem_service_log_release(void);


void log_print_hex(LogLevelEnum level, u8 *data, u16 len, const char * format, ... );


#define KK_ASSERT(handle)       if(!handle)\
                                { \
                                    log_print(LEVEL_FATAL,"poniter NULL at %s,%d",__FUNCTION__, __LINE__);  \
                                    disable_iwdg();                                 \
                                    while(1) \
                                    {          \
                                        kk_os_sleep(10);\
                                    }\
                                }\


#define LOG_BUFFER_NUM 5
                                        
                                        
/**
 * Function:   创建log_service模块
 * Description:创建log_service模块
 * Input:      无
 * Output:     无
 * Return:     KK_SUCCESS——成功；其它错误码——失败
 * Others:     使用前必须调用，否则调用其它接口返回失败错误码
 */
KK_ERRCODE log_service_create(void);

/**
 * Function:   销毁log_service模块
 * Description:销毁log_service模块
 * Input:      无
 * Output:     无
 * Return:     KK_SUCCESS——成功；其它错误码——失败
 * Others:     
 */
KK_ERRCODE log_service_destroy(void);

/**
 * Function:   log_service模块定时处理入口
 * Description:log_service模块定时处理入口
 * Input:      无
 * Output:     无
 * Return:     KK_SUCCESS——成功；其它错误码——失败
 * Others:     
 */
KK_ERRCODE log_service_timer_proc(void);


/*由socket层回调*/
void log_service_connection_failed(void);
void log_service_connection_ok(void);
void log_service_close_for_reconnect(void);



void log_service_upload(LogLevelEnum level,JsonObject* p_root);

extern u8 g_log_level;


#endif

