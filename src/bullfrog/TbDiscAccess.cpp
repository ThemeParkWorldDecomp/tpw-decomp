// TbDiscAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbDiscAccess code, named after it.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include <cstdlib>
#include <cstring>

// The bullfrog file system's disc side, the part reached through vtables: the access
// object behind a disc file (_TbDiscAccess), the disc storage's members
// (_TbFileStorageAccess: opening, listing, deleting, moving), the storage access base
// (_TbStorageAccess), CAccessItem's own slots, and the CFile and CFileStorage members that
// forward to them (bullfrog shared, 0x10004938..0x10005dc8, 0x10007a9c..0x10008024,
// 0x1000a86c..0x1000ae98, 0x1000b224..0x1000c220). The disk itself is macdoze's Win32
// file API (include/platform/macdoze.h).

namespace bfl::FileSystem {

namespace {
NS_MacDoze::InternalHandle* const kNoHandle = reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<intptr_t>(-1));
char gDiscAccessPath[260];   // 0x10073cf4: the full name of the file being opened
}  // namespace

// ---- _TbDiscAccess ------------------------------------------------------------------
_TbDiscAccess::_TbDiscAccess() : mHandle(kNoHandle), mPosition(0), mMapping(nullptr), mView(nullptr) {}
_TbDiscAccess::~_TbDiscAccess() {}

// mode: bits 30 and 31 are the Win32 access rights, the low two bits the share mode; 0x10
// asks for a file to be created: 0x20 open or create, 0x40 create only if new, neither
// replace. Without 0x10 the file must exist.
TbError _TbDiscAccess::Open(const char* name, uint32_t mode) {
    Close();
    LbFile_ToProjectFilename(gDiscAccessPath, 260, name);
    uint32_t creation = 3;   // OPEN_EXISTING
    if ((mode & 0x10) != 0) {
        if ((mode & 0x20) != 0) creation = 4;         // OPEN_ALWAYS
        else if ((mode & 0x40) != 0) creation = 1;    // CREATE_NEW
        else creation = 2;                            // CREATE_ALWAYS
    }
    mHandle = NS_MacDoze::CreateFile(gDiscAccessPath, mode & 0xc0000000u, mode & 3, nullptr, creation, 0x10000000,
                                     nullptr);
    if (mHandle == kNoHandle) return TbError(-1);
    mPosition = 0;
    return TbError(0);
}

void _TbDiscAccess::Close() {
    NS_MacDoze::CloseHandle(mMapping);   // also when there is none, as the original does
    mMapping = nullptr;
    UnmapFile(mView);
    mView = nullptr;
    if (mHandle != kNoHandle) NS_MacDoze::CloseHandle(mHandle);
    mHandle = kNoHandle;
}

int32_t _TbDiscAccess::Read(void* dest, uint32_t size) {
    uint32_t done = 0;
    if (size != 0) NS_MacDoze::ReadFile(mHandle, dest, size, &done, nullptr);
    mPosition += done;
    return static_cast<int32_t>(done);
}

int32_t _TbDiscAccess::Write(const void* source, uint32_t size) {
    uint32_t done = 0;
    if (size != 0) NS_MacDoze::WriteFile(mHandle, source, size, &done, nullptr);
    mPosition += done;
    return static_cast<int32_t>(done);
}

int32_t _TbDiscAccess::Seek(int32_t offset, uint32_t whence) {
    mPosition = NS_MacDoze::SetFilePointer(mHandle, offset, nullptr, whence);
    return static_cast<int32_t>(mPosition);
}

int32_t _TbDiscAccess::GetSize() { return static_cast<int32_t>(NS_MacDoze::GetFileSize(mHandle, nullptr)); }
int32_t _TbDiscAccess::GetPos() { return static_cast<int32_t>(mPosition); }

// The whole file (offset 0, length 0) is mapped once and kept; a part is mapped from the
// allocation boundary below `offset` and the pointer moved up to it. Offsets at or below
// the allocation granularity map from the start of the file with length 0.
const uint8_t* _TbDiscAccess::MapFile(int32_t offset, int32_t length) {
    const uint8_t* view = nullptr;
    if (mHandle == kNoHandle) return nullptr;
    if (mMapping == nullptr) mMapping = NS_MacDoze::CreateFileMapping(mHandle, nullptr, 2, 0, 0, nullptr);
    if (mMapping == nullptr) return nullptr;
    if (offset == 0 && length == 0) {
        if (mView == nullptr)
            mView = static_cast<const uint8_t*>(NS_MacDoze::MapViewOfFileEx(mMapping, 4, 0, 0, 0, nullptr));
        return mView;
    }
    NS_MacDoze::_SYSTEM_INFO info;
    NS_MacDoze::GetSystemInfo(&info);
    uint32_t start = 0;
    uint32_t bytes = 0;
    if (static_cast<int32_t>(info.dwAllocationGranularity) < offset) {
        start = info.dwAllocationGranularity * (static_cast<uint32_t>(offset) / info.dwAllocationGranularity);
        bytes = static_cast<uint32_t>(length) + (static_cast<uint32_t>(offset) - start);
    }
    view = static_cast<const uint8_t*>(NS_MacDoze::MapViewOfFileEx(mMapping, 4, 0, start, bytes, nullptr));
    if (view != nullptr) view += static_cast<uint32_t>(offset) - start;
    return view;
}

void _TbDiscAccess::UnmapFile(const uint8_t* data) {
    if (mMapping != nullptr && data != nullptr) NS_MacDoze::UnmapViewOfFile(const_cast<uint8_t*>(data));
}

_LbSemaphore* _TbDiscAccess::GetSemaphore() { return &mSemaphore; } // 0x100050dc

TbError _TbDiscAccess::Flush() {
    if (mHandle != kNoHandle && NS_MacDoze::FlushFileBuffers(mHandle) != 0) return TbError(0);
    return TbError(-1);
}

int32_t _TbDiscAccess::GetMacFileReference() { return NS_MacDoze::GetMacFileReference(mHandle); }
void _TbDiscAccess::AttachAccessClass(CAccessItem*) {} // 0x1000523c

}  // namespace bfl::FileSystem
