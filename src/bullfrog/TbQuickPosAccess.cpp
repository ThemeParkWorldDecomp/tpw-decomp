// TbQuickPosAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbQuickPosAccess code, named after it.

#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include <algorithm>
#include <cstring>
#include <new>

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

_TbQuickPosAccess::_TbQuickPosAccess() : mPosition(0) {}
}

// Buffered, position-cached and shared access layers, bullfrog library.

namespace bfl::FileSystem {

void _TbQuickPosAccess::Close() { mPosition = 0; }
int32_t _TbQuickPosAccess::Read(void* dest, uint32_t size) {
    const int32_t count = mNext->Read(dest, size);
    mPosition += count;
    return count;
}
int32_t _TbQuickPosAccess::Write(const void* source, uint32_t size) {
    const int32_t count = mNext->Write(source, size);
    mPosition += count;
    return count;
}
int32_t _TbQuickPosAccess::Seek(int32_t offset, uint32_t whence) {
    mPosition = mNext->Seek(offset, whence);
    return static_cast<int32_t>(mPosition);
}
int32_t _TbQuickPosAccess::GetSize() { return mNext->GetSize(); }
int32_t _TbQuickPosAccess::GetPos() { return static_cast<int32_t>(mPosition); }

_TbQuickPosAccess::~_TbQuickPosAccess() {}
}
