// LbFile.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include <cstdio>
#include <cstring>
#include <new>

// Bullfrog library, LbSCBack.cpp range: remaining file wrappers 0x10000048..0x10001e34,
// LbMem_ReAlloc 0x10013dec, plus the out-of-line copies of TbError's inline members:
// 0x10001564 TbError(long) 8 B.
// 0x100014b8 TbError(const TbError&) 12 B.
// 0x100014f0 operator long() 8 B. src/LbFile.cpp, src/LbFile_stamp.cpp and
// src/LbTime.cpp hold the rest of the range.

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

// Bullfrog's project paths and file wrappers (LbSCBack.cpp in the symbol inventory).

// One spare byte makes the boundary-length platform path's trailing NUL representable.
char gProcessPath[261];                   // 0x10073ba0, logical path limit 260

namespace {
char gProjectDir[260];                    // 0x10071702
char gDirectoryPath[260];                 // 0x10073a9c
bool separator(char c) {
    return c == '\\' || c == '/' || c == ':';
}
}

TbError LbFile_CreateDirectory(const char* name) {
    LbFile_ToProjectFilename(gDirectoryPath, 260, name);
    return TbError(NS_MacDoze::CreateDirectory(gDirectoryPath, nullptr) != 0 ? 0 : -1);
}

TbError LbFile_DeleteDirectory(const char* name) {
    LbFile_ToProjectFilename(gDirectoryPath, 260, name);
    return TbError(NS_MacDoze::RemoveDirectory(gDirectoryPath) != 0 ? 0 : -1);
}

TbError LbFile_Open(void*& file, const char* name, uint32_t mode) {
    auto* disc = new bfl::FileSystem::CDiscFile();
    TbError result = disc->Open(name, mode);
    if (!result.Succeeded()) return result;
    file = disc;
    return TbError(0);
}

TbError LbFile_Close(void* file) {
    auto* object = static_cast<bfl::FileSystem::CFile*>(file);
    object->Close();
    delete object;
    return TbError(0);
}

// 0x1000046c (56)
TbError LbFile_Flush(void* file) { return static_cast<bfl::FileSystem::CFile*>(file)->Flush(); }
// 0x100004c8 (44)
int32_t LbFile_GetPosition(void* file) { return static_cast<bfl::FileSystem::CFile*>(file)->GetPos(); }

// LbFile_CompareFileStamps (bullfrog symbols shared 0x10000520, LbSCBack.cpp range): orders two directory listing time stamps.
// Unit w108 (the park's most recent save loader) uses it.

// A NULL stamp is older than any, two NULLs are equal; else NS_MacDoze::CompareFileTime (the platform's; it compares to the
// second).
int32_t LbFile_CompareFileStamps(const TbFileStamp* a, const TbFileStamp* b) {
    if (a == nullptr && b == nullptr) return 0;
    if (a == nullptr) return -1;
    if (b == nullptr) return 1;
    return NS_MacDoze::CompareFileTime(reinterpret_cast<const NS_MacDoze::tagFILETIME*>(a),
                                       reinterpret_cast<const NS_MacDoze::tagFILETIME*>(b));
}

// 0x100005c8 (12)
TbError LbFile_GetFileStamp(void*, TbFileStamp*, TbFileStamp*, TbFileStamp*) { return TbError(0); }
// 0x1000062c (12)
TbError LbFile_SetFileStamp(void*, const TbFileStamp*, const TbFileStamp*, const TbFileStamp*) { return TbError(0); }

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    return static_cast<bfl::FileSystem::CFile*>(file)->Seek(static_cast<uint32_t>(offset), whence);
}

TbError LbFile_GetAttributes(const char* name, uint32_t* attributes) {
    char path[260];
    LbFile_ToProjectFilename(path, 260, name);
    const uint32_t result = NS_MacDoze::GetFileAttributes(path);
    if (result == 0xffffffff) return TbError(-1);
    *attributes = result;
    return TbError(0);
}

TbError LbFile_SetAttributes(const char* name, uint32_t attributes) {
    char path[260];
    LbFile_ToProjectFilename(path, 260, name);
    return TbError(NS_MacDoze::SetFileAttributes(path, attributes) != 0 ? 0 : -1);
}

// The size of an open file: the file object's virtual at vtable offset 0x1c on the Mac, which
// is CFile::GetSize (GetPos is the next slot, 0x20).
int32_t LbFile_GetSize(void* file) {
    return static_cast<bfl::FileSystem::CFile*>(file)->GetSize();
}

int32_t LbFile_GetLength(const char* name) {
    char path[260];
    NS_MacDoze::_WIN32_FIND_DATAA data;
    LbFile_ToProjectFilename(path, 260, name);
    auto* handle = NS_MacDoze::FindFirstFile(path, &data);
    if (handle == reinterpret_cast<NS_MacDoze::InternalHandle*>(intptr_t(-1))) return -1;
    NS_MacDoze::FindClose(handle);
    return static_cast<int32_t>(data.nFileSizeLow);
}

uint8_t LbFile_Exists(const char* name) {
    char path[260];
    LbFile_ToProjectFilename(path, 260, name);
    return NS_MacDoze::GetFileAttributes(path) != 0xffffffff;
}

int32_t LbFile_IsDirectory(const char* name) {
    char path[260];
    LbFile_ToProjectFilename(path, 260, name);
    const uint32_t attributes = NS_MacDoze::GetFileAttributes(path);
    return attributes == 0xffffffff ? 0 : attributes & 0x10;
}

TbError LbFile_Delete(const char* name) {
    char path[260];
    LbFile_ToProjectFilename(path, 260, name);
    return TbError(NS_MacDoze::DeleteFile(path) != 0 ? 0 : -1);
}

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

TbError LbFile_Rename(const char* from, const char* to) {
    return TbError(NS_MacDoze::MoveFile(from, to) != 0 ? 0 : -1);
}

// 0x10000ddc (128 bytes): both names are expanded to full project names (0x104-byte buffers), the file is copied over the
// target (failIfExists 0: an existing target is replaced) and the answer is 0 whatever the copy did. The original does not read
// the expansion's result either.
TbError LbFile_Copy(const char* from, const char* to) {
    char fromFull[0x104] = {};
    char toFull[0x104] = {};
    LbFile_ToProjectFilename(fromFull, 0x104, from);
    LbFile_ToProjectFilename(toFull, 0x104, to);
    NS_MacDoze::CopyFile(fromFull, toFull, 0);
    return TbError(0);
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    int32_t count = static_cast<bfl::FileSystem::CFile*>(file)->Read(buffer, length);
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(count);
    return TbError(static_cast<uint32_t>(count) == length);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    int32_t count = static_cast<bfl::FileSystem::CFile*>(file)->Write(buffer, length);
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(count);
    return TbError(static_cast<uint32_t>(count) == length);
}

void LbFile_SetProjectDir(const char* directory) {
    uint32_t length = static_cast<uint32_t>(std::strlen(directory));
    if (length < 260) {
        // The original reads directory[-1] for an empty input.
        if (separator(directory[static_cast<int32_t>(length) - 1]))
            std::sprintf(gProjectDir, "%s", directory);
        else if (length + 1 < 260)
            std::sprintf(gProjectDir, "%s:", directory);
    }
}

const char* LbFile_GetProjectDir() {
    if (gProjectDir[0] == 0) LbFile_GetProjectPath();
    return gProjectDir;
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

TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* name) {
    if (name == nullptr) {
        std::strcpy(out, LbFile_GetProjectDir());
        return TbError(0);
    }
    if (gProjectDir[0] == 0) LbFile_GetProjectPath();
    if (std::strstr(name, gProjectDir) == name || std::strstr(name, "Theme Park:") == name)
        std::strcpy(out, name);
    else {
        std::strcpy(out, LbFile_GetProjectDir());
        std::strcat(out, separator(name[0]) ? name + 1 : name);
    }
    return TbError(0);
}

// 0x10001758 (128)
char* LbFile_GetProjectFilename(const char* name) {
    return LbFile_ToProjectFilename(gProcessPath, 0x104, name).Succeeded() ? gProcessPath : nullptr;
}

TbError LbFile_Load(const char* name, void* buffer, uint32_t capacity, int32_t* length) {
    char path[260];
    LbFile_ToProjectFilename(path, 260, name);
    int32_t size = LbFile_GetLength(path);
    if (size == -1 || size > static_cast<int32_t>(capacity)) return TbError(-1);
    void* file;
    if (!LbFile_Open(file, path, 0x80000001).Succeeded()) return TbError(-1);
    if (!LbFile_Read(file, buffer, static_cast<uint32_t>(size), nullptr).Succeeded()) return TbError(-1);
    LbFile_Close(file);
    if (length != nullptr) *length = size;
    return TbError(0);
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

void LbFile_GetProjectPath() {
    SamsUtilities::UString path;
    SamsUtilities::gThisProcess.GetFolder()->GetPath(path);
    std::strcpy(gProcessPath, path.mText);
    LbFile_SetProjectDir(gProcessPath);
}

// 0x10001e04 (48)
TbError LbFile_Open(void** file, const char* name, uint32_t mode) { return LbFile_Open(*file, name, mode); }
