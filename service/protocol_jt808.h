

#ifndef __PROTOCOL_JT808_H__
#define __PROTOCOL_JT808_H__

#include "kk_type.h"
#include "kk_error_code.h"

#include "gps_service.h"

/*plen 是输入输出参数*/
void protocol_jt_pack_regist_msg(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_auth_msg(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_iccid_msg(u8 *pdata, u16 *idx, u16 len);


void protocol_jt_pack_logout_msg(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_heartbeat_msg(u8 *pdata, u16 *idx, u16 len);

struct GPSData;
void protocol_jt_pack_gps_msg(GpsDataModeEnum mode, const GPSData *gps, u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_lbs_msg(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_gps_msg2(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_remote_ack(u8 *pdata, u16 *idx, u16 len, u8 *pRet, u16 retlen);
void protocol_jt_pack_general_ack(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_param_ack(u8 *pdata, u16 *idx, u16 len);
void protocol_jt_pack_escape(u8 *src, u16 src_len, u8 *dest, u16 *dest_len);



void protocol_jt_parse_msg(u8 *pdata, u16 len);


#endif




