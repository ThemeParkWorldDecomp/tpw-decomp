#include "TbITable.h"
#include <cstdarg>
#include <cstring>

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

TbVTag* TbITable::FindVariableTag(uint32_t id, char*& arguments) const {
    for (;;) {
        TbVTag* tag;
        std::memcpy(&tag, arguments, sizeof(tag));
        arguments += sizeof(tag);
        if (!tag || tag->mMagic != 0x5641524du) return nullptr;
        if (tag->mId == id) return tag;
    }
}

int32_t TbITable::DoFormat(CStaticBuffer& out, uint32_t id, char* arguments) const {
    return Format(mStrings[id], out, [this, arguments](uint32_t id) {
        char* scan = arguments;
        return FindVariableTag(id, scan);
    });
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    if (id >= mCount) return nullptr;
    const auto* p = mStrings[id];
    const uint32_t word = Word(p);
    if ((word & 0xff) != 1) return nullptr;
    p += 4;
    return Word(p + (((word >> 8) + 3) & ~uint32_t(3))) == 0 ? p : nullptr;
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
