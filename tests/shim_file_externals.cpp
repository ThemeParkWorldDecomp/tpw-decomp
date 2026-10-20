// Outside-unit imports for the combined shim; unit_LbFile supplies recorded fakes.
#include "bfl_FileSystem.h"
#include "platform/macdoze.h"
#include <cstring>

_LbSemaLock::~_LbSemaLock() {}
int32_t _LbSemaLock::Lock(int32_t) { return 1; }
bfl::FileSystem::_TbBufferedFile::~_TbBufferedFile() {}
namespace NS_MacDoze {
int32_t CreateDirectory(const char*, void*) { return 0; }
uint32_t GetFileAttributes(const char*) { return 0xffffffff; }
int32_t SetFileAttributes(const char*, uint32_t) { return 0; }
int32_t DeleteFile(const char*) { return 0; }
int32_t RemoveDirectory(const char*) { return 0; }
int32_t MoveFile(const char*, const char*) { return 0; }
int32_t CopyFile(const char*, const char*, int32_t) { return 0; }
InternalHandle* CreateFile(const char*, uint32_t, uint32_t, void*, uint32_t, uint32_t, InternalHandle*) {
    return reinterpret_cast<InternalHandle*>(intptr_t(-1));
}
int32_t CloseHandle(InternalHandle*) { return 0; }
int32_t ReadFile(InternalHandle*, void*, uint32_t, uint32_t* done, void*) { *done = 0; return 0; }
int32_t WriteFile(InternalHandle*, const void*, uint32_t, uint32_t* done, void*) { *done = 0; return 0; }
uint32_t SetFilePointer(InternalHandle*, int32_t, int32_t*, uint32_t) { return 0xffffffff; }
uint32_t GetFileSize(InternalHandle*, uint32_t*) { return 0xffffffff; }
int32_t FlushFileBuffers(InternalHandle*) { return 0; }
int32_t GetMacFileReference(InternalHandle*) { return -1; }
InternalHandle* CreateFileMapping(InternalHandle*, void*, uint32_t, uint32_t, uint32_t, const char*) { return nullptr; }
void* MapViewOfFileEx(InternalHandle*, uint32_t, uint32_t, uint32_t, uint32_t, void*) { return nullptr; }
int32_t UnmapViewOfFile(void*) { return 0; }
void GetSystemInfo(_SYSTEM_INFO* info) { *info = {}; info->dwAllocationGranularity = 65536; }
int32_t FindNextFile(InternalHandle*, _WIN32_FIND_DATAA*) { return 0; }
InternalHandle* FindFirstFile(const char*, _WIN32_FIND_DATAA*) {
    return reinterpret_cast<InternalHandle*>(intptr_t(-1));
}
int32_t FindClose(InternalHandle*) { return 0; }
int32_t InterlockedIncrement(int32_t* p) { return ++*p; }
int32_t InterlockedDecrement(int32_t* p) { return --*p; }
int32_t IsDBCSLeadByte(uint8_t) { return 0; }
void CopyMemory(void* dest, const void* source, uint32_t size) { std::memcpy(dest, source, size); }
}
