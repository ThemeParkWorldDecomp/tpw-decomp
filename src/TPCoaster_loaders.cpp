// TPCoaster.cpp, main app (unit w148): the coaster type's remaining loaders. CreateTrackSectionMesh 0x10036564 (196 bytes),
// SetupTrainType 0x1003a150 (260), SetupCarType 0x10039bc0 (1,376), LoadCoasterPylons 0x1003a9a8 (380), SetupPylonData 0x1003a28c
// (1,748), CreateCrossSectionSelectsFromSamFile 0x1003851c (552), SetCrossSectionSelectDefaults 0x100369d8 (200),
// OptimiseCrossSections 0x100388cc (460), RepackCrossSection 0x100387b4 (208), GetTempFixedRideParams 0x1003d5b0 (44).
//
// Host names: the listing's sCrossSection is sCoasterRecord here, sCSPoint is sCrossSectionPoint; the invented member names are in
// include/TPCoaster.h. Float compares: the original's fcmpo + bge/ble branches are taken on an unordered result too, so every
// select below is a C ternary on `<` or `>` (a NaN falls to the side the comment names). No fused multiply-add and no lfd occur in
// any of the ten listings, so the plain float expressions are exact.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "CoasterBalance.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"

namespace {

inline const BalanceValue& Slot(const CCoasterBalance* balance, uint32_t offset) {
    return balance->mValues[(offset - 8) / 4];
}
inline float SlotFloat(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mFloat; }
inline int32_t SlotInt(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mInteger; }

inline float ReadFloat(const uint8_t* address) {
    float value;
    std::memcpy(&value, address, sizeof value);
    return value;
}

// The index kinds and indexes SetupPylonData walks, in the order of the flag bits 0x20 << i (the image's words at 0x1029ccbc and
// 0x1029ccd0: kind 3 index 0, kind 10 index 0, kind 2, kind 9, kind 10 index 1).
constexpr uint32_t kPylonAnimKinds[5] = {3, 10, 2, 9, 10};
constexpr uint32_t kPylonAnimIndexes[5] = {0, 0, 0, 0, 1};

// The first value of the embedded array's entry `index` of the set's mesh header (the Mac: header + 0x7c, 0x14 bytes each, word +4).
inline uint32_t EmbeddedValue(tag_sRideAnimSet* set, int32_t index) {
    const sEmbeddedData* embedded = set->mHeads->mHeader->mEmbedded.get();
    return embedded[index].mF04;
}

// What SetupPylonData reads after each AnimateRideSet: the float at +0x34 of the block of the embedded record `index` of the
// instance's mesh (the record table of the sEmbeddedSet at the mesh's +0x28), 0.0f without a block.
inline float EmbeddedBlockFloat(tag_sRideAnimSet* instance, int32_t index) {
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(instance->mHeads->mF28);
    const uint8_t* block = embedded->mRecords[index].mBlock;
    return block != nullptr ? ReadFloat(block + 0x34) : 0.0f;
}

}  // namespace

// 0x10036564: fills the first entry of the track's master mesh set from the type's counts and a 0..800 box, makes the mesh and, when
// DMCreateMesh answers 0, gives the textures back. The original answers the type's mesh set in r3 (the caller ignores it).
void CreateTrackSectionMesh(tag_sCoasterType* type) {
    sDTextureSet* textures = type->mTextures;
    sDMeshEntry* entry = static_cast<sDMeshEntry*>(type->mF14);
    tag_sDMeshSet* meshSet = type->mMeshSet;
    entry->mFaceCount = static_cast<uint16_t>(type->mF3C);
    entry->mVertexCount = static_cast<uint16_t>(type->mF30);
    entry->mUVCount = static_cast<uint16_t>(type->mF34);
    entry->mFaceSetCount = textures->mCount;
    entry->mPointCount = static_cast<uint16_t>(type->mF38);
    entry->mBox[0] = 0.0f;
    entry->mBox[1] = 0.0f;
    entry->mBox[2] = 0.0f;
    entry->mBox[3] = 800.0f;
    entry->mBox[4] = 800.0f;
    entry->mBox[5] = 800.0f;
    if (DMCreateMesh(meshSet, textures, "TrackMaster", 1) == 0) DMFreeTextures(textures);
    SetMatrixIdentity(entry->mMatrix);
}

// 0x1003a150: the three car types (B first; A and C only when they are not B), the offsets between them and the train's car count.
void SetupTrainType(tag_sCoasterType* type, char* folder) {
    sCarType* carA = type->mCarTypeA;
    sCarType* carB = type->mCarTypeB;
    sCarType* carC = type->mCarTypeC;
    SetupCarType(carB, folder);
    if (carA != carB) SetupCarType(carA, folder);
    if (carC != carB) SetupCarType(carC, folder);
    type->mF30C = carA->mF28 + carB->mF24;
    type->mF310 = carB->mF28 + carB->mF24;
    type->mF314 = carB->mF28 + carC->mF24;
    type->mF2F8 = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(type->mF2F4); i++) {
        if (i == 0) {
            type->mF2F8 = static_cast<int32_t>(static_cast<uint32_t>(type->mF2F8) + carA->mHeadCount);
        } else if (i == static_cast<uint32_t>(type->mF2F4) - 1) {
            type->mF2F8 = static_cast<int32_t>(static_cast<uint32_t>(type->mF2F8) + carC->mHeadCount);
        } else {
            type->mF2F8 = static_cast<int32_t>(static_cast<uint32_t>(type->mF2F8) + carB->mHeadCount);
        }
    }
}

// 0x10039bc0: loads the car's mesh as a ride type, takes its box relative to the root node, counts the heads it can receive and, for a
// car with flag 1, widens the boxes of the mesh and of its largest node by the longest corner distance plus 4.
void SetupCarType(sCarType* car, char* folder) {
    car->mRideType = LoadRideType(folder, car->mName, nullptr, nullptr, 0xc0, static_cast<uint32_t>(car->mValue), 1, nullptr,
                                  nullptr, 0, 0);
    if (car->mRideType == 0) return;
    tag_sRideAnimSet* set = gRideTypes[car->mRideType];
    if (set == nullptr) return;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    float box[6];
    const float* headerBox = reinterpret_cast<const float*>(&header->mBox);   // mMin then mMax: six contiguous floats
    for (int i = 0; i < 6; i++) box[i] = headerBox[i];
    // The root node's translation: its matrix (node + 0x10) words +0x30, +0x34, +0x38 (node + 0x40 ...).
    const uint8_t* root = reinterpret_cast<const uint8_t*>(header->mF78.get());
    const float t0 = ReadFloat(root + 0x40);
    const float t1 = ReadFloat(root + 0x44);
    const float t2 = ReadFloat(root + 0x48);
    box[0] = box[0] - t0;
    box[1] = box[1] - t1;
    box[2] = box[2] - t2;
    box[3] = box[3] - t0;
    box[4] = box[4] - t1;
    box[5] = box[5] - t2;
    car->mF28 = (-box[2]) + car->mF14;
    car->mF24 = box[5] + car->mF10;
    car->mHeadCount = CountDataNodesByType(set, 0x80);
    if (car->mHeadCount != 0) {
        int32_t* heads = static_cast<int32_t*>(MReleaseAllocF(car->mHeadCount * 4, 1));
        car->mBlock = heads;
        for (uint32_t i = 0; i < car->mHeadCount; i++) heads[i] = FindNthDataNodeIndex(set, 0x80, i + 1);
    } else {
        car->mBlock = nullptr;
    }
    CTRACE(5, "Car type %s can receive %d heads\n", car->mName, static_cast<int32_t>(car->mHeadCount));
    if ((car->mFlags & 1) == 0) return;
    header = set->mHeads->mHeader;
    // The node with the most vertices (the first of them): the listing also has a branch for the mini nodes after the nodes, which the
    // loop's bound (the node count) never reaches.
    uint32_t best = 0;
    uint32_t bestIndex = 0;
    for (uint32_t i = 0; i < header->mNodeCount; i++) {
        const tag_sNode* node = header->mNodes.get() + i;
        if (node->mVertexCount > best) {
            best = node->mVertexCount;
            bestIndex = i;
        }
    }
    // The same widening for the header's box (six words at +0x80) and the chosen node's box (+0x78): the longer of the two corners'
    // lengths (the single of a double root; the x square is a double product, the other two single products), plus 4.0f, as the
    // limit: the minimum corner is lowered to -limit, the maximum raised to +limit.
    auto widen = [](float* corner) {
        auto length = [](float x, float y, float z) {
            const double xx = static_cast<double>(x) * static_cast<double>(x);   // fmul
            const float yy = y * y;                                              // fmuls
            const float zz = z * z;                                              // fmuls
            const double sum = xx + static_cast<double>(yy);
            const double sum2 = static_cast<double>(zz) + sum;
            return static_cast<float>(std::sqrt(sum2));
        };
        const float r1 = length(corner[0], corner[1], corner[2]);
        const float r2 = length(corner[3], corner[4], corner[5]);
        const float longest = (r1 < r2) ? r2 : r1;   // bge keeps r1 on an unordered compare
        const float limit = 4.0f + longest;
        const float negative = -limit;
        for (int i = 0; i < 3; i++) corner[i] = (negative < corner[i]) ? negative : corner[i];
        for (int i = 3; i < 6; i++) corner[i] = (corner[i] < limit) ? limit : corner[i];
    };
    widen(reinterpret_cast<float*>(&header->mBox));
    tag_sNode* node = header->mNodes.get() + bestIndex;
    widen(reinterpret_cast<float*>(&node->mBox));
    node->mFlags |= 0x200000;
    car->mNodeIndex = bestIndex;
}

// 0x1003a9a8: the two pylon types' ride types (the .sam names), their set-up and the type's flags 1 (first) and 2 (second); the
// flags are set whether or not the mesh loaded.
void LoadCoasterPylons(tag_sCoasterType* type, sPylonControl* first, sPylonControl* second, char* folder) {
    sPylonControl* pylons[2] = {first, second};
    for (int k = 0; k < 2; k++) {
        sPylonControl* pylon = pylons[k];
        if (pylon->mName != nullptr) {
            int32_t size[2] = {pylon->mFB4, pylon->mFB8};
            pylon->mRideType = LoadRideType(folder, pylon->mName, size, nullptr, 0x1013405, pylon->mF0C, 5, nullptr, nullptr, 0, 0);
            if (pylon->mRideType != 0) SetupPylonData(type, pylon, pylon->mRideType);
            pylon->mFlags |= 1;
            type->mFlags |= (k == 0) ? 1 : 2;
        } else {
            pylon->mRideType = 0;
        }
    }
}

// 0x1003a28c: reads the pylon mesh's embedded entries (the kinds are bit masks of CountDataNodesByType) into the pylon's index words,
// then instances the ride once, runs its animations to time 0 and time 1 and keeps the heights of the entries it found. `type` is
// not used.
void SetupPylonData(tag_sCoasterType* type, sPylonControl* pylon, uint32_t rideType) {
    (void)type;
    tag_sRideAnimSet* set = gRideTypes[rideType];
    if (set == nullptr) return;
    if (IsValidAnimation(set, 2, 0)) pylon->mFlags |= 0x80;
    if (IsValidAnimation(set, 3, 0)) pylon->mFlags |= 0x20;
    if (IsValidAnimation(set, 9, 0)) pylon->mFlags |= 0x100;
    if (IsValidAnimation(set, 10, 0)) pylon->mFlags |= 0x40;
    if (IsValidAnimation(set, 10, 1)) pylon->mFlags |= 0x200;
    // Kind 0x1000000: more than one entry sets flag 8 (two-part pylon), the value 1 entry going to mF48, the others to mF34.
    uint32_t count = CountDataNodesByType(set, 0x1000000);
    if (count != 0) {
        if (count > 1) {
            pylon->mFlags |= 8;
            pylon->mF40 = 2;
            pylon->mF54 = 1;
        } else {
            pylon->mF40 = 1;
        }
        for (uint32_t i = 0; i < count; i++) {
            int32_t index = FindNthDataNodeIndex(set, 0x1000000, i + 1);
            if ((pylon->mFlags & 8) != 0) {
                if (EmbeddedValue(set, index) == 1) {
                    pylon->mF48 = index;
                } else {
                    pylon->mF34 = index;
                }
            } else {
                pylon->mF34 = index;
            }
        }
    }
    if (CountDataNodesByType(set, 0x800000) != 0) {
        int32_t index = FindNthDataNodeIndex(set, 0x800000, 1);
        pylon->mF2C = index;
        if ((pylon->mFlags & 8) != 0) {
            pylon->mF30 = 0;
            pylon->mF44 = 1;
        } else {
            pylon->mF30 = static_cast<int32_t>(EmbeddedValue(set, index)) - 1;
        }
    }
    count = CountDataNodesByType(set, 0x2000000);
    if (count != 0) {
        for (uint32_t i = 0; i < count; i++) {
            int32_t index = FindNthDataNodeIndex(set, 0x2000000, i + 1);
            if ((pylon->mFlags & 8) != 0) {
                if (EmbeddedValue(set, index) == 3) {
                    pylon->mF4C = index;
                } else {
                    pylon->mF38 = index;
                }
            } else {
                pylon->mF38 = index;
            }
        }
    } else {
        pylon->mF3C = 0;
        pylon->mF50 = 0;
    }
    if (CountDataNodesByType(set, 0x100000) != 0) {
        pylon->mF18 = FindNthDataNodeIndex(set, 0x100000, 1);
    } else {
        pylon->mF18 = -1;
    }
    if (CountDataNodesByType(set, 0x80000) != 0) {
        for (uint32_t i = 0; i < 4; i++) {   // always four tries, whatever the count
            int32_t index = FindNthDataNodeIndex(set, 0x80000, i + 1);
            if (index == -1) continue;
            switch (static_cast<int32_t>(EmbeddedValue(set, index))) {
                case 1: pylon->mF1C = index; pylon->mFlags |= 0x400; break;
                case 2: pylon->mF20 = index; pylon->mFlags |= 0x800; break;
                case 3: pylon->mF24 = index; pylon->mFlags |= 0x1000; break;
                case 4: pylon->mF28 = index; pylon->mFlags |= 0x2000; break;
                default: break;
            }
        }
    }
    if ((pylon->mFlags & 8) != 0) {
        pylon->mF3C = 1;
        pylon->mF50 = 3;
    } else {
        pylon->mF3C = static_cast<int32_t>(CountDataNodesByType(set, 0x400000));
    }
    sFixedRideParams* params = GetTempFixedRideParams(pylon->mFB4, pylon->mFB8, 0, 0);
    uint32_t instance = InstanceRide(rideType, 0x3fa, params, 0, 0, nullptr, 0, 0, 0xffffffff, 0);
    tag_sRideAnimSet* instanceSet = gRideAnimSets[instance];
    sAnimTimeControl* control = static_cast<sAnimTimeControl*>(instanceSet->mTimeControls);
    for (uint32_t i = 0; i < 5; i++) {
        if ((pylon->mFlags & (0x20u << i)) != 0) {
            uint32_t kind = kPylonAnimKinds[i];
            if ((instanceSet->mGroups[kind].mFlags & 3) != 0) {
                SetNewAnim(instanceSet, kind, kPylonAnimIndexes[i], control, 0, 1.0f);
            }
        }
        control++;   // 0x38 bytes
    }
    instanceSet->mFlags |= 0x1000;
    // Time 0 and time 1 of the pylon's range: k * mFD0[2] + mFD0[3] + 0, clamped to mFD0[0] .. mFD0[1], times the control's length.
    auto animate = [&](float k) {
        float f2 = k * pylon->mFD0[2];
        f2 = f2 + pylon->mFD0[3];
        f2 = f2 + 0.0f;
        if (f2 < pylon->mFD0[0]) f2 = pylon->mFD0[0];
        if (f2 > pylon->mFD0[1]) f2 = pylon->mFD0[1];
        sAnimTimeControl* first = static_cast<sAnimTimeControl*>(gRideAnimSets[instance]->mTimeControls);
        first->mF20 = f2 * first->mF1C;
        AnimateRideSet(gRideAnimSets[instance], 1, 0);
    };
    animate(0.0f);
    pylon->mFBC = EmbeddedBlockFloat(gRideAnimSets[instance], pylon->mF2C);
    pylon->mF68 = (pylon->mF68 < pylon->mFBC) ? pylon->mFBC : pylon->mF68;
    if (pylon->mF18 != -1) {
        pylon->mFC4 = EmbeddedBlockFloat(gRideAnimSets[instance], pylon->mF18);
        pylon->mFC4 = pylon->mFC4 - pylon->mFBC;
    } else {
        pylon->mFC4 = 0.0f;
    }
    animate(1.0f);
    pylon->mFC0 = EmbeddedBlockFloat(gRideAnimSets[instance], pylon->mF2C);
    DestroyRideInstance(instance);
}

// 0x1003d5b0: one static params record (the cell x and z, no turn, a pointer to a static pair {a, b}); the same record every call.
sFixedRideParams* GetTempFixedRideParams(int32_t a, int32_t b, int32_t x, int32_t z) {
    static int32_t sPair[2];              // 0x102b5c08
    static sFixedRideParams sParams;      // 0x102b5bf8
    sPair[0] = a;
    sPair[1] = b;
    sParams.mX = x;
    sParams.mZ = z;
    sParams.mRotation = 0;
    sParams.mF0C = sPair;
    return &sParams;
}

// 0x100369d8: `count` selects (zeroed block, MReleaseAllocF flag 1), each with +-FLT_MAX limits and no first record.
void SetCrossSectionSelectDefaults(tag_sCoasterType* type, uint32_t count, tag_sCrossSectionSelect** out, sCoasterRecord* a,
                                   sCoasterRecord* b, int32_t flag) {
    tag_sCrossSectionSelect* block =
        static_cast<tag_sCrossSectionSelect*>(MReleaseAllocF(count * static_cast<uint32_t>(sizeof(tag_sCrossSectionSelect)), 1));
    if (flag != 0) type->mSelectFlags |= 1;
    type->mSelectCount = count;
    type->mF78 = block;
    type->mSelectA = a;
    type->mSelectB = b;
    uint32_t lowBits = 0xff7fffff;    // 0x10264728: -FLT_MAX
    uint32_t highBits = 0x7f7fffff;   // 0x10264724: FLT_MAX
    float low, high;
    std::memcpy(&low, &lowBits, sizeof low);
    std::memcpy(&high, &highBits, sizeof high);
    for (uint32_t i = 0; i < count; i++) {
        tag_sCrossSectionSelect* select = block + i;
        select->mF00[0] = low;
        select->mF00[1] = high;
        select->mF00[2] = low;
        select->mF00[3] = high;
        select->mF00[4] = low;
        select->mF00[5] = high;
        select->mF00[6] = low;
        select->mF00[7] = high;
        select->mF20 = nullptr;
    }
    if (out != nullptr) *out = block;
}

// 0x1003851c: the selects of the .sam (count at balance 0x38a4, four pairs of degrees per select at 0x3534 + 0x2c * i, three section
// numbers per select at 0x3554, 0x3558, 0x355c). A section number (1 up) above `count` ends the walk with 0; the first of the three
// must also be nonzero. The flag word of the type (+0) gets 0x40 when the .sam names a record (balance 0x3b04).
int32_t CreateCrossSectionSelectsFromSamFile(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count,
                                             CCoasterBalance* balance, int32_t flag) {
    sCoasterRecord* named = nullptr;
    uint32_t number = static_cast<uint32_t>(SlotInt(balance, 0x3b04));
    if (static_cast<int32_t>(number) != 0) {
        type->mFlags |= 0x40;
        named = records + (number - 1);
    }
    uint32_t selectCount = static_cast<uint32_t>(SlotInt(balance, 0x38a4));
    if (selectCount == 0) return 0;
    tag_sCrossSectionSelect* select = nullptr;
    SetCrossSectionSelectDefaults(type, selectCount, &select, records + count, named, flag);
    const float radians = 0.017453292f;   // 0x10264720
    for (uint32_t i = 0; i < selectCount; i++, select++) {
        for (int pair = 0; pair < 4; pair++) {
            float f1 = SlotFloat(balance, 0x3534 + 0x2c * i + 8 * pair);
            float f2 = SlotFloat(balance, 0x3538 + 0x2c * i + 8 * pair);
            if (!(0.0f == f1 && 0.0f == f2)) {
                select->mF00[2 * pair] = radians * f1;
                select->mF00[2 * pair + 1] = radians * f2;
            }
        }
        uint32_t first = static_cast<uint32_t>(SlotInt(balance, 0x3554 + 0x2c * i));
        if (first == 0 || first > count) return 0;
        select->mF20 = records + (first - 1);
        uint32_t second = static_cast<uint32_t>(SlotInt(balance, 0x3558 + 0x2c * i));
        if (second != 0) {
            if (second > count) return 0;
            select->mF24 = records + (second - 1);
        }
        uint32_t third = static_cast<uint32_t>(SlotInt(balance, 0x355c + 0x2c * i));
        if (third != 0) {
            if (third > count) return 0;
            select->mF28 = records + (third - 1);
        }
    }
    return 1;
}

// 0x100387b4: moves the points of `record` whose `used` entry is nonzero down to `dst` (in order), writes each moved point's new
// number into `used`, and (once, flag 1 of the first edge's word) renumbers the edges' two point halfwords.
void RepackCrossSection(uint32_t* used, sCoasterRecord* record, sCrossSectionPoint* dst) {
    sCrossSectionPoint* source = record->mPoints;
    uint32_t next = 0;
    for (uint32_t i = 0; i < record->mF00; i++) {
        if (used[i] != 0) {
            if (dst != source) std::memmove(dst, source, sizeof(sCrossSectionPoint));
            used[i] = next;
            dst++;
            next++;
        }
        source++;
    }
    sCrossSectionEdge* edge = static_cast<sCrossSectionEdge*>(record->mBlock);
    if ((edge->mF00 & 1) != 0) return;
    for (uint32_t j = 0; j < record->mPointCount; j++, edge++) {
        edge->mF06 = static_cast<uint16_t>(used[edge->mF06]);
        edge->mF08 = static_cast<uint16_t>(used[edge->mF08]);
        edge->mF00 |= 1;
    }
}

// 0x100388cc: drops the points no edge uses from each section (two passes: the first counts, the second moves what is left into one
// block at the type's mF98). A section whose points are shared with another section, or that has a second point row, stays as it is
// (but pass 1 still repacks a section in place when no other section shares its points and it has unused points).
// The original's `used` is a 21-word local; here it is a persistent block of 0x10000 words (an edge or section of more than 21 points
// overflows the Mac's stack buffer).
void OptimiseCrossSections(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count) {
    static std::vector<uint32_t> sUsed(0x10000, 0);
    uint32_t total = 0;
    sCrossSectionPoint* out = nullptr;
    for (uint32_t pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            type->mF98 = MReleaseAllocF(total * 0x14, 1);
            out = static_cast<sCrossSectionPoint*>(type->mF98);
        }
        sCoasterRecord* record = records;
        for (uint32_t s = 0; s < count; s++, record++) {
            if (sUsed.size() < record->mF00) sUsed.resize(record->mF00, 0);
            uint32_t* used = sUsed.data();
            for (uint32_t i = 0; i < record->mF00; i++) used[i] = 0;
            sCrossSectionEdge* edge = static_cast<sCrossSectionEdge*>(record->mBlock);
            for (uint32_t j = 0; j < record->mPointCount; j++, edge++) {
                used[edge->mF06] = 1;
                used[edge->mF08] = 1;
            }
            uint32_t n = 0;
            for (uint32_t i = 0; i < record->mF00; i++) n += used[i];
            if (n < record->mF00 && record->mPointsB == nullptr) {
                bool shared = false;
                for (uint32_t t = 0; t < count; t++) {
                    if (s != t && record->mPoints == records[t].mPoints) {
                        shared = true;
                        break;
                    }
                }
                if (!shared) {
                    RepackCrossSection(used, record, record->mPoints);
                } else if (pass == 0) {
                    total += n;
                } else {
                    RepackCrossSection(used, record, out);
                    record->mPoints = out;
                    record->mF00 = n;
                    out += n;
                }
            }
        }
        if (total == 0) {
            type->mF98 = nullptr;
            break;
        }
    }
}
