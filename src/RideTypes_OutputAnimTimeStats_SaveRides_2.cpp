// SaveRides 0x1006acf8 (2308 bytes): writes the ride anim sets to the save file. Two passes over the sets: the first adds up the size
// of the record block, the second fills it (every word swapped to the file's little-endian form the way the original does, field by field).
#include "tpw_probe.h"
#include <cstdio>
#include <cstring>
#include "Debug.h"
#include "Game.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "analyser.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPCoaster.h"
#include "LbFile.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "platform/ltms.h"

// 0x1029d050: the word the compiler keeps for the TbError comparison of the second write (0 in the binary).
uint32_t gSaveRidesTbErrorZero = 0;

namespace {

template <class T>
void Store(uint8_t* where, T value) {
    std::memcpy(where, &value, sizeof value);
}

// A word of the file: swapped, then stored (the original stores it and swaps it in place).
void PutWord(uint8_t* where, uint32_t value) {
    SamsUtilities::USwapInplace32<uint32_t>(value);
    Store(where, value);
}
void PutHalf(uint8_t* where, uint16_t value) {
    SamsUtilities::USwapInplace16<uint16_t>(value);
    Store(where, value);
}
void PutFloat(uint8_t* where, float value) {
    TPCoasterSwap::USwapInplaceFloat(value);
    Store(where, value);
}

}  // namespace

int32_t SaveRides(void* file) {
    uint32_t total = 0xc;
    uint8_t* block = nullptr;
    uint8_t* p = nullptr;
    for (uint32_t pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            block = static_cast<uint8_t*>(MReleaseAllocF(total, 1));
            PutWord(block, gRideCountB);
            PutWord(block + 4, gRideCountA);
            PutWord(block + 8, gRideFirstFree);
            p = block + 12;
        }
        for (uint32_t slot = 0; slot < gRideCountB + gRideCountA; ++slot) {
            tag_sRideAnimSet* set = gRideAnimSets[slot + 1];
            if (pass == 0) {
                total += 1;
            } else if (set != nullptr) {
                *p++ = 1;
            } else {
                *p++ = 0;
            }
            if (set == nullptr) continue;
            if (pass == 0) {
                total += 0x36;
                total += set->mTimeCount * 0x2c;
                total += set->mHeads->mHeader->mEmbeddedCount * 8;
                total += set->mHeads->mHeader->mAllNodeCount * 4;
                continue;
            }
            PutWord(p + 0x00, set->mFF8);
            PutWord(p + 0x04, set->mFE8);
            PutWord(p + 0x08, set->mFEC);
            PutWord(p + 0x0c, set->mFF0);
            PutWord(p + 0x10, set->mFF4);
            PutWord(p + 0x14, set->mFE4);
            PutWord(p + 0x18, set->mFD4);
            uint32_t bits = 0;
            const uint32_t flags = set->mFlags;
            if (flags & 0x20) bits |= 1;
            if (flags & 0x40) bits |= 2;
            if (flags & 0x80) bits |= 4;
            if (flags & 0x100) bits |= 8;
            if (flags & 0x200) bits |= 0x10;
            if (flags & 0x400) bits |= 0x20;
            if (flags & 0x800) bits |= 0x40;
            if (flags & 0x40000) bits |= 0x80;
            if (flags & 0x8000000) bits |= 0x100;
            PutWord(p + 0x1c, bits);
            PutHalf(p + 0x20, set->mF0C);
            PutFloat(p + 0x22, set->mHoardingLevel);
            p += 0x26;

            tag_sMesh* mesh = set->mHeads;
            tag_sM3D2Header* header = mesh->mHeader;
            PutWord(p + 0, static_cast<uint32_t>(mesh->mF10));
            PutHalf(p + 4, header->mAllNodeCount);
            PutHalf(p + 6, header->mEmbeddedCount);
            sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(mesh->mF28);
            if (embedded != nullptr) {
                PutWord(p + 8, embedded->mFlags);
                PutWord(p + 0xc, embedded->mF08);
                p += 0x10;
                sEmbeddedRecord* record = embedded->mRecords;
                for (uint32_t i = 0; i < mesh->mHeader->mEmbeddedCount; ++i) {
                    PutWord(p, record->mFlags);
                    PutWord(p + 4, record->mF0C);
                    ++record;
                    p += 8;
                }
            } else {
                // Words 8 and 0xc are swapped as they stand in the block (zeros from the allocator).
                uint32_t w;
                std::memcpy(&w, p + 8, 4);
                PutWord(p + 8, w);
                std::memcpy(&w, p + 0xc, 4);
                PutWord(p + 0xc, w);
                p += 0x10;
            }

            for (uint32_t i = 0; i < header->mAllNodeCount; ++i) {
                const uint8_t* node;
                if (i < header->mNodeCount) {
                    node = reinterpret_cast<const uint8_t*>(header->mNodes.get()) + i * 0xa0;
                } else {
                    node = reinterpret_cast<const uint8_t*>(header->mMiniNodes.get()) + (i - header->mNodeCount) * 0x58;
                }
                const uint32_t nflags = *reinterpret_cast<const uint32_t*>(node);
                uint32_t out = 0;
                if (nflags & 0x10) out |= 1;
                if (nflags & 0x20) out |= 2;
                if (nflags & 0x8000) out |= 4;
                if (nflags & 0x10000) out |= 8;
                if (nflags & 0x40000) out |= 0x20;
                if (nflags & 0x80000) out |= 0x40;
                if (nflags & 0x200000) out |= 0x80;
                if (nflags & 0x400000) out |= 0x100;
                if (nflags & 0x100000) out |= 0x800;
                if (nflags & 0x20000000) out |= 0x200;
                if (nflags & 0x80000000u) out |= 0x400;
                PutWord(p, out);
                p += 4;
            }

            sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
            for (uint32_t i = 0; i < set->mTimeCount; ++i) {
                if (control->mF04 != 0xc) CTRACE(5, "Saving anim time control\n");
                OutputAnimTimeStats(control, set);
                PutWord(p + 0x00, control->mFlags);
                PutWord(p + 0x04, control->mF04);
                PutWord(p + 0x08, control->mF08);
                PutWord(p + 0x0c, static_cast<uint32_t>(control->mF10));
                Store(p + 0x10, static_cast<uint32_t>(control->mF14));   // not swapped: the original has no call for it
                PutWord(p + 0x14, static_cast<uint32_t>(control->mF18));
                PutFloat(p + 0x18, control->mF0C);
                PutWord(p + 0x1c, control->mF24);
                PutWord(p + 0x20, control->mF28);
                PutWord(p + 0x24, control->mF2C);
                PutFloat(p + 0x28, control->mF30);
                ++control;
                p += 0x2c;
            }
        }
    }
    uint32_t length = total;
    SamsUtilities::USwapInplace32<uint32_t>(length);
    uint32_t written = 0;
    LbFile_Write(file, &length, 4, &written);
    if (written >> 2 != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    TbError result = LbFile_Write(file, block, total, nullptr);
    if (result.Succeeded() != (gSaveRidesTbErrorZero == 0)) return 0;
    MReleaseFreeF(block);
    return 1;
}
