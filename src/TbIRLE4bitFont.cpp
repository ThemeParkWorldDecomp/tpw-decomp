// ltms: the 4-bit run-length font resource (ltms shared, 0x10002cb8 and
// 0x100040d8..0x100048ec). Drawing it is the font render methods' job.
#include "TbIRLE4bitFont.h"

#include "TbIResourceIndex.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"

TbIFont::~TbIFont() {}

// Lead 2026-10-20 (v5L02): the default constructor 0x100040d8 is src/TbIRLE4bitFont_TbIRLE4bitFont_TbIRLE4bitFont.cpp (v5744).

// Puts the file into host order in place: the header's magic word and character count,
// the offset table, and every character header's fields except the 16 bits at 0x14, which
// the original leaves as they are in the file. Then takes the language's multibyte to
// Unicode converter from the index if item 1 is one (type 0) and is already open.
TbIRLE4bitFont::TbIRLE4bitFont(unsigned char* data, TbIResourceIndex* index)
    : mData(data), mHeader(data), mOffsets(reinterpret_cast<uint32_t*>(data + 8)) {
    SamsUtilities::USwapInplace32(*reinterpret_cast<uint32_t*>(data));
    SamsUtilities::USwapInplace16(*reinterpret_cast<uint16_t*>(data + 6));
    const uint16_t count = *reinterpret_cast<uint16_t*>(mHeader + 6);
    SamsUtilities::_USwapBlock32(mOffsets, count);
    for (int32_t i = 0; i < static_cast<int32_t>(count); ++i) {
        auto* record = reinterpret_cast<uint16_t*>(mData + mOffsets[i]);
        SamsUtilities::USwapInplace16(record[0]);
        SamsUtilities::USwapInplace16(record[1]);
        SamsUtilities::USwapInplace32(*reinterpret_cast<uint32_t*>(record + 2));
        SamsUtilities::USwapInplace32(*reinterpret_cast<uint32_t*>(record + 4));
        SamsUtilities::USwapInplace32(*reinterpret_cast<uint32_t*>(record + 6));
        SamsUtilities::USwapInplace16(record[8]);
        SamsUtilities::USwapInplace16(record[9]);
        SamsUtilities::USwapInplace16(record[11]);
    }
    void* handle = index->GetResourceItemHandle(1);
    if (index->GetResourceType(handle) == 0) index->GetPreOpenedResource(handle, reinterpret_cast<void**>(&mMBToUnicode));
    else mMBToUnicode = nullptr;
}

TbIRLE4bitFont::~TbIRLE4bitFont() {}

// The same index rule as TbIMBInfo::GetIndex, with the converter's first lead byte.
uint32_t TbIRLE4bitFont::GetCharIndex(const unsigned char*& text) {
    const uint8_t firstLead = mMBToUnicode->mFirstLead;
    const uint32_t index = static_cast<uint32_t>(*text++) - 1;
    if ((index & 0xffff) < firstLead) return index;
    return ((index & 0xffff) * 0xff + static_cast<uint32_t>(firstLead) * static_cast<uint32_t>(-0xfe) + *text++) - 1;
}

uint16_t TbIRLE4bitFont::GetCharCount() {
    return mData == nullptr ? 0 : *reinterpret_cast<const uint16_t*>(mHeader + 6);
}

// Width from the character's header, height the font's line height. An index equal to the
// character count passes the check (one past the offset table), as in the original.
TbFontCharSize TbIRLE4bitFont::GetCharSize(uint16_t index) {
    if (mData != nullptr && index <= *reinterpret_cast<const uint16_t*>(mHeader + 6)) {
        const uint8_t height = mHeader[5];
        return {*reinterpret_cast<const uint16_t*>(mData + mOffsets[index] + 0x16), height};
    }
    return {0, 0};
}

uint8_t TbIRLE4bitFont::GetVerticalSpacing() { return mData == nullptr ? 0 : mHeader[5]; }
uint32_t TbIRLE4bitFont::GetType() { return mData == nullptr ? 5 : 4; }
uint32_t TbIRLE4bitFont::GetCaps() { return mData == nullptr ? 3 : 0xe; }
TbIMBToUnicode* TbIRLE4bitFont::GetMBToUnicode() { return mMBToUnicode; }

const unsigned char* TbIRLE4bitFont::GetCharData(uint16_t index) const {
    if (mData != nullptr && index <= *reinterpret_cast<const uint16_t*>(mHeader + 6))
        return mData + mOffsets[index] + 0x18;
    return nullptr;
}

const unsigned char* TbIRLE4bitFont::GetCharHeader(uint16_t index) const {
    if (mData != nullptr && index <= *reinterpret_cast<const uint16_t*>(mHeader + 6)) return mData + mOffsets[index];
    return nullptr;
}
