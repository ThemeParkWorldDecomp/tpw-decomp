// RideDB.cpp, main app: UTF-16 token/line helpers and custom effect getters.
#include "RideDB.h"
#include "LbFile.h"

const char16_t* const gRideStringTags[4] = {u"NAME", u"SIGNA", u"SIGNB", nullptr}; // 0x102a5d00

// 0x10155170, 172 bytes. The old destination is overwritten without being freed.
void copy_line(const char16_t** destination, const char16_t* text) {
    uint32_t count = 0;
    const char16_t* scan = text;
    while (*scan != 0 && *scan != 10 && *scan != 13) {
        ++scan;
        ++count;
    }
    *destination = static_cast<char16_t*>(LbMem_Alloc((count + 1u) << 1));
    auto* output = const_cast<char16_t*>(*destination);
    while (count != 0) {
        const char16_t value = *text++;
        --count;
        *output++ = value;
    }
    *output = 0;
}

// 0x10155244, 928 bytes. Simultaneous delimiters end a matching token.
int32_t comp(const char16_t* left, const char16_t* right) {
    for (;;) {
        const char16_t a = *left;
        const bool endA = a == 0 || a == 32 || a == 9 || a == 10 || a == 13;
        const char16_t b = *right;
        const bool endB = b == 0 || b == 32 || b == 9 || b == 10 || b == 13;
        if (endA && endB) return 1;
        if (a == 0 || b == 0) return 0;
        if (a != b) return 0;
        ++left;
        ++right;
    }
}

// 0x10155604, 116 bytes. The table is terminated by a null pointer.
int32_t find_string_id(const char16_t* text) {
    const char16_t* const* tags = gRideStringTags;
    uint32_t index = 0;
    for (;;) {
        const char16_t* tag = tags[index];
        if (tag == nullptr) return -1;
        if (comp(text, tag) != 0) return static_cast<int32_t>(index);
        ++index;
    }
}

// 0x101556a0, 224 bytes. Return the next nonblank line's start, or the final NUL.
const char16_t* find_line(const char16_t* text) {
    while (*text != 0 && *text != 10 && *text != 13) ++text;
    if (*text == 0) return text;
    if ((*text == 10 && text[1] == 13) || (*text == 13 && text[1] == 10)) text += 2;
    else ++text;
    const char16_t* scan = text;
    while (*scan == 32 || *scan == 9) ++scan;
    if (*scan == 0 || *scan == 10 || *scan == 13) return find_line(text);
    return text;
}

uint32_t RideInfo::GetCustomParticleEffect(int32_t index) const { // 0x10156db0, 16 bytes
    return mEmitterWords[static_cast<uint32_t>(index) + 1u];
}

int32_t RideInfo::GetCustomSoundBank() const { // 0x10156df8, 8 bytes
    return mSoundCategory;
}
