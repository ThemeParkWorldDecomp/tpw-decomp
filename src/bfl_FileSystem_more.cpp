// Bullfrog LbSCBack.cpp file access and wide storage wrappers, and LbMaths.cpp CMemFile::Open.
#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "bfl_Strings.h"
#include "bfl_ZLib.h"
#include "platform/macdoze.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace bfl::FileSystem {

TbError CFileStorage::FindFirst(const char16_t* pattern, CUniFileInfo& info) { // 0x1000c2b4
    CFileInfo entry;
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, pattern);
    if (static_cast<int32_t>(error) < 0) return error;
    error = FindFirst(name, entry);
    if (static_cast<int32_t>(error) >= 0) {
        info.mF00 = entry.mF00;
        info.mAttributes = entry.mAttributes;
        std::memcpy(info.mF08, entry.mF08, sizeof info.mF08);
        error = bfl::Strings::Convert(info.mNameBuffer, 0x104, entry.mNameBuffer);
        info.mName = info.mNameBuffer;
        if (static_cast<int32_t>(error) >= 0) info.mName += entry.mName - entry.mNameBuffer;
    }
    return error;
}

TbError CFileStorage::FindNext(CUniFileInfo& info) { // 0x1000c448
    CFileInfo entry;
    TbError error = FindNext(entry);
    if (static_cast<int32_t>(error) >= 0) {
        info.mF00 = entry.mF00;
        info.mAttributes = entry.mAttributes;
        std::memcpy(info.mF08, entry.mF08, sizeof info.mF08);
        error = bfl::Strings::Convert(info.mNameBuffer, 0x104, entry.mNameBuffer);
        info.mName = info.mNameBuffer;
        if (static_cast<int32_t>(error) >= 0) info.mName += entry.mName - entry.mNameBuffer;
    }
    return error;
}

TbError CFileStorage::OpenFile(CFile& file, const char16_t* path, uint32_t mode, void* data) { // 0x1000c5b0
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return OpenFile(file, name, mode, data);
}

TbError CFileStorage::FullPath(char16_t* path, const char16_t* source, uint32_t size) { // 0x1000c69c
    char name[0x104];
    char full[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, source);
    if (static_cast<int32_t>(error) < 0) return error;
    error = FullPath(full, name, size);
    if (static_cast<int32_t>(error) < 0) return error;
    return bfl::Strings::Convert(path, size, full);
}

TbError CFileStorage::FullPath(TbDynamicStringTemplate<char16_t>& path,
                              const TbDynamicStringTemplate<char16_t>& source) { // 0x1000c7a0
    char name[0x104];
    char full[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, source.mData);
    if (static_cast<int32_t>(error) < 0) return error;
    error = FullPath(full, name, 0x104);
    if (static_cast<int32_t>(error) < 0) return error;
    return bfl::Strings::Convert(path, full);
}

TbError CFileStorage::SetAttribs(const char16_t* path, uint32_t attributes) { // 0x1000c8ec
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return SetAttribs(name, attributes);
}

TbError CFileStorage::CreateDir(const char16_t* path) { // 0x1000c9c4
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return CreateDir(name);
}

TbError CFileStorage::Delete(const char16_t* path) { // 0x1000ca88
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return Delete(name);
}

TbError CFileStorage::Move(const char16_t* from, const char16_t* to) { // 0x1000cb48
    char first[0x104];
    char second[0x104];
    TbError error = bfl::Strings::Convert(first, 0x104, from);
    if (static_cast<int32_t>(error) < 0) return error;
    error = bfl::Strings::Convert(second, 0x104, to);
    if (static_cast<int32_t>(error) < 0) return error;
    return Move(first, second);
}

TbError CDiscFileStorage::Open(const char16_t* path) { // 0x1000b060
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    Close();
    auto* access = new (std::nothrow) _TbFileStorageAccess;
    if (!access->Open(name).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}

TbError CWadFileStorage::Open(const char16_t* path, uint32_t flags) { // 0x1000d3b8
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    Close();
    auto* access = new (std::nothrow) _TbWadStorageAccess;
    if (!access->Open(name, flags).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}

_TbMemFileAccess::_TbMemFileAccess(uint32_t blockSize) { // 0x10005f20
    mBlockSize = blockSize;
    mFirst = mLast = mCurrent = nullptr;
    mCursor = nullptr;
    mPosition = mSize = 0;
}
_TbMemFileAccess::~_TbMemFileAccess() {} // 0x10006730: blocks survive destruction.
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

TbError CMemFile::Open(uint32_t size) { // 0x1002951c
    if (mImplementation != nullptr) return TbError(-1);
    auto* access = new (std::nothrow) _TbMemFileAccess(size);
    AttachAccessClass(access);
    return TbError(0);
}

_TbZLibFileAccess::_TbZLibFileAccess() : mSize(0), mBuffer(nullptr), mCursor(nullptr) {} // 0x1000a224
_TbZLibFileAccess::~_TbZLibFileAccess() {} // 0x10009c3c: buffer survives destruction.
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
