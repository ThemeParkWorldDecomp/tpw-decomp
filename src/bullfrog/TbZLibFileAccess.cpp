// TbZLibFileAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbZLibFileAccess code, named after it.

#include "bfl_FileSystem.h"
#include "bfl_Strings.h"
#include "bfl_WadFile.h"
#include "bfl_ZLib.h"
#include "platform/macdoze.h"
#include <algorithm>
#include <cstring>
#include <new>

// Bullfrog LbSCBack.cpp file access and wide storage wrappers, and LbMaths.cpp CMemFile::Open.

namespace bfl::FileSystem {

_TbZLibFileAccess::_TbZLibFileAccess() : mSize(0), mBuffer(nullptr), mCursor(nullptr) {} // 0x1000a224
TbError _TbZLibFileAccess::Open(uint32_t size) { // 0x1000a2b0
    mSize = size;
    mBuffer = static_cast<uint8_t*>(::operator new[](size, std::nothrow));
    mCursor = mBuffer;
    return TbError(0);
}
int32_t _TbZLibFileAccess::Write(const void*, uint32_t) { return 0; } // 0x1000a34c
int32_t _TbZLibFileAccess::Read(void* dest, uint32_t size) { // 0x1000a39c
    uint32_t count = std::min(size, mSize - static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mCursor) - reinterpret_cast<uintptr_t>(mBuffer)));
    if (count != 0) NS_MacDoze::CopyMemory(dest, mCursor, count);
    mCursor = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(mCursor) + count);
    return count;
}
int32_t _TbZLibFileAccess::Seek(int32_t offset, uint32_t whence) { // 0x1000a46c
    if (whence == 0) {
        if (offset < 0 || offset > static_cast<int32_t>(mSize)) return -1;
        mCursor = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(mBuffer) + offset);
    } else if (whence == 1) {
        uintptr_t cursor = reinterpret_cast<uintptr_t>(mCursor) + static_cast<intptr_t>(offset);
        uintptr_t start = reinterpret_cast<uintptr_t>(mBuffer);
        if (cursor < start || cursor > start + mSize) return -1;
        mCursor = reinterpret_cast<uint8_t*>(cursor);
    } else if (whence == 2) {
        if (offset > 0 || static_cast<int32_t>(0u - static_cast<uint32_t>(offset)) > static_cast<int32_t>(mSize)) return -1;
        uint32_t position = mSize + static_cast<uint32_t>(offset);
        mCursor = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(mBuffer) + position);
    } else {
        return -1;
    }
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mCursor) - reinterpret_cast<uintptr_t>(mBuffer));
}
int32_t _TbZLibFileAccess::GetPos() { // 0x1000a58c
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mCursor) - reinterpret_cast<uintptr_t>(mBuffer));
}
int32_t _TbZLibFileAccess::GetSize() { return mSize; } // 0x1000a5e0
void _TbZLibFileAccess::Close() { // 0x1000a62c
    mSize = 0;
    ::operator delete[](mBuffer);
    mCursor = nullptr;
}
void _TbZLibFileAccess::AttachAccessClass(CAccessItem* access) { // 0x1000a6b8
    CAccessItem::AttachAccessClass(access);
    void* buffer = ::operator new[](0x4000, std::nothrow);
    LbZLib_BufferedDecompress(mBuffer, buffer, 0x4000, _GetMoreData, mNext, nullptr);
    ::operator delete[](buffer);
}
int32_t _GetMoreData(void* dest, uint32_t size, void* context) { // 0x1000a79c
    static_cast<CAccessItem*>(context)->Read(dest, size);
    return 0;
}
}
