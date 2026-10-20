// Buffered, position-cached and shared access layers, bullfrog library.
#include "bfl_FileSystem.h"
#include <algorithm>
#include <cstring>

namespace bfl::FileSystem {

_TbBufferAccess::~_TbBufferAccess() {}
void _TbBufferAccess::Close() { delete[] mBuffer; }

int32_t _TbBufferAccess::BasicRead(void* dest, uint32_t size) {
    if (mPosition != mBaseOffset) {
        mBaseOffset = mNext->Seek(static_cast<int32_t>(mPosition - mBaseOffset), 1);
        if (mBaseOffset != mPosition) return 0;
    }
    const int32_t count = mNext->Read(dest, size);
    mBaseOffset += count;
    return count;
}

int32_t _TbBufferAccess::ReadDirectToOutputBuffer(void* dest, uint32_t size) {
    const int32_t count = BasicRead(dest, size);
    mPosition += count;
    return count;
}

uint32_t _TbBufferAccess::CacheBufferToOutputBuffer(void* dest, uint32_t size) {
    if (mPosition >= mCacheEnd || mPosition < mCacheStart) return 0;
    const uint32_t count = std::min(size, mCacheEnd - mPosition);
    std::memcpy(dest, mCacheBuffer + (mPosition - mCacheStart), count);
    mPosition += count;
    return count;
}

void _TbBufferAccess::RefillCacheBuffer() {
    const int32_t count = BasicRead(mBuffer, mBufferSize);
    mCacheBuffer = mBuffer;
    mCacheStart = mBaseOffset - count;
    mCacheEnd = mBaseOffset;
}

int32_t _TbBufferAccess::Read(void* dest, uint32_t size) {
    if (mNext == nullptr) return 0;
    if ((mPosition < mCacheStart || mPosition >= mCacheEnd) && size < mBufferSize)
        RefillCacheBuffer();
    const uint32_t cached = CacheBufferToOutputBuffer(dest, size);
    int32_t direct = 0;
    if (size - cached != 0)
        direct = ReadDirectToOutputBuffer(static_cast<uint8_t*>(dest) + cached, size - cached);
    return static_cast<int32_t>(cached + direct);
}

int32_t _TbBufferAccess::Seek(int32_t offset, uint32_t whence) {
    if (mNext == nullptr) return -1;
    const uint32_t size = GetSize();
    if (whence == 0) mPosition = offset;
    else if (whence == 1) mPosition += offset;
    else if (whence == 2) mPosition = size + offset;
    if (mPosition > size) {
        mPosition = size;
        return -1;
    }
    return static_cast<int32_t>(mPosition);
}

int32_t _TbBufferAccess::Write(const void* source, uint32_t size) {
    if (mPosition != mBaseOffset) {
        mBaseOffset = mNext->Seek(static_cast<int32_t>(mPosition - mBaseOffset), 1);
        if (mBaseOffset != mPosition) return 0;
    }
    const int32_t count = mNext->Write(source, size);
    mBaseOffset += count;
    if (mPosition < mCacheStart || mPosition >= mCacheEnd) {
        if (mBaseOffset < mCacheStart || mBaseOffset >= mCacheEnd) {
            if (mBaseOffset >= mCacheEnd && mPosition < mCacheStart) mCacheStart = mCacheEnd;
        } else {
            const uint32_t advance = mBaseOffset - mCacheStart;
            mCacheStart += advance;
            mCacheBuffer += advance;
        }
    } else {
        mCacheEnd = mPosition;
    }
    mPosition = mBaseOffset;
    return count;
}

int32_t _TbBufferAccess::GetPos() { return mNext == nullptr ? -1 : static_cast<int32_t>(mPosition); }
int32_t _TbBufferAccess::GetSize() { return mNext == nullptr ? -1 : mNext->GetSize(); }
void _TbBufferAccess::AttachAccessClass(CAccessItem* access) {
    CAccessItem::AttachAccessClass(access);
    mBaseOffset = mNext->GetPos();
    mPosition = mBaseOffset;
}

_TbQuickPosAccess::~_TbQuickPosAccess() {}
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

_TbSharedAccess::~_TbSharedAccess() {}
void _TbSharedAccess::ValidateFilePosition() {
    if (static_cast<uint32_t>(mNext->GetPos()) != mPosition)
        mNext->Seek(static_cast<int32_t>(mPosition), 0);
}
void _TbSharedAccess::AttachAccessClass(CAccessItem* access) {
    CAccessItem::AttachAccessClass(access);
    mPosition = mNext->GetPos();
}
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
}
