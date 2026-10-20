#include "TbIResourceIndex.h"
#include "platform/ltms.h"
#include <cstring>

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

TbError TbIResourceIndex::GetPreOpenedResource(void* handle, void** out) {
    auto* item = static_cast<resourceIndexItem*>(handle);
    if (item == nullptr) return TbError(-1);
    if (item->mResource != nullptr) item->mResource->AddRef();
    *out = item->mResource;
    return TbError(*out != nullptr ? 0 : -1);
}

uint16_t TbIResourceIndex::GetResourceType(void* handle) {
    return handle ? static_cast<resourceIndexItem*>(handle)->mType : 6;
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

// The same search, emitted again (128 bytes each).
void* TbIResourceIndex::GetResourceItemHandle(uint32_t id) { return GetItem(id); }
