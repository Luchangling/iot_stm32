
#include "circular_queue.h"
#include <stdlib.h>

KK_ERRCODE circular_queue_create(PCircularQueue p_queue, const U16 capacity, const KK_QUEUE_TYPE type)
{
    //多申请1个单元,留个空
	p_queue->capacity = (capacity + 1);
    if (KK_QUEUE_TYPE_FLOAT == type)
    {
        p_queue->f_buf = (float*)malloc(sizeof(float) * p_queue->capacity);
        if (NULL == p_queue->f_buf)
        {
            return KK_MEM_NOT_ENOUGH;
        }
    }
    else
    {
        p_queue->i_buf = (S32*)malloc(sizeof(S32) * p_queue->capacity);
        if (NULL == p_queue->i_buf)
        {
            return KK_MEM_NOT_ENOUGH;
        }
    }
    p_queue->head = 0;
    p_queue->tail = 0;
    return KK_SUCCESS;
}

void circular_queue_destroy(PCircularQueue p_queue, const KK_QUEUE_TYPE type)
{
    if (KK_QUEUE_TYPE_FLOAT == type)
    {
        free(p_queue->f_buf);
		p_queue->f_buf = NULL;
		p_queue->head = 0;
		p_queue->tail = 0;
    }
    else
    {
        free(p_queue->i_buf);
		p_queue->i_buf = NULL;
	    p_queue->head = 0;
		p_queue->tail = 0;
    }
}

U16 circular_queue_get_capacity(const PCircularQueue p_queue)
{
    return (p_queue->capacity - 1);
}

bool circular_queue_is_empty(const PCircularQueue p_queue)
{
    return (bool)(p_queue->head == p_queue->tail);
}

void circular_queue_empty(PCircularQueue p_queue)
{
    p_queue->head = 0;
    p_queue->tail = 0;
}

bool circular_queue_is_full(const PCircularQueue p_queue)
{
    return (bool)(p_queue->head == ((p_queue->tail + 1) % p_queue->capacity));
}

U16 circular_queue_get_len(const PCircularQueue p_queue)
{
    return ((p_queue->tail + p_queue->capacity - p_queue->head) % p_queue->capacity);
}

void circular_queue_en_queue_f(PCircularQueue p_queue, float value)
{
    //如果队列已满,队头被覆盖
    if (circular_queue_is_full(p_queue))
    {
        p_queue->head = (p_queue->head + 1) % p_queue->capacity;
    }
	
    *(p_queue->f_buf + p_queue->tail) = value;
    p_queue->tail = (p_queue->tail + 1) % p_queue->capacity;
}
void circular_queue_en_queue_i(PCircularQueue p_queue, S32 value)
{
	//如果队列已满,队头被覆盖
	if (circular_queue_is_full(p_queue))
	{
		p_queue->head = (p_queue->head + 1) % p_queue->capacity;
	}

    *(p_queue->i_buf + p_queue->tail) = value;
    p_queue->tail = (p_queue->tail + 1) % p_queue->capacity;
}

bool circular_queue_de_queue_f(PCircularQueue p_queue, float* p_value)
{
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->f_buf + p_queue->head);
        p_queue->head = (p_queue->head + 1) % p_queue->capacity;
        return true;
    }
    else
    {
        return false;
    }
}

bool circular_queue_de_queue_i(PCircularQueue p_queue, S32* p_value)
{
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->i_buf + p_queue->head);
        p_queue->head = (p_queue->head + 1) % p_queue->capacity;
        return true;
    }
    else
    {
        return false;
    }
}

bool circular_queue_get_head_f(const PCircularQueue p_queue, float* p_value)
{
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->f_buf + p_queue->head);
        return true;
    }
    else
    {
        return false;
    }
}

bool circular_queue_get_head_i(const PCircularQueue p_queue, S32* p_value)
{
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->i_buf + p_queue->head);
        return true;
    }
    else
    {
        return false;
    }
}

bool circular_queue_get_tail_f(const PCircularQueue p_queue, float* p_value)
{
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->f_buf + (p_queue->tail - 1 + p_queue->capacity) % p_queue->capacity);
        return true;
    }
    else
    {
        return false;
    }
}
bool circular_queue_get_tail_i(const PCircularQueue p_queue, S32* p_value)
{
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->i_buf + (p_queue->tail - 1 + p_queue->capacity) % p_queue->capacity);
        return true;
    }
    else
    {
        return false;
    }
}

bool circular_queue_get_by_index_f(const PCircularQueue p_queue, const U16 index, float* p_value)
{
    if (index >= circular_queue_get_capacity(p_queue) || index >= circular_queue_get_len(p_queue))
    {
        return false;
    }
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->f_buf + (p_queue->tail - 1 - index + 2 * p_queue->capacity) % p_queue->capacity);
        return true;
    }
    else
    {
        return false;
    }
}

bool circular_queue_get_by_index_i(const PCircularQueue p_queue, const U16 index, S32* p_value)
{
    if (index >= circular_queue_get_capacity(p_queue) || index >= circular_queue_get_len(p_queue))
    {
        return false;
    }
    if (!circular_queue_is_empty(p_queue))
    {
        *p_value = *(p_queue->i_buf + (p_queue->tail - 1 - index + 2 * p_queue->capacity) % p_queue->capacity);
        return true;
    }
    else
    {
        return false;
    }
}

