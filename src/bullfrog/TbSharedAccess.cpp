// TbSharedAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSharedAccess code, named after it.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include <algorithm>
#include <cstring>
#include <new>

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

_TbSharedAccess::_TbSharedAccess() { mPosition = 0; }
}

// Buffered, position-cached and shared access layers, bullfrog library.

namespace bfl::FileSystem {

int32_t _TbSharedAccess::Read(void* dest, uint32_t size) {
    _LbSemaLock lock(*mNext->GetSemaphore());
    lock.Lock(1);
    ValidateFilePosition();
    return _TbQuickPosAccess::Read(dest, size);
}
int32_t _TbSharedAccess::Write(const void* source, uint32_t size) {
    _LbSemaLock lock(*mNext->GetSemaphore());
    lock.Lock(1);
    ValidateFilePosition();
    return _TbQuickPosAccess::Write(source, size);
}
int32_t _TbSharedAccess::Seek(int32_t offset, uint32_t whence) {
    _LbSemaLock lock(*mNext->GetSemaphore());
    lock.Lock(1);
    ValidateFilePosition();
    return _TbQuickPosAccess::Seek(offset, whence);
}
int32_t _TbSharedAccess::GetSize() {
    _LbSemaLock lock(*mNext->GetSemaphore());
    lock.Lock(1);
    ValidateFilePosition();
    return _TbQuickPosAccess::GetSize();
}
int32_t _TbSharedAccess::GetPos() { return static_cast<int32_t>(mPosition); }
void _TbSharedAccess::ValidateFilePosition() {
    if (static_cast<uint32_t>(mNext->GetPos()) != mPosition)
        mNext->Seek(static_cast<int32_t>(mPosition), 0);
}
void _TbSharedAccess::AttachAccessClass(CAccessItem* access) {
    CAccessItem::AttachAccessClass(access);
    mPosition = mNext->GetPos();
}
_TbSharedAccess::~_TbSharedAccess() {}
}
