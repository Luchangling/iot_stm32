#ifndef __KK_OS_CONFIG_API_H__
#define __KK_OS_CONFIG_API_H__

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include "time.h"

#include "includes.h"
#include "kk_type.h"
#include "kk_error_code.h"
#define KK_OS_STACK                 OS_STK

#define KK_OS_TIMER_ENTRY           OS_TMR

#define KK_OS_TICKS                 (1000/OS_TICKS_PER_SEC)

#define KK_OS_MAX_TIMER_ID          OS_TMR_CFG_MAX

#define KK_OS_TIMER_CB_FUN          OS_TMR_CALLBACK

#define KK_OS_TIMER_TICKS           (OS_TICKS_PER_SEC / OS_TMR_CFG_TICKS_PER_SEC)

#define KK_OS_MAX_EVENT_TBL_COUNT   OS_MAX_EVENTS - 1

#define KK_OS_EVENT_TBL             OS_EVENT

#define KK_OS_MSG_QUEUE             OS_EVENT

#define KK_OS_SUPPORT_QUEUE_COUNTS  5

#define KK_OS_QUE_NODE              void

#define KK_OS_ENTER_CRITICAL        OS_CPU_SR cpu_sr = OS_CPU_SR_Save();
                                    
#define KK_OS_EXIT_CRITICAL         OS_CPU_SR_Restore(cpu_sr);

enum
{
    KK_OS_TIMER_ONE_SHOT = OS_TMR_OPT_ONE_SHOT,
    KK_OS_TIMER_PERIODIC = OS_TMR_OPT_PERIODIC
};

typedef enum
{
    KK_OS_MSG_PEND, //阻塞获取MSG
    KK_OS_MSG_QUERY //查询MSG
}KKOSMsgEnum;



extern void kk_os_init(void);

extern s32  kk_os_create_task(void (*task)(void *p_arg),u32 stack_size,u16 prio);

extern s32  kk_os_delete_task(u16 prio);

extern void kk_os_startup(void);

extern void kk_os_sleep(u32 ms);

extern s32  kk_os_timer_start(u16 timer_id,KK_OS_TIMER_CB_FUN callback,u32 time_out);

extern s32  kk_os_timer_stop(u16 timer_id);

extern s32  kk_os_semaphore_create(void);

extern s32  kk_os_semaphore_request(s32 sem_id , u32 time_out);

extern void kk_os_semaphore_release(s32 sem_id);

extern void kk_os_semaphore_clear(s32 sem_id);

extern void kk_os_semaphore_destory(s32 sem_id);
#if 1
extern s32  kk_os_mutex_create(void);

extern void kk_os_mutex_request(s32 mutex_id);

extern void kk_os_mutex_release(s32 mutex_id);

extern void kk_os_mutex_destory(s32 mutex_id);
#else

#define kk_os_mutex_create kk_os_semaphore_create

#define kk_os_mutex_request(sem_id) kk_os_semaphore_request(sem_id ,0)

#define kk_os_mutex_release kk_os_semaphore_release

//extern void kk_os_semaphore_clear(s32 sem_id);

#define kk_os_mutex_destory kk_os_semaphore_destory


#endif

extern s32  kk_os_msg_queue_create(s32 max_q_count);

extern s32  kk_os_send_msg(s32 que_id ,void *msg);

extern void *kk_os_recive_msg(s32 que_id , KKOSMsgEnum is_pend);

extern void kk_os_queue_destory(s32 que_id);

extern void kk_os_sched_lock(void);

extern void kk_os_sched_unlock(void);

extern u32 kk_os_ticks_get(void);

extern u8 kk_os_ticks_clac_timeout(u32 start , u32 ms);

extern s32 kk_os_event_create(void);

extern s32 kk_os_event_send(s32 id , u16 flag);

extern s32 kk_os_event_recv(s32 id , u16 flag , u32 timeout);

extern s32 kk_os_event_clear(s32 id , u16 flag );

extern u8 event_err;

#endif

