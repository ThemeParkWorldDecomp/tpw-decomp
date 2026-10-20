#include <cstdint>
#include <cstring>
#include "LbFile.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "platform/samsutils.h"
#include "SaveLoad.h"

// SaveCoasters 0x10044280 (2156 bytes): the coasters' part of a save. The count of instances and the object table's three counters, then for each
// instance a 0x20 byte record, for each of its braces a count and the brace's heights (kind 2 braces) and a 0x22 byte record, then the trains
// (SaveTrainsAndCars). Every write is checked against the count of items written; a short write logs "write failed" and answers 0.
// The file's words are little-endian: the swaps are the real ones on a big-endian host and nothing on this one (TPCoasterSwap).

namespace {

template <class T>
T Get(const uint8_t* p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void Swap32(uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof v);
    TPCoasterSwap::USwapInplace32ULong(v);
    std::memcpy(p, &v, sizeof v);
}

void Swap16(uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof v);
    TPCoasterSwap::USwapInplace16UShort(v);
    std::memcpy(p, &v, sizeof v);
}

void SwapFloat(uint8_t* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    TPCoasterSwap::USwapInplaceFloat(v);
    std::memcpy(p, &v, sizeof v);
}

void SwapInstanceRecord(uint8_t* r) {
    Swap32(r + 0x00);
    Swap32(r + 0x04);
    Swap16(r + 0x08);
    Swap16(r + 0x0a);
    Swap16(r + 0x0c);
    Swap16(r + 0x0e);
    Swap16(r + 0x10);
    SwapFloat(r + 0x12);
    Swap16(r + 0x16);
    Swap16(r + 0x18);
    Swap16(r + 0x1a);
    Swap32(r + 0x1c);
}

void SwapBraceRecord(uint8_t* r) {
    Swap32(r + 0x00);
    Swap32(r + 0x04);
    Swap16(r + 0x08);
    Swap16(r + 0x0a);
    SwapFloat(r + 0x0d);
    SwapFloat(r + 0x11);
    SwapFloat(r + 0x15);
    SwapFloat(r + 0x19);
    SwapFloat(r + 0x1d);
}

// A 32-bit word swapped where it is, written, swapped back; answers whether one item went out.
bool WriteWord(void* file, uint32_t& word, uint32_t shift) {
    uint32_t transferred;
    TPCoasterSwap::USwapInplace32ULong(word);
    LbFile_Write(file, &word, 4, &transferred);
    TPCoasterSwap::USwapInplace32ULong(word);
    return (transferred >> shift) == 1;
}

bool WriteHeights(void* file, const tag_sTrackBrace* brace) {
    // The collision list (mCollisions on the host): each brace's first word, its number.
    for (uint32_t i = 0; i < brace->mFB0; ++i) {
        uint16_t value = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(brace->mCollisions[i]));
        uint32_t transferred;
        TPCoasterSwap::USwapInplace16UShort(value);
        LbFile_Write(file, &value, 2, &transferred);
        TPCoasterSwap::USwapInplace16UShort(value);
        if ((transferred >> 1) != 1) {
            SamsUtilities::UDebug("write failed");
            return false;
        }
    }
    return true;
}

}  // namespace

int32_t SaveCoasters(void* file) {
    uint32_t count = 0;
    for (tag_sCoasterInstance* it = gCoasterInstances; it != nullptr; it = it->mNext) ++count;
    if (!WriteWord(file, count, 2) || !WriteWord(file, gObjectTableCount, 2) || !WriteWord(file, gObjectTableFirstFree, 2) ||
        !WriteWord(file, gObjectTableLowest, 2)) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    for (tag_sCoasterInstance* inst = gCoasterInstances; inst != nullptr; inst = inst->mNext) {
        uint16_t visible = 0;
        for (tag_sTrackBrace* b = inst->mLastBrace; b != nullptr; b = b->mPrev)
            if ((b->mData->mFlags & 0x10) == 0) ++visible;

        uint32_t flags = 0;
        if (inst->mBlock3C & 1) flags |= 1;
        if (inst->mBlock3C & 2) flags |= 2;
        if (inst->mFlags & 0x20) flags |= 4;
        if (inst->mFlags & 0x40) flags |= 8;
        if (inst->mFlags & 0x80) flags |= 0x10;
        if (inst->mFlags & 0x100) flags |= 0x20;
        if (inst->mFlags & 0x200) flags |= 0x40;
        if (inst->mFlags & 0x400) flags |= 0x80;
        if (inst->mFlags & 0x800) flags |= 0x100;

        uint8_t rec[0x20];
        std::memcpy(rec + 0x00, &flags, 4);
        std::memcpy(rec + 0x04, &inst->mState, 4);
        const uint16_t h08 = static_cast<uint16_t>(inst->mTypeID);
        const uint16_t h0a = static_cast<uint16_t>(inst->mEntranceID);
        const uint16_t h0c = static_cast<uint16_t>(inst->mObjectInstanceID);
        const uint16_t h0e = static_cast<uint16_t>(inst->mX);
        const uint16_t h10 = static_cast<uint16_t>(inst->mZ);
        std::memcpy(rec + 0x08, &h08, 2);
        std::memcpy(rec + 0x0a, &h0a, 2);
        std::memcpy(rec + 0x0c, &h0c, 2);
        std::memcpy(rec + 0x0e, &h0e, 2);
        std::memcpy(rec + 0x10, &h10, 2);
        std::memcpy(rec + 0x12, &inst->mF2C, 4);
        std::memcpy(rec + 0x16, &visible, 2);
        const uint16_t h18 = static_cast<uint16_t>(inst->mTrainCount);
        const uint16_t h1a = static_cast<uint16_t>(Get<uint32_t>(inst->mFF0));
        std::memcpy(rec + 0x18, &h18, 2);
        std::memcpy(rec + 0x1a, &h1a, 2);
        std::memcpy(rec + 0x1c, inst->mF140, 4);
        SwapInstanceRecord(rec);
        uint32_t transferred;
        LbFile_Write(file, rec, 0x20, &transferred);
        const uint32_t written = transferred >> 5;
        SwapInstanceRecord(rec);
        if (written != 1) {
            SamsUtilities::UDebug("write failed");
            return 0;
        }

        for (tag_sTrackBrace* b = inst->mLastBrace; b != nullptr; b = b->mPrev) {
            if (b->mNumber == 2) {
                if (!WriteWord(file, b->mFB0, 2)) {
                    SamsUtilities::UDebug("write failed");
                    return 0;
                }
                if (!WriteHeights(file, b)) return 0;
            }
            if ((b->mData->mFlags & 0x10) != 0) continue;

            uint32_t bf = 0;
            if (b->mFlags & 0x1) bf |= 1;
            if (b->mFlags & 0x2) bf |= 2;
            if (b->mFlags & 0x4) bf |= 4;
            if (b->mFlags & 0x80) bf |= 8;
            if (b->mFlags & 0x40) bf |= 0x10;
            if (b->mFlags & 0x100) bf |= 0x20;
            if (b->mFlags & 0x10000) bf |= 0x40;
            if (b->mFlags & 0x20000) bf |= 0x80;
            if (b->mFlags & 0x40000) bf |= 0x100;
            if (b->mFlags & 0x80000) bf |= 0x200;
            if (b->mFlags & 0x200000) bf |= 0x400;
            if (b->mFlags & 0x400000) bf |= 0x800;

            uint8_t br[0x22];
            std::memcpy(br + 0x00, &bf, 4);
            std::memcpy(br + 0x04, &b->mWord70, 4);
            const uint16_t c08 = static_cast<uint16_t>(b->mData->mCellX);
            const uint16_t c0a = static_cast<uint16_t>(b->mData->mCellZ);
            std::memcpy(br + 0x08, &c08, 2);
            std::memcpy(br + 0x0a, &c0a, 2);
            br[0x0c] = static_cast<uint8_t>(Get<uint32_t>(b->mF00 + 4));
            std::memcpy(br + 0x0d, &b->mFE8w, 4);
            std::memcpy(br + 0x11, &b->mFD8, 4);
            std::memcpy(br + 0x15, &b->mFDC, 4);
            std::memcpy(br + 0x19, &b->mAngle, 4);
            std::memcpy(br + 0x1d, &b->mFF8, 4);
            br[0x21] = static_cast<uint8_t>(b->mFB0);
            SwapBraceRecord(br);
            LbFile_Write(file, br, 0x22, &transferred);
            const uint32_t bwritten = transferred / 0x22;
            SwapBraceRecord(br);
            if (bwritten != 1) {
                SamsUtilities::UDebug("write failed");
                return 0;
            }
            if (!WriteHeights(file, b)) return 0;
        }
        SaveTrainsAndCars(file, inst);
    }
    return 1;
}
