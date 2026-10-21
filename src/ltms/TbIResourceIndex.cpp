// TbIResourceIndex.cpp: name proven by __sinit_TbIResourceIndex_cpp in the ltms library.

#include "platform/ltms.h"
#include "TbIResourceIndex.h"
#include "TbIRLE4bitFont.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

// ltms: multibyte character sets and the multibyte to Unicode converter (ltms shared,
// 0x1000063c, 0x10002f3c, 0x10003204..0x10003338). Their loaders are in
// TbResourceFactories.cpp.

TbIMBToUnicode::~TbIMBToUnicode() {}

// The counter is 16 bits and is decremented before it is tested, so running out of room
// leaves it at 0xffff and the "too small" answer comes only when the text ends with the
// counter exactly at zero (the last character is then overwritten by the terminator).
TbError TbIMBToUnicode::ConvertString(char16_t* out, const unsigned char* source, uint16_t size) {
    uint16_t remaining = size;
    while (*source != 0) {
        const bool exhausted = remaining == 0;
        --remaining;
        if (exhausted) break;
        uint16_t index = static_cast<uint16_t>(*source++ - 1);
        if (mFirstLead <= index)
            index = static_cast<uint16_t>(index * 0xff + mFirstLead * -0xfe + *source++ - 1);
        *out++ = index < mCount ? mTable[index] : 0;
    }
    if (remaining == 0) {
        out[-1] = 0;
        return TbError(-1);
    }
    *out = 0;
    return TbError(0);
}

namespace {
MBToUnicodeFactory mb;
UnicodeToMBFactory unicode;
MBInfoFactory info;
TableFactory table;
RLE4bitFontFactory font;
}
TbResourceFactory* gLtmsResourceFactories[6] = {&mb, &unicode, &info, &table, &font, nullptr};

TbIResourceIndex::TbIResourceIndex() : mFirst(nullptr), mLast(nullptr), mData(nullptr) {}

TbIResourceIndex::~TbIResourceIndex() { Close(); }

// 0x1000362c
void TbIResourceIndex::ReleaseItem(resourceIndexItem* item) {
    if (item) {
        if (item->mResource) item->mResource->ManagerRelease();
        item->mResource = nullptr;
    }
}
// 0x100036d8: the handle is an item; its resource (Mac +8) is released through ManagerRelease, then cleared.
void TbIResourceIndex::ReleaseResource(void* handle) {
    auto* item = static_cast<resourceIndexItem*>(handle);
    if (item) {
        if (item->mResource) item->mResource->ManagerRelease();
        item->mResource = nullptr;
    }
}

void TbIResourceIndex::Close() {
    if (mFirst) {
        for (auto* item = mFirst; item <= mLast; ++item) {
            if (item->mResource) item->mResource->ManagerRelease();
            item->mResource = nullptr;
        }
        mFirst = nullptr;
    }
    mStorage.Close();
}

// 0x1000383c (unit v5L21): the binary search over the sorted ids between the sentinels mFirst - 1 and mLast + 1; NULL when absent.
TbIResourceIndex::resourceIndexItem* TbIResourceIndex::GetItem(uint32_t id) {
    // Integer positions preserve the original's sentinels without forming a pointer
    // before the allocation.
    std::ptrdiff_t low = -1;
    std::ptrdiff_t high = mLast - mFirst + 1;
    while (high - low > 1) {
        const auto mid = low + (high - low) / 2;
        auto* item = mFirst + mid;
        if (item->mId == id) return item;
        if (id < item->mId) high = mid;
        else low = mid;
    }
    return nullptr;
}

TbError TbIResourceIndex::GetResource(void* handle, void** out) {
    *out = nullptr;
    auto* item = static_cast<resourceIndexItem*>(handle);
    if (!item) return TbError(-1);
    if (item->mResource) {
        *out = item->mResource;
        item->mResource->AddRef();
        return TbError(0);
    }
    return OpenResource(item, out);
}

TbError TbIResourceIndex::GetPreOpenedResource(void* handle, void** out) {
    auto* item = static_cast<resourceIndexItem*>(handle);
    if (item == nullptr) return TbError(-1);
    if (item->mResource != nullptr) item->mResource->AddRef();
    *out = item->mResource;
    return TbError(*out != nullptr ? 0 : -1);
}

TbError TbIResourceIndex::OpenResource(resourceIndexItem* item, void** out) {
    *out = nullptr;
    if (item->mType >= 6) return TbError(-1);
    bfl::FileSystem::CFile file;
    if (!OpenResourceFile(file, item).Succeeded()) return TbError(-1);
    auto* factory = gLtmsResourceFactories[item->mType];
    if (!factory) return TbError(-1);
    if (!factory->CreateInterface(file, out, item->mId, this).Succeeded()) return TbError(-1);
    item->mResource = static_cast<TbResource*>(*out);
    item->mResource->ManagerAddRef();
    return TbError(0);
}

TbError TbIResourceIndex::OpenResourceFile(bfl::FileSystem::CFile& file, resourceIndexItem* item) {
    const auto* name = reinterpret_cast<const char*>(mData) + item->mFilenameOffset;
    return TbError(mStorage.OpenFile(file, name, 0x80000001u, nullptr).Succeeded() ? 0 : -1);
}

TbError TbIResourceIndex::Initialise(const char* path, void* data, uint32_t count) {
    if (!mStorage.Open(path).Succeeded()) return TbError(-1);
    mData = static_cast<uint8_t*>(data);
    mItems.resize(count ? count : 1);
    mFirst = mItems.data();
    mLast = mFirst + count - 1;
    // Decode each fixed 12-byte record only when the original reaches it.
    auto decode = [this](resourceIndexItem* value) {
        const auto* bytes = mData + (value - mFirst) * 12;
        std::memcpy(&value->mId, bytes, 4);
        std::memcpy(&value->mType, bytes + 4, 2);
        std::memcpy(&value->mFilenameOffset, bytes + 6, 2);
        SamsUtilities::USwapInplace32(value->mId);
        SamsUtilities::USwapInplace16(value->mType);
        SamsUtilities::USwapInplace16(value->mFilenameOffset);
    };
    auto* item = mFirst;
    decode(item);
    item->mResource = nullptr;
    std::memset(mData + 8, 0, 4);
    uint32_t previous = item->mId;
    for (++item; item <= mLast; ++item) {
        decode(item);
        if (item->mId <= previous) {
            delete[] mData;
            mFirst = nullptr;
            return TbError(-1);
        }
        item->mResource = nullptr;
        std::memset(mData + (item - mFirst) * 12 + 8, 0, 4);
        previous = item->mId;
    }
    return TbError(0);
}

uint16_t TbIResourceIndex::GetResourceType(void* handle) {
    return handle ? static_cast<resourceIndexItem*>(handle)->mType : 6;
}

// The same search, emitted again (128 bytes each).
void* TbIResourceIndex::GetResourceItemHandle(uint32_t id) { return GetItem(id); }

// Unit v51120 (ltms): the small functions of the range 0x100032a8..0x10009400: destructors with the hidden delete flag, the
// reference-releasing smart pointers, two byte swaps, two what() answers, TbMBStringVTag::Format and the resource index's
// static initialiser. (IsDelimiter, SetHorizontalJustification and TbITable::AddRef are in their own files.)

// ---- the static initialiser
char gRLE4bitFontFactoryVtbl[16];
char gTableFactoryVtbl[16];
char gMBInfoFactoryVtbl[16];
char gUnicodeToMBFactoryVtbl[16];
char gMBToUnicodeFactoryVtbl[16];
const void* gRLE4bitFontFactoryVptr;
const void* gTableFactoryVptr;
const void* gMBInfoFactoryVptr;
const void* gUnicodeToMBFactoryVptr;
const void* gMBToUnicodeFactoryVptr;

void __sinit_TbIResourceIndex_cpp() {   // 0x10004068
    gMBToUnicodeFactoryVptr = gMBToUnicodeFactoryVtbl;
    gUnicodeToMBFactoryVptr = gUnicodeToMBFactoryVtbl;
    gMBInfoFactoryVptr = gMBInfoFactoryVtbl;
    gTableFactoryVptr = gTableFactoryVtbl;
    gRLE4bitFontFactoryVptr = gRLE4bitFontFactoryVtbl;
}
