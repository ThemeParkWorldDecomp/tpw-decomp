// ltms: a string table as a resource: built from its file, reference counted, destroyed
// (ltms shared: SwapString 0x1000492c, the constructor 0x100049f0, the destructor
// 0x10004b98, AddRef 0x10005324, Release 0x10005370). The formatting members are in
// TbITable.cpp.
#include "TbIResourceIndex.h"
#include "TbITable.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"

namespace {
// 0x1000492c: puts one string's words (TbITable.cpp describes them) into host order,
// walking it by its own opcodes. The file is little-endian, so there is something to do
// only on a big-endian host.
void SwapString(unsigned char* string) {
    const uint16_t probe = 1;
    if (*reinterpret_cast<const uint8_t*>(&probe) == 1) return;
    auto* p = reinterpret_cast<uint32_t*>(string);
    for (;;) {
        SamsUtilities::USwapInplace32(*p);
        const uint32_t word = *p++;
        const uint32_t op = word & 0xff;
        if (op == 0 || op >= 7) return;
        if (op == 1) p = reinterpret_cast<uint32_t*>(reinterpret_cast<unsigned char*>(p) + (((word >> 8) + 3) & ~3u));
        else if (op == 3) p += word >> 8;
    }
}
}  // namespace

// The table takes the language's two converters from the index if they are already open
// there: item 2 when it is a Unicode to multibyte converter (type 1), item 1 when it is a
// multibyte to Unicode converter (type 0).
TbITable::TbITable(uint32_t count, void* data, TbIResourceIndex* index) : mCount(count) {
    auto* offsets = static_cast<uint32_t*>(data);
    SamsUtilities::_USwapBlock32(offsets, static_cast<int32_t>(mCount));
    mOwnedStrings = new const unsigned char*[mCount != 0 ? mCount : 1];
    mStrings = mOwnedStrings;
    for (uint32_t i = 0; i < mCount; ++i) {
        unsigned char* string = static_cast<unsigned char*>(data) + offsets[i];
        mStrings[i] = string;
        SwapString(string);
    }
    void* handle = index->GetResourceItemHandle(2);
    if (index->GetResourceType(handle) == 1) index->GetPreOpenedResource(handle, reinterpret_cast<void**>(&mUnicodeToMB));
    else mUnicodeToMB = nullptr;
    handle = index->GetResourceItemHandle(1);
    if (index->GetResourceType(handle) == 0) index->GetPreOpenedResource(handle, reinterpret_cast<void**>(&mMBToUnicode));
    else mMBToUnicode = nullptr;
}

TbITable::~TbITable() {
    if (mUnicodeToMB != nullptr) {
        mUnicodeToMB->Release();
        mUnicodeToMB = nullptr;
    }
    if (mMBToUnicode != nullptr) {
        mMBToUnicode->Release();
        mMBToUnicode = nullptr;
    }
    delete[] mOwnedStrings;
    mOwnedStrings = nullptr;
}

void TbITable::AddRef() { TbResource::AddRef(); }

// The table is destroyed in place and its array allocation freed (the object and the
// file's data are one block).
int32_t TbITable::Release() {
    const int32_t count = NS_MacDoze::InterlockedDecrement(&mReferences);
    if (count == 0) {
        this->~TbITable();
        ::operator delete[](this);
        return 0;
    }
    return count;
}
