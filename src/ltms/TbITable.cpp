// TbITable.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbITable code, named after it.

#include "bfl_FileSystem.h"
#include "CStaticBuffer.h"
#include "helpers/TbFontRenderParamsHelpers.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"
#include "TbIResourceIndex.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

// ltms: a string table as a resource: built from its file, reference counted, destroyed
// (ltms shared: SwapString 0x1000492c, the constructor 0x100049f0, the destructor
// 0x10004b98, AddRef 0x10005324, Release 0x10005370). The formatting members are in
// TbITable.cpp.

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

namespace {
uint32_t Word(const unsigned char* p) {
    uint32_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

template<class Find>
int32_t Format(const unsigned char* p, CStaticBuffer& out, Find find) {
    TbVarFormat params{nullptr, -1, 0, 0};
    for (;;) {
        const uint32_t word = Word(p);
        p += 4;
        switch (word & 0xff) {
        case 0:
            out.Close();
            return 0;
        case 1:
            out.AppendMB(p, word >> 8);
            p += ((word >> 8) + 3) & ~uint32_t(3);
            break;
        case 2: {
            auto* tag = find(word >> 8);
            if (!tag) return -1;
            const auto result = tag->Format(out, params);
            params = {nullptr, -1, 0, 0};
            if (result < 0) return -1;
            break;
        }
        case 3:
            params.mFormat = p;
            p += (word >> 8) * 4;
            break;
        case 4:
            params.mFlags = word >> 8;
            break;
        case 5:
            params.mPrecision = static_cast<uint16_t>(word >> 16);
            break;
        case 6:
            params.mWidth = static_cast<int16_t>(word >> 16);
            break;
        default:
            return -1;
        }
    }
}
}

int32_t TbITable::DoFormat(CStaticBuffer& out, uint32_t id, char* arguments) const {
    return Format(mStrings[id], out, [this, arguments](uint32_t id) {
        char* scan = arguments;
        return FindVariableTag(id, scan);
    });
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    CStaticUnicodeBuffer buffer;
    buffer.mCursor = out;
    buffer.mEnd = out + size;
    buffer.mConverter = mMBToUnicode;
    if (buffer.mConverter) buffer.mConverter->AddRef();
    buffer.mOverflow = false;
    va_list args;
    va_start(args, id);
    const auto result = Format(mStrings[id], buffer, [&args](uint32_t id) {
        va_list scan;
        va_copy(scan, args);
        TbVTag* found = nullptr;
        for (;;) {
            auto* tag = va_arg(scan, TbVTag*);
            if (!tag || tag->mMagic != 0x5641524du) break;
            if (tag->mId == id) {
                found = tag;
                break;
            }
        }
        va_end(scan);
        return found;
    });
    va_end(args);
    buffer.Close();
    if (buffer.mConverter) buffer.mConverter->Release();
    return result;
}

// TbITable::FormatString, the multibyte overload (ltms 0x10004e30).

// CStaticMBBuffer's Close and appends are unit v5312's: src/CStaticUnicodeBuffer_CStaticMBBuffer_AppendMB.cpp.
// Lead 2026-10-20 (v51128): its destructor (0x10000b24, the vtable with it) is translated in src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp;
// the Mac inlines it here, and the buffer's destructor at the return does that Close and Release.

int32_t TbITable::FormatString(unsigned char* out, uint32_t size, uint32_t id, ...) const {
    CStaticMBBuffer buffer;
    buffer.mCursor = out;
    buffer.mEnd = out + size;
    buffer.mConverter = mUnicodeToMB;
    if (buffer.mConverter) buffer.mConverter->AddRef();
    buffer.mOverflow = false;
    va_list args;
    va_start(args, id);
    const int32_t result = DoFormat(buffer, id, reinterpret_cast<char*>(args));
    va_end(args);
    return result;
}

// Unit v51120 (ltms): the small functions of the range 0x100032a8..0x10009400: destructors with the hidden delete flag, the
// reference-releasing smart pointers, two byte swaps, two what() answers, TbMBStringVTag::Format and the resource index's
// static initialiser. (IsDelimiter, SetHorizontalJustification and TbITable::AddRef are in their own files.)

// ---- the resource destructors: only the vtable steps (the base destructors are inlined or, for the font, called)
// (TbIUnicodeToMB, TbIMBToUnicode and TbIRLE4bitFont destructors now live in src/TbIUnicodeToMB.cpp,
// src/TbIMBToUnicode.cpp and src/TbIRLE4bitFont.cpp; the test links those files.)

template <class T>
TbRefReleaseSmartPtr<T>::~TbRefReleaseSmartPtr() {   // 0x1000514c, 0x10004f64
    if (mPtr != nullptr) mPtr->Release();
}
template struct TbRefReleaseSmartPtr<TbIMBToUnicode>;
template struct TbRefReleaseSmartPtr<TbIUnicodeToMB>;

TbVTag* TbITable::FindVariableTag(uint32_t id, char*& arguments) const {
    for (;;) {
        TbVTag* tag;
        std::memcpy(&tag, arguments, sizeof(tag));
        arguments += sizeof(tag);
        if (!tag || tag->mMagic != 0x5641524du) return nullptr;
        if (tag->mId == id) return tag;
    }
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    if (id >= mCount) return nullptr;
    const auto* p = mStrings[id];
    const uint32_t word = Word(p);
    if ((word & 0xff) != 1) return nullptr;
    p += 4;
    return Word(p + (((word >> 8) + 3) & ~uint32_t(3))) == 0 ? p : nullptr;
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

// ltms: the resource factories. TbIResourceIndex::OpenResource opens a resource's file and
// hands it to the factory of the resource's type (gLtmsResourceFactories), which reads the
// file and builds the object, usually at the start of one allocation with the file's data
// behind it. File data is little-endian; the Mac swaps it after reading.

// 0x100053fc: {u32 "BFST", u32 id, u32 string count}, then the offset table and the
// strings to the end of the file. The id in the file must be the one asked for.
TbError TableFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t id,
                                      TbIResourceIndex* index) const {
    uint32_t header[3];
    if (file.Read(header, 12) != 12) return TbError(-1);
    SamsUtilities::USwapInplace32(header[0]);
    SamsUtilities::USwapInplace32(header[1]);
    SamsUtilities::USwapInplace32(header[2]);
    if (header[0] != 0x54534642u) return TbError(-1);
    if (header[1] != id) return TbError(-1);
    const uint32_t size = Remaining(file);
    auto* block = new uint8_t[sizeof(TbITable) + size];
    auto* data = block + sizeof(TbITable);
    if (static_cast<uint32_t>(file.Read(data, size)) != size) {
        delete[] block;
        return TbError(-1);
    }
    *out = new (block) TbITable(header[2], data, index);
    return TbError(0);
}
