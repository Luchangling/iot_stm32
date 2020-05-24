
#ifndef __AGPS_SERVICE_H__
#define __AGPS_SERVICE_H__

#include "kk_error_code.h"

void agps_service_timer_proc(void);

KK_ERRCODE agps_service_create(void);

KK_ERRCODE agps_service_close(void);

KK_ERRCODE agps_service_destory(void);

#endif

