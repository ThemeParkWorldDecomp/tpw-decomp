// TbWadStorageAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbWadStorageAccess code, named after it.

#include "bfl_FileSystem.h"
#include "bfl_Strings.h"
#include "bfl_WadFile.h"
#include "bfl_ZLib.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "TbStringMatch.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace bfl::FileSystem {

_TbWadStorageAccess::_TbWadStorageAccess() : mSubpath(""), mFindIndex(-1), mOriginalAccess(nullptr) {}
_TbWadStorageAccess::~_TbWadStorageAccess() { Close(); }
void _TbWadStorageAccess::Close() {
    mTree.Close();
    mFile.Close();
    mSubpath = "";
    mRoot = "";
    mOriginalAccess = nullptr;
}

TbError _TbWadStorageAccess::Open(const char* path, uint32_t flags) {
    Close();
    mRoot = "";
    mSubpath = "";
    TbDynamicStringTemplate<char> name(path);
    char* text = name.LockBuffer(name.Length());
    for (int32_t split = name.FindOneOf("/:\\", 0); split >= 0;
         split = name.Find("/:\\", static_cast<uint32_t>(split))) text[split] = ':';
    name.ReleaseBuffer(0x7fffffff);
    int32_t split = name.Find("*:", 0);
    if (split >= 0) {
        mSubpath = name.Mid(static_cast<uint32_t>(split + 1));
        if (mSubpath.Right(1) != ":") mSubpath += ":";
        name = name.Left(static_cast<uint32_t>(split));
    }
    char full[260];
    LbFile_ToProjectFilename(full, 260, name.mData);
    if ((NS_MacDoze::GetFileAttributes(full) & 0x10) != 0) return TbError(-1);
    if (!mFile.Open(full, (flags & 0x8000) != 0 ? 0x80008001 : 0x80000001).Succeeded()) return TbError(-1);
    mOriginalAccess = mFile.GetAccessClass();
    auto* shared = new (std::nothrow) _TbSharedAccess;
    mFile.AttachAccessClass(shared);
    if (mFile.Read(&mHeader, 0x58) != 0x58) {
        mFile.Close();
        return TbError(-1);
    }
    SamsUtilities::USwapInplace32(mHeader.mSignature);
    SamsUtilities::USwapInplace32(mHeader.mVersion);
    SamsUtilities::USwapInplace32(mHeader.mCount);
    SamsUtilities::_USwapBlock32(&mHeader.mNameOffset, 3);
    if (mHeader.mSignature != 0x42465744 || mHeader.mVersion > 2) {
        mFile.Close();
        return TbError(-1);
    }
    mTree.Initialise(mFile, mHeader.mVersion == 2);
    CFileInfo info;
    if (mSubpath != "" && (!mTree.FindFirst(info, mSubpath).Succeeded() || info.mAttributes != 0x10)) {
        mFile.Close();
        return TbError(-1);
    }
    mRoot = full;
    mRoot += "*";
    mRoot += static_cast<const TbStringBase<char>&>(mSubpath);
    return TbError(0);
}

TbError _TbWadStorageAccess::FindFirst(const char* pattern, CFileInfo& info) {
    if (mOriginalAccess == nullptr || pattern == nullptr || *pattern == 0) return TbError(-1);
    int32_t separator = static_cast<int32_t>(std::strlen(pattern)) - 1;
    while (separator >= 0 && pattern[separator] != ':' && pattern[separator] != '/') --separator;
    std::strcat(info.mNameBuffer, pattern);
    info.mName = info.mNameBuffer + separator + 1;
    TbDynamicStringTemplate<char> full(mRoot);
    full += pattern;
    mPattern = mSubpath;
    mPattern += info.mNameBuffer;
    if (!mTree.FindFirst(info, mPattern).Succeeded()) return TbError(-1);
    StripSubPath(info);
    return TbError(0);
}

void _TbWadStorageAccess::StripSubPath(CFileInfo& info) {
    const int32_t length = mSubpath.Length();
    if (length > 0) {
        char* source = info.mNameBuffer + length - 1;
        char* dest = info.mNameBuffer;
        do { *dest++ = *source; } while (*source++ != 0);
    }
}

TbError _TbWadStorageAccess::FindNext(CFileInfo& info) {
    if (mOriginalAccess == nullptr || !mTree.FindNext(info).Succeeded()) return TbError(-1);
    StripSubPath(info);
    return TbError(0);
}

TbError _TbWadStorageAccess::OpenFile(CFile& file, const char* name, uint32_t mode, void*) {
    if (mOriginalAccess == nullptr || (mode & 0x40000012) != 0 || file.GetAccessClass() != nullptr)
        return TbError(-1);
    file.AttachAccessClass(mOriginalAccess);
    return OpenWadItem(file, name);
}

TbError _TbWadStorageAccess::OpenWadItem(CFile& file, const char* name) {
    TbDynamicStringTemplate<char> full(mSubpath);
    full += name;
    file.AttachAccessClass(new (std::nothrow) _TbSharedAccess);
    if (mTree.mRoot == nullptr) mTree.Initialise(mFile, mHeader.mVersion == 2);
    uint32_t offset;
    TreeIterator iterator(mTree, full, &offset);
    if (iterator.done()) {
        file.Close();
        return TbError(-1);
    }
    file.Seek(static_cast<int32_t>(offset), 0);
    uint32_t entry[10];
    file.Read(entry, sizeof entry);
    for (int i = 0; i < 6; ++i) SamsUtilities::USwapInplace32(entry[i]);
    SamsUtilities::USwapBlock32(entry + 6, 4);
    file.Seek(static_cast<int32_t>(entry[3]), 0);
    auto* access = new (std::nothrow) _TbWadFileAccess;
    if (!access->Open(entry[4], entry[3]).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    file.AttachAccessClass(access);
    if ((entry[5] & 1) != 0) return TbError(-1);
    if ((entry[5] & 2) != 0) {
        auto* compressed = new (std::nothrow) _TbZLibFileAccess;
        if (!compressed->Open(entry[6]).Succeeded()) {
            delete compressed;
            return TbError(-1);
        }
        file.AttachAccessClass(compressed);
    } else if ((entry[5] & 4) != 0) {
        auto* compressed = new (std::nothrow) _TbRefpackFileAccess;
        if (!compressed->Open(entry[6]).Succeeded()) {
            delete compressed;
            return TbError(-1);
        }
        file.AttachAccessClass(compressed);
    }
    return TbError(0);
}

TbError _TbWadStorageAccess::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError _TbWadStorageAccess::CreateDir(const char*) { return TbError(0); }
TbError _TbWadStorageAccess::Delete(const char*) { return TbError(-1); }
TbError _TbWadStorageAccess::Move(const char*, const char*) { return TbError(-1); }

} // namespace bfl::FileSystem

// TbStringMatch (bullfrog library): a wildcard mask kept as a list of parts. A part is a run
// of plain text, or a '*' or '?' followed by the plain text after it; a run of wildcards
// after a '*' is one '*'. MatchString walks the parts: '?' takes exactly one character, '*'
// goes to the first place its text occurs (it never tries a later one), and a last part of
// plain text or '?' must end the string, while anything may follow a last '*' part.

TbStringMatch::~TbStringMatch() {}
TbCharStringList::~TbCharStringList() {}

// Bullfrog LbSCBack.cpp file access and wide storage wrappers, and LbMaths.cpp CMemFile::Open.

namespace bfl::FileSystem {

_TbZLibFileAccess::~_TbZLibFileAccess() {} // 0x10009c3c: buffer survives destruction.
}

// The name tree of a WAD archive (bullfrog library): the archive's directory read into a tree
// of folders and files, and the iterator that answers a search pattern or an exact name.
//
// Initialise reads the directory once: every name is folded to lower case, an entry whose
// name has a folder part moves the "current folder", and an entry without one goes into the
// folder of the entry before it. A lookup of a name without wildcards compares the hash the
// archive stores for each entry before it compares names; a pattern goes through the masks
// of TbStringMatch. Nodes come from allocators that hand out blocks of 100.

namespace bfl::FileSystem {

TreeIterator::~TreeIterator() {}

} // namespace bfl::FileSystem
