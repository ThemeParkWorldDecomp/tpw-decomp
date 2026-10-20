// The loaders CreateCoasterFromSamFile builds a coaster type with (main app TPCoaster.cpp, unit w138):
// CreateCoasterFromSamFile 0x10039518 (1,212 bytes), SetCoasterTypeDefaults 0x10036664 (824), SetupPylonControlsFromSamFile
// 0x10038d00 (1,400), SetupCarAndTrainTypesFromSamFile 0x100392cc (500), CreateCoasterTrackMasterDMeshSet 0x10036b20 (480) and
// CreateCrossSectionsFromSamFile 0x10038ae4 (452).
//
// The .sam values are read out of a CCoasterBalance: the Mac's byte offset into it is 8 + 4 * slot (the header is 8 bytes, the slots
// 4), the host's slot is balance->mValues[slot]; Slot() below turns the Mac offset the listing shows into the slot. An "optional"
// float is set when it differs from 0.0f (fcmpu: -0.0f is unset, a NaN is set). Every loop count is unsigned (cmplw).
#include <cmath>
#include <cstdint>
#include <cstring>

#include "CoasterBalance.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"

// The image's initialised data (names invented): 0x1029cdf0 and 0x1029cce4.
sDTextureEntry gCoasterStaticTextureEntries[3] = {
    {{const_cast<char*>("m_erase.tga")}, 7, 0, 0},
    {{const_cast<char*>("red.tga")}, 7, 0, 0},
    {{const_cast<char*>("blue.tga")}, 7, 0, 0},
};
float gCoasterDefaultPairs[10] = {4.0f, 8.0f, 4.0f, 0.0f, 0.0f, -3.0f, -4.0f, -1.0f, -4.0f, 8.0f};

namespace {

// The balance slot at Mac byte offset `offset`.
inline const BalanceValue& Slot(const CCoasterBalance* balance, uint32_t offset) {
    return balance->mValues[(offset - 8) / 4];
}
inline float SlotFloat(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mFloat; }
inline int32_t SlotInt(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mInteger; }
inline char* SlotString(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mString; }

// A float from its bit pattern (the listing's constants).
inline float Bits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}
// The word's bits as a float / an int (lwz/stw of a float slot is a plain copy).
inline uint32_t Word(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

// fsubs of (0x43300000, word) and 2^52, then the double sqrt and frsp: the root of an unsigned word, as a single.
inline float RootOfWord(uint32_t word) {
    return static_cast<float>(std::sqrt(static_cast<double>(static_cast<float>(word))));
}

// Stores `source` when the .sam float is set.
inline void StoreIfSet(float source, float& destination) {
    if (source != 0.0f) destination = source;
}

// A copy of a string in a block of strlen + 1 bytes (MReleaseAllocF flag 1: zeroed).
inline char* CopyString(const char* text) {
    char* copy = static_cast<char*>(MReleaseAllocF(static_cast<uint32_t>(std::strlen(text)) + 1, 1));
    std::strcpy(copy, text);
    return copy;
}

}  // namespace

// 0x10036664 (824 bytes). The two pylon controls (+0xa8 and +0x1c8) get their defaults; the roots at +0xa4 and +0xa8 of each are
// the double sqrt of the unsigned words at +0x9c and +0xa0 rounded to single (9 and 0x40 for pylon A, 9 and 0x51 for B).
void SetCoasterTypeDefaults(tag_sCoasterType* type) {
    type->mFlags = 0;
    type->mF40 = 0.0f;
    type->mF08 = Bits(0x3fa00000);
    type->mF0C = Bits(0x3fa00000);
    type->mF10 = Bits(0x40800000);
    type->mF24 = Bits(0x3fc00000);
    type->mF28 = Bits(0x3f800000);
    type->mF2C = Bits(0x3f800000);
    type->mF2E8 = 1;
    type->mF2EC = 15000;
    type->mFA4 = Bits(0x41200000);
    type->mF30 = 100;
    type->mF34 = 100;
    type->mF38 = 0;
    type->mF3C = 0x96;
    type->mF70 = Bits(0x3f800000);
    type->mF44 = Bits(0x40a00000);
    type->mF48 = Bits(0x40a00000);
    type->mF4C = Bits(0x42a00000);
    type->mF50 = Bits(0x43020000);
    type->mF54 = Bits(0x3f4ccccd);
    type->mF58 = 0.0f;
    type->mF5C = 0.0f;
    type->mF60 = 0.0f;
    type->mF64 = Bits(0xbe99999a);
    type->mF68 = 0.0f;
    type->mF6C = Bits(0x3e4ccccd);
    type->mF320 = 0.0f;
    type->mF324 = Bits(0x3f800000);
    type->mF328 = 0.0f;
    type->mF32C = Bits(0x3f000000);
    type->mF330 = Bits(0x40400000);
    type->mF334 = Bits(0x3dcccccd);
    type->mF338 = Bits(0x3f800000);
    type->mF33C = Bits(0x3f800000);
    type->mF340 = Bits(0x40a00000);
    type->mF344 = Bits(0x3f800000);
    type->mF348 = Bits(0x40000000);
    type->mF34C = Bits(0x3f000000);
    type->mF350 = Bits(0x3f800000);
    type->mF358 = Bits(0x41a00000);
    type->mF354 = Bits(0x41200000);
    type->mPairs = gCoasterDefaultPairs;
    type->mPairCount = kCoasterDefaultPairCount;
    for (uint32_t i = 0; i < 5; ++i) {
        float* a = &type->mPylonsA.mFD0[4 * i];
        a[0] = 0.0f;
        a[1] = Bits(0x3f800000);
        a[2] = Bits(0x3f800000);
        a[3] = 0.0f;
        float* b = &type->mPylonsB.mFD0[4 * i];
        b[0] = 0.0f;
        b[1] = Bits(0x3f800000);
        b[2] = Bits(0x3f800000);
        b[3] = 0.0f;
    }
    sPylonControl& a = type->mPylonsA;
    a.mFlags = 4;
    a.mFB0 = Bits(0xbf7fbe77);
    a.mFB4 = 1;
    a.mFB8 = 1;
    a.mF58 = Bits(0x40a00000);
    a.mFCC = 1;
    a.mF68 = Bits(0x41200000);
    a.mF6C = Bits(0x41200000);
    a.mAngles[0] = Bits(0x401c61aa);
    a.mAngles[1] = Bits(0x40490fdb);
    a.mAngles[2] = Bits(0x3ff5be0b);
    a.mAngles[3] = Bits(0x40490fdb);
    a.mAngles[4] = Bits(0x3fb2b8c2);
    a.mAngles[5] = Bits(0x40490fdb);
    a.mAngles[6] = Bits(0x3f5f66f3);
    a.mAngles[7] = Bits(0x40490fdb);
    a.mF9C = 9;
    a.mFA0 = 0x40;
    a.mFA4 = RootOfWord(static_cast<uint32_t>(a.mF9C));
    a.mFA8 = RootOfWord(static_cast<uint32_t>(a.mFA0));
    a.mAngles[8] = Bits(0x3f490fdb);
    a.mAngles[9] = 0.0f;
    a.mFAC = 0.0f;
    sPylonControl& b = type->mPylonsB;
    b.mFlags = 0x4004;
    b.mFB0 = Bits(0xbf4ccccd);
    b.mFB4 = 1;
    b.mFB8 = 1;
    b.mF58 = Bits(0x40a00000);
    b.mFCC = 1;
    b.mF68 = Bits(0x41200000);
    b.mF6C = Bits(0x41200000);
    b.mAngles[0] = Bits(0x40060a92);
    b.mAngles[1] = Bits(0x40425c20);
    b.mAngles[2] = Bits(0x40060a92);
    b.mAngles[3] = Bits(0x40425c20);
    b.mAngles[4] = 0.0f;
    b.mAngles[5] = Bits(0x40425c20);
    b.mAngles[6] = 0.0f;
    b.mAngles[7] = Bits(0x40425c20);
    b.mF9C = 9;
    b.mFA0 = 0x51;
    b.mFA4 = RootOfWord(static_cast<uint32_t>(b.mF9C));
    b.mFA8 = RootOfWord(static_cast<uint32_t>(b.mFA0));
    b.mAngles[8] = Bits(0x3fc90fdb);
    b.mAngles[9] = Bits(0x3fc90fdb);
    b.mFAC = Bits(0x41c80000);
}

// 0x10036b20 (480 bytes). The .sam names n textures (count at +0xfc, then 3 slots each from +0xc: name, flag, scroll rate): the
// mesh entry (0x30 bytes), the mesh set (0x40), the texture set (0x20) and its n + gCoasterTextureBase entries (0x10 each) are
// allocated in that order; the n dynamic entries come FIRST, the static entries (gCoasterTextureBase of them) are copied after.
int32_t CreateCoasterTrackMasterDMeshSet(tag_sCoasterType* type, char* texturePath, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0xfc));
    if (count == 0) return 0;
    sDMeshEntry* entry = static_cast<sDMeshEntry*>(MReleaseAllocF(sizeof(sDMeshEntry), 1));
    tag_sDMeshSet* meshSet = static_cast<tag_sDMeshSet*>(MReleaseAllocF(sizeof(tag_sDMeshSet), 1));
    sDTextureSet* textureSet = static_cast<sDTextureSet*>(MReleaseAllocF(sizeof(sDTextureSet), 1));
    sDTextureEntry* entries = static_cast<sDTextureEntry*>(
        MReleaseAllocF((count + gCoasterTextureBase) * static_cast<uint32_t>(sizeof(sDTextureEntry)), 1));
    meshSet->mEntries = entry;
    entry->mFlags = 3;
    meshSet->mF00 = 3;
    meshSet->mCount = 1;
    textureSet->mFlags = 0;
    textureSet->mCount = static_cast<uint16_t>(count + gCoasterTextureBase);
    textureSet->mEntries = entries;
    textureSet->mFolder = texturePath;
    sDTextureEntry* cursor = entries;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t row = 12 * i;
        cursor->mFlags = SlotInt(balance, 0x10 + row) != 0 ? 6 : 2;
        const float rate = SlotFloat(balance, 0x14 + row);
        if (rate != 0.0f) {
            cursor->mFlags |= 0x20;
            type->mFlags |= 4;
            cursor->mF0C = Word(rate);
        }
        const char* name = SlotString(balance, 0xc + row);
        cursor->mName = CopyString(name);
        ++cursor;
    }
    for (uint32_t j = 0; j < gCoasterTextureBase; ++j) {
        *cursor = gCoasterStaticTextureEntries[j];
        ++cursor;
    }
    type->mMeshSet = meshSet;
    type->mTextures = textureSet;
    type->mF14 = meshSet->mEntries;
    type->mTextureCount = count;
    return 1;
}

// 0x10038ae4 (452 bytes). n cross sections (count at +0x3530) in a table of n + 1 records (0x18 bytes each, at +0x88 of the
// type, the count at +0x8c); the builders fill the first n, the extra record at [n] is a copy of the section the .sam names at
// +0x3b00 (1-based, 0 = the first) with its own copy of the point block. The two empty loops of the original are omitted.
int32_t CreateCrossSectionsFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0x3530));
    if (count == 0) return 0;
    sCoasterRecord* records = static_cast<sCoasterRecord*>(
        MReleaseAllocF((count + 1) * static_cast<uint32_t>(sizeof(sCoasterRecord)), 1));
    type->mRecords = records;
    type->mRecordIndex = count;
    int32_t out = 0;
    if (BuildCrossSectionPoints(type, records, balance, &out) == 0) return 0;
    if (BuildCrossSectionEdges(type, records, balance) == 0) return 0;
    const int32_t chosen = SlotInt(balance, 0x3b00);
    const int32_t index = chosen != 0 ? chosen - 1 : 0;
    sCoasterRecord* source = records + index;
    sCoasterRecord* copy = records + count;
    const uint32_t points = source->mPointCount;
    copy->mF00 = source->mF00;
    copy->mPointCount = points;
    const uint32_t size = points * 0xc;
    copy->mPoints = source->mPoints;
    copy->mBlock = source->mBlock;
    copy->mPointsB = source->mPointsB;
    copy->mF14 = source->mF14;
    copy->mBlock = MReleaseAllocF(size, 1);
    if (copy->mBlock != nullptr && source->mBlock != nullptr && points != 0) std::memcpy(copy->mBlock, source->mBlock, size);
    if (CreateCrossSectionSelectsFromSamFile(type, records, count, balance, out) == 0) return 0;
    OptimiseCrossSections(type, records, count);
    return 1;
}

// 0x10038d00 (1,400 bytes). Up to the .sam's pylon count (+0x3a20) of rows (0xbc bytes each from +0x38a8; the first row is
// pylon A at type +0xa8, the others pylon B at +0x1c8). A row without a name, a word at +4 or a float at +0xc returns 0.
int32_t SetupPylonControlsFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0x3a20));
    for (uint32_t i = 0; i < count; ++i) {
        sPylonControl* pylon = i == 0 ? &type->mPylonsA : &type->mPylonsB;
        const uint32_t row = i * 0xbc;
        char* name = SlotString(balance, 0x38a8 + row);
        if (name == nullptr || SlotInt(balance, 0x38ac + row) == 0 || SlotFloat(balance, 0x38b4 + row) == 0.0f) return 0;
        pylon->mName = CopyString(name);
        pylon->mF0C = static_cast<uint32_t>(SlotInt(balance, 0x38ac + row));
        if (SlotInt(balance, 0x38b0 + row) != 0) pylon->mFlags |= 0x10;
        pylon->mF58 = SlotFloat(balance, 0x38b4 + row);
        StoreIfSet(SlotFloat(balance, 0x38b8 + row), pylon->mFB0);
        if (SlotInt(balance, 0x38bc + row) != 0) pylon->mFB4 = SlotInt(balance, 0x38bc + row);
        if (SlotInt(balance, 0x38c0 + row) != 0) pylon->mFB8 = SlotInt(balance, 0x38c0 + row);
        if (SlotInt(balance, 0x38c4 + row) != 0) pylon->mFCC = SlotInt(balance, 0x38c4 + row);
        StoreIfSet(SlotFloat(balance, 0x38c8 + row), pylon->mF68);
        StoreIfSet(SlotFloat(balance, 0x38cc + row), pylon->mF6C);
        StoreIfSet(SlotFloat(balance, 0x38d0 + row), pylon->mF70);
        for (uint32_t k = 0; k < 10; ++k) {
            const float degrees = SlotFloat(balance, 0x38d4 + 4 * k + row);
            if (degrees != 0.0f) pylon->mAngles[k] = Bits(0x3c8efa35) * degrees;
        }
        const int32_t first = SlotInt(balance, 0x38fc + row);
        if (first != 0) {
            pylon->mF9C = static_cast<int32_t>(static_cast<uint32_t>(first) * static_cast<uint32_t>(first));
            pylon->mFA4 = static_cast<float>(SlotInt(balance, 0x38fc + row));
        }
        const int32_t second = SlotInt(balance, 0x3900 + row);
        if (second != 0) {
            pylon->mFA0 = static_cast<int32_t>(static_cast<uint32_t>(second) * static_cast<uint32_t>(second));
            pylon->mFA8 = static_cast<float>(SlotInt(balance, 0x3900 + row));
        }
        const float tilt = SlotFloat(balance, 0x3904 + row);
        if (tilt != 0.0f) {
            const float scaled = Bits(0x41200000) * tilt;
            pylon->mFAC = Bits(0x3c8efa35) * scaled;
        }
        pylon->mF5C = SlotFloat(balance, 0x3908 + row);
        pylon->mF60 = SlotFloat(balance, 0x390c + row);
        pylon->mF64 = SlotFloat(balance, 0x3910 + row);
        for (uint32_t k = 0; k < 20; ++k) StoreIfSet(SlotFloat(balance, 0x3914 + 4 * k + row), pylon->mFD0[k]);
    }
    return 1;
}

// 0x100392cc (500 bytes). The .sam's car types (count at +0x3a78, rows of 0x1c bytes from +0x3a24) in one array of 0x34-byte
// records (zeroed, so the block at +0x2c stays NULL); a row without a value (+4) or a name (+8) returns 0 and leaves the array
// unreferenced (a leak). The type's three sets (+0x2fc, +0x300, +0x304) are the .sam's indices (+0x3a80 to +0x3a88) into it.
int32_t SetupCarAndTrainTypesFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0x3a78));
    if (count == 0) return 0;
    sCarType* cars = static_cast<sCarType*>(MReleaseAllocF(count * static_cast<uint32_t>(sizeof(sCarType)), 1));
    sCarType* car = cars;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t row = i * 0x1c;
        car->mFlags = 0;
        car->mValue = 0;
        car->mName = nullptr;
        car->mF14 = 0.0f;
        car->mF10 = 0.0f;
        car->mF18 = 0.0f;
        car->mF1C = Bits(0x3dcccccd);
        if (SlotInt(balance, 0x3a28 + row) == 0 || SlotString(balance, 0x3a2c + row) == nullptr) return 0;
        if (SlotInt(balance, 0x3a24 + row) != 0) car->mFlags |= 1;
        car->mValue = SlotInt(balance, 0x3a28 + row);
        car->mName = CopyString(SlotString(balance, 0x3a2c + row));
        car->mF10 = SlotFloat(balance, 0x3a30 + row);
        car->mF14 = SlotFloat(balance, 0x3a34 + row);
        car->mF18 = SlotFloat(balance, 0x3a38 + row);
        StoreIfSet(SlotFloat(balance, 0x3a3c + row), car->mF1C);
        ++car;
    }
    type->mCarTypes = cars;
    type->mF2F0 = 0;
    type->mF2F4 = 3;
    type->mCarTypeA = cars + SlotInt(balance, 0x3a80);
    type->mCarTypeB = cars + SlotInt(balance, 0x3a84);
    type->mCarTypeC = cars + SlotInt(balance, 0x3a88);
    if (type->mCarTypeA != type->mCarTypeB) type->mF2F0 |= 1;
    if (type->mCarTypeC != type->mCarTypeB) type->mF2F0 |= 2;
    if (SlotInt(balance, 0x3a7c) != 0) type->mF2F4 = SlotInt(balance, 0x3a7c);
    type->mF30C = SlotFloat(balance, 0x3a8c);
    type->mF310 = SlotFloat(balance, 0x3a90);
    type->mF314 = SlotFloat(balance, 0x3a94);
    return 1;
}

// 0x10039518 (1,212 bytes): builds the type's track, cross sections, pylon controls and car types (each failure shows its
// message and returns 0), then the optional .sam values.
int32_t CreateCoasterFromSamFile(tag_sCoasterType* type, char* texturePath, CCoasterBalance* balance) {
    StoreIfSet(SlotFloat(balance, 0x3b08), type->mF40);
    if (CreateCoasterTrackMasterDMeshSet(type, texturePath, balance) == 0) {
        ERRORBox("Failed to setup track for coaster");
        return 0;
    }
    if (CreateCrossSectionsFromSamFile(type, balance) == 0) {
        ERRORBox("Failed to setup cross sections for coaster");
        return 0;
    }
    if (SetupPylonControlsFromSamFile(type, balance) == 0) {
        ERRORBox("Failed to setup pylon controls for coaster");
        return 0;
    }
    if (SetupCarAndTrainTypesFromSamFile(type, balance) == 0) {
        ERRORBox("Failed to setup car and trains for coaster");
        return 0;
    }
    const int32_t pairs = SlotInt(balance, 0x3ac8);
    if (pairs != 0) {
        type->mPairCount = static_cast<uint32_t>(pairs);
        type->mPairs = static_cast<float*>(MReleaseAllocF(type->mPairCount << 3, 1));
        for (uint32_t i = 0; i < type->mPairCount; ++i) {
            type->mPairs[2 * i] = SlotFloat(balance, 0x3a98 + 8 * i);
            type->mPairs[2 * i + 1] = SlotFloat(balance, 0x3a9c + 8 * i);
        }
    }
    if (SlotInt(balance, 0x3ae8) != 0) type->mF2E8 = SlotInt(balance, 0x3ae8);
    if (SlotInt(balance, 0x3aec) != 0) type->mF2EC = SlotInt(balance, 0x3aec);
    StoreIfSet(SlotFloat(balance, 0x3ae4), type->mFA4);
    StoreIfSet(SlotFloat(balance, 0x3acc), type->mF08);
    StoreIfSet(SlotFloat(balance, 0x3ad0), type->mF0C);
    StoreIfSet(SlotFloat(balance, 0x3ad8), type->mF24);
    StoreIfSet(SlotFloat(balance, 0x3adc), type->mF28);
    StoreIfSet(SlotFloat(balance, 0x3ae0), type->mF2C);
    type->mF31C = SlotFloat(balance, 0x3b74);
    StoreIfSet(SlotFloat(balance, 0x3b0c), type->mF44);
    StoreIfSet(SlotFloat(balance, 0x3b10), type->mF48);
    StoreIfSet(SlotFloat(balance, 0x3b14), type->mF4C);
    StoreIfSet(SlotFloat(balance, 0x3b18), type->mF50);
    StoreIfSet(SlotFloat(balance, 0x3b28), type->mF60);
    StoreIfSet(SlotFloat(balance, 0x3b2c), type->mF64);
    StoreIfSet(SlotFloat(balance, 0x3b30), type->mF68);
    StoreIfSet(SlotFloat(balance, 0x3b34), type->mF6C);
    StoreIfSet(SlotFloat(balance, 0x3b38), type->mF320);
    StoreIfSet(SlotFloat(balance, 0x3b3c), type->mF324);
    StoreIfSet(SlotFloat(balance, 0x3b40), type->mF328);
    StoreIfSet(SlotFloat(balance, 0x3b44), type->mF32C);
    StoreIfSet(SlotFloat(balance, 0x3b48), type->mF330);
    StoreIfSet(SlotFloat(balance, 0x3b4c), type->mF334);
    StoreIfSet(SlotFloat(balance, 0x3b50), type->mF338);
    StoreIfSet(SlotFloat(balance, 0x3b54), type->mF33C);
    StoreIfSet(SlotFloat(balance, 0x3b58), type->mF340);
    StoreIfSet(SlotFloat(balance, 0x3b5c), type->mF344);
    StoreIfSet(SlotFloat(balance, 0x3b60), type->mF348);
    StoreIfSet(SlotFloat(balance, 0x3b64), type->mF34C);
    StoreIfSet(SlotFloat(balance, 0x3b68), type->mF350);
    StoreIfSet(SlotFloat(balance, 0x3b6c), type->mF354);
    StoreIfSet(SlotFloat(balance, 0x3b70), type->mF358);
    if (SlotInt(balance, 0x3af0) != 0) {
        type->mPylonsB.mFlags &= 0xffffbfffu;
        type->mPylonsB.mFlags |= 0x8000;
        type->mFlags |= 0x100;
    }
    StoreIfSet(SlotFloat(balance, 0x3b1c), type->mF54);
    StoreIfSet(SlotFloat(balance, 0x3b20), type->mF58);
    StoreIfSet(SlotFloat(balance, 0x3b24), type->mF5C);
    type->mFlags |= 0xa0;
    if (SlotInt(balance, 0x3afc) != 0) type->mFlags |= 0x20;
    if (SlotInt(balance, 0x3af8) == 0) type->mFlags |= 0x10;
    if (SlotInt(balance, 0x3af4) != 0) {
        type->mFlags |= 8;
        type->mFlags &= 0xffffffefu;
    }
    if (SlotInt(balance, 0x3b78) != 0) {
        type->mF30 = SlotInt(balance, 0x3b78);
        type->mF34 = SlotInt(balance, 0x3b78);
    }
    if (SlotInt(balance, 0x3b7c) != 0) type->mF3C = SlotInt(balance, 0x3b7c);
    return 1;
}
