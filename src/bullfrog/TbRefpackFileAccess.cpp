// TbRefpackFileAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbRefpackFileAccess code, named after it.

#include "bfl_Refpack.h"
#include "bfl_WadFile.h"
#include "platform/macdoze.h"
#include <algorithm>
#include <new>

namespace bfl::FileSystem {
_TbRefpackFileAccess::_TbRefpackFileAccess() : mSize(0), mBuffer(nullptr), mCursor(nullptr) {}
TbError _TbRefpackFileAccess::Open(uint32_t size) {
    mSize = size;
    mBuffer = new (std::nothrow) uint8_t[mSize];
    mCursor = mBuffer;
    return TbError(0);
}
int32_t _TbRefpackFileAccess::Write(const void*, uint32_t) { return 0; }
int32_t _TbRefpackFileAccess::Read(void* dest, uint32_t size) {
    const uint32_t remaining = mSize - static_cast<uint32_t>(mCursor - mBuffer);
    const uint32_t count = std::min(size, remaining);
    if (count != 0) NS_MacDoze::CopyMemory(dest, mCursor, count);
    mCursor += count;
    return static_cast<int32_t>(count);
}
int32_t _TbRefpackFileAccess::Seek(int32_t offset, uint32_t whence) {
    if (whence == 0) {
        if (offset < 0 || static_cast<int32_t>(mSize) < offset) return -1;
        mCursor = mBuffer + offset;
    } else if (whence == 1) {
        const int64_t position = (mCursor - mBuffer) + static_cast<int64_t>(offset);
        if (position < 0 || position > mSize) return -1;
        mCursor = mBuffer + position;
    } else if (whence == 2) {
        if (offset > 0 || static_cast<int32_t>(mSize) < static_cast<int32_t>(0u - offset)) return -1;
        mCursor = mBuffer + mSize + offset;
    } else return -1;
    return static_cast<int32_t>(mCursor - mBuffer);
}
int32_t _TbRefpackFileAccess::GetPos() { return static_cast<int32_t>(mCursor - mBuffer); }
int32_t _TbRefpackFileAccess::GetSize() { return static_cast<int32_t>(mSize); }
void _TbRefpackFileAccess::Close() {
    mSize = 0;
    delete[] mBuffer;
    mCursor = nullptr;
}
void _TbRefpackFileAccess::AttachAccessClass(CAccessItem* access) {
    CAccessItem::AttachAccessClass(access);
    const uint32_t size = access->GetSize();
    auto* packed = new uint8_t[size];
    access->Read(packed, size);
    compress::CRefpack decoder;
    decoder.MemDecompress(mBuffer, packed, nullptr);
    delete[] packed;
}
_TbRefpackFileAccess::~_TbRefpackFileAccess() {}
}

namespace bfl::compress {
CRefpack::~CRefpack() {}
CCompressionBase::~CCompressionBase() {}
}
