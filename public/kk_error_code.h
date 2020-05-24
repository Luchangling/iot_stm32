
#ifndef __KK_ERROR_CODE_H__
#define __KK_ERROR_CODE_H__

typedef enum 
{
	KK_SUCCESS = 0,             //成功
	KK_UNKNOWN = -1,            //未知错误
	KK_NOT_INIT = -2,           //未初始化
	KK_MEM_NOT_ENOUGH = -3,     //内存不足
	KK_PARAM_ERROR = -4,        //参数错误
	KK_EMPTY_BUF = -5,          //没有数据
    KK_ERROR_STATUS = -6,       //状态错误
    KK_HARD_WARE_ERROR = -7,    //硬件错误
    KK_SYSTEM_ERROR = -8,       //系统错误
    KK_WILL_DELAY_EXEC = -9,    //延迟执行
    KK_NET_ERROR = -10,         //网络错误 
    KK_TIMOUT_ERROR = -11,      //超时错误
    KK_AT_ERROR     = -12,      //AT执行错误
}KK_ERRCODE;



#endif

