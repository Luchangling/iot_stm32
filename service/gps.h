
#ifndef __GPS_H__
#define __GPS_H__

#include <time.h>
#include "kk_type.h"
#include "kk_error_code.h"

//GPSState的第0位表示是否差分定位定位,0——非差分定位；1——差分定位
//GPSState的第1位表示2D/3D定位,0——2D；1——3D
//GPSState的第2位表示是否定位定位,0——未定位；1——已定位

typedef enum
{	KK_GPS_OFF = 0xF0,      //关闭11110000
	KK_GPS_FIX_NONE = 0,    //未定位00000000
	KK_GPS_FIX_2D = 4,      //2D定位00000100
	KK_GPS_FIX_2D_DIFF = 5, //2D差分定位00000101
	KK_GPS_FIX_3D = 6,      //3D定位00000110
	KK_GPS_FIX_3D_DIFF = 7, //3D差分定位00000111
}GPSState;

typedef enum 
{
    //型号未知
	KK_GPS_TYPE_UNKNOWN = 0,

	//2503D芯片内置GPS模块
	KK_GPS_TYPE_MTK_EPO = 1,

	//外接泰斗GPS模块
	KK_GPS_TYPE_TD_AGPS = 2,

	//外接中科微GPS模块
	KK_GPS_TYPE_AT_AGPS = 3,
}GPSChipType;

typedef struct
{
	//卫星编号
	u8 prn;
	//信噪比
	u8 snr;
}SNRInfo;

#define SNR_INFO_NUM 5



typedef struct
{	
	//GPS时间(UTC时间)
	time_t gps_time;

	//纬度
	float lat;

	//经度
	float lng;

	//高度(单位米)
	float alt;

	//速度(公里/小时)
	float speed;

	//航向:表示范围:000—359度,正北为0度,顺时针方向
	float course;

	//信号强度等级[0,100],数值越大信号越强
	U8 signal_intensity_grade;

	//定位精度[0.5,99.9],数值越小精度越高
	float hdop;

	//跟踪到的卫星个数
	U8 satellites_tracked;
}GPSData;

#define HD_HEADER 0xF1D9

typedef enum
{
    HD_AID_PEPH_GPS = 0x320B,

    HD_AID_PEPH_BDS = 0x330B,

    HD_AID_POS      = 0x100B,

    HD_AID_TIME     = 0x110B,

    HD_COLD_START   = 0x4006,

    HD_INJECT_RESP  = 0x0105,
    
}HDClassIDEnum;

#pragma pack(1)
typedef struct
{
    u16 head;

    u16 classID;

    u16 framelen;

    u8  payload[1];

    u16 check_sum;
    
    
}HDGpsFrameStruct;

#pragma pack()


/**
 * Function:   创建gps模块
 * Description:创建gps模块
 * Input:	   无
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   使用前必须调用,否则调用其它接口返回失败错误码
 */
KK_ERRCODE gps_create(void);

/**
 * Function:   销毁gps模块
 * Description:销毁gps模块
 * Input:	   无
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
KK_ERRCODE gps_destroy(void);


/**
 * Function:   唤醒
 * Description:
 * Input:	   push_data_enbale:是否推送数据到服务模块
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
KK_ERRCODE gps_power_on(bool push_data_enbale);

/**
 * Function:   写入参考定位和时间
 * Description:
 * Input:	   lng:经度；lat:纬度；utc_time:当前时间;leap_sencond:闰秒,泰斗AGPS直接从AGPS文件中读出,中科微的从AGPS服务器获取
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   状态>=KK_GPS_STATE_INITED时可以写入
 */
KK_ERRCODE gps_write_agps_info(const float lng,const float lat,const U8 leap_sencond,const U32 data_start_time);

/**
 * Function:   写入agps（epo）数据
 * Description:
 * Input:	   seg_index:分片ID；p_data:数据指针；len:数据长度
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   状态>=KK_GPS_STATE_REF_TIME_OK时可以写入
 */
KK_ERRCODE gps_write_agps_data(const U16 seg_index, const U8* p_data, const U16 len);

/**
 * Function:   收到串口数据
 * Description:
 * Input:	   p_data:数据指针；len数据长度
 * Output:	   无
 * Return:	   无
 * Others:	   内存无须释放
 */
void gps_on_rcv_uart_data(char* p_data, const U16 len);

/**
 * Function:   休眠
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
KK_ERRCODE gps_power_off(void);

/**
 * Function:   获取GPS模块状态
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   FixState
 * Others:	   
 */
GPSState gps_get_state(void);

/**
 * Function:   是否已定位
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   true——已定位；false——未定位
 * Others:	   
 */
bool gps_is_fixed(void);


/**
 * Function:   获取GPS定位时间
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   定位时间
 * Others:	   
 */
U16 gps_get_fix_time(void);

/**
 * Function:   获取最大信噪比
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   最大信噪比
 * Others:	   
 */
U8 gps_get_max_snr(void);

/**
 * Function:   获取跟踪卫星数
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   跟踪卫星数
 * Others:	   
 */
U8 gps_get_satellites_tracked(void);

/**
 * Function:	 获取可见卫星数(1秒更新一次)
 * Description:
 * Input: 	 	 无
 * Output:	     无
 * Return:	     可见卫星数
 * Others:	 
 */
U8 gps_get_satellites_inview(void);

/**
 * Function:	 获取信号好的卫星数(SNR>35)
 * Description:
 * Input: 	 	 无
 * Output:	     无
 * Return:	     可见卫星数
 * Others:	 
 */
U8 gps_get_satellites_good(void);


 /**
  * Function:	获取最好的5颗星的信噪比
  * Description:
  * Input:	    无
  * Output:     无
  * Return:     数组首地址
  * Others:   
  */
 const SNRInfo* gps_get_snr_array(void);


/**
 * Function:   获取最新GPS数据
 * Description:
 * Input:	   无
 * Output:	   p_data:指向定位数据的指针
 * Return:	   true——成功；false——失败
 * Others:	   
 */
bool gps_get_last_data(GPSData* p_data);

/**
 * Function:   获取最近n秒的GPS数据
 * Description:
 * Input:	   seconds:第几秒,从0开始
 * Output:	   p_data:指向定位数据的指针
 * Return:	   true——成功；false——失败
 * Others:	   
 */
bool gps_get_last_n_senconds_data(U8 seconds,GPSData* p_data);


/**
 * Function:   获取匀速行驶时间
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   匀速行驶时间（秒）
 * Others:	   
 */
U32 gps_get_constant_speed_time(void);

/**
 * Function:   获取行驶加速度
 * Description:
 * Input:	   无
 * Output:	   无
 * Return:	   行驶加速度（g）
 * Others:	   
 */
float gps_get_aclr(void);



void gps_task(void *arg);

bool gps_find_optimal_data_by_time_and_hdop(GPSData* p_data);

void gps_cold_start(void);

void gps_inject_start(void);


#endif

