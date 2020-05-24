
#ifndef __FS_API_H__
#define __FS_API_H__


typedef enum
{
    UFS_SUCCESS = 0,
    UFS_INVALID_VALUE = -1,
    UFS_DRIVER_FULL = -3,
    UFS_FILE_NOT_FOUND = -5,
    UFS_INVALID_FILE_NAME = -6,
    UFS_FILE_ALREADY_EXSIST = -7,
    UFS_FAIL_TO_WRITE = -9,
    UFS_FAIL_TO_OPEN  = -10,
    UFS_FAIL_TO_READ  = -11,
    UFS_FAIL_TO_LIST  = -17,
    UFS_FAIL_TO_DEL   = -18,
    UFS_FAIL_GET_DISK = -19,
    UFS_NO_SPACE      = -20,
    UFS_TIMEOUT       = -21,
    UFS_FILE_TOO_LARGE= -23,
    UFS_INVALID_PARAMS= -25,
    UFS_FILE_ALREADY_OPEN = -26,

    UFS_ERROR_UNKOWN = -99,
}UFSErrorEnum;

#define MAN_PARAM_NAME "ManPara"
#define BKP_PARAM_NAME "BkpPara"
#define HIS_FILE_NAME  "HisFile"
#define STA_FILE_NAME  "Stafile"
#define AGPS1_FILE_NAME "AgpsFile"
#define NULL_FILE_NAME "NullFile"
#define SYS_INFO_NAME  "SysInfo"
#define SYS_FILE_NAME  "SYS"
#define EX_INFO_NAME   "ExtInfo"
#define EX_FILE_NAME   "EXT"


extern int FS_Close(int FileHandle);

extern int FS_Read(int FileHandle, void * DataPtr, u32 Length, u32 * Read);

extern int FS_Write(int FileHandle, void * DataPtr, u32 Length, u32 * Written);

extern int FS_Seek(int FileHandle, int Offset, int Whence);

extern int FS_Commit(int FileHandle);

extern int FS_GetFileSize(const u8 *Filename, u32 * Size);

extern int FS_GetFilePosition(int FileHandle, u32 * Position);

extern int FS_Delete(const u8 * FileName);

extern int FS_CheckFile(const u8  * FileName);

extern int FS_Open(const u8 * FileName, u32 Flag);

#endif


