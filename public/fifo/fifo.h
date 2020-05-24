/**
 */

#ifndef __FILO_H__
#define __FILO_H__

#include "kk_type.h"
#include "kk_error_code.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIFO_MSG_LEN 4096

typedef struct
{
    u32 read_idx;
    u32 write_idx;
	u32 size;   /*  total size  */
	u32 msg_count;
    u8* base_addr;
}FifoType;


KK_ERRCODE fifo_init(FifoType * fifo, u32 size);
KK_ERRCODE fifo_reset(FifoType * fifo);
KK_ERRCODE fifo_delete(FifoType * fifo);

u32 fifo_get_msg_length(FifoType * fifo);
u32 fifo_get_left_space(FifoType * fifo);

KK_ERRCODE fifo_insert(FifoType * fifo, u8 *data, u32 len);
KK_ERRCODE fifo_retrieve(FifoType * fifo, u8 *data, u32 *len_p);
KK_ERRCODE fifo_peek(FifoType * fifo, u8 *data, u32 len);
KK_ERRCODE fifo_pop_len(FifoType * fifo, u32 len);

//取出最大长度为*len_p的数据
KK_ERRCODE fifo_peek_and_get_len(FifoType * fifo, u8 *data, u32 *len_p);


//取出最大长度为*len_p的数据,遇到split_by就停止,返回的数据包含until_char
KK_ERRCODE fifo_peek_until(FifoType* fifo, u8* data, u16* len_p, const u8 until_char);
#endif



