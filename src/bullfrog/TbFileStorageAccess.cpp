// TbFileStorageAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbFileStorageAccess code, named after it.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include <cstdlib>
#include <cstring>
#include <new>

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

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

}

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
}  // namespace

// ---- _TbFileStorageAccess -------------------------------------------------------------
_TbFileStorageAccess::~_TbFileStorageAccess() { _FindClose(); }
void _TbFileStorageAccess::Close() {}   // 0x100053f0

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

// 0x10005780: size, attributes, the three file times (write, access, creation), the name.
void _TbFileStorageAccess::ConvertFindData(CFileInfo& info, const NS_MacDoze::_WIN32_FIND_DATAA& data) {
    info.mF00 = data.nFileSizeLow;
    info.mAttributes = data.dwFileAttributes;
    std::memcpy(info.mF08, data.ftLastWriteTime, 8);
    std::memcpy(info.mF08 + 8, data.ftLastAccessTime, 8);
    std::memcpy(info.mF08 + 16, data.ftCreationTime, 8);
    std::strcpy(info.mName, data.cFileName);
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

const char* _TbStorageAccess::GetRootPath() { return mRoot.mData; }   // 0x10005ed0

}  // namespace bfl::FileSystem
