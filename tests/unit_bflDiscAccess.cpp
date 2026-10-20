// Test entry points for the bullfrog file system's disc side (library unit_bflDiscAccess:
// src/bfl_DiscAccess.cpp with src/bfl_FileSystem.cpp and the string sources, all real). A
// case is a script of calls on one storage and one file, driven through the public
// classes as the game drives them. Only the edges are fakes, recorded with their arguments
// and answering from per-case queues: macdoze's Win32 file calls and the LbFile_* wrappers.
#include <cstdio>
#include <cstring>

#include "LbFile.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "platform/macdoze.h"

using namespace bfl::FileSystem;
using calllog::ev;
using calllog::ret;

// ---- members the unit's sources reference and no case reaches --------------------------
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}
#ifndef TPW_REAL_FILE_ACCESS
_TbBufferAccess::~_TbBufferAccess() {}
int32_t _TbBufferAccess::Read(void*, uint32_t) { return -1; }
int32_t _TbBufferAccess::Write(const void*, uint32_t) { return -1; }
int32_t _TbBufferAccess::Seek(int32_t, uint32_t) { return -1; }
int32_t _TbBufferAccess::GetSize() { return -1; }
int32_t _TbBufferAccess::GetPos() { return -1; }
void _TbBufferAccess::Close() {}
void _TbBufferAccess::AttachAccessClass(CAccessItem*) {}
_TbQuickPosAccess::~_TbQuickPosAccess() {}
int32_t _TbQuickPosAccess::Read(void*, uint32_t) { return -1; }
int32_t _TbQuickPosAccess::Write(const void*, uint32_t) { return -1; }
int32_t _TbQuickPosAccess::Seek(int32_t, uint32_t) { return -1; }
int32_t _TbQuickPosAccess::GetSize() { return -1; }
int32_t _TbQuickPosAccess::GetPos() { return -1; }
void _TbQuickPosAccess::Close() {}
_TbSharedAccess::~_TbSharedAccess() {}
int32_t _TbSharedAccess::Read(void*, uint32_t) { return -1; }
int32_t _TbSharedAccess::Write(const void*, uint32_t) { return -1; }
int32_t _TbSharedAccess::Seek(int32_t, uint32_t) { return -1; }
int32_t _TbSharedAccess::GetSize() { return -1; }
int32_t _TbSharedAccess::GetPos() { return -1; }
void _TbSharedAccess::AttachAccessClass(CAccessItem*) {}
#endif

namespace {
int gFindCount;

// Handles are small numbers on both sides; -1 stays -1.
NS_MacDoze::InternalHandle* AsHandle(int64_t value) {
    return reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<intptr_t>(static_cast<int32_t>(value)));
}
uint32_t Number(const void* pointer) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)); }

// What every directory search finds: the n-th entry, with queued attributes and size.
void FillFindData(NS_MacDoze::_WIN32_FIND_DATAA* data) {
    std::memset(data, 0, sizeof *data);
    data->dwFileAttributes = static_cast<uint32_t>(ret("Find.attributes", 0x80));
    auto* times = reinterpret_cast<uint8_t*>(data->ftCreationTime);
    for (int i = 0; i < 24; ++i) times[i] = static_cast<uint8_t>(i + 1 + gFindCount);
    data->nFileSizeHigh = 0x11;
    data->nFileSizeLow = static_cast<uint32_t>(ret("Find.size", 1234));
    std::snprintf(data->cFileName, sizeof data->cFileName, "found%d.dat", gFindCount);
    ++gFindCount;
}
}  // namespace

// ---- macdoze: recorded fakes ------------------------------------------------------------
namespace NS_MacDoze {
int32_t InterlockedIncrement(int32_t* value) {
#ifdef TPW_REAL_FILE_ACCESS
    ev("InterlockedIncrement", {calllog::lab(value), *value});
#endif
    return ++*value;
}
int32_t InterlockedDecrement(int32_t* value) {
#ifdef TPW_REAL_FILE_ACCESS
    ev("InterlockedDecrement", {calllog::lab(value), *value});
#endif
    return --*value;
}
int32_t IsDBCSLeadByte(uint8_t) { return 0; }
uint32_t GetFileAttributes(const char* name) {
    ev("GetFileAttributes", {}, {name});
    return static_cast<uint32_t>(ret("GetFileAttributes", 0x10));
}
InternalHandle* CreateFile(const char* name, uint32_t access, uint32_t share, void* security, uint32_t creation,
                           uint32_t flags, InternalHandle* templateFile) {
    ev("CreateFile", {access, share, Number(security), creation, flags, Number(templateFile)}, {name});
    return AsHandle(ret("CreateFile", 0x5001));
}
int32_t CloseHandle(InternalHandle* handle) {
    ev("CloseHandle", {Number(handle)});
    return 1;
}
int32_t ReadFile(InternalHandle* handle, void* buffer, uint32_t count, uint32_t* done, void* overlapped) {
    ev("ReadFile", {Number(handle), count, Number(overlapped)});
    uint32_t n = static_cast<uint32_t>(ret("ReadFile.done", count));
    if (n > count) n = count;
    for (uint32_t i = 0; i < n; ++i) static_cast<uint8_t*>(buffer)[i] = static_cast<uint8_t>(i * 7 + 3);
    *done = n;
    return static_cast<int32_t>(ret("ReadFile", 1));
}
int32_t WriteFile(InternalHandle* handle, const void* buffer, uint32_t count, uint32_t* done, void* overlapped) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) sum = sum * 31 + static_cast<const uint8_t*>(buffer)[i];
    ev("WriteFile", {Number(handle), count, sum, Number(overlapped)});
    *done = static_cast<uint32_t>(ret("WriteFile.done", count));
    return static_cast<int32_t>(ret("WriteFile", 1));
}
uint32_t SetFilePointer(InternalHandle* handle, int32_t distance, int32_t* distanceHigh, uint32_t method) {
    ev("SetFilePointer", {Number(handle), distance, Number(distanceHigh), method});
    return static_cast<uint32_t>(ret("SetFilePointer", static_cast<uint32_t>(distance)));
}
uint32_t GetFileSize(InternalHandle* handle, uint32_t* sizeHigh) {
    ev("GetFileSize", {Number(handle), Number(sizeHigh)});
    return static_cast<uint32_t>(ret("GetFileSize", 1000));
}
int32_t FlushFileBuffers(InternalHandle* handle) {
    ev("FlushFileBuffers", {Number(handle)});
    return static_cast<int32_t>(ret("FlushFileBuffers", 1));
}
int32_t GetMacFileReference(InternalHandle* handle) {
    ev("GetMacFileReference", {Number(handle)});
    return static_cast<int32_t>(ret("GetMacFileReference", 7));
}
InternalHandle* CreateFileMapping(InternalHandle* handle, void* security, uint32_t protect, uint32_t sizeHigh,
                                  uint32_t sizeLow, const char* name) {
    ev("CreateFileMapping", {Number(handle), Number(security), protect, sizeHigh, sizeLow, Number(name)});
    return AsHandle(ret("CreateFileMapping", 0x7001));
}
void* MapViewOfFileEx(InternalHandle* mapping, uint32_t access, uint32_t offsetHigh, uint32_t offsetLow,
                      uint32_t bytes, void* base) {
    ev("MapViewOfFileEx", {Number(mapping), access, offsetHigh, offsetLow, bytes, Number(base)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("MapViewOfFileEx", 0x200000))));
}
int32_t UnmapViewOfFile(void* view) {
    ev("UnmapViewOfFile", {Number(view)});
    return 1;
}
void GetSystemInfo(_SYSTEM_INFO* info) {
    std::memset(info, 0, sizeof *info);
    info->dwAllocationGranularity = static_cast<uint32_t>(ret("Granularity", 0x10000));
}
InternalHandle* FindFirstFile(const char* pattern, _WIN32_FIND_DATAA* data) {
    ev("FindFirstFile", {}, {pattern});
    FillFindData(data);
    return AsHandle(ret("FindFirstFile", 0x6001));
}
int32_t FindNextFile(InternalHandle* handle, _WIN32_FIND_DATAA* data) {
    ev("FindNextFile", {Number(handle)});
    FillFindData(data);
    return static_cast<int32_t>(ret("FindNextFile", 1));
}
int32_t FindClose(InternalHandle* handle) {
    ev("FindClose", {Number(handle)});
    return 1;
}
}  // namespace NS_MacDoze

// ---- the LbFile_* wrappers: recorded fakes ------------------------------------------------
// The full project name of `name` is "P:" + name here, so every path shows where it went.
TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* name) {
    ev("LbFile_ToProjectFilename", {size}, {name});
    std::strcpy(out, "P:");
    std::strcat(out, name);
    return TbError(0);
}
TbError LbFile_CreateDirectory(const char* name) {
    ev("LbFile_CreateDirectory", {}, {name});
    return TbError(static_cast<int32_t>(ret("LbFile_CreateDirectory", 0)));
}
TbError LbFile_DeleteDirectory(const char* name) {
    ev("LbFile_DeleteDirectory", {}, {name});
    return TbError(static_cast<int32_t>(ret("LbFile_DeleteDirectory", 0)));
}
TbError LbFile_Delete(const char* name) {
    ev("LbFile_Delete", {}, {name});
    return TbError(static_cast<int32_t>(ret("LbFile_Delete", 0)));
}
int32_t LbFile_IsDirectory(const char* name) {
    ev("LbFile_IsDirectory", {}, {name});
    return static_cast<int32_t>(ret("LbFile_IsDirectory", 0));
}
TbError LbFile_SetAttributes(const char* name, uint32_t attributes) {
    ev("LbFile_SetAttributes", {attributes}, {name});
    return TbError(static_cast<int32_t>(ret("LbFile_SetAttributes", 0)));
}
TbError LbFile_Rename(const char* from, const char* to) {
    ev("LbFile_Rename", {}, {from, to});
    return TbError(static_cast<int32_t>(ret("LbFile_Rename", 0)));
}

// ---- the script ----------------------------------------------------------------------------
namespace {
CDiscFileStorage* gStorage;
CFile* gFile;
CFileInfo* gInfo;

void InfoText(char* text) {
    std::snprintf(text, 600, "%s|%s", gInfo->mName, gInfo->mNameBuffer);
}
}  // namespace

extern "C" {

void tpw_da_begin() {
    gFindCount = 0;
    gStorage = new CDiscFileStorage;
    gFile = new CFile;
    gInfo = new CFileInfo;
}

// One step. Returns the call's result; `text` receives any text result; `io` is the read
// or write buffer, and for the directory searches receives {size, attributes, 24 time bytes}.
int64_t tpw_da_step(int32_t op, int32_t a, int32_t b, const char* s1, const char* s2, uint8_t* io, char* text) {
    text[0] = '\0';
    switch (op) {
    case 0: return gStorage->Open(s1);
    case 1:
    case 2: {
        const int32_t result = op == 1 ? gStorage->FindFirst(s1, *gInfo) : gStorage->FindNext(*gInfo);
        std::memcpy(io, &gInfo->mF00, 4);
        std::memcpy(io + 4, &gInfo->mAttributes, 4);
        std::memcpy(io + 8, gInfo->mF08, 24);
        InfoText(text);
        return result;
    }
    case 3: return gStorage->OpenFile(*gFile, s1, static_cast<uint32_t>(a), nullptr);
    case 4: return gFile->Read(io, static_cast<uint32_t>(a));
    case 5: return gFile->Write(io, static_cast<uint32_t>(a));
    case 6: return gFile->Seek(static_cast<uint32_t>(a), static_cast<uint32_t>(b));
    case 7: return gFile->GetSize();
    case 8: return gFile->GetPos();
    case 9: return gFile->Flush();
    case 10: return static_cast<int64_t>(reinterpret_cast<uintptr_t>(gFile->MapFile()));
    case 11: return gFile->GetMacFileReference();
    case 12: gFile->Close(); return 0;
    case 13: {
        char buffer[400];
        std::memset(buffer, 'z', sizeof buffer);
        buffer[sizeof buffer - 1] = '\0';
        const int32_t result = gStorage->FullPath(buffer, s1, static_cast<uint32_t>(a));
        std::snprintf(text, 600, "%s", buffer);
        return result;
    }
    case 14: {
        TbDynamicStringTemplate<char> out("previous");
        TbDynamicStringTemplate<char> name(s1);
        const int32_t result = gStorage->FullPath(out, name);
        std::snprintf(text, 600, "%s", out.mData);
        return result;
    }
    case 15: {
        const char* root = gStorage->GetRootPath();
        std::snprintf(text, 600, "%s", root != nullptr ? root : "(null)");
        return 0;
    }
    case 16: return gStorage->SetAttribs(s1, static_cast<uint32_t>(a));
    case 17: return gStorage->CreateDir(s1);
    case 18: return gStorage->Delete(s1);
    case 19: return gStorage->Move(s1, s2);
    case 20: gStorage->Close(); return 0;
    case 21: {   // a copy shares the open file's access object
        CFile copy(*gFile);
        return copy.GetPos();
    }
    case 22: {
        CFile other;
        other = *gFile;
        return other.GetPos();
    }
    case 23: {
        CFileStorage copy(*gStorage);
        const char* root = copy.GetRootPath();
        std::snprintf(text, 600, "%s", root != nullptr ? root : "(null)");
        return 0;
    }
    case 24: {   // a partial mapping straight from the access object, then released
        CAccessItem* access = gFile->GetAccessClass();
        const uint8_t* view = access->MapFile(a, b);
        access->UnmapFile(view);
        return static_cast<int64_t>(reinterpret_cast<uintptr_t>(view));
    }
    case 25: {   // the open file's access object opened again on another file
        auto* access = static_cast<_TbDiscAccess*>(gFile->GetAccessClass());
        return access->Open(s1, static_cast<uint32_t>(a));
    }
    default: {   // 26: a second open file takes over the first one's access object
        CFile other;
        gStorage->OpenFile(other, s1, static_cast<uint32_t>(a), nullptr);
        other = *gFile;
        return other.GetPos();
    }
    }
}

void tpw_da_end() {
    delete gFile;
    delete gStorage;
    delete gInfo;
}

}  // extern "C"
