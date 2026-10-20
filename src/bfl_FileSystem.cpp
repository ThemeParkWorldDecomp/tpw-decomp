// The bullfrog access chains and disc path.
#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include <new>

namespace bfl::FileSystem {

CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
void CAccessItem::AddRef() { NS_MacDoze::InterlockedIncrement(&mReferences); }

int32_t CAccessItem::Release() {
    if (NS_MacDoze::InterlockedDecrement(&mReferences) > 0) return mReferences;
    Close();
    if (mNext != nullptr) {
        mNext->CAccessItem::Release();
        mNext = nullptr;
    }
    delete this;
    return 0;
}

_TbBufferAccess::_TbBufferAccess()
    : mBuffer(nullptr), mCacheBuffer(nullptr), mPosition(0), mCacheStart(0),
      mBaseOffset(0), mBufferSize(0) {}

TbError _TbBufferAccess::Open(uint32_t size) {
    mBufferSize = size;
    mBuffer = static_cast<uint8_t*>(::operator new[](size, std::nothrow));
    if (mBuffer == nullptr) return TbError(-1);
    mCacheBuffer = mBuffer;
    mBaseOffset = 0;
    mPosition = mCacheStart = mCacheEnd = 0;
    return TbError(0);
}

_TbQuickPosAccess::_TbQuickPosAccess() : mPosition(0) {}
_TbSharedAccess::_TbSharedAccess() { mPosition = 0; }
_TbStorageAccess::_TbStorageAccess() : mNext(nullptr), mReferences(0) {}
void _TbStorageAccess::AddRef() { NS_MacDoze::InterlockedIncrement(&mReferences); }
_TbFileStorageAccess::_TbFileStorageAccess() : mFindHandle(reinterpret_cast<void*>(intptr_t(-1))) {}

TbError _TbFileStorageAccess::Open(const char* path) {
    char full[260];
    LbFile_ToProjectFilename(full, 260, path);
    if (NS_MacDoze::GetFileAttributes(full) == 0xffffffff) return TbError(-1);
    mRoot = full;
    char last = mRoot.mData[static_cast<int32_t>(mRoot.Length()) - 1];
    // The original tests colon twice and never tests backslash.
    if (last != ':' && last != '/' && last != ':') mRoot += ':';
    return TbError(0);
}

CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() { Close(); }
void CFile::Close() {
    if (mImplementation != nullptr) {
        mImplementation->CAccessItem::Release();
        mImplementation = nullptr;
    }
}
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    return mImplementation == nullptr ? -1 : mImplementation->Seek(static_cast<int32_t>(offset), whence);
}
int32_t CFile::GetSize() { return mImplementation == nullptr ? -1 : mImplementation->GetSize(); }
int32_t CFile::GetPos() { return mImplementation == nullptr ? -1 : mImplementation->GetPos(); }
CAccessItem* CFile::GetAccessClass() { return mImplementation; }
void CFile::AttachAccessClass(CAccessItem* access) {
    if (access == nullptr) return;
    access->CAccessItem::AddRef();
    if (mImplementation != nullptr) access->AttachAccessClass(mImplementation);
    mImplementation = access;
}

CFileStorage::CFileStorage() : mAccess(nullptr) {}
CFileStorage::~CFileStorage() { Close(); }
CFileStorage& CFileStorage::operator=(const CFileStorage& other) {
    if (&other != this) {
        Close();
        AttachAccessClass(other.mAccess);
    }
    return *this;
}
TbError CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    return mAccess == nullptr ? TbError(-1) : mAccess->FindFirst(pattern, info);
}
TbError CFileStorage::FindNext(CFileInfo& info) {
    return mAccess == nullptr ? TbError(-1) : mAccess->FindNext(info);
}
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    return mAccess == nullptr ? TbError(-1) : mAccess->OpenFile(file, name, mode, data);
}
void CFileStorage::AttachAccessClass(_TbStorageAccess* access) {
    access->_TbStorageAccess::AddRef();
    if (mAccess != nullptr) access->AttachAccessClass(mAccess);
    mAccess = access;
}
TbError CDiscFileStorage::Open(const char* path) {
    Close();
    auto* access = new (std::nothrow) _TbFileStorageAccess();
    if (!access->Open(path).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}

CDiscFile::CDiscFile() {}
CDiscFile::~CDiscFile() {}
TbError CDiscFile::Open(const char* name, uint32_t mode) {
    TbDynamicStringTemplate<char> path(name);
    int32_t split = 20000;
    uint32_t start = 0xffffffff;
    bool found = false;
    while (split > 0 && !found) {
        split = path.FindReverseOneOf("\\/:", start);
        if (split > 0 && !NS_MacDoze::IsDBCSLeadByte(static_cast<uint8_t>(path.mData[split - 1]))) {
            found = true;
            ++split;
        } else start = static_cast<uint32_t>(split - 1);
    }
    if (split == -1) split = 0;
    TbDynamicStringTemplate<char> basename(path.Mid(static_cast<uint32_t>(split)));
    path = path.Left(static_cast<uint32_t>(split));
    CDiscFileStorage storage;
    if (!storage.Open(path.mData).Succeeded()) return TbError(-1);
    if (!storage.OpenFile(*this, basename.mData, mode, nullptr).Succeeded()) return TbError(-1);
    return TbError(0);
}

TbError _TbBufferedFile::Open(CFile& file, uint32_t size) {
    CAccessItem* access = file.GetAccessClass();
    auto* buffer = new (std::nothrow) _TbBufferAccess();
    if (buffer == nullptr) return TbError(static_cast<int32_t>(0xcffe0100));
    if (!buffer->Open(size).Succeeded()) {
        delete buffer;
        return TbError(-1);
    }
    AttachAccessClass(access);
    AttachAccessClass(buffer);
    return TbError(0);
}
}
