// TbStorageAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbStorageAccess code, named after it.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include <cstdlib>
#include <cstring>
#include <new>

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

_TbStorageAccess::_TbStorageAccess() : mNext(nullptr), mReferences(0) {}
void _TbStorageAccess::AddRef() { NS_MacDoze::InterlockedIncrement(&mReferences); }
}

// The bullfrog file system's disc side, the part reached through vtables: the access
// object behind a disc file (_TbDiscAccess), the disc storage's members
// (_TbFileStorageAccess: opening, listing, deleting, moving), the storage access base
// (_TbStorageAccess), CAccessItem's own slots, and the CFile and CFileStorage members that
// forward to them (bullfrog shared, 0x10004938..0x10005dc8, 0x10007a9c..0x10008024,
// 0x1000a86c..0x1000ae98, 0x1000b224..0x1000c220). The disk itself is macdoze's Win32
// file API (include/platform/macdoze.h).

namespace bfl::FileSystem {

// ---- _TbStorageAccess ---------------------------------------------------------------
_TbStorageAccess::~_TbStorageAccess() {}
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

}  // namespace bfl::FileSystem
