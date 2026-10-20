#include "bfl_WadFile.h"
#include "bfl_Refpack.h"
#include "platform/macdoze.h"
#include <algorithm>
#include <new>

namespace bfl::FileSystem {
_TbWadFileAccess::_TbWadFileAccess()
    : mSize(0), mPosition(0), mBaseOffset(0), mView(nullptr) {}
_TbWadFileAccess::~_TbWadFileAccess() {}
TbError _TbWadFileAccess::Open(uint32_t size, uint32_t baseOffset) {
    mSize = size;
    mPosition = 0;
    mBaseOffset = baseOffset;
    mView = nullptr;
    return TbError(0);
}
void _TbWadFileAccess::Close() { mNext->UnmapFile(mView); }
int32_t _TbWadFileAccess::Write(const void*, uint32_t) { return 0; }
int32_t _TbWadFileAccess::Read(void* dest, uint32_t size) {
    const uint32_t count = std::min(size, mSize - mPosition);
    const int32_t read = count == 0 ? 0 : mNext->Read(dest, count);
    mPosition += read;
    return read;
}
int32_t _TbWadFileAccess::Seek(int32_t offset, uint32_t whence) {
    if (whence == 0) {
        if (offset < 0 || static_cast<int32_t>(mSize) < offset) return -1;
        offset = static_cast<int32_t>(mBaseOffset + offset);
    } else if (whence == 1) {
        if (mPosition + static_cast<uint32_t>(offset) > mSize) return -1;
    } else if (whence == 2) {
        if (offset > 0 || static_cast<int32_t>(mSize) < static_cast<int32_t>(0u - offset)) return -1;
        whence = 0;
        offset = static_cast<int32_t>(mBaseOffset + mSize + offset);
    } else return -1;
    const int32_t position = mNext->Seek(offset, whence);
    if (position < 0) return -1;
    mPosition = static_cast<uint32_t>(position) - mBaseOffset;
    return static_cast<int32_t>(mPosition);
}
int32_t _TbWadFileAccess::GetPos() { return static_cast<int32_t>(mPosition); }
int32_t _TbWadFileAccess::GetSize() { return static_cast<int32_t>(mSize); }
const uint8_t* _TbWadFileAccess::MapFile(int32_t, int32_t) {
    if (mView == nullptr) mView = mNext->MapFile(mBaseOffset, mSize);
    return mView;
}

_TbRefpackFileAccess::_TbRefpackFileAccess() : mSize(0), mBuffer(nullptr), mCursor(nullptr) {}
_TbRefpackFileAccess::~_TbRefpackFileAccess() {}
TbError _TbRefpackFileAccess::Open(uint32_t size) {
    mSize = size;
    mBuffer = new (std::nothrow) uint8_t[mSize];
    mCursor = mBuffer;
    return TbError(0);
}
void _TbRefpackFileAccess::Close() {
    mSize = 0;
    delete[] mBuffer;
    mCursor = nullptr;
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
void _TbRefpackFileAccess::AttachAccessClass(CAccessItem* access) {
    CAccessItem::AttachAccessClass(access);
    const uint32_t size = access->GetSize();
    auto* packed = new uint8_t[size];
    access->Read(packed, size);
    compress::CRefpack decoder;
    decoder.MemDecompress(mBuffer, packed, nullptr);
    delete[] packed;
}
}
