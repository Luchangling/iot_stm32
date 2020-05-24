#include "gps_save.h"
#include <stdio.h>
#include <stdlib.h>
#include "service_log.h"
#include "fs_api.h"
#include "utility.h"

typedef struct  // 6460
{
    int handle; // file handle
    u32 his_data_head;
    u16 read_idx;
    u16 write_idx;
    u16 record_num;  // total records in file
}HisDatFileType;

#define CLOSE_FILE_HANDLE( handle ) \
{ \
    if((handle) >= 0) \
    { \
        FS_Close((handle)); \
        (handle) = -1; \
    } \
}

#define MAX_HIS_DATA_NUM 10000

static HisDatFileType s_his_dat; /* 对应历史文件数据 */

static LocationCurData s_gps_data = {NULL, 0, 0, 0};
static LocationCurData s_gps_sending = {NULL, 0, 0, 0};

static KK_ERRCODE gps_data_init(LocationCurData *pdata, HisDatFileType *phis, u16 size, bool read_from_file);
static u16 gps_data_get_len(LocationCurData *pdata);
static u16 gps_data_get_free_size(LocationCurData *pdata);
static KK_ERRCODE gps_data_peek_one(LocationCurData *pdata, HisDatFileType *phis, LocationSaveData *one, bool read_file);
static KK_ERRCODE gps_data_commit_peek(LocationCurData *pdata, HisDatFileType *phis,bool from_head,bool write_log);
static void gps_data_save_to_history_file(LocationCurData *pdata, HisDatFileType *phis);


static void gps_his_file_delete(HisDatFileType *phis);
static u32 gps_his_file_write_head(HisDatFileType *phis, u8 *head_buff);
static void gps_his_file_delete_record(HisDatFileType *phis, u16 del_num);
static void gps_data_release(LocationSaveData *one);
static u32 gps_his_file_write(HisDatFileType *phis, void * DataPtr, u32 Length, int Offset);
static KK_ERRCODE gps_his_file_create(HisDatFileType *phis);
static u32 gps_his_file_read(HisDatFileType *phis, void * DataPtr, u32 Length, int Offset);
static KK_ERRCODE gps_data_save_file(LocationCurData *pdata, HisDatFileType *phis);
static KK_ERRCODE gps_data_read_file(LocationCurData *pdata, HisDatFileType *phis,bool read_from_file);

/*add to tail*/
static KK_ERRCODE gps_data_insert_one(LocationCurData *pdata, HisDatFileType *phis, u8 *data, u8 len, bool log);



KK_ERRCODE gps_data_init(LocationCurData *pdata, HisDatFileType *phis, u16 size, bool read_from_file)
{
    // the last block have to empty to keep read==write means empty.
    size +=1;
    
	/* allocate gps space. */
	pdata->his = (LocationSaveData *) malloc(size * sizeof(LocationSaveData));
	if (pdata->his == NULL)
	{
        LOG(LEVEL_WARN,"gps_data_init(%p,%p) assert(malloc(%d) failed", pdata,phis, size * sizeof(LocationSaveData));
		return KK_MEM_NOT_ENOUGH;
	}
	
    memset((u8 *)pdata->his, 0x00, sizeof(LocationSaveData) * size);

	pdata->read_idx = 0;
	pdata->write_idx = 0;
	pdata->size = size;

    if(phis)
    {
        phis->handle = -1;
        phis->record_num = 0;

        //if file exist, read from history file.
        gps_data_read_file(pdata,phis,read_from_file);
    }

	return KK_SUCCESS;
}


u16 gps_data_get_len(LocationCurData *pdata)
{
    int read_point = 0;
    int write_point = 0;
    int LeftDataSize = 0;
	
    read_point = pdata->read_idx;
    write_point = pdata->write_idx;

    LeftDataSize = write_point - read_point;
    if(LeftDataSize < 0)
    {
        LeftDataSize += pdata->size;
    }
    
    return (u16)LeftDataSize;
}


u16 gps_data_get_free_size(LocationCurData *pdata)
{
    // the last block have to empty to keep read==write means empty.
    return (pdata->size - gps_data_get_len(pdata)) - 1;
}


void gps_data_save_to_history_file(LocationCurData *pdata, HisDatFileType *phis)
{
    if(gps_data_get_len(pdata) > 0)
    {
        gps_data_save_file(pdata, phis);
    }
}



KK_ERRCODE gps_data_insert_one(LocationCurData *pdata, HisDatFileType *phis, u8 *data, u8 len, bool log)
{
    //int read_point = 0;
    int write_point = 0;
    int write_idx;
	
    //read_point = pdata->read_idx;
    write_idx = write_point = pdata->write_idx;

    pdata->his[write_idx].buf = (u8 *)malloc(len);
    if(NULL == pdata->his[write_idx].buf)
    {
        LOG(LEVEL_WARN,"gps_data_init(%p,%p) assert(malloc(%d) failed",pdata, phis, len);
        return KK_SYSTEM_ERROR;
    }
    
    pdata->his[write_idx].len = len;
    memcpy(pdata->his[write_idx].buf, data, len);

    write_point ++;

    if(write_point >= pdata->size)
    {
        write_point -= (int)pdata->size;
    }

    pdata->write_idx = write_point;

    if(log)  // when read form file , no need log
    {
        LOG(LEVEL_DEBUG,"gps_data_insert_one(%p,%p)(r:%d,w:%d,size:%d, len:%d) his_num(%d).", 
            pdata,phis,pdata->read_idx,pdata->write_idx,pdata->size,len,(phis?phis->record_num:0));
    }
    return KK_SUCCESS;
}


KK_ERRCODE gps_data_peek_one(LocationCurData *pdata, HisDatFileType *phis, LocationSaveData *one, bool read_file)
{
    int read_point = 0;
    int write_point = 0;
    //int idx;

    if(pdata->his == NULL)
    {
        return KK_EMPTY_BUF;
    }

	
    read_point = pdata->read_idx;
    write_point = pdata->write_idx;

    
    if(write_point == read_point)
    {     
        if(phis == NULL) return KK_NOT_INIT;

        if(read_file && (KK_SUCCESS == gps_data_read_file(pdata, phis, true)))
        {
            read_point = pdata->read_idx;
            write_point = pdata->write_idx;
        }
        else
        {
            return KK_EMPTY_BUF;
        }

    }

    //data is between read_idx(include) and write_idx(not include)
    *one = pdata->his[read_point];

    read_point++;

    if(read_point >= pdata->size)
    {
        read_point -= (int)pdata->size;
    }

    //pdata->read_idx = read_point;
    
    return KK_SUCCESS;
}


KK_ERRCODE gps_data_commit_peek(LocationCurData *pdata, HisDatFileType *phis,bool from_head,bool write_log)
{
    int read_point = 0;
    int write_point = 0;
    int idx = 0;
	int len = 0;
    
    read_point = pdata->read_idx;
    write_point = pdata->write_idx;

    if(write_point == read_point)
    {
        return KK_EMPTY_BUF;
    }

    //if(from_head)
    {
        //删除头部一个元素
        idx = read_point;
        len = pdata->his[idx].len;
        gps_data_release(&pdata->his[idx]);
        read_point ++;
        if(read_point >= pdata->size)
        {
            read_point -= (int)pdata->size;
        }
        pdata->read_idx = read_point;
    }
    
    if(write_log)
    {
        LOG(LEVEL_DEBUG,"gps_data_commit_peek(%p,%p)(r:%d,w:%d,size:%d, len:%d, from_head:%d) his_num(%d).", 
            pdata,phis,pdata->read_idx,pdata->write_idx,pdata->size,len,from_head,(phis?phis->record_num:0));
    }
    return KK_SUCCESS;
}




static void gps_data_release(LocationSaveData *one)
{
    if(one && one->buf)
    {
        free(one->buf);
        one->buf = NULL;
        one->len = 0;
    }
}

void gps_his_file_delete(HisDatFileType *phis)
{
    int ret = -1;

    CLOSE_FILE_HANDLE(phis->handle)

    ret = FS_CheckFile(KK_HIS_FILE);
    if (ret >= 0)
    {
        FS_Delete(KK_HIS_FILE);
        LOG(LEVEL_INFO,"gps_his_file_delete", util_clock());
    }
}

/*
文件没打开, 则打开文件, 并写入数据, 文件不会被关闭. 
出错的情况下, 文件会被关闭
*/
static u32 gps_his_file_write(HisDatFileType *phis, void * DataPtr, u32 Length, int Offset)
{
    u32 fs_len = 0;
    int ret = -1;

    if(phis->handle < 0)
    {
        phis->handle = FS_Open(KK_HIS_FILE, 0);
        if (phis->handle < 0)
        {
            LOG(LEVEL_INFO,"gps_his_file_write-open fail return[%d]", phis->handle);
            phis->handle = -1;
            return 0;
        }
        LOG(LEVEL_DEBUG,"gps_his_file_write open or create file");
    }

    FS_Seek(phis->handle, Offset, 0);
    ret = FS_Write(phis->handle, DataPtr, Length, &fs_len);
    if (ret < 0)
    {
        LOG(LEVEL_INFO,"goome_his_file_write-write fail return[%d]", ret);
        CLOSE_FILE_HANDLE(phis->handle)
        return 0;
    }

    FS_Commit(phis->handle);

    return fs_len;
}


/*文件一定会被删除, handle一定会被关掉, 再重新打开*/
static KK_ERRCODE gps_his_file_create(HisDatFileType *phis)
{
    u8 file_head[20];

    gps_his_file_delete(phis);

    memset(file_head, 0x00, sizeof(file_head));
    file_head[0] = (HIS_FILE_HEAD_FLAG >> 24) & 0xFF;
    file_head[1] = (HIS_FILE_HEAD_FLAG >> 16) & 0xFF;
    file_head[2] = (HIS_FILE_HEAD_FLAG >> 8) & 0xFF;
    file_head[3] = (HIS_FILE_HEAD_FLAG ) & 0xFF;

    if (HIS_FILE_OFFSET_CNT == gps_his_file_write(phis, (void *)&file_head, HIS_FILE_OFFSET_CNT, 0))
    {
        LOG(LEVEL_INFO,"gps_his_file_create ok");
        phis->record_num = 0;
        return KK_SUCCESS;
    }
    else
    {
        LOG(LEVEL_INFO,"gps_his_file_create failed");
        CLOSE_FILE_HANDLE(phis->handle)
        return KK_SYSTEM_ERROR;
    }
}


static u32 gps_his_file_read(HisDatFileType *phis, void * DataPtr, u32 Length, int Offset)
{
    u32 fs_len = 0;
    int ret = -1;

    //return Length;
    
    if(phis->handle < 0)
    {
        phis->handle = FS_Open(KK_HIS_FILE, 0);
        if (phis->handle < 0)
        {
            LOG(LEVEL_DEBUG,"gps_his_file_read open failed, return [%d]",phis->handle);
            phis->handle = -1;
            return 0;
        }
        LOG(LEVEL_DEBUG,"gps_his_file_read open file");
    }

    FS_Seek(phis->handle, Offset, 0);
    ret = FS_Read(phis->handle, DataPtr, Length, &fs_len);
    if (ret < 0)
    {
        LOG(LEVEL_INFO,"gps_his_file_read failed, ret[%d]",ret);
        CLOSE_FILE_HANDLE(phis->handle)
        return 0;
    }

    FS_Commit(phis->handle);

    return fs_len;
}


/*将pdata缓存中的内容添加到文件后面*/
static KK_ERRCODE gps_data_save_file(LocationCurData *pdata, HisDatFileType *phis)
{
    u8 head_buff[20];
    u8 one_frame[HIS_FILE_FRAME_SIZE];
    LocationSaveData one;
    u16 fs_len = 0;
    u16 write_len = 0;

    //return KK_SUCCESS;

    memset(head_buff, 0x00, sizeof(head_buff));
    fs_len = gps_his_file_read(phis, (void *)&head_buff, HIS_FILE_OFFSET_CNT, 0);
    if ((HIS_FILE_OFFSET_CNT != fs_len) || (HIS_FILE_HEAD_FLAG != MKDWORD(head_buff[0], head_buff[1], head_buff[2], head_buff[3])))
    {
        if (KK_SUCCESS != gps_his_file_create(phis))
        {
            //创建文件失败
            return KK_SYSTEM_ERROR;
        }
    }
    else
    {
        phis->read_idx   = MKWORD(head_buff[4], head_buff[5]);
        phis->write_idx  = MKWORD(head_buff[6], head_buff[7]);
        phis->record_num = MKWORD(head_buff[8], head_buff[9]);
        LOG(LEVEL_INFO,"phis read %d,write %d record %d",phis->read_idx,phis->write_idx,phis->record_num);
    }

    while(KK_SUCCESS == gps_data_peek_one(pdata, phis, &one, false))
    { 
        one_frame[0] = one.len;
        if(one.len >= HIS_FILE_FRAME_SIZE)
        {
            LOG(LEVEL_ERROR,"gps_data_save_file assert(len(%d)) failed.", one.len);
            break;
        }
        
        memcpy(&one_frame[1], one.buf,one.len);
        write_len = gps_his_file_write(phis, one_frame, HIS_FILE_FRAME_SIZE, phis->write_idx*HIS_FILE_FRAME_SIZE + HIS_FILE_OFFSET_CNT);
        if(write_len == HIS_FILE_FRAME_SIZE)
        {
            phis->record_num++;
            if(phis->record_num > MAX_HIS_DATA_NUM)phis->record_num = MAX_HIS_DATA_NUM;
            phis->write_idx = (phis->write_idx+1)%MAX_HIS_DATA_NUM;
            gps_data_commit_peek(pdata, phis, true, true);
        }
        else
        {
            LOG(LEVEL_WARN,"gps_data_save_file assert(write) failed, ret[%d]",write_len);
            break;
        }
    }

    gps_his_file_write_head(phis, head_buff);

    CLOSE_FILE_HANDLE(phis->handle)

    LOG(LEVEL_DEBUG,"gps_data_save_file his_data(num:%d)",  phis->record_num);

    return KK_SUCCESS;
}


/*将文件中的内容读到pdata缓存, 如果缓存满了, 剩余的文件中的记录前移*/
static KK_ERRCODE gps_data_read_file(LocationCurData *pdata, HisDatFileType *phis, bool read_from_file)
{
    u8 head_buff[20];
    u8 one_frame[HIS_FILE_FRAME_SIZE];
    u16 fs_len = 0;
    //u16 write_len = 0;
    int idx = 0;
    int new_idx = 0;

    memset(head_buff, 0x00, sizeof(head_buff));
    
    //避免重复读取
    if(phis->his_data_head != HIS_FILE_HEAD_FLAG)
    {
        fs_len = gps_his_file_read(phis, (void *)&head_buff, HIS_FILE_OFFSET_CNT, 0);
        if ((HIS_FILE_OFFSET_CNT != fs_len) || (HIS_FILE_HEAD_FLAG != MKDWORD(head_buff[0], head_buff[1], head_buff[2], head_buff[3])))
        {
            //record_num means records in history file.
            phis->his_data_head = HIS_FILE_HEAD_FLAG;//MKDWORD(head_buff[0], head_buff[1], head_buff[2], head_buff[3]);
            phis->read_idx      = 0;
            phis->write_idx     = 0;
            phis->record_num = 0;
            gps_his_file_write_head(phis, head_buff);

            CLOSE_FILE_HANDLE(phis->handle)

            LOG(LEVEL_DEBUG,"gps_data_read_file no history file exist. read_len(%d).", fs_len);

            
            return KK_SYSTEM_ERROR;
        }
        else
        {
            phis->his_data_head = MKDWORD(head_buff[0], head_buff[1], head_buff[2], head_buff[3]);
            phis->read_idx      = MKWORD(head_buff[4], head_buff[5]);
            phis->write_idx     = MKWORD(head_buff[6], head_buff[7]);
            phis->record_num    = MKWORD(head_buff[8], head_buff[9]);
            
        }
    }
    
    if(phis->his_data_head == HIS_FILE_HEAD_FLAG && phis->record_num == 0) return KK_EMPTY_BUF;

    if(!read_from_file)
    {
        // only read head. to get record_num
        LOG(LEVEL_INFO,"gps_data_read_file only head(num:%d)",  phis->record_num);
        return KK_SUCCESS;
    }

    for(idx = 0; idx < phis->record_num; ++idx)
    {
        fs_len = gps_his_file_read(phis, (void *)&one_frame[0], HIS_FILE_FRAME_SIZE, HIS_FILE_OFFSET_CNT + phis->read_idx * HIS_FILE_FRAME_SIZE);
        if(fs_len != HIS_FILE_FRAME_SIZE)
        {
            LOG(LEVEL_WARN,"gps_data_read_file assert(read) failed, ret[%d]",fs_len);
            break;
        }
        if(gps_data_get_free_size(pdata) > 0)
        {
            gps_data_insert_one(pdata, phis, &one_frame[1], one_frame[0], false);

            new_idx++;
        }
        else
        {
            break;
        }

        phis->read_idx = (phis->read_idx + 1)%MAX_HIS_DATA_NUM;
        
    }

    phis->record_num = phis->record_num - new_idx;
    
    gps_his_file_write_head(phis, head_buff);

    CLOSE_FILE_HANDLE(phis->handle)

    LOG(LEVEL_INFO,"gps_data_read_file his_data(num:%d)",  phis->record_num);

    return KK_SUCCESS;
}


static u32 gps_his_file_write_head(HisDatFileType *phis, u8 *head_buff)
{
    head_buff[0] = (HIS_FILE_HEAD_FLAG >> 24) & 0xFF;
    head_buff[1] = (HIS_FILE_HEAD_FLAG >> 16) & 0xFF;
    head_buff[2] = (HIS_FILE_HEAD_FLAG >> 8) & 0xFF;
    head_buff[3] = (HIS_FILE_HEAD_FLAG) & 0xFF;

    head_buff[4] = (phis->read_idx >> 8) & 0xFF;
    head_buff[5] = (phis->read_idx) & 0xFF;
    head_buff[6] = (phis->write_idx >> 8) & 0xFF;
    head_buff[7] = (phis->write_idx) & 0xFF;
    head_buff[8] = (phis->record_num >> 8) & 0xFF;
    head_buff[9] = (phis->record_num) & 0xFF;
    return gps_his_file_write(phis, head_buff, HIS_FILE_OFFSET_CNT, 0);
}


static void gps_his_file_delete_record(HisDatFileType *phis, u16 del_num)
{
    u8 head_buff[20];
    u8 one_frame[HIS_FILE_FRAME_SIZE];
    u16 fs_len = 0;
    u16 write_len = 0;
    int idx = 0;
    int new_idx = 0;

    if(phis->record_num <= 0)
    {
        LOG(LEVEL_ERROR,"gps_his_file_delete_record. assert(record_num(%d)) failed.", phis->record_num);
        return;
    }
    if(phis->handle < 0)
    {
        LOG(LEVEL_ERROR,"gps_his_file_delete_record. assert(handle(%d)) failed.", phis->handle);
        return;
    }

    for(idx = 0; idx < phis->record_num; ++idx)
    {
        fs_len = gps_his_file_read(phis, (void *)&one_frame[0], HIS_FILE_FRAME_SIZE, HIS_FILE_OFFSET_CNT + idx * HIS_FILE_FRAME_SIZE);
        if(fs_len != HIS_FILE_FRAME_SIZE)
        {
            LOG(LEVEL_WARN,"gps_data_read_file assert(read) failed, ret[%d]",fs_len);
            break;
        }
        if(idx >= del_num)
        {
            write_len = gps_his_file_write(phis, one_frame, HIS_FILE_FRAME_SIZE, new_idx*HIS_FILE_FRAME_SIZE + HIS_FILE_OFFSET_CNT);
            if(write_len == HIS_FILE_FRAME_SIZE)
            {
                new_idx ++;
            }
            else
            {
                LOG(LEVEL_WARN,"gps_his_file_delete_record assert(write) failed, ret[%d]",write_len);
                break;
            }
        }
    }

    phis->record_num = new_idx;
    gps_his_file_write_head(phis, head_buff);

    LOG(LEVEL_INFO,"gps_his_file_delete_record(%d) his_data(num:%d)", del_num, phis->record_num);

    return;
}


KK_ERRCODE gps_service_save_to_cache(u8 *data, u8 len)
{
    if(s_gps_sending.his == NULL)
    {
        //gps_data_init(&s_gps_sending, NULL, SAVE_CATCH_MAX_NUM, false); 

        s_gps_sending.his = (LocationSaveData *) malloc((SAVE_CATCH_MAX_NUM + 1) * sizeof(LocationSaveData));
    	if (s_gps_sending.his == NULL)
    	{
            LOG(LEVEL_WARN,"gps_service_save_to_cache assert(malloc failed");
    		return KK_MEM_NOT_ENOUGH;
    	}
    	
        memset((u8 *)s_gps_sending.his, 0x00, sizeof(LocationSaveData) * (SAVE_CATCH_MAX_NUM + 1));

    	s_gps_sending.read_idx = 0;
    	s_gps_sending.write_idx = 0;
    	s_gps_sending.size = (SAVE_CATCH_MAX_NUM + 1);
    }
    
    if(gps_data_get_free_size(&s_gps_sending) == 0)
    {
        return KK_MEM_NOT_ENOUGH;
    }

    return gps_data_insert_one(&s_gps_sending, NULL, data, len, true);
}

KK_ERRCODE gps_service_cache_peek_one(LocationSaveData *one, bool from_head)
{
    if(s_gps_sending.his == NULL)
    {
        //gps_data_init(&s_gps_sending, NULL, SAVE_CATCH_MAX_NUM, false); 

        s_gps_sending.his = (LocationSaveData *) malloc((SAVE_CATCH_MAX_NUM + 1) * sizeof(LocationSaveData));
    	if (s_gps_sending.his == NULL)
    	{
            LOG(LEVEL_WARN,"gps_service_save_to_cache assert(malloc failed");
    		return KK_MEM_NOT_ENOUGH;
    	}
    	
        memset((u8 *)s_gps_sending.his, 0x00, sizeof(LocationSaveData) * (SAVE_CATCH_MAX_NUM + 1));

    	s_gps_sending.read_idx = 0;
    	s_gps_sending.write_idx = 0;
    	s_gps_sending.size = (SAVE_CATCH_MAX_NUM + 1);
    }
    
    return gps_data_peek_one(&s_gps_sending, NULL, one, false);
}

KK_ERRCODE gps_service_cache_commit_peek(bool from_head)
{
    return gps_data_commit_peek(&s_gps_sending, NULL, from_head, true);
}


void gps_service_confirm_cache(u32 *from, u32 ack_place)
{
    u32 confirm_len = (ack_place - (*from));
    u32 confirm_total = 0;
    LocationSaveData one;

    if(s_gps_sending.his == NULL)
    {
        //gps_data_init(&s_gps_sending, NULL, SAVE_CATCH_MAX_NUM, false);
        return;
    }
    
    if(0 == gps_data_get_len(&s_gps_sending))
    {
        (*from) = ack_place;
        return;
    }

    if((*from == 0) && (ack_place == 0))
    {
        ack_place = 1000000;   //sdk还没支持KK_GetTcpStatus ， 直接confirm 所有数据, 最后, (*from) = ack_place = 0;
        LOG(LEVEL_DEBUG,"gps_service_confirm_cache(%d,%d) cache_data(num:%d)", *from, ack_place, gps_data_get_len(&s_gps_sending));
    }
    
    while(KK_SUCCESS == gps_data_peek_one(&s_gps_sending, NULL, &one, false))
    {
        confirm_total += one.len;
        if(confirm_total <= confirm_len)
        {
            LOG(LEVEL_DEBUG,"gps_service_confirm_cache(%d,%d) (%d < %d)", *from, ack_place, confirm_total, confirm_len);
            gps_data_commit_peek(&s_gps_sending, NULL, true, true);
            (*from) += one.len;
        }
        else
        {
            break;
        }
    }

    if(0 == gps_data_get_len(&s_gps_sending))
    {
        (*from) = ack_place;
    }
}



KK_ERRCODE gps_service_push_to_stack(u8 *data, u8 len)
{
    if(s_gps_data.his == NULL)
    {
        gps_data_init(&s_gps_data, &s_his_dat, SAVE_HIS_MAX_NUM/3, false);  
    }

    if(gps_data_get_free_size(&s_gps_data) == 0)
    {
        LOG(LEVEL_INFO,"gps_data overflow! write to flash");
        gps_data_save_file(&s_gps_data, &s_his_dat);
    }

    return gps_data_insert_one(&s_gps_data, &s_his_dat, data, len, true);
}

KK_ERRCODE gps_service_peek_one(LocationSaveData *one, bool from_head)
{
    if(s_gps_data.his == NULL)
    {
        gps_data_init(&s_gps_data, &s_his_dat, SAVE_HIS_MAX_NUM/3, true);  
    }

    return gps_data_peek_one(&s_gps_data, &s_his_dat, one, true);
}

KK_ERRCODE gps_service_commit_peek(bool from_head)
{
    return gps_data_commit_peek(&s_gps_data, &s_his_dat, from_head, true);
}

void gps_service_his_file_delete(void)
{
    gps_his_file_delete(&s_his_dat);
}

void gps_service_save_to_history_file(void)
{
    LocationSaveData one;
    while(gps_data_get_len(&s_gps_sending) > 0)
    {
        one.len = 0;
        gps_data_peek_one(&s_gps_sending, NULL, &one, false);
        if(one.len > 0)
        {
            gps_service_push_to_stack(one.buf, one.len);
        }
        gps_data_commit_peek(&s_gps_sending, NULL, true, true);
    }
    
    gps_data_save_to_history_file(&s_gps_data, &s_his_dat);
}


