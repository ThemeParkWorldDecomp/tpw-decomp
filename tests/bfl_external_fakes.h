// Outside-u11 methods only. A unit can supply recorded callbacks for these fakes.
#pragma once
#include "bfl_FileSystem.h"
#ifndef BFL_EXTERN_LOG
#define BFL_EXTERN_LOG(...) ((void)0)
#define BFL_LAB(p) ((p) != nullptr ? 1 : 0)
#define BFL_RETURN(name) 0
#endif
#ifndef BFL_ACCESS_DESTROY
#define BFL_ACCESS_DESTROY(p) ((void)0)
#endif
#ifndef BFL_STORAGE_DESTROY
#define BFL_STORAGE_DESTROY(p) ((void)0)
#endif

_LbSemaphore::_LbSemaphore() { BFL_EXTERN_LOG("Semaphore::ctor", {BFL_LAB(this)}); }
_LbSemaphore::~_LbSemaphore() { BFL_EXTERN_LOG("Semaphore::dtor", {BFL_LAB(this)}); }

namespace bfl::FileSystem {
#ifndef TPW_SKIP_ACCESS_FAKES
CAccessItem::~CAccessItem() {
    BFL_ACCESS_DESTROY(this);
    BFL_EXTERN_LOG("Access::dtor", {BFL_LAB(this), BFL_LAB(mNext), mReferences});
}
int32_t CAccessItem::Read(void* p, uint32_t n) {
    BFL_EXTERN_LOG("Access::Read", {BFL_LAB(this), BFL_LAB(p), n});
    return BFL_RETURN("Access::Read");
}
int32_t CAccessItem::Write(const void* p, uint32_t n) {
    BFL_EXTERN_LOG("Access::Write", {BFL_LAB(this), BFL_LAB(p), n});
    return BFL_RETURN("Access::Write");
}
int32_t CAccessItem::Seek(int32_t offset, uint32_t mode) {
    BFL_EXTERN_LOG("Access::Seek", {BFL_LAB(this), offset, mode});
    return BFL_RETURN("Access::Seek");
}
int32_t CAccessItem::GetSize() {
    BFL_EXTERN_LOG("Access::GetSize", {BFL_LAB(this)});
    return BFL_RETURN("Access::GetSize");
}
int32_t CAccessItem::GetPos() {
    BFL_EXTERN_LOG("Access::GetPos", {BFL_LAB(this)});
    return BFL_RETURN("Access::GetPos");
}
const uint8_t* CAccessItem::MapFile(int32_t offset, int32_t length) {
    BFL_EXTERN_LOG("Access::MapFile", {BFL_LAB(this), offset, length});
    return nullptr;
}
void CAccessItem::UnmapFile(const uint8_t* p) {
    BFL_EXTERN_LOG("Access::UnmapFile", {BFL_LAB(this), BFL_LAB(p)});
}
TbError CAccessItem::Flush() {
    BFL_EXTERN_LOG("Access::Flush", {BFL_LAB(this)});
    return TbError(BFL_RETURN("Access::Flush"));
}
int32_t CAccessItem::GetMacFileReference() {
    BFL_EXTERN_LOG("Access::MacRef", {BFL_LAB(this)});
    return BFL_RETURN("Access::MacRef");
}
void CAccessItem::Close() { BFL_EXTERN_LOG("Access::Close", {BFL_LAB(this)}); }
void CAccessItem::AttachAccessClass(CAccessItem* access) {
    BFL_EXTERN_LOG("Access::Attach", {BFL_LAB(this), BFL_LAB(access)});
}
_LbSemaphore* CAccessItem::GetSemaphore() {
    BFL_EXTERN_LOG("Access::Semaphore", {BFL_LAB(this)});
    return nullptr;
}
#endif

// Read/Write/position and access-chain operations are not in the assigned inventory.
#define BFL_IO_FAKES(Class) \
int32_t Class::Read(void* p, uint32_t n) { \
    BFL_EXTERN_LOG(#Class "::Read", {BFL_LAB(this), BFL_LAB(p), n}); \
    return BFL_RETURN(#Class "::Read"); \
} \
int32_t Class::Write(const void* p, uint32_t n) { \
    BFL_EXTERN_LOG(#Class "::Write", {BFL_LAB(this), BFL_LAB(p), n}); \
    return BFL_RETURN(#Class "::Write"); \
} \
int32_t Class::Seek(int32_t offset, uint32_t mode) { \
    BFL_EXTERN_LOG(#Class "::Seek", {BFL_LAB(this), offset, mode}); \
    return BFL_RETURN(#Class "::Seek"); \
} \
int32_t Class::GetSize() { \
    BFL_EXTERN_LOG(#Class "::GetSize", {BFL_LAB(this)}); \
    return BFL_RETURN(#Class "::GetSize"); \
} \
int32_t Class::GetPos() { \
    BFL_EXTERN_LOG(#Class "::GetPos", {BFL_LAB(this)}); \
    return BFL_RETURN(#Class "::GetPos"); \
}
BFL_IO_FAKES(_TbBufferAccess)
BFL_IO_FAKES(_TbQuickPosAccess)
BFL_IO_FAKES(_TbSharedAccess)
#undef BFL_IO_FAKES
_TbBufferAccess::~_TbBufferAccess() { BFL_EXTERN_LOG("Buffer::dtor", {BFL_LAB(this)}); }
_TbQuickPosAccess::~_TbQuickPosAccess() { BFL_EXTERN_LOG("Quick::dtor", {BFL_LAB(this)}); }
_TbSharedAccess::~_TbSharedAccess() { BFL_EXTERN_LOG("Shared::dtor", {BFL_LAB(this)}); }
void _TbBufferAccess::Close() { BFL_EXTERN_LOG("Buffer::Close", {BFL_LAB(this)}); }
void _TbQuickPosAccess::Close() { BFL_EXTERN_LOG("Quick::Close", {BFL_LAB(this)}); }
void _TbBufferAccess::AttachAccessClass(CAccessItem* access) {
    BFL_EXTERN_LOG("Buffer::Attach", {BFL_LAB(this), BFL_LAB(access)});
}
void _TbSharedAccess::AttachAccessClass(CAccessItem* access) {
    BFL_EXTERN_LOG("Shared::Attach", {BFL_LAB(this), BFL_LAB(access)});
}

_TbStorageAccess::~_TbStorageAccess() {
    BFL_STORAGE_DESTROY(this);
    BFL_EXTERN_LOG("StorageAccess::dtor", {BFL_LAB(this)});
}
_TbFileStorageAccess::~_TbFileStorageAccess() { BFL_EXTERN_LOG("DiscAccess::dtor", {BFL_LAB(this)}); }
#define BFL_STORAGE_FAKES(Class) \
TbError Class::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) { \
    BFL_EXTERN_LOG(#Class "::OpenFile", {BFL_LAB(this), BFL_LAB(&file), mode, BFL_LAB(data)}, {name}); \
    return TbError(BFL_RETURN(#Class "::OpenFile")); \
} \
TbError Class::FindFirst(const char* name, CFileInfo& info) { \
    BFL_EXTERN_LOG(#Class "::FindFirst", {BFL_LAB(this), BFL_LAB(&info)}, {name}); \
    return TbError(BFL_RETURN(#Class "::FindFirst")); \
} \
TbError Class::FindNext(CFileInfo& info) { \
    BFL_EXTERN_LOG(#Class "::FindNext", {BFL_LAB(this), BFL_LAB(&info)}); \
    return TbError(BFL_RETURN(#Class "::FindNext")); \
} \
TbError Class::SetAttribs(const char* name, uint32_t attrs) { \
    BFL_EXTERN_LOG(#Class "::SetAttribs", {BFL_LAB(this), attrs}, {name}); \
    return TbError(BFL_RETURN(#Class "::SetAttribs")); \
} \
TbError Class::CreateDir(const char* name) { \
    BFL_EXTERN_LOG(#Class "::CreateDir", {BFL_LAB(this)}, {name}); \
    return TbError(BFL_RETURN(#Class "::CreateDir")); \
} \
TbError Class::Delete(const char* name) { \
    BFL_EXTERN_LOG(#Class "::Delete", {BFL_LAB(this)}, {name}); \
    return TbError(BFL_RETURN(#Class "::Delete")); \
} \
TbError Class::Move(const char* from, const char* to) { \
    BFL_EXTERN_LOG(#Class "::Move", {BFL_LAB(this)}, {from, to}); \
    return TbError(BFL_RETURN(#Class "::Move")); \
}
BFL_STORAGE_FAKES(_TbStorageAccess)
BFL_STORAGE_FAKES(_TbFileStorageAccess)
#undef BFL_STORAGE_FAKES
TbError _TbStorageAccess::FullPath(char* out, const char* name, uint32_t size) {
    BFL_EXTERN_LOG("StorageAccess::FullPath", {BFL_LAB(this), BFL_LAB(out), size}, {name});
    return TbError(0);
}
TbError _TbStorageAccess::FullPath(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char>& name) {
    BFL_EXTERN_LOG("StorageAccess::FullPathString", {BFL_LAB(this), BFL_LAB(&out), BFL_LAB(&name)});
    return TbError(0);
}
const char* _TbStorageAccess::GetRootPath() { return mRoot.mData; }
void _TbStorageAccess::AttachAccessClass(_TbStorageAccess* access) {
    BFL_EXTERN_LOG("StorageAccess::Attach", {BFL_LAB(this), BFL_LAB(access)});
}
void _TbStorageAccess::Close() { BFL_EXTERN_LOG("StorageAccess::Close", {BFL_LAB(this)}); }
void _TbFileStorageAccess::Close() { BFL_EXTERN_LOG("DiscAccess::Close", {BFL_LAB(this)}); }
void _TbFileStorageAccess::_FindClose() { BFL_EXTERN_LOG("DiscAccess::FindClose", {BFL_LAB(this)}); }

void CFileStorage::Close() { BFL_EXTERN_LOG("Storage::Close", {BFL_LAB(this)}); }
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
const char* CFileStorage::GetRootPath() { return ""; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }
_TbBufferedFile::~_TbBufferedFile() { BFL_EXTERN_LOG("BufferedFile::dtor", {BFL_LAB(this)}); }
}
