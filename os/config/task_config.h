#ifndef __TASK_CONFIG_H__
#define __TASK_CONFIG_H__

/*系统任务配置信息*/

/*Start Task*/
//设置任务优先级
#define START_TASK_PRIO      			10 //开始任务的优先级设置为最低
//设置任务堆栈大小
#define START_STK_SIZE  				1024/4




/*UART task*/
#define EXT_DEVICE_TASK_PRIO       		 	6
//设置任务堆栈大小
#define EXT_DEVICE_STK_SIZE  		        512/4


/*NET Task*/
#define NET_TASK_PRIO       			3 
//设置任务堆栈大小
#define NET_STK_SIZE  					1024

/*AT TASK*/
#define AT_TASK_PRIO       			    2 
//设置任务堆栈大小
#define AT_STK_SIZE  					512/4


#endif


