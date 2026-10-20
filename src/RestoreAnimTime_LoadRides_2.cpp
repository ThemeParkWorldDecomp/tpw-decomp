// LoadRides 0x1006b878 (2200 bytes, unit v5890): reads the rides module of a save: every ride instance is made again with InstanceRide, its
// flags, hoardings, embedded data, node flags and animation time controls restored from the file.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LbFile.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPEngine.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "rsse.h"

int32_t gLoadRidesTbErrorZero = 0;      // 0x1029d054
int32_t gLoadRidesShape[2] = {0, 0};    // 0x102c6d04

namespace {

// The save file is little endian: the Mac swaps each word where it lies (the swaps are real there); a little-endian host leaves them as
// they are, a big-endian one (or TPW_SWAP_FILE_WORDS) swaps. The file's words are not aligned, so they go through memcpy.
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
constexpr bool kSwapFileWords = true;
#else
constexpr bool kSwapFileWords = false;
#endif

void Swap32At(uint8_t* p) {
    if (!kSwapFileWords) return;
    uint32_t v;
    std::memcpy(&v, p, 4);
    v = __builtin_bswap32(v);
    std::memcpy(p, &v, 4);
}

void Swap16At(uint8_t* p) {
    if (!kSwapFileWords) return;
    uint16_t v;
    std::memcpy(&v, p, 2);
    v = __builtin_bswap16(v);
    std::memcpy(p, &v, 2);
}

uint32_t Word(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

int32_t SWord(const uint8_t* p) {
    int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

uint16_t Half(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

}  // namespace

int32_t LoadRides(void* file) {
    uint32_t sizeWord = 0;
    uint32_t transferred = 0;

    LbFile_Read(file, &sizeWord, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    Swap32At(reinterpret_cast<uint8_t*>(&sizeWord));
    uint8_t* block = static_cast<uint8_t*>(MReleaseAllocF(sizeWord, 0));

    TbError error = LbFile_Read(file, block, sizeWord, nullptr);
    // The compiler's comparison with a constant TbError: (error >= 0) == (zero word == 0) is the success.
    if (error.Succeeded() != (gLoadRidesTbErrorZero == 0)) return 0;

    uint8_t* cursor = block;
    std::memcpy(&gRideCountB, cursor, 4);
    cursor += 4;
    std::memcpy(&gRideCountA, cursor, 4);
    cursor += 4;
    std::memcpy(&gRideFirstFree, cursor, 4);
    cursor += 4;
    Swap32At(reinterpret_cast<uint8_t*>(&gRideCountB));
    Swap32At(reinterpret_cast<uint8_t*>(&gRideCountA));
    Swap32At(reinterpret_cast<uint8_t*>(&gRideFirstFree));

    TickRideAnims();

    for (uint32_t index = 0; index < gRideCountB + gRideCountA; index++) {
        uint8_t marker = *cursor++;
        if (marker != 1) continue;

        uint8_t* rec = cursor;          // r30: the ride's record
        cursor += 0x26;
        uint8_t* hdr = cursor;          // r31: the node and embedded counts and the embedded words
        cursor += 0x10;

        Swap32At(rec + 0x00);
        Swap32At(rec + 0x04);
        Swap32At(rec + 0x08);
        Swap32At(rec + 0x0c);
        Swap32At(rec + 0x10);
        Swap32At(rec + 0x14);
        Swap32At(rec + 0x18);
        Swap32At(rec + 0x1c);
        Swap16At(rec + 0x20);
        Swap32At(rec + 0x22);

        Swap32At(hdr + 0x00);
        Swap16At(hdr + 0x04);
        Swap16At(hdr + 0x06);
        Swap32At(hdr + 0x08);
        Swap32At(hdr + 0x0c);

        uint32_t typeNumber = Word(rec);
        uint32_t typeIndex = gRideTypeCount;
        for (;;) {
            if (typeIndex == 0) break;
            if (typeNumber == gRideTypes[typeIndex]->mFF8) break;
            typeIndex--;
        }

        uint32_t rotation = Word(hdr);
        int32_t e = SWord(rec + 0x10);
        int32_t d = SWord(rec + 0xc);
        int32_t z = SWord(rec + 8);
        int32_t x = SWord(rec + 4);

        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = static_cast<uint16_t>(typeNumber);
        RideInfo* info = gRideDatabase.GetRideInfo(id);

        sFixedRideParams params;
        params.mX = x;
        params.mZ = z;
        params.mRotation = static_cast<int32_t>(rotation);
        if (info != nullptr) {
            params.mF0C = info->Value(kRideSlot_Shape).mShape;
        } else if (d != 0 && e != 0) {
            gLoadRidesShape[0] = d;
            gLoadRidesShape[1] = e;
            params.mF0C = gLoadRidesShape;
        } else {
            params.mF0C = nullptr;
        }

        uint32_t number = InstanceRide(typeIndex, Word(rec + 0x14), &params, 0, SWord(rec + 0x18), nullptr, SWord(rec + 0xc),
                                       SWord(rec + 0x10), index + 1, 1);
        tag_sRideAnimSet* set = gRideAnimSets[number];
        tag_sMesh* mesh = set->mHeads;
        tag_sM3D2Header* header = mesh->mHeader;

        bool same;
        if (Half(hdr + 6) == header->mEmbeddedCount && Half(hdr + 4) == header->mAllNodeCount) {
            same = true;
        } else {
            same = false;
            ERRORBox("Node/Embedded node count changed on mesh %s since save\n"
                     "Cannot safely restore node data - expect some (hopefully minor) problems with this ride",
                     header->mName);
        }

        set->mFlags &= 0xf7fff01fu;
        uint32_t saved = Word(rec + 0x1c);
        if ((saved & 1) != 0) set->mFlags |= 0x20;
        if ((saved & 2) != 0) set->mFlags |= 0x40;
        if ((saved & 4) != 0) set->mFlags |= 0x80;
        if ((saved & 8) != 0) set->mFlags |= 0x100;
        if ((saved & 0x10) != 0) set->mFlags |= 0x200;
        if ((saved & 0x20) != 0) set->mFlags |= 0x400;
        if ((saved & 0x40) != 0) set->mFlags |= 0x800;
        if ((saved & 0x80) != 0) set->mFlags |= 0x40000;
        if ((saved & 0x100) != 0) set->mFlags |= 0x8000000;
        set->mF0C = Half(rec + 0x20);
        std::memcpy(&set->mHoardingLevel, rec + 0x22, 4);
        ReactivateHoardings(set);

        sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(mesh->mF28);
        if (embedded != nullptr && same) {
            embedded->mFlags = Word(hdr + 8);
            embedded->mF08 = Word(hdr + 0xc);
            sEmbeddedRecord* records = embedded->mRecords;
            for (uint32_t i = 0; i < set->mHeads->mHeader->mEmbeddedCount; i++) {
                Swap32At(cursor);
                Swap32At(cursor + 4);
                records->mFlags = Word(cursor);
                records->mF0C = Word(cursor + 4);
                records++;
                cursor += 8;
            }
        } else {
            cursor += static_cast<uint32_t>(Half(hdr + 6)) * 8;
        }

        if (same) {
            for (uint32_t i = 0; i < header->mAllNodeCount; i++) {
                uint32_t* nodeFlags;
                if (i < header->mNodeCount) {
                    nodeFlags = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mNodes.get()) + i * 0xa0);
                } else {
                    nodeFlags = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mMiniNodes.get()) +
                                                            (i - header->mNodeCount) * 0x58);
                }
                *nodeFlags &= 0x5f827fcfu;
                Swap32At(cursor);
                uint32_t w = Word(cursor);
                if ((w & 1) != 0) *nodeFlags |= 0x10;
                if ((w & 2) != 0) *nodeFlags |= 0x20;
                if ((w & 4) != 0) *nodeFlags |= 0x8000;
                if ((w & 8) != 0) *nodeFlags |= 0x10000;
                if ((w & 0x20) != 0) *nodeFlags |= 0x40000;
                if ((w & 0x40) != 0) *nodeFlags |= 0x80000;
                if ((w & 0x80) != 0) *nodeFlags |= 0x200000;
                if ((w & 0x100) != 0) *nodeFlags |= 0x400000;
                if ((w & 0x800) != 0) *nodeFlags |= 0x100000;
                if ((w & 0x200) != 0) *nodeFlags |= 0x20000000;
                if ((w & 0x400) != 0) *nodeFlags |= 0x80000000u;
                cursor += 4;
            }
        } else {
            cursor += static_cast<uint32_t>(Half(hdr + 4)) * 4;
        }

        sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
        int32_t restored = 0;
        for (uint32_t i = 0; i < set->mTimeCount; i++) {
            Swap32At(cursor + 0x00);
            Swap32At(cursor + 0x04);
            Swap32At(cursor + 0x08);
            Swap32At(cursor + 0x0c);
            Swap32At(cursor + 0x14);
            Swap32At(cursor + 0x18);
            Swap32At(cursor + 0x1c);
            Swap32At(cursor + 0x20);
            Swap32At(cursor + 0x24);
            Swap32At(cursor + 0x28);
            if (Word(cursor + 4) != 0xc) CTRACE(5, "Restoring anim time control\n");
            restored += RestoreAnimTime(set, control, reinterpret_cast<sAnimTimeControlSave*>(cursor));
            OutputAnimTimeStats(control, set);
            control++;
            cursor += 0x2c;
        }
        if (restored == 0) StaticifyCreateOnlyAnimNodes(set);

        for (uint32_t i = 0; i < set->mTimeCount; i++) {
            if (static_cast<sAnimTimeControl*>(set->mTimeControls)[i].mF04 < 0xc) set->mFlags |= 0x400000;
        }
        if ((set->mFlags & 0x1000000) != 0) {
            OverrideActiveChannels(set);
        } else {
            GetNumActiveChannels(set);
        }
    }

    MReleaseFreeF(block);
    return 1;
}
