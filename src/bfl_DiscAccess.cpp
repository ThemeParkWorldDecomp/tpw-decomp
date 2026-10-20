// The bullfrog file system's disc side, the part reached through vtables: the access
// object behind a disc file (_TbDiscAccess), the disc storage's members
// (_TbFileStorageAccess: opening, listing, deleting, moving), the storage access base
// (_TbStorageAccess), CAccessItem's own slots, and the CFile and CFileStorage members that
// forward to them (bullfrog shared, 0x10004938..0x10005dc8, 0x10007a9c..0x10008024,
// 0x1000a86c..0x1000ae98, 0x1000b224..0x1000c220). The disk itself is macdoze's Win32
// file API (include/platform/macdoze.h).
#include "bfl_FileSystem.h"

#include <cstdlib>
#include <cstring>

#include "LbFile.h"
#include "platform/macdoze.h"

// The semaphore is a stub in this build of the library (0x10024a24, 0x10024a54).
_LbSemaphore::_LbSemaphore() {}
_LbSemaphore::~_LbSemaphore() {}

namespace bfl::FileSystem {

namespace {
NS_MacDoze::InternalHandle* const kNoHandle = reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<intptr_t>(-1));
char gDiscAccessPath[260];   // 0x10073cf4: the full name of the file being opened
}  // namespace

// ---- CAccessItem: the slots of the base class ---------------------------------------
// Read, Write, Seek, GetSize and GetPos are pure in the original's base vtable; a call
// that lands there is a bug in the access chain.
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { std::abort(); }
int32_t CAccessItem::Write(const void*, uint32_t) { std::abort(); }
int32_t CAccessItem::Seek(int32_t, uint32_t) { std::abort(); }
int32_t CAccessItem::GetSize() { std::abort(); }
int32_t CAccessItem::GetPos() { std::abort(); }
void CAccessItem::Close() {}   // 0x1000a958

// 0x1000ac38: `access` goes behind this item; what was there is handed on to it first.
void CAccessItem::AttachAccessClass(CAccessItem* access) {
    if (mNext != nullptr) access->AttachAccessClass(mNext);
    mNext = access;
}

// 0x1000acf8..0x1000ae98: an item that does not answer these passes them down the chain.
const uint8_t* CAccessItem::MapFile(int32_t offset, int32_t length) { return mNext->MapFile(offset, length); } // 0x1000acf8
void CAccessItem::UnmapFile(const uint8_t* data) { mNext->UnmapFile(data); } // 0x1000ad68
_LbSemaphore* CAccessItem::GetSemaphore() { return mNext->GetSemaphore(); } // 0x1000addc
TbError CAccessItem::Flush() { return TbError(-1); } // 0x1000ae50
int32_t CAccessItem::GetMacFileReference() { return mNext->GetMacFileReference(); } // 0x1000ae98

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

// ---- _TbStorageAccess ---------------------------------------------------------------
_TbStorageAccess::~_TbStorageAccess() {}
// Pure in the original's base vtable (slots 0xc..0x14, 0x24..0x30 and 0x38).
TbError _TbStorageAccess::OpenFile(CFile&, const char*, uint32_t, void*) { std::abort(); }
TbError _TbStorageAccess::FindFirst(const char*, CFileInfo&) { std::abort(); }
TbError _TbStorageAccess::FindNext(CFileInfo&) { std::abort(); }
TbError _TbStorageAccess::SetAttribs(const char*, uint32_t) { std::abort(); }
TbError _TbStorageAccess::CreateDir(const char*) { std::abort(); }
TbError _TbStorageAccess::Delete(const char*) { std::abort(); }
TbError _TbStorageAccess::Move(const char*, const char*) { std::abort(); }
void _TbStorageAccess::Close() { std::abort(); }

// 0x10007bc0. The last reference closes the storage, releases what is chained behind it
// and deletes it.
int32_t _TbStorageAccess::Release() {
    if (NS_MacDoze::InterlockedDecrement(&mReferences) > 0) return mReferences;
    Close();
    if (mNext != nullptr) {
        mNext->Release();
        mNext = nullptr;
    }
    delete this;
    return 0;
}

void _TbStorageAccess::AttachAccessClass(_TbStorageAccess* access) {   // 0x10007e64
    if (mNext != nullptr) access->AttachAccessClass(mNext);
    mNext = access;
}

// 0x10007f2c: root + name into a caller's buffer; -1 when it does not fit.
TbError _TbStorageAccess::FullPath(char* path, const char* name, uint32_t size) {
    TbStaticStringTemplate<char> full(path, size, 1);
    full = static_cast<const TbStringBase<char>&>(mRoot);
    full += name;
    return TbError(full.mTruncated == 0 ? 0 : -1);
}

TbError _TbStorageAccess::FullPath(TbDynamicStringTemplate<char>& path, const TbDynamicStringTemplate<char>& name) {   // 0x10008024
    path = mRoot;
    path += static_cast<const TbStringBase<char>&>(name);
    return TbError(0);
}

const char* _TbStorageAccess::GetRootPath() { return mRoot.mData; }   // 0x10005ed0

// ---- _TbFileStorageAccess -------------------------------------------------------------
_TbFileStorageAccess::~_TbFileStorageAccess() { _FindClose(); }
void _TbFileStorageAccess::Close() {}   // 0x100053f0

// 0x10005780: size, attributes, the three file times (write, access, creation), the name.
void _TbFileStorageAccess::ConvertFindData(CFileInfo& info, const NS_MacDoze::_WIN32_FIND_DATAA& data) {
    info.mF00 = data.nFileSizeLow;
    info.mAttributes = data.dwFileAttributes;
    std::memcpy(info.mF08, data.ftLastWriteTime, 8);
    std::memcpy(info.mF08 + 8, data.ftLastAccessTime, 8);
    std::memcpy(info.mF08 + 16, data.ftCreationTime, 8);
    std::strcpy(info.mName, data.cFileName);
}

// 0x10005438. The pattern is relative to the storage's root. The entry's buffer first
// receives the pattern, and its name pointer the place after the pattern's last ':' or
// '/', so each name found replaces the pattern's last component: the buffer then reads as
// the found file's path inside the storage.
TbError _TbFileStorageAccess::FindFirst(const char* pattern, CFileInfo& info) {
    _FindClose();
    if (pattern == nullptr || pattern[0] == '\0') return TbError(-1);
    const char* last = pattern + std::strlen(pattern) - 1;
    while (*last != ':' && *last != '/' && pattern <= last) --last;
    std::strcpy(info.mNameBuffer, pattern);
    info.mName = info.mNameBuffer + (last + 1 - pattern);
    TbDynamicStringTemplate<char> full(mRoot);
    full += pattern;
    NS_MacDoze::_WIN32_FIND_DATAA data;
    mFindHandle = NS_MacDoze::FindFirstFile(full.mData, &data);
    if (mFindHandle == kNoHandle) return TbError(-1);
    ConvertFindData(info, data);
    return TbError(0);
}

TbError _TbFileStorageAccess::FindNext(CFileInfo& info) {   // 0x100055f0
    if (mFindHandle == kNoHandle) return TbError(-1);
    NS_MacDoze::_WIN32_FIND_DATAA data;
    if (NS_MacDoze::FindNextFile(static_cast<NS_MacDoze::InternalHandle*>(mFindHandle), &data) == 0) return TbError(-1);
    ConvertFindData(info, data);
    return TbError(0);
}

void _TbFileStorageAccess::_FindClose() {   // 0x100056ec
    if (mFindHandle != kNoHandle) {
        NS_MacDoze::FindClose(static_cast<NS_MacDoze::InternalHandle*>(mFindHandle));
        mFindHandle = kNoHandle;
    }
}

// 0x100058a0: a file that is already open cannot be opened again; a failed open releases
// the new access object.
TbError _TbFileStorageAccess::OpenFile(CFile& file, const char* name, uint32_t mode, void*) {
    if (file.GetAccessClass() != nullptr) return TbError(-1);
    TbDynamicStringTemplate<char> full(mRoot);
    full += name;
    auto* access = new _TbDiscAccess;
    if (access->Open(full.mData, mode) < 0) {
        access->CAccessItem::Release();
        return TbError(-1);
    }
    file.AttachAccessClass(access);
    return TbError(0);
}

TbError _TbFileStorageAccess::SetAttribs(const char* name, uint32_t attributes) {   // 0x10005b40
    TbDynamicStringTemplate<char> full(mRoot);
    full += name;
    return LbFile_SetAttributes(full.mData, attributes);
}

TbError _TbFileStorageAccess::CreateDir(const char* name) {   // 0x10005c14
    TbDynamicStringTemplate<char> full(mRoot);
    full += name;
    return LbFile_CreateDirectory(full.mData);
}

TbError _TbFileStorageAccess::Delete(const char* name) {   // 0x10005cd4
    TbDynamicStringTemplate<char> full(mRoot);
    full += name;
    if (LbFile_IsDirectory(full.mData) == 0) return LbFile_Delete(full.mData);
    return LbFile_DeleteDirectory(full.mData);
}

TbError _TbFileStorageAccess::Move(const char* from, const char* to) {   // 0x10005dc8
    TbDynamicStringTemplate<char> source(mRoot);
    source += from;
    TbDynamicStringTemplate<char> target(mRoot);
    target += to;
    return LbFile_Rename(source.mData, target.mData);
}

// ---- CFile and CFileStorage: the members that forward ----------------------------------
CFile::CFile(const CFile& other) : mImplementation(other.mImplementation) {   // 0x1000b224
    mImplementation->CAccessItem::AddRef();
}

CFile& CFile::operator=(const CFile& other) {   // 0x1000b3dc
    if (&other != this) {
        Close();
        AttachAccessClass(other.mImplementation);
    }
    return *this;
}

void* CFile::MapFile() {   // 0x1000b788
    return mImplementation == nullptr ? nullptr : const_cast<uint8_t*>(mImplementation->MapFile(0, 0));
}
TbError CFile::Flush() { return mImplementation == nullptr ? TbError(-1) : mImplementation->Flush(); }   // 0x1000b808
int32_t CFile::GetMacFileReference() {   // 0x1000b358
    return mImplementation == nullptr ? -1 : mImplementation->GetMacFileReference();
}

CFileStorage::CFileStorage(const CFileStorage& other) : mAccess(nullptr) {   // 0x1000b8e0
    if (&other != this && other.mAccess != nullptr) AttachAccessClass(other.mAccess);
}

void CFileStorage::Close() {   // 0x1000bb00
    if (mAccess != nullptr) {
        mAccess->Release();
        mAccess = nullptr;
    }
}

TbError CFileStorage::FullPath(char* path, const char* name, uint32_t size) {   // 0x1000bc50
    return mAccess == nullptr ? TbError(-1) : mAccess->FullPath(path, name, size);
}
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>& path, const TbDynamicStringTemplate<char>& name) {   // 0x1000bb80
    return mAccess == nullptr ? TbError(-1) : mAccess->FullPath(path, name);
}
const char* CFileStorage::GetRootPath() { return mAccess == nullptr ? nullptr : mAccess->GetRootPath(); }   // 0x1000bf04
TbError CFileStorage::SetAttribs(const char* name, uint32_t attributes) {   // 0x1000c058
    return mAccess == nullptr ? TbError(-1) : mAccess->SetAttribs(name, attributes);
}
TbError CFileStorage::CreateDir(const char* name) {   // 0x1000c0f4
    return mAccess == nullptr ? TbError(-1) : mAccess->CreateDir(name);
}
TbError CFileStorage::Delete(const char* name) {   // 0x1000c18c
    return mAccess == nullptr ? TbError(-1) : mAccess->Delete(name);
}
TbError CFileStorage::Move(const char* from, const char* to) {   // 0x1000c220
    return mAccess == nullptr ? TbError(-1) : mAccess->Move(from, to);
}

}  // namespace bfl::FileSystem
