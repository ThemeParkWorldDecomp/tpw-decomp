// ltms TbFontRender.cpp, the glyph decoding helpers (unit w45a; ltms symbols shared 0x100018d8..0x10001d30 and 0x1000a68c):
// the nybble reader and writer, the one-bit reader and the shared decode buffer that Tb4BitFontRenderMethod::DrawChar
// uses to turn a glyph's run-length or one-bit data into plain nybbles. The Mac classes are two words (a flag or a mask
// byte at +0, the pointer at +4; the writer has a count at +8); the host ones keep the members in the same order with
// native pointers. All members are inline here; the original has an out-of-line copy of each, at the address in the
// comment.
#pragma once
#include <cstdint>

namespace FourBit {

// Reads a stream of nybbles. Mac 8 bytes: the flag at +0 (false: the byte's high nybble is next, true: its low
// nybble), the pointer at +4.
class NybbleIterator {
public:
    // 0x10001cec
    NybbleIterator(uint8_t* pointer, bool low) : mLow(low), mPointer(pointer) {}
    // 0x10001bc4: the nybble at the position. Not const, as in the Mac mangling.
    uint8_t operator*() { return mLow ? static_cast<uint8_t>(*mPointer & 0xf) : static_cast<uint8_t>(*mPointer >> 4); }
    // 0x10001c2c: answers the iterator as it was and steps on: the pointer moves after the LOW nybble, the flag flips.
    NybbleIterator operator++(int) {
        const NybbleIterator old = *this;
        if (mLow) ++mPointer;
        mLow = !mLow;
        return old;
    }
    // 0x1000a68c: the same nybble as operator*.
    uint8_t current() { return mLow ? static_cast<uint8_t>(*mPointer & 0xf) : static_cast<uint8_t>(*mPointer >> 4); }

    bool mLow;            // +0x00
    uint8_t* mPointer;    // +0x04
};

// Writes a stream of nybbles, high nybble first. Mac 12 bytes: the pointer +0, the "second nybble" flag +4, the
// "count only" flag +5 (the constructor's second argument) and the count +8.
class NybbleWriter {
public:
    // 0x10001c9c
    NybbleWriter(uint8_t* pointer, bool countOnly) : mPointer(pointer), mSecond(false), mCountOnly(countOnly), mCount(0) {}
    // 0x10001b28: with the flag set nothing is stored (the count and the position still move). The first nybble of a
    // byte REPLACES the byte (low nybble cleared), the second is OR-ed in and the pointer moves on.
    void Write(uint8_t nybble) {
        if (!mCountOnly) {
            if (!mSecond) {
                *mPointer = static_cast<uint8_t>((nybble << 4) & 0xf0);
            } else {
                uint8_t* const at = mPointer++;
                *at = static_cast<uint8_t>(*at | (nybble & 0xf));
            }
        }
        mSecond = !mSecond;
        ++mCount;
    }
    // 0x10001ae8
    uint32_t Nybbles() { return mCount; }

    uint8_t* mPointer;   // +0x00
    bool mSecond;        // +0x04  the next nybble is the low one
    bool mCountOnly;     // +0x05
    uint32_t mCount;     // +0x08
};

// The decode buffer shared by every glyph decode: a local static of FontData::I (Mac: the size word at 0x100116fc, the
// block pointer at 0x10011700, the "made" flag byte at 0x10011704; the destructor is registered with
// __register_global_object). First use makes a block of 0x800 bytes; DrawChar grows it (delete[] then new[]) when a
// glyph's data size is larger.
class FontData {
public:
    // 0x10001d30: lets the block go.
    ~FontData();
    // 0x100018d8: the instance, made on first use.
    static FontData& I();

    uint32_t mSize;      // +0x00  bytes in the block (the Mac compares it as a signed word)
    uint8_t* mBlock;     // +0x04
    // Host only: the Mac's flag byte at 0x10011704. The tests clear it (and set mSize and mBlock) to start a case afresh.
    static bool sMade;
};

}  // namespace FourBit

namespace OneBit {

// Reads a stream of single bits, most significant first. Mac 8 bytes: the mask byte at +0 and the pointer at +4.
class BitIterator {
public:
    // What operator* answers: the bit's byte and mask.
    struct BitReference {
        // 0x1000197c: 1 when the bit is set.
        operator uint8_t() const { return (*mPointer & mMask) != 0 ? 1 : 0; }
        uint8_t mMask;          // +0x00
        const uint8_t* mPointer;   // +0x04
    };
    // 0x10001a98: the mask is 0x80 shifted right by `bit` (the arithmetic shift of a PowerPC takes six bits of the count,
    // and a count of 32 or more shifts everything out; the mask is a byte).
    BitIterator(uint8_t* pointer, uint8_t bit) : mMask(static_cast<uint8_t>((bit & 0x3f) >= 32 ? 0 : 0x80u >> (bit & 0x3f))), mPointer(pointer) {}
    // 0x100019e0
    BitReference operator*() { return BitReference{mMask, mPointer}; }
    // 0x10001a28: answers the iterator as it was; the mask turns right by one and the pointer moves on when it comes
    // round to 0x80.
    BitIterator operator++(int) {
        const BitIterator old = *this;
        mMask = static_cast<uint8_t>((mMask >> 1) | (mMask << 7));
        if (mMask == 0x80) ++mPointer;
        return old;
    }

    uint8_t mMask;       // +0x00
    uint8_t* mPointer;   // +0x04
};

}  // namespace OneBit

// unit w45b
namespace FourBit {

// What the one-colour writers inline after each row of the glyph: step the reader on by `count` nybbles (the glyph's width
// less the pixels drawn per row, as an UNSIGNED number: a source wider than the width byte wraps it). An even count moves
// the pointer by half of it; an odd one moves it by half of it rounded up when the reader is on a low nybble (and flips
// the flag), else rounded down (and flips the flag). No out-of-line copy exists.
inline void SkipNybbles(NybbleIterator& iterator, uint32_t count) {
    if (count == 0) return;
    if ((count & 1) == 0) {
        iterator.mPointer += count >> 1;
    } else {
        iterator.mPointer += iterator.mLow ? (count + 1) >> 1 : count >> 1;
        iterator.mLow = !iterator.mLow;
    }
}

}  // namespace FourBit
