
#ifndef __KK_OS_HEADER_H__
#define __KK_OS_HEADER_H__
#include "includes.h"
#include "kk_type.h"
#include "kk_error_code.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include "kk_os_config_api.h"
#include "fifo.h"
#include "service_log.h"
#include "common.h"
#include "json.h"
#include "utility.h"
#include "system_state.h"
#include "config_service.h"
#include "led_service.h"
enum
{

    TIMER_TEST,

    TIMER_LOG_DBG,
    
    TIMER_REGISER_QUERY,

    TIMER_GPS_CHECK_RECEIVED,

    TIMER_GPS_CHECK_STATE,

    TIMER_NET_WORK,

    TIMER_GPS_INJECT,
    
    MAX_TIMER_ID = KK_OS_MAX_TIMER_ID,
};

enum
{
    
    AT_RESP_TASK_PRIO = 2,
    SOCKET_TASK_PRIO  = 3,
    OS_TIMER_PRIO     = 4,
    GPS_TASK_PRIO     = 5,
    
    MAIN_TASK_PRIO    = 6,

    LOWEST_TASK_PRIO  = MAIN_TASK_PRIO,
};

#define LOG log_print

#define LOG_HEX log_print_hex

#endif
