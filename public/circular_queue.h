
#ifndef __CIRCULAR_QUEUE_H__
#define __CIRCULAR_QUEUE_H__

#include "kk_type.h"
#include "kk_error_code.h"

typedef enum
{
	KK_QUEUE_TYPE_INT = 0,
    KK_QUEUE_TYPE_FLOAT = 1
}KK_QUEUE_TYPE;

//使用者不要直接使用结构体成员变量
typedef struct
{
    float* f_buf;
    S32* i_buf;
	
	//指向第一个元素
    U16 head;

	//指向最后一个元素的下一个位置
    U16 tail;

	//队列容量
    U16 capacity;
} CircularQueue, *PCircularQueue;

/**
 * Function:   创建CircularQueue
 * Description:不要直接使用结构体内的成员变量 
 * Input:	   capacity:队列容量（单元个数,不是字节数）；type:整型或者浮点型
 * Output:	   p_queue:指向队列的指针
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   使用前必须调用,否则调用其它接口返回失败错误码
 */
KK_ERRCODE circular_queue_create(PCircularQueue p_queue, const U16 capacity, const KK_QUEUE_TYPE type); 

/**
 * Function:   销毁CircularQueue
 * Description:不要直接使用结构体内的成员变量 
 * Input:	   p_queue:指向队列的指针；type:整型或者浮点型
 * Output:	   p_queue:指向队列的指针
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
void circular_queue_destroy(PCircularQueue p_queue, KK_QUEUE_TYPE type);

/**
 * Function:   获取队列的容量
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针 
 * Output:	   无
 * Return:	   队列的容量
 * Others:	   
 */
U16 circular_queue_get_capacity(const PCircularQueue p_queue);

/**
 * Function:   队列是否为空
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针 
 * Output:	   无
 * Return:	   true——空;false——不空
 * Others:	   
 */
bool circular_queue_is_empty(const PCircularQueue p_queue);

/**
 * Function:   清空队列（注意不是销毁）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针 
 * Output:	   p_queue:指向队列的指针
 * Return:	   无
 * Others:	   
 */
void circular_queue_empty(PCircularQueue p_queue);

/**
 * Function:   队列是否已满
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针 
 * Output:	   无
 * Return:	   true——满;false——未满
 * Others:	   
 */
bool circular_queue_is_full(const PCircularQueue p_queue);

/**
 * Function:   获取队列长度
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针 
 * Output:	   无
 * Return:	   队列长度
 * Others:	   
 */
U16 circular_queue_get_len(const PCircularQueue p_queue);

/**
 * Function:   浮点型数据入队
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针；value:入队的数据
 * Output:	   p_queue:指向队列的指针
 * Return:	   无
 * Others:	   如果队列已满,会覆盖队头数据
 */
void circular_queue_en_queue_f(PCircularQueue p_queue, float value);

/**
 * Function:   整型数据入队
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针；value:入队的数据
 * Output:	   p_queue:指向队列的指针
 * Return:	   无
 * Others:	   如果队列已满,会覆盖队头数据
 */
void circular_queue_en_queue_i(PCircularQueue p_queue, S32 value);

/**
 * Function:   浮点型数据出队
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针；p_value:出队的数据指针
 * Output:	   p_queue:指向队列的指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_de_queue_f(PCircularQueue p_queue, float* p_value);

/**
 * Function:   整型数据出队
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针；p_value:出队的数据指针
 * Output:	   p_queue:指向队列的指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_de_queue_i(PCircularQueue p_queue, S32* p_value);

/**
 * Function:   获取队头元素（浮点型）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针
 * Output:	   p_value:数据指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_get_head_f(const PCircularQueue p_queue, float* p_value);

/**
 * Function:   获取队头元素（整型）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针
 * Output:	   p_value:数据指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_get_head_i(const PCircularQueue p_queue, S32* p_value);

/**
 * Function:   获取队尾元素（浮点型）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针
 * Output:	   p_value:数据指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_get_tail_f(const PCircularQueue p_queue, float* p_value);

/**
 * Function:   获取队尾元素（整型）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针
 * Output:	   p_value:数据指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_get_tail_i(const PCircularQueue p_queue, S32* p_value);

/**
 * Function:   获取第index（从0开始）个元素（浮点型）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针
 * Output:	   p_value:数据指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_get_by_index_f(const PCircularQueue p_queue, const U16 index, float* p_value);

/**
 * Function:   获取第index（从0开始）个元素（整型）
 * Description:不要直接使用结构体内的成员变量
 * Input:	   p_queue:指向队列的指针
 * Output:	   p_value:数据指针
 * Return:	   true——成功；false——失败（队列空)
 * Others:	   
 */
bool circular_queue_get_by_index_i(const PCircularQueue p_queue, const U16 index, S32* p_value);

#endif

