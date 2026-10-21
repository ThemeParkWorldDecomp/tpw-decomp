// Test entry points for the ltms character-set converters and their factories (library
// unit_TbConverters: src/ltms/TbFontRenderParams.cpp, src/ltms/TbIRLE4bitFont.cpp, src/ltms/TbITable.cpp, src/ltms/TbIUnicodeToMB.cpp, src/ltms/StaticUnicodeBuffer.cpp, src/ltms/TbIResourceIndex.cpp,
// src/ltms/TbIUnicodeToMB.cpp). The file the factories read is the queued fake of
// ltms_file_fakes.h; the base resource's reference counting is not under test here.
#include "TbIResourceIndex.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"
#include "ltms_file_fakes.h"

#include <cstring>

#include "TbIRLE4bitFont.h"
#include "TbITable.h"

// The base resource's counting is not under test here (test_TbIResourceIndex has it).
int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}
int32_t NS_MacDoze::InterlockedIncrement(int32_t* value) { return ++*value; }
int32_t NS_MacDoze::InterlockedDecrement(int32_t* value) { return --*value; }
void SamsUtilities::_USwapBlock32(uint32_t*, int32_t count) { ev("Swap32", {count}); }

namespace {
// A converter an index item already holds: only its reference calls are recorded. The
// first one doubles as the font's multibyte description (its first lead byte is read).
class Held : public TbIMBToUnicode {
public:
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t Release() override {
        ev("Release", {lab(this)});
        return 0;
    }
};
Held gHeld[2];
TbIResourceIndex gIndex;   // built at load time, before any call is recorded

int32_t HeldLabel(const void* p) { return p == &gHeld[0] ? 1 : p == &gHeld[1] ? 2 : p == nullptr ? 0 : -1; }

// Seeds the index with (id, type, held resource or -1) triples, in rising id order.
void SeedIndex(const int32_t* items, int32_t count) {
    gIndex.mItems.assign(static_cast<size_t>(count > 0 ? count : 1), {});
    for (int32_t i = 0; i < count; ++i) {
        gIndex.mItems[i] = {static_cast<uint32_t>(items[i * 3]), static_cast<uint16_t>(items[i * 3 + 1]), 0,
                           items[i * 3 + 2] < 0 ? nullptr : static_cast<TbResource*>(&gHeld[items[i * 3 + 2]])};
    }
    gIndex.mFirst = gIndex.mItems.data();
    gIndex.mLast = gIndex.mFirst + count - 1;
    gHeld[0].mFirstLead = gHeld[1].mFirstLead = 0x81;   // as the PowerPC side's held objects
}
}  // namespace

// The Mac swaps the little-endian file words it has just read; the host has nothing to
// swap. Recorded, so a dropped or misplaced call shows.
void SamsUtilities::_USwapBlock16(uint16_t*, int32_t count) { ev("Swap16", {count}); }

extern "C" {

// Runs factory `which` (0 MBToUnicode, 1 UnicodeToMB, 2 MBInfo) over a file holding
// `bytes`. out: [0] the result, [1] the kind of object made (-1: the out pointer was not
// written, 0: NULL, 1 TbIMBInfo, 2 TbIMBToUnicode, 3 TbIUnicodeToMB), [2] its reference
// count, [3] first lead byte, [4] character count, [5] words copied to `words`.
void tpw_conv_factory(int32_t which, const uint8_t* bytes, uint32_t length, uint32_t id, int32_t* out,
                      uint16_t* words, uint32_t wordCount) {
    static MBToUnicodeFactory mbToUnicode;
    static UnicodeToMBFactory unicodeToMB;
    static MBInfoFactory mbInfo;
    const TbResourceFactory* factories[3] = {&mbToUnicode, &unicodeToMB, &mbInfo};
    fileBytes = bytes;
    fileLength = length;
    filePosition = 0;
    void* const untouched = reinterpret_cast<void*>(static_cast<uintptr_t>(0x77777777));
    void* made = untouched;
    {
        CFile file;
        file.mImplementation = &fileImpl;
        out[0] = factories[which]->CreateInterface(file, &made, id, nullptr);
    }
    out[1] = made == untouched ? -1 : 0;
    out[2] = out[3] = out[4] = out[5] = 0;
    if (made == untouched || made == nullptr) return;
    auto* resource = static_cast<TbResource*>(made);
    out[2] = resource->mReferences;
    if (auto* toUnicode = dynamic_cast<TbIMBToUnicode*>(resource)) {
        out[1] = 2;
        out[3] = toUnicode->mFirstLead;
        out[4] = toUnicode->mCount;
        for (uint32_t i = 0; i < wordCount; ++i) words[i] = toUnicode->mTable[i];
        out[5] = static_cast<int32_t>(wordCount);
    } else if (auto* info = dynamic_cast<TbIMBInfo*>(resource)) {
        out[1] = 1;
        out[3] = info->mFirstLead;
        out[4] = info->mCount;
    } else if (auto* toMB = dynamic_cast<TbIUnicodeToMB*>(resource)) {
        out[1] = 3;
        for (uint32_t i = 0; i < wordCount; ++i) words[i] = toMB->mData[i];
        out[5] = static_cast<int32_t>(wordCount);
    }
    delete resource;
}

void* tpw_conv_held(int32_t which) { return &gHeld[which]; }

// TableFactory over a file holding `bytes`, with the index seeded from `items`. out: [0]
// the result, [1] 4 if a table was made (-1: out pointer untouched, 0: NULL), [2] its
// reference count, [3] string count, [4] and [5] which held converter became the
// multibyte-to-Unicode and the Unicode-to-multibyte one (0 none). words: per string its
// offset from the start of the data, then per string its first word. The table is
// released at the end, which lets go of the converters.
void tpw_conv_table(const uint8_t* bytes, uint32_t length, uint32_t id, const int32_t* items, int32_t itemCount,
                    int32_t* out, uint32_t* words, uint32_t stringCount) {
    static TableFactory factory;
    SeedIndex(items, itemCount);
    fileBytes = bytes;
    fileLength = length;
    filePosition = 0;
    void* const untouched = reinterpret_cast<void*>(static_cast<uintptr_t>(0x77777777));
    void* made = untouched;
    {
        CFile file;
        file.mImplementation = &fileImpl;
        out[0] = factory.CreateInterface(file, &made, id, &gIndex);
    }
    out[1] = made == untouched ? -1 : 0;
    out[2] = out[3] = out[4] = out[5] = 0;
    if (made == untouched || made == nullptr) return;
    auto* table = static_cast<TbITable*>(made);
    out[1] = 4;
    out[2] = table->mReferences;
    out[3] = static_cast<int32_t>(table->mCount);
    out[4] = HeldLabel(table->mMBToUnicode);
    out[5] = HeldLabel(table->mUnicodeToMB);
    const auto* data = reinterpret_cast<const unsigned char*>(table) + sizeof(TbITable);
    for (uint32_t i = 0; i < stringCount && i < table->mCount; ++i) {
        words[i] = static_cast<uint32_t>(table->mStrings[i] - data);
        std::memcpy(&words[stringCount + i], table->mStrings[i], 4);
    }
    table->Release();
}

// RLE4bitFontFactory likewise. out: [0] result, [1] 5 if a font was made, [2] references,
// [3] character count, [4] line height, [5] type, [6] caps, [7] held converter, [8] and [9]
// GetCharIndex of `text` and how far it advanced (when there is a converter). words: 13
// per character: the header's fields (the two bytes at 0x14 as they lie), GetCharSize,
// and the offsets of GetCharData and GetCharHeader from the file's start.
void tpw_conv_font(const uint8_t* bytes, uint32_t length, const int32_t* items, int32_t itemCount,
                   const unsigned char* text, int32_t* out, uint32_t* words, uint32_t charCount) {
    static RLE4bitFontFactory factory;
    SeedIndex(items, itemCount);
    fileBytes = bytes;
    fileLength = length;
    filePosition = 0;
    void* const untouched = reinterpret_cast<void*>(static_cast<uintptr_t>(0x77777777));
    void* made = untouched;
    {
        CFile file;
        file.mImplementation = &fileImpl;
        out[0] = factory.CreateInterface(file, &made, 0, &gIndex);
    }
    out[1] = made == untouched ? -1 : 0;
    for (int i = 2; i < 10; ++i) out[i] = 0;
    if (made == untouched || made == nullptr) return;
    auto* font = static_cast<TbIRLE4bitFont*>(made);
    out[1] = 5;
    out[2] = font->mReferences;
    out[3] = font->GetCharCount();
    out[4] = font->GetVerticalSpacing();
    out[5] = static_cast<int32_t>(font->GetType());
    out[6] = static_cast<int32_t>(font->GetCaps());
    out[7] = HeldLabel(font->GetMBToUnicode());
    if (font->GetMBToUnicode() != nullptr) {
        const unsigned char* p = text;
        out[8] = static_cast<int32_t>(font->GetCharIndex(p));
        out[9] = static_cast<int32_t>(p - text);
    }
    for (uint32_t i = 0; i < charCount; ++i) {
        const unsigned char* header = font->GetCharHeader(static_cast<uint16_t>(i));
        uint32_t* w = words + i * 13;
        uint16_t h[12];
        std::memcpy(h, header, 24);
        uint32_t d[3];
        std::memcpy(d, header + 4, 12);
        w[0] = h[0]; w[1] = h[1]; w[2] = d[0]; w[3] = d[1]; w[4] = d[2]; w[5] = h[8]; w[6] = h[9];
        w[7] = static_cast<uint32_t>(header[20]) << 8 | header[21];
        w[8] = h[11];
        const TbFontCharSize size = font->GetCharSize(static_cast<uint16_t>(i));
        w[9] = size.mWidth;
        w[10] = size.mHeight;
        w[11] = static_cast<uint32_t>(font->GetCharData(static_cast<uint16_t>(i)) - font->mData);
        w[12] = static_cast<uint32_t>(header - font->mData);
    }
    delete font;
}

// TbIResourceIndex::GetPreOpenedResource on item `which` of the seeded index (-1: a NULL
// handle). out: [0] the result, [1] which held converter came back (-1: out untouched).
void tpw_conv_preopened(const int32_t* items, int32_t itemCount, int32_t which, int32_t* out) {
    SeedIndex(items, itemCount);
    void* const untouched = reinterpret_cast<void*>(static_cast<uintptr_t>(0x77777777));
    void* resource = untouched;
    out[0] = gIndex.GetPreOpenedResource(which < 0 ? nullptr : &gIndex.mItems[which], &resource);
    out[1] = resource == untouched ? -1 : HeldLabel(resource);
}

// TbIMBInfo::GetIndex: the index, and through `advance` how far the text pointer moved.
uint32_t tpw_conv_index(uint32_t firstLead, uint32_t count, const unsigned char* text, int32_t* advance) {
    TbIMBInfo info(static_cast<uint8_t>(firstLead), static_cast<uint16_t>(count));
    const unsigned char* p = text;
    const uint32_t index = info.GetIndex(p);
    *advance = static_cast<int32_t>(p - text);
    return index;
}

int32_t tpw_conv_to_unicode(uint32_t firstLead, uint32_t count, uint16_t* table, char16_t* out,
                            const unsigned char* source, uint32_t size) {
    TbIMBToUnicode converter(static_cast<uint8_t>(firstLead), static_cast<uint16_t>(count), table);
    return converter.ConvertString(out, source, static_cast<uint16_t>(size));
}

uint32_t tpw_conv_mb_value(uint16_t* tree, uint32_t value) {
    TbIUnicodeToMB converter(tree);
    return converter.GetMBValue(static_cast<char16_t>(value));
}

}  // extern "C"
