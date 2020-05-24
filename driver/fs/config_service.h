
#ifndef __CONFIG_SERVICE_H__
#define __CONFIG_SERVICE_H__

#include "kk_type.h"
#include "kk_error_code.h"
//#include "socket_service.h"
//#include "socket.h"
#include "gps.h"
#include "applied_math.h"
//发布版修改这个数

#define VERSION_NUMBER  "1.3.0"

#define KK_DNS_MAX_LENTH 50
#define APN_DEFAULT "CMNET"
#define APN_USER_DEFAULT ""
#define APN_PWD_DEFAULT ""
#define KK_IMEI_DEFAULT "866717048888889"
#define UPDATE_DEVICE_CODE "SYS"
#define UPDATE_BOOT_CODE "boot"
#define CONFIG_DEFAULT_MAIN_ADDR       "webbo.yunjiwulian.com:10003"
#define CONFIG_LOG_SERVER_ADDERSS      "yj03.yunjiwulian.com:10009"
//#define CONFIG_LOG_SERVER_ADDERSS      "firmwarelog.gpsorg.net:39998"

#define CONFIG_LOG_SERVER_IP           "47.106.251.151"
#define KK_UPDATE_SERVER_DNS        "yj03.yunjiwulian.com:10008"
#define KK_UPDATE_SERVER_IP         "47.106.251.151"

#define CFG_CUSTOM_CODE  "FOTA"
#define CFG_SYSTEM_MODEL "SYS"
#define CFG_EXTSYS_MODEL "EXT"

#define CONFIG_MAX_CELL_NUM_LEN    17
#define CONFIG_CMD_MAX_LEN         10
#define CONFIG_STRING_MAX_LEN      100

#define CONFIG_UPLOAD_TIME_MAX     10800ul
#define CONFIG_UPLOAD_TIME_DEFAULT 10 
#define CONFIG_UPLOAD_TIME_MIN     0 
#define CONFIG_SPEEDTHR_MAX        180
#define CONFIG_SPEEDTHR_MIN        5
#define CONFIG_STATIC_TIME_MAX     500
#define CONFIG_ACCTHR_MAX          2.0f
#define CONFIG_ACCTHR_MIN          0.1f
#define CONFIG_BRAKETHR_MAX        3.0f
#define CONFIG_BRAKETHR_MIN        0.5f
#define CONFIG_CRASHLTHR_MAX       6.0f
#define CONFIG_CRASHLTHR_MIN       1.0f
#define CONFIG_CRASHMTHR_MAX       8.0f
#define CONFIG_CRASHMTHR_MIN       4.0f
#define CONFIG_CRASHHTHR_MAX       15.0f
#define CONFIG_CRASHHTHR_MIN       8.0f
#define CONFIG_TURNTHR_MAX         5.0f
#define CONFIG_TURNTHR_MIN         0.1f
#define CONFIG_STATICTHR_MAX       1.0f
#define CONFIG_STATICTHR_MIN       0.0f
#define CONFIG_RUNTHR_MAX          1.0f
#define CONFIG_RUNTHR_MIN          0.0f
#define CONFIG_QUAKETHR_MAX        1.0f
#define CONFIG_QUAKETHR_MIN        0.0f
#define CONFIG_LOW_VOLTHR_MAX      15.0f
#define CONFIG_LOW_VOLTHR_MIN      0.0f
#define KK_HEARTBEAT_MAX        360ul
#define KK_HEARTBEAT_DEFAULT    180ul
#define KK_HEARTBEAT_MIN        3
#define KK_SLEEPTIME_MAX        30
#define KK_SLEEPTIME_DEFAULT    5
#define KK_SLEEPTIME_MIN        0
#define CONFIG_ANGLETIME_MAX       0xFF
#define CONFIG_ANGLETIME_DEFAULT   10
#define CONFIG_ANGLETIME_MIN       0
#define CONFIG_SHAKECOUT_MAX       100
#define CONFIG_SHAKECOUT_DEFAULT   5
#define CONFIG_SHAKECOUT_MIN       1
#define CONFIG_AUTODEFDELAY_MAX       1200
#define CONFIG_AUTODEFDELAY_DEFAULT   300
#define CONFIG_AUTODEFDELAY_MIN       1
#define CONFIG_SHAKEALARMDELAY_MAX       1200
#define CONFIG_SHAKEALARMDELAY_DEFAULT   30
#define CONFIG_SHAKEALARMDELAY_MIN       1
#define CONFIG_SHAKEALARMINTERVAL_MAX       3600
#define CONFIG_SHAKEALARMINTERVAL_DEFAULT   1800
#define CONFIG_SHAKEALARMINTERVAL_MIN       1
#define CONFIG_MAX_DNS_LEN 50
#define CONFIG_MAX_APN_LEN 30
#define CONFIG_MAX_APN_USR_LEN 30
#define CONFIG_MAX_APN_PSW_LEN 30

#define KK_MAPS_URL_DEFAULT "http://ditu.google.cn/maps?q="
//部标厂商标志.    后面可通过参数配置来修�?
#define LOW_VOLTAGE_ALARM_PERCENT 0.8
#define MAX_JT_AUTH_CODE_LEN 100
#define MAX_JT_OEM_ID_LEN 5
#define JT_DEFAULT_CELL_NUM "20191111001"
#define JT_OEM_ID "11111"  
#define MAX_JT_DEV_ID_LEN 7
#define JT_DEV_ID "9887125"
#define MAX_JT_VEHICLE_NUMBER_LEN 12
#define JT_VEHICLE_NUMBER "��B 99988"
#define GB_ATTYPE_STANDARD     0x00   //标准
#define GB_ATTYPE_CUSTOM       0x01   //补充
#define LBS_UPLOAD_DEFAULT 60
#define GPS_NOT_FIXED_DEFAULT 60

/* Socket Type */
typedef enum
{
    STREAM_TYPE_STREAM = 0,      /* stream socket, TCP */
    STREAM_TYPE_DGRAM = 1,       /* datagram socket, UDP */
    STREAM_TYPE_SMS = 2,         /* SMS bearer */
    STREAM_TYPE_RAW = 3,         /* raw socket */
} StreamType;


typedef enum
{
    MAGIC_CRC,
    //为兼容的原因, 这些配置要连在一�?BEGIN
    CFG_SERVERADDR,           // 主服务器地址,xxx.xxx.xxx.xxx:port      = 0
    CFG_UPLOADTIME,           // 上传gps间隔
    CFG_SPEEDTHR,             // 超速阀�?
    CFG_PROTOCOL,             // ConfigProtocolEnum


    CFG_HEART_INTERVAL,      // = 40

    CFG_APN_NAME,   //80
    CFG_APN_USER,
    CFG_APN_PWD,
	
    //时区[TYPE_BYTE]
    CFG_TIME_ZONE,

    //部标 是否已注�?只在设备开电后注册一�?改设备号则要重新注册
    CFG_JT_ISREGISTERED,
    // 部标协议类型
    CFG_JT_AT_TYPE, 
    // 终端ID
    CFG_JT_DEVICE_ID,
    // 车牌号码
    CFG_JT_VEHICLE_NUMBER,
    // 车牌颜色
    CFG_JT_VEHICLE_COLOR,
    // 厂商ID
    CFG_JT_OEM_ID,
    // 省域ID
    CFG_JT_PROVINCE,
    // 市域ID
    CFG_JT_CITY,
    //验证�?
    CFG_JT_AUTH_CODE, // 110

    CFG_JT_CEL_NUM,

    CFG_JT_HBINTERVAL, ////休眠时上报间隔

    CFG_IS_STATIC_UPLOAD,
    
    CFG_TURN_ANGLE,
    
    CFG_SPEED_ALARM_ENABLE,
    
    CFG_SPEED_CHECK_TIME,

    //当前是否测试模式
    CFG_IS_TEST_MODE,

    CFG_LOGSERVERADDR,

    CFG_UPDATESERVERADDR,

    CFG_UPDATEFILESERVER,

    CFG_RELAY_STATE,

    CFG_AGPS_EN,

    CFG_PARAM_MAX
}ConfigParamEnum;

typedef enum
{
    TYPE_NONE = 0x00,
    TYPE_INT = 0x01,
    TYPE_SHORT = 0x02,
    TYPE_STRING = 0x03,
    TYPE_BOOL = 0X04,
    TYPE_FLOAT = 0X05,
    TYPE_BYTE = 0x06,
    TYPE_ARRY = 0x07,
    TYPE_MAX
}ConfigParamDataType;

typedef enum
{
    CFG_CMD_REQ = 0x13,
    CFG_CMD_ACK = 0x92,
    CFG_CMD_RESULT = 0x14,
    CFG_CMD_MAX
}ConfigCmdEnum;

typedef enum
{
    CFG_CMD_REQ_ALL = 0x00,
    CFG_CMD_REQ_ONCE = 0x01,
    CFG_CMD_REQ_PERMANENT= 0x02,
}ConfigCmdReqType;

typedef enum
{
    HEART_SMART = 0x00,  //0x03 or 0x07
    HEART_NORMAL = 0x01,  //0x03
    HEART_EXPAND = 0x02,  //0x07
    HEART_MAX
}ConfigHearUploadEnum;


typedef struct
{
    ConfigParamDataType type;
    u16 len;
    
} ConfigParamItems;


typedef enum
{
    PROTOCOL_NONE = 0x00,
    PROTOCOL_JT808 = 0x01,
    PROTOCOL_MAX
}ConfigProtocolEnum;





//断电报警方式 00: 只GPRS方式 01:SMS+GPRS  10: GPRS+SMS+CALL 11:GPRS+CALL 默认:01
typedef enum
{
    PWRALM_GPRS = 0,
    PWRALM_GPRS_SMS = 1,
    PWRALM_GPRS_SMS_CALL = 2,
    PWRALM_GPRS_CALL = 3,
}PowerAlarmMode;

/**
 * Function:   创建config_service模块
 * Description:创建config_service模块
 * Input:	   
 * Output:	   
 * Return:	   KK_SUCCESS——成功；其它错误码——失�?
 * Others:	   使用前必须调�?否则调用其它接口返回失败错误�?
 */
KK_ERRCODE config_service_create(void);

/**
 * Function:   销毁config_service模块
 * Description:销毁config_service模块
 * Input:	   
 * Output:	   
 * Return:	   KK_SUCCESS——成功；其它错误码——失�?
 * Others:	   
 */
KK_ERRCODE config_service_destroy(void);


/**
 * Function:   
 * Description: 获取指定id对应的配�?
 * Input:	   id:配置id    type:数据类型
 * Output:	   
 * Return:	   数据长度
 * Others:	   该函数只有字符串类型的配�? 不知道长度里, 需要先调用以获知长�?
 */
u16 config_service_get_length(ConfigParamEnum id, ConfigParamDataType type);

/**
 * Function:   
 * Description: 根据id与type 获取配置到buf�?
 * Input:	   id:配置id    type:数据类型      buf:输出,用来存储配置数据            len:buf长度
 * Output:	   
 * Return:	   
 * Others:	   
 */
KK_ERRCODE config_service_get(ConfigParamEnum id, ConfigParamDataType type, void* buf, u16 len);

/**
 * Function:   
 * Description: 根据id与type �?buf中的内容 设置到config_service模块
 * Input:	   id:配置id    type:数据类型      buf:用来存储配置数据            len:buf长度
 * Output:	   
 * Return:	   
 * Others:	   
 */
void config_service_set(ConfigParamEnum id, ConfigParamDataType type, const void* buf, u16 len);

/**
 * Function:   
 * Description: 此函数可代替config_service_get, 特别是string类型, 会少掉字符串拷贝的时�?
 * Input:	   id:配置id 
 * Output:	  
 * Return:	   指向id对应配置
 * Others:	  
 */
void* config_service_get_pointer(ConfigParamEnum id);

    
/**
 * Function:   
 * Description: 获取当前协议配置
 * Input:	   
 * Output:	   
 * Return:	   当前协议
 * Others:	  
 */
ConfigProtocolEnum config_service_get_app_protocol(void);

/**
 * Function:   
 * Description: 获取当前心跳协议配置
 * Input:	   
 * Output:	   
 * Return:	   心跳协议
 * Others:	  
 */
ConfigHearUploadEnum config_service_get_heartbeat_protocol(void);

/**
 * Function:   
 * Description: 获取当前时区配置
 * Input:	   
 * Output:	   
 * Return:	  
 * Others:	   
 */
s8 config_service_get_zone(void);


/**
 * Function:   
 * Description: 用何种心跳协�?
 * Input:	   
 * Output:	   
 * Return:	   心跳协议
 * Others:	   
 */
bool config_service_is_main_server_goome(void);



/**
 * Function:   
 * Description: 告警只发送给一个号
 * Input:	   
 * Output:	  
 * Return:	   心跳协议
 * Others:	  
 */
bool config_service_is_alarm_sms_to_one(void);

/**
 * Function:   
 * Description: 获取短信发送号�?
 * Input:	   
 * Output:	   
 * Return:	   号码长度
 * Others:	  
 */
u8 config_service_get_sos_number(u8 index, u8 *buf, u16 len);



/**
 * Function:   
 * Description: 是否启用agps, 如果不是, 表示启用epo
 * Input:	   
 * Output:	  
 * Return:	   1 apgs    0 epo
 * Others:	  
 */
bool config_service_is_agps(void);



/**
 * Function:   
 * Description: 获取当前gps类型
 * Input:	  
 * Output:	  
 * Return:	   时区
 * Others:	   
 */
GPSChipType config_service_get_gps_type(void);



/**
 * Function:   
 * Description: 从本地配置文件中获取配置信息
 * Input:	   configs列表指针
 * Output:	   configs列表指针
 * Return:	   KK_SUCCESS 成功, 其它失败
 * Others:	  
 */
KK_ERRCODE config_service_read_from_local(void);

/**
 * Function:   
 * Description: 保存配置到本地文件中
 * Input:	   configs列表指针
 * Output:	   
 * Return:	   KK_SUCCESS 成功, 其它失败
 * Others:	  
 */
KK_ERRCODE config_service_save_to_local(void);

/**
 * Function:   根据ID获取设备型号
 * Description:获取设备型号(字符�?
 * Input:	   设备型号
 * Output:	   
 * Return:	   设备型号(字符
 * Others:	   
 */
const char * config_service_get_device_type(u16 index);


/**
 * Function:   
 * Description: 恢复出厂设置
 * Input:	   configs列表指针     is_all(是不是包括ip地址设置)
 * Output:	   configs列表指针
 * Return:	   KK_SUCCESS 成功, 其它失败
 * Others:	 
 */
void config_service_restore_factory_config(bool is_all);

/**
 * Function:   
 * Description: 设置测试模式
 * Input:	   state:true——进入测试模�?false——退出测试模�?
 * Output:	   
 * Return:	  
 * Others:	   
 */
void config_service_set_test_mode(bool state);


/**
 * Function:   
 * Description: 判断当前是不是测试模�?
 * Input:	  
 * Output:	  
 * Return:	   true 测试模式 , false 非测试模�?
 * Others:	 
 */
bool config_service_is_test_mode(void);

/**
 * Function:   
 * Description: 判断当前的imei是否写死在代码中的默认imei
 * Input:	   
 * Output:	   
 * Return:	   true KK_IMEI_DEFAULT , false �?KK_IMEI_DEFAULT
 * Others:	
 */
bool config_service_is_default_imei(void);

/**
 * Function:   
 * Description: 设置设备的sim�?
 * Input:	   sim
 * Output:	   KK_SUCCESS 成功, 其它失败
 * Return:	   
 * Others:	  
 */
KK_ERRCODE config_service_set_device_sim(u8 *pdata);


/**
 * Function:   
 * Description: 改变ip地址
 * Input:	   idx:配置id,其值是(xxx.xxx.xxx.xxx:port)形式;   buf: dns  ;len: dns长度
 * Output:	   
 * Return:	   
 * Others:	   
 */
void config_service_change_ip(ConfigParamEnum idx, u8 *buf, u16 len);

/**
 * Function:   
 * Description: 改变port
 * Input:	   idx:配置id,其值是(xxx.xxx.xxx.xxx:port)形式;   port
 * Output:	 
 * Return:	  
 * Others:	   �
 */
void config_service_change_port(ConfigParamEnum idx, u16 port);


/*
    
*/
/**
 * Function:   
 * Description: 升级使用的socket类型
 * Input:     
 * Output:     
 * Return:     升级使用的socket类型
 * Others:    
 */
StreamType config_service_update_socket_type(void);

StreamType config_service_agps_socket_type(void);

KK_ERRCODE config_service_save_to_local(void);

KK_ERRCODE config_service_read_from_fs(void);

#endif


