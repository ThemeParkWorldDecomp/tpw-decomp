// Bullfrog library, LbSCBack.cpp range: remaining file wrappers 0x10000048..0x10001e34,
// LbMem_ReAlloc 0x10013dec, plus the out-of-line copies of TbError's inline members:
// 0x10001564 TbError(long) 8 B.
// 0x100014b8 TbError(const TbError&) 12 B.
// 0x100014f0 operator long() 8 B. src/LbFile.cpp, src/LbFile_stamp.cpp and
// src/LbTime.cpp hold the rest of the range.
#include "LbFile.h"
#include "bfl_FileSystem.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include <cstring>

// 0x10000048 (104): the length byte, not the buffer address, is passed as a pointer.
int32_t LbFile_GetCurrentDrive() {
    unsigned char volName[256];
    int16_t vRefNum;
    int32_t dirID;
    if (HGetVol(volName, &vRefNum, &dirID) != 0) return -1;
    auto* text = reinterpret_cast<unsigned char*>(intptr_t(int8_t(volName[0])));
    UpperString(text, true);
    return int8_t(reinterpret_cast<uintptr_t>(p2cstr(text)));
}
// 0x100000e0 (176)
int32_t LbFile_GetFreeDriveSpace(char drive, TbDriveInfo* info) {
    TbDriveInfo local;
    if (info == nullptr) info = &local;
    char root[4] = {char(int8_t(drive) == -1 ? 0xff : msl_upper_map[uint8_t(drive)]), ':', '\\', 0};
    if (NS_MacDoze::GetDiskFreeSpace(root, &info->mSectorsPerCluster, &info->mBytesPerSector,
                                    &info->mFreeClusters, &info->mTotalClusters) == 0) return -1;
    return static_cast<int32_t>(info->mBytesPerSector * (info->mFreeClusters * info->mSectorsPerCluster));
}
// 0x1000046c (56)
TbError LbFile_Flush(void* file) { return static_cast<bfl::FileSystem::CFile*>(file)->Flush(); }
// 0x100004c8 (44)
int32_t LbFile_GetPosition(void* file) { return static_cast<bfl::FileSystem::CFile*>(file)->GetPos(); }
// 0x100005c8 (12)
TbError LbFile_GetFileStamp(void*, TbFileStamp*, TbFileStamp*, TbFileStamp*) { return TbError(0); }
// 0x1000062c (12)
TbError LbFile_SetFileStamp(void*, const TbFileStamp*, const TbFileStamp*, const TbFileStamp*) { return TbError(0); }
// 0x10000ab0 (308)
TbError LbFile_FindFirst(const char* pattern, TbFileFind* find, uint32_t flags) {
    find->mHandle = NS_MacDoze::FindFirstFile(pattern, &find->mData);
    if (find->mHandle == reinterpret_cast<NS_MacDoze::InternalHandle*>(intptr_t(-1))) return TbError(-1);
    find->mFlags = flags;
    std::strcpy(find->mPath, pattern);
    find->mName = std::strchr(find->mPath, 0);
    while (find->mName > find->mPath && find->mName[-1] != '\\' && find->mName[-1] != '/') --find->mName;
    do {
        uint32_t attributes = find->mData.dwFileAttributes;
        if ((attributes == 0 && find->mFlags == 0xffffffffu) || (attributes & find->mFlags)) {
            std::strcpy(find->mName, find->mData.cFileName);
            return TbError(0);
        }
    } while (NS_MacDoze::FindNextFile(find->mHandle, &find->mData) != 0);
    return TbError(-1);
}
// 0x10000c1c (156)
TbError LbFile_FindNext(TbFileFind* find) {
    while (NS_MacDoze::FindNextFile(find->mHandle, &find->mData) != 0) {
        uint32_t attributes = find->mData.dwFileAttributes;
        if ((attributes == 0 && find->mFlags == 0xffffffffu) || (attributes & find->mFlags)) {
            std::strcpy(find->mName, find->mData.cFileName);
            return TbError(0);
        }
    }
    return TbError(-1);
}
// 0x10000cec (60)
TbError LbFile_FindEnd(TbFileFind* find) {
    NS_MacDoze::FindClose(find->mHandle);
    return TbError(0);
}
// 0x10001520 (24): LB_MAX<char*, char*>
char* const& LbMaxPointer(char* const& a, char* const& b) {
    return reinterpret_cast<uintptr_t>(b) < reinterpret_cast<uintptr_t>(a) ? a : b;
}
// 0x10001228 (616): the original inlines one level of this recursion.
TbError LbFile_MakePath(const char* path) {
    if (LbFile_Exists(path) != 0) return TbError(0);
    char local[0x104];
    std::strcpy(local, path);
    char* slash = std::strrchr(local, '/');
    char* backslash = std::strrchr(local, '\\');
    char* colon = std::strrchr(local, ':');
    char* sep = LbMaxPointer(colon, LbMaxPointer(backslash, slash));
    TbError result(-1);
    if (sep == nullptr) return result;
    if (sep[-1] == ':') ++sep;
    char saved = *sep;
    *sep = 0;
    result = LbFile_MakePath(local);
    if (int32_t(result) == 0) {
        *sep = saved;
        LbFile_CreateDirectory(local); // Its result is ignored, including failures.
    }
    return result;
}
// 0x10001758 (128)
char* LbFile_GetProjectFilename(const char* name) {
    return LbFile_ToProjectFilename(gProcessPath, 0x104, name).Succeeded() ? gProcessPath : nullptr;
}
// 0x10001994 (436)
TbError LbFile_AllocLoad(const char* name, void* block, int32_t* length) {
    if (length != nullptr) *length = 0;
    if (!LbFile_ToProjectFilename(gProcessPath, 0x104, name).Succeeded()) return TbError(-1);
    if (LbFile_Exists(gProcessPath) == 0) return TbError(-1);
    int32_t size = LbFile_GetLength(gProcessPath);
    if (size == -1) return TbError(-1);
    void* data = LbMem_Alloc(static_cast<uint32_t>(size));
    if (data == nullptr) return TbError(-1);
    void* file;
    if (!LbFile_Open(file, gProcessPath, 0x80000001).Succeeded()) {
        LbMem_Free(data);
        return TbError(-1);
    }
    if (!LbFile_Read(file, data, size, nullptr).Succeeded()) {
        LbFile_Close(file);
        LbMem_Free(data);
        return TbError(-1);
    }
    LbFile_Close(file);
    *static_cast<void**>(block) = data;
    if (length != nullptr) *length = size;
    return TbError(0);
}
// 0x10001b78 (280)
TbError LbFile_Save(const char* name, void* data, uint32_t size) {
    LbFile_ToProjectFilename(gProcessPath, 0x104, name);
    void* file;
    if (!LbFile_Open(file, gProcessPath, 0xc0000010).Succeeded()) return TbError(-1);
    if (!LbFile_Write(file, data, size, nullptr).Succeeded()) return TbError(-1); // Original leaks the open file.
    LbFile_Close(file);
    return TbError(0);
}
// 0x10001e04 (48)
TbError LbFile_Open(void** file, const char* name, uint32_t mode) { return LbFile_Open(*file, name, mode); }
// 0x10013dec (140)
void* LbMem_ReAlloc(void* block, uint32_t oldSize, uint32_t newSize) {
    void* p = LbMem_Alloc(newSize);
    if (p != nullptr) {
        std::memcpy(p, block, oldSize < newSize ? oldSize : newSize);
        LbMem_Free(block);
    }
    return p;
}
