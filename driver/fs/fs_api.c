/*
    //文件系统的API
*/
#include "kk_os_header.h"
#include "fs_api.h"
#include "spi_flash_board_config.h"

enum 
{
    MAN_PARA_START = 0,
    MAN_PARA_SIZE = 0x1000,
    MAN_PARA_END  = MAN_PARA_START + MAN_PARA_SIZE,

    MINOR_PARA_START = MAN_PARA_END, //0x1000
    MINOR_PARA_SIZE  = 0x1000,
    MINOR_PARA_END   = MINOR_PARA_START + MINOR_PARA_SIZE, 

    HIS_FILE_START   = MINOR_PARA_END, //0x2000
    HIS_FILE_SIZE    = 0x200000,
    HIS_FILE_END     = HIS_FILE_START + HIS_FILE_SIZE,

    STA_FILE_START   = HIS_FILE_END,   //0x202000
    STA_FILE_SIZE    = 0x1000,
    STA_FILE_END     = STA_FILE_START + STA_FILE_SIZE,

    AGPS_FILE_START  = STA_FILE_END,   //0x203000
    AGPS_FILE_SIZE   = 0x80000,
    AGPS_FILE_END    = AGPS_FILE_START + AGPS_FILE_SIZE,

    UNDEF_FILE_START = AGPS_FILE_END,  //0x283000
    UNDEF_FILE_SIZE  = 0x100000,
    UNDEF_FILE_END   = UNDEF_FILE_START + UNDEF_FILE_SIZE,

    SYS_FILE_INFO_START = UNDEF_FILE_END,  //0x383000
    SYS_FILE_INFO_SIZE  = 0x1000,         
    SYS_FILE_INFO_END   = SYS_FILE_INFO_START + SYS_FILE_INFO_SIZE,

    SYS_FILE_START   = SYS_FILE_INFO_END,  //0x384000
    SYS_FILE_SIZE    = 0x100000,
    SYS_FILE_END     = SYS_FILE_START + SYS_FILE_SIZE,

    EXFILE_INFO_START  = SYS_FILE_START,   //0x484000
    EXFILE_INFO_SIZE   = 0x1000,
    EXFILE_INFO_END    = EXFILE_INFO_START + EXFILE_INFO_SIZE,


    EXFILE_START     = EXFILE_INFO_START,  //0x485000
    EXFILE_SIZE      = 0x100000,
    EXFILE_END       = EXFILE_START + EXFILE_SIZE,

    
};

typedef struct
{
   const u8  *f_name;
   const u32 start_adr;
   const u32 size;
   const u32 end_adr;
   u32 offset;
   u8  opend;
   s32 write_size;
}File_Opration_Struct;

#define MAX_FILE_CONUTS 10

File_Opration_Struct s_fop[MAX_FILE_CONUTS] = {
    {
        MAN_PARAM_NAME,
        MAN_PARA_START,
        MAN_PARA_SIZE,
        MAN_PARA_END,
        0,
        0
    },
    {
        BKP_PARAM_NAME,
        MINOR_PARA_START,
        MINOR_PARA_SIZE,
        MINOR_PARA_END,
        0,
        0
    },
    {
        HIS_FILE_NAME,
        HIS_FILE_START,
        HIS_FILE_SIZE,
        HIS_FILE_END,
        0,
        0
    },
    {
        STA_FILE_NAME,
        STA_FILE_START,
        STA_FILE_SIZE,
        STA_FILE_END,
        0,
        0
    },
    {
        AGPS1_FILE_NAME,
        AGPS_FILE_START,
        AGPS_FILE_SIZE,
        AGPS_FILE_END,
        0,
        0
    },
    {
        NULL_FILE_NAME,
        UNDEF_FILE_START,
        UNDEF_FILE_SIZE,
        UNDEF_FILE_END,
        0,
        0
    },
    {
        SYS_INFO_NAME,
        SYS_FILE_INFO_START,
        SYS_FILE_INFO_SIZE,
        SYS_FILE_INFO_END,
        0,
        0
    },
    {
        SYS_FILE_NAME,
        SYS_FILE_START,
        SYS_FILE_SIZE,
        SYS_FILE_END,
        0,
        0
    },
    {
        EX_INFO_NAME,
        EXFILE_INFO_START,
        EXFILE_INFO_SIZE,
        EXFILE_INFO_END,
        0,
        0
    },
    {
        EX_FILE_NAME,
        EXFILE_START,
        EXFILE_SIZE,
        EXFILE_END,
        0,
        0
    }
    
};

UFSErrorEnum normalization_file_errorcode(s32 result)
{
    s32 res = 0;
    
    if(result > 400)
    {
        res = (result - 400)*(-1);
    }
    else if(result == 400)
    {
        res = -1;
    }
    else
    {
        res = UFS_FILE_ALREADY_OPEN;
    }

    return (UFSErrorEnum)res;
}

u8 fs_get_file_id(const u8 * FileName)
{
    u8 i = 0;
    
    for(i = 0 ; i < MAX_FILE_CONUTS ; i++)
    {
        if(strcmp((const char*)s_fop[i].f_name , (const char*)FileName) == 0)
        {
           return i;
        }
    }

    return 0xFF;
}

int FS_Open(const u8 * FileName, u32 Flag)
{
    u8 i = 0;
    int handle = -1;

    i = fs_get_file_id(FileName);

    if(i < MAX_FILE_CONUTS)
    {
        if(!s_fop[i].opend)
        {
            s_fop[i].opend = 1;

            s_fop[i].offset= 4;

            SPI_Flash_Read((u8 *)&s_fop[i].write_size, s_fop[i].start_adr , 4);
            
            handle = i;

            LOG(LEVEL_DEBUG,"file name %s opend! len %d",s_fop[i].f_name,s_fop[i].write_size);
        }
        else
        {
            handle = -1;
        }
    }


    LOG(LEVEL_DEBUG,"file handle %d",handle);

    return handle;
}

int FS_Close(int FileHandle)
{
    u32 len = 0;
    
    int handle = UFS_INVALID_VALUE;

    if(FileHandle < MAX_FILE_CONUTS)
    {
        //LOG(DEBUG,"clock(%d) file name %s close! len %d ",clock(),s_fop[FileHandle].f_name,s_fop[FileHandle].write_size);

        if(s_fop[FileHandle].opend)
        {
            s_fop[FileHandle].opend = 0;

            s_fop[FileHandle].offset = 0;


            SPI_Flash_Read((u8 *)&len,s_fop[FileHandle].start_adr,4);

            if(len != s_fop[FileHandle].write_size)
            {
                SPI_Flash_Write((u8 *)&s_fop[FileHandle].write_size,s_fop[FileHandle].start_adr,4);
            }


            handle = UFS_SUCCESS;
        }
        

    }

    return handle;
    
}



int FS_Read(int FileHandle, void * DataPtr, u32 Length, u32 * Read)
{
    u32 rlen = 0;
    u32 addr = 0;
    
    if(FileHandle < MAX_FILE_CONUTS)
    {
        if(!s_fop[FileHandle].opend)return UFS_FAIL_TO_OPEN;
        
        rlen = Length;

        if(Length > s_fop[FileHandle].size)
        {
            rlen = s_fop[FileHandle].size;
        }

        addr = s_fop[FileHandle].start_adr + s_fop[FileHandle].offset;

        if((s_fop[FileHandle].offset +  rlen  - 4)  > (s_fop[FileHandle].write_size))
        {
            rlen = (s_fop[FileHandle].write_size + 4) - s_fop[FileHandle].offset;
        }

        SPI_Flash_Read((u8 *) DataPtr, addr , rlen);
        *Read = rlen;

        s_fop[FileHandle].offset += rlen;

        //LOG(DEBUG,"clock(%d) file name %s read! offset %d ",clock(),s_fop[FileHandle].f_name,s_fop[FileHandle].offset);

        return rlen;
    }

    return UFS_INVALID_VALUE;
}


int FS_Write(int FileHandle, void * DataPtr, u32 Length, u32 * Written)
{
 //   s8 err = 0;
    File_Opration_Struct *p = NULL;

    u32 addr = 0;
    
    if(FileHandle  < MAX_FILE_CONUTS && Length > 0)
    {        
        p = &s_fop[FileHandle];

        if(!p->opend) return UFS_FAIL_TO_OPEN;

        addr = p->start_adr + p->offset;

        LOG(LEVEL_DEBUG,"file name %s write len %d! cur size %d ",p->f_name,Length,p->write_size);
        
        if((addr + Length) < p->end_adr)
        {   
            
            SPI_Flash_Write((u8 *)DataPtr,addr,Length);

            *Written = Length;

            if(p->write_size < 0) p->write_size = 0;

            p->write_size = ((s32)p->offset + Length - 4) > (s32)p->write_size?((s32)p->offset + Length - 4):(s32)p->write_size;

            return Length;
        }
    }

    return UFS_INVALID_VALUE;
}

int FS_Seek(int FileHandle, int Offset, int Whence)
{
    File_Opration_Struct *p = NULL;

    u32 addr = 0;
    
    if(FileHandle  < MAX_FILE_CONUTS)
    {        
        p = &s_fop[FileHandle];

        if(!p->opend) return UFS_FAIL_TO_OPEN;

        //addr = p->start_adr + p->offset;
        
        if((addr + p->offset) < p->end_adr)
        {
            p->offset = Offset + 4;

            return UFS_SUCCESS;
        }
    }

    return UFS_INVALID_VALUE;
    
}

int FS_Commit(int FileHandle)
{
    return UFS_SUCCESS;
}

int FS_GetFileSize(const u8 *Filename, u32 * Size)
{
    u8 i = 0;

    i = fs_get_file_id((const u8 *)Filename);

    if(i < MAX_FILE_CONUTS)
    {
        if(s_fop[i].opend)
        {
            *Size = s_fop[i].write_size;

            return UFS_SUCCESS;
        }
        
    }

    return UFS_INVALID_VALUE;
}

int FS_GetFilePosition(int FileHandle, u32 * Position)
{
    return UFS_SUCCESS;
}

int FS_Delete(const u8 * FileName)
{
    u8  i = 0;

    i = fs_get_file_id(FileName);

    if(i < MAX_FILE_CONUTS)
    {
        s_fop[i].write_size = 0;
        s_fop[i].offset = 0;
        s_fop[i].opend = 0;
    

        SPI_Flash_Write_NoCheck((u8 *)&s_fop[i].write_size,s_fop[i].start_adr,4);

        return UFS_SUCCESS;
    }

    return UFS_INVALID_VALUE;
}

int FS_CheckFile(const u8  * FileName)
{
    u8  i = 0;

    //u32 size = 0;

    i = fs_get_file_id(FileName);

    if(i < MAX_FILE_CONUTS)
    {
        return UFS_SUCCESS;
    }

    return UFS_INVALID_VALUE;
}


