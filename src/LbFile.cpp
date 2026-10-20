// Bullfrog's project paths and file wrappers (LbSCBack.cpp in the symbol inventory).
#include "LbFile.h"
#include "bfl_FileSystem.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

#include <cstdio>
#include <cstring>
#include <new>

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

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    return static_cast<bfl::FileSystem::CFile*>(file)->Seek(static_cast<uint32_t>(offset), whence);
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

void LbFile_GetProjectPath() {
    SamsUtilities::UString path;
    SamsUtilities::gThisProcess.GetFolder()->GetPath(path);
    std::strcpy(gProcessPath, path.mText);
    LbFile_SetProjectDir(gProcessPath);
}

void* LbMem_Alloc(uint32_t size) {
    return size == 0 ? nullptr : ::operator new[](size);
}

void LbMem_Free(void* block) {
    ::operator delete[](block);
}
