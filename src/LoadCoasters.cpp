// LoadCoasters (0x10044d68, 2848 bytes, TPCoaster.cpp on the Mac): reads the coasters of a park file back (the counterpart of SaveCoasters).
// The file is little-endian; the swaps are TPCoasterSwap's (real on a big-endian host, no-ops here).
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/samsutils.h"

namespace {

// The unaligned floats of the file's records go through a local (the Mac swaps them in place).
float SwapFloatAt(const uint8_t* p) {
    float value;
    std::memcpy(&value, p, 4);
    TPCoasterSwap::USwapInplaceFloat(value);
    return value;
}
uint32_t SwapWordAt(const uint8_t* p) {
    uint32_t value;
    std::memcpy(&value, p, 4);
    TPCoasterSwap::USwapInplace32ULong(value);
    return value;
}
uint16_t SwapHalfAt(const uint8_t* p) {
    uint16_t value;
    std::memcpy(&value, p, 2);
    TPCoasterSwap::USwapInplace16UShort(value);
    return value;
}

// The height of an entrance's ride: set -> mesh -> header -> the node at header + 0x78, its float at +0x44.
float EntranceHeight(uint32_t entrance) {
    const tag_sRideAnimSet* set = gRideAnimSets[entrance];
    const tag_sM3D2Header* header = set->mHeads->mHeader;
    const uint8_t* node = reinterpret_cast<const uint8_t*>(header->mF78.get());
    float value;
    std::memcpy(&value, node + 0x44, 4);
    return value;
}

// Reads a brace's mFB0 collision entries (halfwords of the file: the braces' numbers) into mCollisions, the numbers held as
// pointers until they are resolved; false on a short read. A count above 8 fills slots the Mac shares with mFD4..mFDC.

bool ReadBraceList(void* file, tag_sTrackBrace* brace) {
    for (uint32_t k = 0; k < brace->mFB0; k++) {
        uint16_t half = 0;
        uint32_t transferred = 0;
        LbFile_Read(file, &half, 2, &transferred);
        if (transferred >> 1 != 1) {
            SamsUtilities::UDebug("read failed");
            return false;
        }
        TPCoasterSwap::USwapInplace16UShort(half);
        brace->mCollisions[k] = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(half));
    }
    return true;
}

}  // namespace

int32_t LoadCoasters(void* file) {
    uint32_t count = 0;
    uint32_t transferred = 0;

    LbFile_Read(file, &count, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(count);

    LbFile_Read(file, &gObjectTableCount, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(gObjectTableCount);

    LbFile_Read(file, &gObjectTableFirstFree, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(gObjectTableFirstFree);

    LbFile_Read(file, &gObjectTableLowest, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(gObjectTableLowest);

    tag_sTrackBrace* previewBrace = nullptr;   // r17: the brace of flag 0x40 (or the start of the list)
    tag_sTrackBrace* flagBrace = nullptr;      // r15: the brace of flag 0x80

    for (uint32_t index = 0; index < count; index++) {
        gPylonStackSkip = 1;

        // The coaster's record: 0x20 bytes at (offsets) 0 flags, 4 state, 8 half, 10 entrance, 12 extra, 14 x, 16 z, 18 float, 22 pylon count,
        // 24, 26 halves, 28 word.
        uint8_t rec[0x20];
        LbFile_Read(file, rec, 0x20, &transferred);
        uint32_t recordsRead = transferred >> 5;
        uint32_t recFlags = SwapWordAt(rec + 0);
        uint32_t recState = SwapWordAt(rec + 4);
        (void)SwapHalfAt(rec + 8);
        uint32_t entrance = SwapHalfAt(rec + 10);
        uint32_t extra = SwapHalfAt(rec + 12);
        uint32_t cellX = SwapHalfAt(rec + 14);
        uint32_t cellZ = SwapHalfAt(rec + 16);
        float recFloat = SwapFloatAt(rec + 18);
        uint32_t pylonCount = SwapHalfAt(rec + 22);
        uint32_t recTrains = SwapHalfAt(rec + 24);
        uint32_t recWord = SwapHalfAt(rec + 26);
        uint32_t recLast = SwapWordAt(rec + 28);
        if (recordsRead != 1) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }

        tag_sCoasterInstance* instance = gCoasterInstances;
        for (; instance != nullptr; instance = instance->mNext) {
            if (instance->mEntranceID == entrance) {
                gCoasterEditInstance = instance;
                gCoasterCurrentData = &instance->mBlock3C;
                InstanceCoasterTrains(reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType)), instance, 1);
                PlaceEntrancePylons(instance, cellX, cellZ, EntranceHeight(entrance), 1);
                instance->mObjectInstanceID =
                    AllocateObjectInstanceToTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest, 0xff, gObjectTable, instance, extra);
                instance->mFlags |= 2;
                break;
            }
        }
        // No coaster with that entrance: the original goes on with a NULL instance (it reads address 0).
        instance->mFlags |= 0x4000;

        if (recFlags & 2) {
            flagBrace = instance->mBraces;
            previewBrace = instance->mLastBrace->mPrev->mPrev;
        }
        tag_sTrackBrace* brace = instance->mLastBrace->mPrev->mPrev;

        LbFile_Read(file, &brace->mFB0, 4, &transferred);
        if (transferred >> 2 != 1) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }
        TPCoasterSwap::USwapInplace32ULong(brace->mFB0);
        if (!ReadBraceList(file, brace)) return 0;

        for (uint32_t p = 0; p < pylonCount; p++) {
            // The pylon's record: 0x22 bytes at 0 flags, 4 extra, 8 x, 10 z, 12 level, 13 loft, 17 wobble x, 21 wobble z, 25 rotate, 29 wobble
            // (flag 0x10), 33 brace count.
            uint8_t prec[0x22];
            LbFile_Read(file, prec, 0x22, &transferred);
            uint32_t pRead = transferred / 0x22;
            uint32_t pFlags = SwapWordAt(prec + 0);
            uint32_t pExtra = SwapWordAt(prec + 4);
            uint32_t pX = SwapHalfAt(prec + 8);
            uint32_t pZ = SwapHalfAt(prec + 10);
            uint8_t pLevel = prec[12];
            float loft = SwapFloatAt(prec + 13);
            float wobbleX = SwapFloatAt(prec + 17);
            float wobbleZ = SwapFloatAt(prec + 21);
            float rotate = SwapFloatAt(prec + 25);
            float wobbleAlt = SwapFloatAt(prec + 29);
            uint8_t pBraces = prec[33];
            if (pRead != 1) {
                SamsUtilities::UDebug("read failed");
                return 0;
            }
            if (pFlags & 8) SetPlaceLoop();

            tag_sPylon* pylon = nullptr;
            PlacePylon(pX, pZ, 1, &pylon, &brace, pLevel, pExtra);
            if (pylon == nullptr) continue;

            SetLoftPylon(loft, brace->mNumber, 0, 1);
            SetRotatePylon(rotate, brace->mNumber, 0, 1);
            if (pFlags & 0x10)
                SetWobblePylon(wobbleAlt, 0.0f, brace->mNumber, 0, 1);
            else
                SetWobblePylon(wobbleX, wobbleZ, brace->mNumber, 0, 1);
            if (pFlags & 1) brace->mFlags |= 1;
            if (pFlags & 2) brace->mFlags |= 2;
            if (pFlags & 4) brace->mFlags |= 4;
            if (pFlags & 0x100) brace->mFlags |= 0x40000;
            if (pFlags & 0x200) brace->mFlags |= 0x80000;
            if (pFlags & 0x40) {
                brace->mFlags |= 0x10000;
                previewBrace = brace;
            }
            if (pFlags & 0x80) {
                brace->mFlags |= 0x20000;
                flagBrace = brace;
            }
            if (pFlags & 0x400)
                brace->mFlags |= 0x200000;
            else
                brace->mFlags &= 0xffdfffffu;
            if (pFlags & 0x800) brace->mFlags |= 0x400000;
            brace->mFB0 = pBraces;
            if (!ReadBraceList(file, brace)) return 0;
        }

        gPylonStackSkip = 0;
        for (sCoasterPylon* pylon = instance->mPylons; pylon != nullptr; pylon = pylon->mNext) {
            if (pylon->mFlags & 1) continue;
            if (gPylonStackSkip == 0) {
                for (uint32_t k = 0; k < pylon->mCount; k++) {
                    tag_sTrackBrace* b = pylon->mBraces[k];
                    uint32_t n = pylon->mCount;
                    if (n > 1 && k < n - 1)
                        b->mFlags |= 0x100;
                    else
                        b->mFlags &= 0xfffffeffu;
                    uint8_t* raw = reinterpret_cast<uint8_t*>(b->mF70);
                    const uint8_t* table = reinterpret_cast<const uint8_t*>(b->mData->mF18Table);
                    uint32_t bflags = b->mFlags;
                    if ((bflags & 0x2000) == 0 && (bflags & 0x100) != 0 && (*reinterpret_cast<const uint32_t*>(table) & 8) != 0) {
                        b->mF90 = reinterpret_cast<sBraceSpline*>(raw + (0x84 - 0x70));
                        b->mF74 = reinterpret_cast<const uint32_t*>(table + 0x44);
                    } else {
                        b->mF90 = reinterpret_cast<sBraceSpline*>(raw + (0x78 - 0x70));
                        b->mF74 = reinterpret_cast<const uint32_t*>(table + 0x30);
                    }
                }
            }
            RecalcTrackParams(instance);
            instance->mFlags |= 1;
            instance->mFlags |= 0x1000;
            UpdatePylonStackMeshes(pylon->mControl, reinterpret_cast<tag_sPylon*>(pylon), 0, 1);
        }

        if (recFlags & 1) {
            gCoasterEditInstance->mBlock3C |= 1;
            PlaceEndPylons(gCoasterEditInstance, EntranceHeight(gCoasterEditInstance->mEntranceID));
            tag_sCoasterInstance* edit = gCoasterEditInstance;
            RecalcTrackParams(edit);
            edit->mFlags |= 1;
            edit->mFlags |= 0x1000;
            if (gActionRecorder.mRecording == 0) SetCoasterEditState(0x10);
            SetCoasterState(gCoasterEditInstance, 2);
        }

        for (tag_sTrackBrace* b = instance->mLastBrace; b != nullptr; b = b->mPrev) {
            for (uint32_t k = 0; k < b->mFB0; k++) {
                const uint32_t number = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(b->mCollisions[k]));
                b->mCollisions[k] = GetTrackBrace(reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C), number);
            }
        }

        if (recFlags & 2) {
            instance->mBlock3C |= 2;
            instance->mPreviewBrace = previewBrace;
            instance->mBrace64 = flagBrace;
            if (previewBrace->mNumber == 2) previewBrace->mFlags |= 0x50000;
        }
        if (recFlags & 4) instance->mFlags |= 0x20;
        if (recFlags & 8) instance->mFlags |= 0x40;
        if (recFlags & 0x10) instance->mFlags |= 0x80;
        if (recFlags & 0x20) instance->mFlags |= 0x100;
        if (recFlags & 0x40) instance->mFlags |= 0x200;
        if (recFlags & 0x80) instance->mFlags |= 0x400;
        if (recFlags & 0x100) instance->mFlags |= 0x800;

        instance->mState = recState;
        instance->mF2C = recFloat;
        instance->mTrainCount = static_cast<int32_t>(recTrains);
        std::memcpy(instance->mFF0, &recWord, 4);
        std::memcpy(instance->mF140, &recLast, 4);

        LoadTrainsAndCars(file, instance);
        SetTrainCapacity(instance, 0, reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF2F4, 0);
        ClearCoasterButtons();
        if ((instance->mBlock3C & 2) != 0 || (instance->mBlock3C & 1) == 0) FlashEndPylons(instance, 1);
        gCoasterEditInstance = nullptr;
    }
    return 1;
}
