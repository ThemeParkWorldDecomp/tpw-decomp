// LbFile: bullfrog library interface (LbSCBack.cpp range: Read 0x10000e84,
// Write 0x10000fa0, IsDirectory 0x1000099c, Delete 0x10000a20, ToProjectFilename
// 0x10001590). Results are TbErrors, returned in a hidden r3 buffer on PowerPC, so the
// other arguments start at r4 (Read and Write: file r4, buffer r5, length r6,
// transferred count r7). Unit w221 completes the drive, find, path and file wrappers below.
#pragma once
#include <cstdint>
#include <cstddef>
#include "platform/macdoze.h"

#include "TbError.h"

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred);
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred);
// Nonzero (FILE_ATTRIBUTE_DIRECTORY, 0x10) if the project file name is a directory.
int32_t LbFile_IsDirectory(const char* name);
TbError LbFile_Delete(const char* name);
// Expands `name` into the full project file name. The original ignores `size`.
TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* name);
// Sets the directory project file names are relative to (0x100010bc); a separator is
// appended unless the name already ends in one.
void LbFile_SetProjectDir(const char* directory);
// 1 if the project file exists, else 0 (0x10000924).
uint8_t LbFile_Exists(const char* name);
const char* LbFile_GetProjectDir();                 // the directory set by LbFile_SetProjectDir
TbError LbFile_Open(void*& file, const char* name, uint32_t mode);   // bullfrog import
TbError LbFile_Close(void* file);                   // bullfrog import
// 0x100004c8 (src/LbFile_more.cpp): the open file's position (CFile::GetPos, vtable offset 0x20).
int32_t LbFile_GetPosition(void* file);
int32_t LbFile_GetSize(void* file);                 // 0x10000840 (44 bytes, src/LbFile.cpp): the open file's size in bytes (CFile::GetSize, vtable offset 0x1c)
TbError LbFile_CreateDirectory(const char* name);   // bullfrog import
TbError LbFile_DeleteDirectory(const char* name);   // bullfrog import (an empty directory)
TbError LbFile_SetAttributes(const char* name, uint32_t attributes);   // 0x10000798
TbError LbFile_GetAttributes(const char* name, uint32_t* attributes);  // 0x100006f0
TbError LbFile_Rename(const char* from, const char* to);               // 0x10000d5c
// unit w48: 0x10000ddc, 128 bytes (src/LbFile.cpp, unit w90): copies the project file `from` over `to` (both names go through
// LbFile_ToProjectFilename; NS_MacDoze::CopyFile overwrites); the answer is 0 whether or not the copy worked.
TbError LbFile_Copy(const char* from, const char* to);
int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence);
int32_t LbFile_GetLength(const char* name);
TbError LbFile_Load(const char* name, void* buffer, uint32_t capacity, int32_t* length);
void LbFile_GetProjectPath();
// A file's time stamp as a directory listing gives it (CFileInfo bytes 8 to 15, the last write time): a Win32 FILETIME, two 32-bit
// words, the low one first, counting 100 nanosecond ticks.
struct TbFileStamp {
    uint32_t mLow;
    uint32_t mHigh;
};
// 0x10000520 (bullfrog, 92 bytes, src/LbFile_stamp.cpp): 1, 0 or -1 for the stamp `a` newer than, the same as, or older than `b`
// (to the second: NS_MacDoze::CompareFileTime); a NULL stamp is older than any (two NULLs are the same).
int32_t LbFile_CompareFileStamps(const TbFileStamp* a, const TbFileStamp* b);
// LbMem: the library's heap (bullfrog imports).
void* LbMem_Alloc(uint32_t size);
void LbMem_Free(void* block);

// Unit w221 (src/LbFile_more.cpp). Field names are invented.
struct TbDriveInfo {
    uint32_t mTotalClusters;       // 0x00
    uint32_t mFreeClusters;        // 0x04
    uint32_t mSectorsPerCluster;   // 0x08
    uint32_t mBytesPerSector;      // 0x0c
};
static_assert(sizeof(TbDriveInfo) == 0x10);
struct TbFileFind {
    NS_MacDoze::_WIN32_FIND_DATAA mData; // 0x000
    NS_MacDoze::InternalHandle* mHandle; // 0x140
    uint32_t mFlags;                    // Mac 0x144
    char mPath[0x104];                  // Mac 0x148
    char* mName;                        // Mac 0x24c, Mac size 0x250
};
static_assert(sizeof(NS_MacDoze::_WIN32_FIND_DATAA) == 0x140);
static_assert(offsetof(TbFileFind, mHandle) == 0x140);
int32_t LbFile_GetCurrentDrive(); // 0x10000048
int32_t LbFile_GetFreeDriveSpace(char drive, TbDriveInfo* info); // 0x100000e0
TbError LbFile_Flush(void* file); // 0x1000046c
TbError LbFile_GetFileStamp(void*, TbFileStamp*, TbFileStamp*, TbFileStamp*); // 0x100005c8
TbError LbFile_SetFileStamp(void*, const TbFileStamp*, const TbFileStamp*, const TbFileStamp*); // 0x1000062c
TbError LbFile_FindFirst(const char*, TbFileFind*, uint32_t); // 0x10000ab0
TbError LbFile_FindNext(TbFileFind*); // 0x10000c1c
TbError LbFile_FindEnd(TbFileFind*); // 0x10000cec
TbError LbFile_MakePath(const char*); // 0x10001228
char* LbFile_GetProjectFilename(const char*); // 0x10001758
TbError LbFile_AllocLoad(const char*, void*, int32_t*); // 0x10001994
TbError LbFile_Save(const char*, void*, uint32_t); // 0x10001b78
TbError LbFile_Open(void**, const char*, uint32_t); // 0x10001e04
void* LbMem_ReAlloc(void*, uint32_t, uint32_t); // 0x10013dec
char* const& LbMaxPointer(char* const&, char* const&); // 0x10001520, invented spelling of LB_MAX
extern char gProcessPath[261]; // 0x10073ba0 (src/LbFile.cpp)
