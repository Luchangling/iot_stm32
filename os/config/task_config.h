#ifndef __TASK_CONFIG_H__
#define __TASK_CONFIG_H__

/*ϵͳ����������Ϣ*/

/*Start Task*/
//�����������ȼ�
#define START_TASK_PRIO      			10 //��ʼ��������ȼ�����Ϊ���
//���������ջ��С
#define START_STK_SIZE  				1024/4




/*UART task*/
#define EXT_DEVICE_TASK_PRIO       		 	6
//���������ջ��С
#define EXT_DEVICE_STK_SIZE  		        512/4


/*NET Task*/
#define NET_TASK_PRIO       			3 
//���������ջ��С
#define NET_STK_SIZE  					1024

/*AT TASK*/
#define AT_TASK_PRIO       			    2 
//���������ջ��С
#define AT_STK_SIZE  					512/4


#endif


