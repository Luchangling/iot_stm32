
#include "kk_os_config_api.h"
#include "kk_os_header.h"

#define KKOS_MAX_TASK_COUNT 64

KK_OS_STACK         *s_kk_os_task_stack[KKOS_MAX_TASK_COUNT]            = {0};
KK_OS_QUE_NODE      *s_kk_os_queue_node[KK_OS_MAX_EVENT_TBL_COUNT]      = {0};
KK_OS_TIMER_ENTRY   *s_kk_os_timer_context[KK_OS_MAX_TIMER_ID]          = {0};
KK_OS_EVENT_TBL     *s_kk_os_event_pointer[KK_OS_MAX_EVENT_TBL_COUNT]   = {0};



void kk_os_init(void)
{
    memset(s_kk_os_task_stack   ,0,sizeof(s_kk_os_task_stack));

    memset(s_kk_os_queue_node   ,0,sizeof(s_kk_os_queue_node));

    memset(s_kk_os_timer_context,0,sizeof(s_kk_os_timer_context));

    memset(s_kk_os_event_pointer,0,sizeof(s_kk_os_event_pointer));
    
    OSInit();
}

s32 kk_os_create_task(void (*task)(void *p_arg),u32 stack_size,u16 prio)
{
    KK_OS_STACK *p = NULL;

    u32 size = (stack_size/4)*4;
    
    if(s_kk_os_task_stack[prio] != 0 || task == NULL || size == 0) return KK_PARAM_ERROR;

    p = (KK_OS_STACK *)malloc(size);

    if(!p) return KK_MEM_NOT_ENOUGH;

    memset((u8 *)p,0,size);

    OSTaskCreate(task,(void *)0,(KK_OS_STACK *)(p + size/4 - 1),prio);

    s_kk_os_task_stack[prio] = p;

    return prio;
}

s32 kk_os_delete_task(u16 prio)
{
    INT8U err = OS_ERR_NONE;
    
    err = OSTaskDel(prio);

    if(err != OS_ERR_NONE) return KK_PARAM_ERROR;

    free(s_kk_os_task_stack[prio]);

    s_kk_os_task_stack[prio] = 0;

    return KK_SUCCESS;
}


void kk_os_startup(void)
{
    OSStart();
}

/****************************************************************************************************
                                  OS Timer API
****************************************************************************************************/

void kk_os_sleep(u32 ms)
{
    assert_param(ms >= KK_OS_TICKS);
    
    OSTimeDly(ms/KK_OS_TICKS);
}


s32 kk_os_timer_start(u16 timer_id,KK_OS_TIMER_CB_FUN callback,u32 time_out)
{
    u8 err;

    u32 para = time_out*OS_TMR_CFG_TICKS_PER_SEC/1000;
    
    if(timer_id >= KK_OS_MAX_TIMER_ID || para == 0 || s_kk_os_timer_context[timer_id] != 0) return KK_PARAM_ERROR;

    s_kk_os_timer_context[timer_id] = OSTmrCreate(para,para,KK_OS_TIMER_PERIODIC,callback,NULL,NULL,&err);

    if(!s_kk_os_timer_context[timer_id] || err != OS_ERR_NONE) return KK_UNKNOWN;

    OSTmrStart(s_kk_os_timer_context[timer_id],&err);

    return KK_SUCCESS;
}

s32 kk_os_timer_stop(u16 timer_id)
{
    u8 err;
    
    if(timer_id >= KK_OS_MAX_TIMER_ID || s_kk_os_timer_context[timer_id] == 0) return KK_PARAM_ERROR;

    OSTmrStop(s_kk_os_timer_context[timer_id],0,NULL,&err);

    s_kk_os_timer_context[timer_id] = 0;

    return KK_SUCCESS;
}

/********************************************************************************************************
                                OS Semaphore API
********************************************************************************************************/

s32 kk_os_search_idle_event_tbl(void)
{
    u16 i = 0;

    for(i = 0 ; i < KK_OS_MAX_EVENT_TBL_COUNT ; i++)
    {
        if(!s_kk_os_event_pointer[i]) return i;
    }

    return KK_MEM_NOT_ENOUGH;
}

s32 kk_os_semaphore_create(void)
{
    s32 id = -1;

    id = kk_os_search_idle_event_tbl();

    if(id >= 0)
    {
        s_kk_os_event_pointer[id] = OSSemCreate(0);
    }

    return id;
}

u8 event_err = 0;

s32 kk_os_semaphore_request(s32 sem_id , u32 time_out)
{
    u8 err = OS_ERR_NONE;
    
    assert_param((sem_id >= 0 && sem_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[sem_id])
    {
        OSSemPend(s_kk_os_event_pointer[sem_id],time_out/KK_OS_TICKS,&err);

        event_err = err;

        if(err == OS_ERR_NONE) return KK_SUCCESS;
    }

    return KK_UNKNOWN;
}


void kk_os_semaphore_release(s32 sem_id)
{
    //u8 err;
    
    assert_param((sem_id >= 0 && sem_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[sem_id])
    {
        OSSemPost(s_kk_os_event_pointer[sem_id]);
        //OSSemSet(s_kk_os_event_pointer[sem_id]);
    }
}

void kk_os_semaphore_clear(s32 sem_id)
{
    u8 err;
    
    assert_param((sem_id >= 0 && sem_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[sem_id])
    {
        //OSSemPost(s_kk_os_event_pointer[sem_id]);
        OSSemSet(s_kk_os_event_pointer[sem_id],0,&err);
    }
}


void kk_os_semaphore_destory(s32 sem_id)
{
    u8 err;
    
    assert_param((sem_id >= 0 && sem_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[sem_id])
    {
        OSSemDel(s_kk_os_event_pointer[sem_id],0,&err);

        err = err;
    }

    s_kk_os_event_pointer[sem_id] = 0;
}
#if 1
/*********************************************************************************************
                                OS Mutex API
**********************************************************************************************/

s32 kk_os_mutex_create(void)
{
    s32 id = -1;

    u8 err = OS_ERR_NONE;

    id = kk_os_search_idle_event_tbl();

    if(id >= 0)
    {
        //s_kk_os_event_pointer[id] = OSMutexCreate(1,&err);

        err = err;
    }

    return id;
}

void kk_os_mutex_request(s32 mutex_id)
{
    u8 err;

    assert_param((mutex_id >= 0 && mutex_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[mutex_id])
    {
        //OSMutexPend(s_kk_os_event_pointer[mutex_id],0,&err);
    }
}

void kk_os_mutex_release(s32 mutex_id)
{
    u8 err;

    err = err;
    
    assert_param((mutex_id >= 0 && mutex_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[mutex_id])
    {
        //OSMutexPost(s_kk_os_event_pointer[mutex_id]);
    }
}

void kk_os_mutex_destory(s32 mutex_id)
{
    u8 err;
    
    assert_param((mutex_id >= 0 && mutex_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[mutex_id])
    {
        //OSMutexDel(s_kk_os_event_pointer[mutex_id],0,&err);
    }

    s_kk_os_event_pointer[mutex_id] = 0;
}
#endif
/********************************************************************************************
                    OS MSG QUE API
********************************************************************************************/

s32 kk_os_msg_queue_create(s32 max_q_count)
{
    s32 id = -1;    

    id = kk_os_search_idle_event_tbl();

    if(id >= 0)
    {
        s_kk_os_queue_node[id] = (void *)malloc(max_q_count * sizeof(u32));

        if(!s_kk_os_queue_node[id]) return KK_MEM_NOT_ENOUGH;
        
        s_kk_os_event_pointer[id] = OSQCreate(&s_kk_os_queue_node[id],(u16)max_q_count);
    }

    return id;
}

s32 kk_os_send_msg(s32 que_id ,void *msg)
{
    u16 i = 0;

    u8 err = 0;
    
    assert_param((que_id >= 0 && que_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[que_id])
    {
        if(((err = OSQPost(s_kk_os_event_pointer[que_id],msg)) != OS_ERR_NONE)  && i < 1000)
        {
            i++;
            kk_os_sleep(10);
        }
    }

    if(err != OS_ERR_NONE) return KK_MEM_NOT_ENOUGH;

    return KK_SUCCESS;
}

void *kk_os_recive_msg(s32 que_id , KKOSMsgEnum is_pend)
{
    void *ptr = NULL;

    u8 err;
    
    if(s_kk_os_event_pointer[que_id])
    {
        if(is_pend == KK_OS_MSG_QUERY)
        {
            ptr =  OSQAccept(s_kk_os_event_pointer[que_id],&err);
        }
        else
        {
            ptr =  OSQPend(s_kk_os_event_pointer[que_id],20,&err);
        }
    }

    return ptr;
}


void kk_os_queue_destory(s32 que_id)
{
    u8 err;
    
    assert_param((que_id >= 0 && que_id < KK_OS_MAX_EVENT_TBL_COUNT));

    if(s_kk_os_event_pointer[que_id])
    {
        OSQDel(s_kk_os_event_pointer[que_id],01,&err);
    }

    s_kk_os_event_pointer[que_id] = 0;

    free(s_kk_os_queue_node[que_id]);

    s_kk_os_queue_node[que_id]    = 0;
}

/***********************************************************************************
            OS Event
************************************************************************************/

s32 kk_os_event_create(void)
{
    u8 err;

    s32 id = 0;
    
    id = (s32)OSFlagCreate(0,&err);

    if(err == OS_ERR_NONE)
    {
        return id;
    }

    return 0;
}

s32 kk_os_event_send(s32 id , u16 flag)
{
    u8 err;
    OSFlagPost((OS_FLAG_GRP*)id,flag,OS_FLAG_SET,&err);

    if(err == OS_ERR_NONE)
    {
        return KK_SUCCESS;
    }

    return KK_UNKNOWN;
}

s32 kk_os_event_recv(s32 id , u16 flag , u32 timeout)
{
    u8 err;

    U16 flg;
    
    //OSFlagAccept((OS_FLAG_GRP*)id,flag,OS_FLAG_WAIT_SET_ANY|OS_FLAG_CONSUME,&err);

    flg = OSFlagPend((OS_FLAG_GRP*)id,flag,OS_FLAG_WAIT_SET_ANY,timeout/KK_OS_TICKS,&err);

    if(err == OS_ERR_NONE)
    {
        return (s32)flg;
    }

    return KK_TIMOUT_ERROR;
}

s32 kk_os_event_clear(s32 id , u16 flag )
{
    u8 err;
    
    OSFlagAccept((OS_FLAG_GRP*)id,flag,OS_FLAG_WAIT_SET_ANY|OS_FLAG_CONSUME,&err);

    if(err == OS_ERR_NONE)
    {
        return KK_SUCCESS;
    }

    return KK_UNKNOWN;
}

void kk_os_sched_lock(void)
{
    OSSchedLock();
}

void kk_os_sched_unlock(void)
{
    OSSchedUnlock();
}

///
u32 kk_os_ticks_get(void)
{
    return OSTime;
}

u8 kk_os_ticks_clac_timeout(u32 start , u32 ms)
{
    //u32 timeout = ms*OS_TICKS_PER_SEC/1000;

    u32 ticks = kk_os_ticks_get();

    if(start <= ticks)
    {
        ticks = ticks - start;
    }
    else
    {
        ticks = 0xFFFFFFFF - start + ticks;
    }

    return (ticks >= (ms*OS_TICKS_PER_SEC/1000));
}

