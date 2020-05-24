

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "kk_type.h"

#define COMMAND_MAX_RESP_LEN 256

typedef enum
{
    COMMAND_UART = 0,
    COMMAND_SMS = 1,
    COMMAND_GPRS = 2,
}CommandReceiveFromEnum;


KK_ERRCODE command_on_receive_data(CommandReceiveFromEnum from, char* p_cmd, u16 src_len, char* p_rsp, void * pmsg);

void debug_port_command_process(void);

u8 is_nmea_log_enable(void);

u8 is_at_log_enable(void);

#endif


