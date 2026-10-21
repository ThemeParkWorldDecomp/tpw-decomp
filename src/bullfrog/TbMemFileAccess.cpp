// TbMemFileAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbMemFileAccess code, named after it.

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

_TbMemFileAccess::_TbMemFileAccess(uint32_t blockSize) { // 0x10005f20
    mBlockSize = blockSize;
    mFirst = mLast = mCurrent = nullptr;
    mCursor = nullptr;
    mPosition = mSize = 0;
}
void _TbMemFileAccess::Close() { RemoveAllBlocks(); } // 0x10005fdc

int32_t _TbMemFileAccess::Read(void* dest, uint32_t size) { // 0x10006040
    auto* out = static_cast<uint8_t*>(dest);
    uint32_t total = 0;
    while (size != 0) {
        uint32_t count = mCurrent == nullptr ? 0 :
            (mCurrent == mLast ? mSize - mPosition : mCurrent->mEnd - mPosition);
        if (count == 0) return total;
        if (static_cast<int32_t>(count) > static_cast<int32_t>(size)) {
            std::memcpy(out, mCursor, size);
            total += size;
            mPosition += size;
            mCursor += size;
            size = 0;
        } else {
            std::memcpy(out, mCursor, count);
            out += count;
            total += count;
            size -= count;
            mCurrent = mCurrent->mNext;
            mPosition += count;
            mCursor = mCurrent == nullptr ? nullptr : mCurrent->mData;
        }
    }
    return total;
}

int32_t _TbMemFileAccess::Write(const void* source, uint32_t size) { // 0x100061a8
    auto* in = static_cast<const uint8_t*>(source);
    uint32_t total = 0;
    while (size != 0) {
        if (mCurrent == nullptr) {
            if (!AddBlock().Succeeded()) return total;
            mCurrent = mLast;
            mCursor = mCurrent->mData;
        }
        uint32_t count = mBlockSize - static_cast<uint32_t>(mCursor - mCurrent->mData);
        if (count > size) {
            std::memcpy(mCursor, in, size);
            in += size;
            total += size;
            mPosition += size;
            mCursor += size;
            size = 0;
        } else {
            std::memcpy(mCursor, in, count);
            in += count;
            size -= count;
            total += count;
            mPosition += count;
            mCurrent = mCurrent->mNext;
            mCursor = mCurrent == nullptr ? nullptr : mCurrent->mData;
        }
        if (mPosition > mSize) mSize = mPosition;
    }
    return total;
}

int32_t _TbMemFileAccess::Seek(int32_t offset, uint32_t whence) { // 0x10006360
    if (whence == 1) offset = static_cast<uint32_t>(offset) + mPosition;
    else if (whence == 2) offset = static_cast<uint32_t>(offset) + mSize;
    else if (whence != 0) return mPosition;
    if (offset > static_cast<int32_t>(mSize)) offset = mSize;
    if (offset < 0) offset = 0;
    mPosition = offset;
    mCurrent = nullptr;
    mCursor = nullptr;
    for (Block* block = mFirst; block != nullptr && mCurrent == nullptr; block = block->mNext) {
        if (block->mEnd > mPosition) {
            mCurrent = block;
            mCursor = block->mData + mPosition - block->mStart;
        }
    }
    return mPosition;
}
int32_t _TbMemFileAccess::GetSize() { return mSize; } // 0x10006478
int32_t _TbMemFileAccess::GetPos() { return mPosition; } // 0x100064c4

TbError _TbMemFileAccess::AddBlock() { // 0x10006510
    uint32_t bytes = mBlockSize + static_cast<uint32_t>(sizeof(Block));
    auto* block = static_cast<Block*>(::operator new[](bytes, std::nothrow));
    if (block == nullptr) return TbError(-1);
    std::memset(block, 0, sizeof(Block));
    block->mData = reinterpret_cast<uint8_t*>(block) + sizeof(Block);
    if (mFirst == nullptr) {
        mFirst = mLast = block;
        block->mStart = 0;
    } else {
        mLast->mNext = block;
        block->mPrev = mLast;
        mLast = block;
        block->mStart = block->mPrev->mEnd;
    }
    block->mEnd = block->mStart + mBlockSize;
    return TbError(0);
}

void _TbMemFileAccess::RemoveAllBlocks() { // 0x10006628
    Block* block = mFirst;
    while (block != nullptr) {
        Block* next = block->mNext;
        ::operator delete[](block);
        block = next;
    }
    mFirst = mLast = mCurrent = nullptr;
    mCursor = nullptr;
    mPosition = mSize = 0;
}
_LbSemaphore* _TbMemFileAccess::GetSemaphore() { return &mSemaphore; } // 0x100066e0

_TbMemFileAccess::~_TbMemFileAccess() {} // 0x10006730: blocks survive destruction.
}
