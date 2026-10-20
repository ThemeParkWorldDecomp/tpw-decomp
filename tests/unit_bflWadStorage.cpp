// WAD storage with recorded file, tree and compression edges. Strings are functional
// fakes, as in the original u11 file-system tests.
#include "bfl_WadFile.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "calllog.h"
#include "fake_strings.h"
#include <new>
#include <cstdio>

using namespace bfl::FileSystem;
using calllog::ev;
using calllog::lab;
using calllog::ret;

template<> bool TbStringBase<char>::operator!=(const char* text) const {
    return std::strcmp(mData, text) != 0;
}
template<> char* TbStringBase<char>::LockBuffer(uint32_t) { return mData; }
template<> void TbStringBase<char>::ReleaseBuffer(uint32_t) { mLength = std::strlen(mData); }
template<> int32_t TbStringBase<char>::Find(const char* text, uint32_t start) const {
    if (start > static_cast<uint32_t>(mLength)) return -1;
    const char* found = std::strstr(mData + start, text);
    return found == nullptr ? -1 : static_cast<int32_t>(found - mData);
}
template<> int32_t TbStringBase<char>::FindOneOf(const char* text, uint32_t start) const {
    for (uint32_t i = start; i < static_cast<uint32_t>(mLength); ++i)
        if (std::strchr(text, mData[i]) != nullptr) return i;
    return -1;
}
template<> int32_t TbStringBase<char>::FindReverse(const char& ch, uint32_t start) const {
    for (int32_t i = std::min(start, static_cast<uint32_t>(mLength - 1)); i >= 0; --i)
        if (mData[i] == ch) return i;
    return -1;
}

namespace {
uint32_t gHeader[22];
uint32_t gEntry[10];
const char* gFound = "scope:found.dat";
CAccessItem* gOriginal;
int32_t gOperation;
int32_t gReadLabel;
int32_t gEntryAllocation;

void FillInfo(CFileInfo& info) {
    info.mF00 = ret("Info.size", 91);
    info.mAttributes = ret("Info.attributes", 0x10);
    std::memset(info.mF08, 7, sizeof info.mF08);
    std::strcpy(info.mNameBuffer, gFound);
    info.mName = info.mNameBuffer + 6;
}
void Born(const char* name, void* p) {
    ev(name, {lab(p)});
}
}

// The game allocates classes through nothrow new; report their Mac sizes and labels.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    void* p = calllog::on() && ret("allocation failure", 0) ? nullptr : std::malloc(size);
    if (p != nullptr) std::memset(p, 0x5a, size);
    if (calllog::on()) {
        const int32_t label = p != nullptr ? calllog::newlab(p) : 0;
        uint32_t macSize = 24;
        if (size == sizeof(_TbWadStorageAccess)) macSize = 224;
        else if (size == sizeof(_TbSharedAccess)) macSize = 16;
        else if (gEntryAllocation++ == 0) macSize = 28;
        ev("new", {macSize, label});
        if (p != nullptr && macSize == 224) {
            auto* access = static_cast<_TbWadStorageAccess*>(p);
            tpw_cl_label(label + 200, &access->mTree);
            tpw_cl_label(label + 300, &access->mFile);
        }
    }
    return p;
}
namespace {
// Destruction tests retain storage for named-field snapshots after the deleting flag.
void Deleted(void* p, uint32_t flag) {
    if (static_cast<int16_t>(flag) > 0) {
        ev("delete", {lab(p)});
        calllog::forget(p);
    }
}
}

_LbSemaphore::_LbSemaphore() {}
_LbSemaphore::~_LbSemaphore() {}
CAccessItem::CAccessItem() : mNext(nullptr), mReferences(1) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void* buffer, uint32_t count) {
    ev("File.Read", {gReadLabel, count});
    std::memcpy(buffer, count == 88 ? gHeader : gEntry, count);
    return ret("File.Read", count);
}
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(-1); }
int32_t CAccessItem::GetMacFileReference() { return -1; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

_TbStorageAccess::_TbStorageAccess() : mNext(nullptr), mReferences(1) {}
_TbStorageAccess::~_TbStorageAccess() { ev("StorageAccess.dtor", {lab(this)}); }
TbError _TbStorageAccess::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(-1); }
TbError _TbStorageAccess::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError _TbStorageAccess::FindNext(CFileInfo&) { return TbError(-1); }
TbError _TbStorageAccess::FullPath(char*, const char*, uint32_t) { return TbError(-1); }
TbError _TbStorageAccess::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(-1); }
const char* _TbStorageAccess::GetRootPath() { return mRoot.mData; }
TbError _TbStorageAccess::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError _TbStorageAccess::CreateDir(const char*) { return TbError(-1); }
TbError _TbStorageAccess::Delete(const char*) { return TbError(-1); }
TbError _TbStorageAccess::Move(const char*, const char*) { return TbError(-1); }
void _TbStorageAccess::AttachAccessClass(_TbStorageAccess*) {}
void _TbStorageAccess::Close() {}

CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() { ev("File.dtor", {lab(this)}); }
void CFile::Close() {
    ev("File.Close", {lab(this)});
    mImplementation = nullptr;
}
int32_t CFile::Seek(uint32_t offset, uint32_t mode) {
    ev("File.Seek", {lab(this), static_cast<int32_t>(offset), mode});
    return ret("File.Seek", offset);
}
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(-1); }
int32_t CFile::GetMacFileReference() { return -1; }
CAccessItem* CFile::GetAccessClass() {
    ev("File.GetAccess", {lab(this)});
    return mImplementation;
}
void CFile::AttachAccessClass(CAccessItem* access) {
    ev("File.Attach", {lab(this), lab(access)});
    if (access != nullptr) mImplementation = access;
    gReadLabel = lab(this);
}
CDiscFile::CDiscFile() {}
CDiscFile::~CDiscFile() { ev("DiscFile.dtor", {lab(this)}); }
TbError CDiscFile::Open(const char* name, uint32_t flags) {
    ev("DiscFile.Open", {lab(this), flags}, {name});
    const int32_t result = ret("DiscFile.Open", 0);
    if (result >= 0) mImplementation = gOriginal;
    return TbError(result);
}

CFileStorage::CFileStorage() : mAccess(nullptr) {}
CFileStorage::~CFileStorage() { ev("Storage.dtor", {lab(this)}); }
void CFileStorage::Close() { ev("Storage.Close", {lab(this)}); mAccess = nullptr; }
void CFileStorage::AttachAccessClass(_TbStorageAccess* access) {
    ev("Storage.Attach", {lab(this), lab(access)});
    mAccess = access;
}
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    ev("Storage.OpenFile", {lab(this), lab(&file), mode, lab(data)}, {name});
    return TbError(ret("Storage.OpenFile", 0));
}
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(-1); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(-1); }
const char* CFileStorage::GetRootPath() { return ""; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError CFileStorage::CreateDir(const char*) { return TbError(-1); }
TbError CFileStorage::Delete(const char*) { return TbError(-1); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(-1); }
TbError CDiscFileStorage::Open(const char* path) {
    ev("DiscStorage.Open", {lab(this)}, {path});
    return TbError(ret("DiscStorage.Open", 0));
}

template<class T> NodeAllocator<T>::NodeAllocator() : mBlock(nullptr), mUsed(0) {}
template<class T> NodeAllocator<T>::~NodeAllocator() {}
template class bfl::FileSystem::NodeAllocator<DirNode>;
template class bfl::FileSystem::NodeAllocator<FileNode>;
WadNameTree::WadNameTree() : mRoot(nullptr), mNames(nullptr), mIterator(nullptr) {}
WadNameTree::~WadNameTree() { ev("Tree.dtor", {lab(this)}); }
void WadNameTree::Close() { ev("Tree.Close", {lab(this)}); mRoot = nullptr; }
TbError WadNameTree::Initialise(CDiscFile& file, bool borrow) {
    ev("Tree.Initialise", {lab(this), lab(&file), borrow});
    mRoot = reinterpret_cast<DirNode*>(gOriginal);
    return TbError(ret("Tree.Initialise", 0));
}
TbError WadNameTree::FindFirst(CFileInfo& info, const TbDynamicStringTemplate<char>& pattern) {
    ev("Tree.FindFirst", {lab(this)}, {pattern.mData, info.mNameBuffer});
    FillInfo(info);
    return TbError(ret("Tree.FindFirst", 0));
}
TbError WadNameTree::FindNext(CFileInfo& info) {
    ev("Tree.FindNext", {lab(this)}, {info.mNameBuffer});
    FillInfo(info);
    return TbError(ret("Tree.FindNext", 0));
}
TreeIterator::TreeIterator(WadNameTree& tree, const TbDynamicStringTemplate<char>& pattern, uint32_t* offset) {
    ev("Iterator.ctor", {lab(&tree)}, {pattern.mData});
    *offset = ret("Iterator.offset", 88);
}
TreeIterator::~TreeIterator() {}
bool TreeIterator::done() {
    ev("Iterator.done");
    return ret("Iterator.done", 0);
}
TbCharStringList::~TbCharStringList() {}
TbStringMatch::~TbStringMatch() {}

_TbQuickPosAccess::_TbQuickPosAccess() : mPosition(0) {}
_TbQuickPosAccess::~_TbQuickPosAccess() {}
int32_t _TbQuickPosAccess::Read(void*, uint32_t) { return -1; }
int32_t _TbQuickPosAccess::Write(const void*, uint32_t) { return -1; }
int32_t _TbQuickPosAccess::Seek(int32_t, uint32_t) { return -1; }
int32_t _TbQuickPosAccess::GetSize() { return -1; }
int32_t _TbQuickPosAccess::GetPos() { return -1; }
void _TbQuickPosAccess::Close() {}
_TbSharedAccess::_TbSharedAccess() { Born("Shared.ctor", this); }
_TbSharedAccess::~_TbSharedAccess() {}
int32_t _TbSharedAccess::Read(void* buffer, uint32_t size) { return CAccessItem::Read(buffer, size); }
int32_t _TbSharedAccess::Write(const void*, uint32_t) { return -1; }
int32_t _TbSharedAccess::Seek(int32_t, uint32_t) { return -1; }
int32_t _TbSharedAccess::GetSize() { return -1; }
int32_t _TbSharedAccess::GetPos() { return -1; }
void _TbSharedAccess::AttachAccessClass(CAccessItem*) {}

#define COMPRESSION_FAKE(Class, Label) \
Class::Class() { Born(Label ".ctor", this); } \
Class::~Class() { ev(Label ".dtor", {lab(this)}); } \
TbError Class::Open(uint32_t size) { ev(Label ".Open", {lab(this), size}); return TbError(ret(Label ".Open", 0)); } \
int32_t Class::Read(void*, uint32_t) { return -1; } \
int32_t Class::Write(const void*, uint32_t) { return -1; } \
int32_t Class::Seek(int32_t, uint32_t) { return -1; } \
int32_t Class::GetSize() { return -1; } \
int32_t Class::GetPos() { return -1; } \
void Class::Close() {} \
void Class::AttachAccessClass(CAccessItem*) {}
COMPRESSION_FAKE(_TbRefpackFileAccess, "Refpack")
COMPRESSION_FAKE(_TbZLibFileAccess, "Zlib")
#undef COMPRESSION_FAKE
_TbWadFileAccess::_TbWadFileAccess() { Born("WadFile.ctor", this); }
_TbWadFileAccess::~_TbWadFileAccess() { ev("WadFile.dtor", {lab(this)}); }
TbError _TbWadFileAccess::Open(uint32_t size, uint32_t offset) {
    ev("WadFile.Open", {lab(this), size, offset});
    return TbError(ret("WadFile.Open", 0));
}
int32_t _TbWadFileAccess::Read(void*, uint32_t) { return -1; }
int32_t _TbWadFileAccess::Write(const void*, uint32_t) { return -1; }
int32_t _TbWadFileAccess::Seek(int32_t, uint32_t) { return -1; }
int32_t _TbWadFileAccess::GetSize() { return -1; }
int32_t _TbWadFileAccess::GetPos() { return -1; }
const uint8_t* _TbWadFileAccess::MapFile(int32_t, int32_t) { return nullptr; }
void _TbWadFileAccess::Close() {}

namespace NS_MacDoze {
uint32_t GetFileAttributes(const char* path) {
    ev("GetFileAttributes", {}, {path});
    return ret("GetFileAttributes", 0x80);
}
}
TbError LbFile_ToProjectFilename(char* dest, uint32_t size, const char* name) {
    ev("LbFile_ToProjectFilename", {size}, {name});
    std::strcpy(dest, ret("Project.prefix", 1) ? "P:" : "");
    std::strcat(dest, name);
    return TbError(ret("LbFile_ToProjectFilename", 0));
}
namespace SamsUtilities {
void _USwapBlock32(uint32_t*, int32_t count) { ev("SwapBlock32", {count}); }
}

namespace {
_TbWadStorageAccess* gAccess;
CWadFile* gFile;
CWadFileStorage* gStorage;
CFileInfo gInfo;
}
extern "C" {
void tpw_ws_begin(const uint32_t* header, const uint32_t* entry, const char* subpath,
                  int32_t original, int32_t fileOpen, int32_t treeOpen, const char* info) {
    std::memcpy(gHeader, header, sizeof gHeader);
    std::memcpy(gEntry, entry, sizeof gEntry);
    gOriginal = new CAccessItem;
    gAccess = new _TbWadStorageAccess;
    gFile = new CWadFile;
    gStorage = new CWadFileStorage;
    gAccess->mRoot = "root";
    gAccess->mSubpath = subpath;
    gAccess->mPattern = "old-pattern";
    gAccess->mOriginalAccess = original ? gOriginal : nullptr;
    gAccess->mFile.mImplementation = gOriginal;
    std::memcpy(&gAccess->mHeader, gHeader, sizeof gHeader);
    gAccess->mTree.mRoot = treeOpen ? reinterpret_cast<DirNode*>(gOriginal) : nullptr;
    gFile->mImplementation = fileOpen ? gOriginal : nullptr;
    std::memset(static_cast<void*>(&gInfo), 0x5a, sizeof gInfo);
    std::strcpy(gInfo.mNameBuffer, info);
    gInfo.mName = gInfo.mNameBuffer;
    tpw_cl_label(1, gAccess);
    tpw_cl_label(2, gFile);
    tpw_cl_label(3, gStorage);
    tpw_cl_label(4, gOriginal);
    tpw_cl_label(5, &gAccess->mTree);
    tpw_cl_label(6, &gAccess->mFile);
}
int32_t tpw_ws_call(int32_t op, const char* name, const char* path, uint32_t flags) {
    gOperation = op;
    gEntryAllocation = 0;
    switch (op) {
    case 0: new (gAccess) _TbWadStorageAccess; return 0;
    case 1: gAccess->Close(); return 0;
    case 2: return gAccess->Open(name, flags);
    case 3: return gAccess->FindFirst(name, gInfo);
    case 4: return gAccess->FindNext(gInfo);
    case 5: gAccess->StripSubPath(gInfo); return 0;
    case 6: return gAccess->OpenFile(*gFile, name, flags, nullptr);
    case 7: return gAccess->OpenWadItem(*gFile, name);
    case 8: return gAccess->SetAttribs(name, flags);
    case 9: return gAccess->CreateDir(name);
    case 10: return gAccess->Delete(name);
    case 11: return gAccess->Move(name, path);
    case 12: return gStorage->Open(name, flags);
    case 13: return gFile->Open(name, path);
    case 14: gAccess->~_TbWadStorageAccess(); Deleted(gAccess, flags); return 0;
    case 15: gFile->~CWadFile(); Deleted(gFile, flags); return 0;
    default: return -999;
    }
}
void tpw_ws_state(uint32_t* values, char* text) {
    values[0] = lab(gAccess->mOriginalAccess);
    values[1] = lab(gAccess->mFile.mImplementation);
    values[2] = lab(gFile->mImplementation);
    values[3] = gAccess->mFindIndex;
    values[4] = gInfo.mName - gInfo.mNameBuffer;
    values[5] = gInfo.mF00;
    values[6] = gInfo.mAttributes;
    std::memcpy(values + 7, &gAccess->mHeader, sizeof gHeader);
    for (int i = 0; i < 24; ++i) values[29 + i] = gInfo.mF08[i];
    values[53] = lab(gAccess->mTree.mRoot);
    values[54] = lab(gStorage->mAccess);
    std::snprintf(text, 1200, "%s|%s|%s|%s", gAccess->mRoot.mData,
                  gAccess->mSubpath.mData, gAccess->mPattern.mData, gInfo.mNameBuffer);
}
}
