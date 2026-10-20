// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.
#include <cstdio>

#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "platform/samsutils.h"

// Globals (names invented; the Mac addresses are in the headers).
void* gPolySearchTable;
float gHeightmapScaleA;
float gHeightmapScaleB;
void* gHighlightFaceColours;
void* gHighlightNodeColours;
uint32_t gCoasterTypeCount;
tag_sCoasterType* gCoasterTypes[kMaxCoasterTypes];
uint32_t gCoasterTextureBase = 3;     // initialised data
uint32_t gExtrudeFaceCountA;
uint32_t gExtrudeFaceCountB;

namespace {

// The text DestroyLevelLights hands DataErrorHandler for a light of type 0 (0x1023d3de).
char gInvalidLightText[] = "Invalid lightsource creation attempted";

// What the Mac's DataErrorHandler titles its box (0x1023ebf6).
const char kDataErrorCaption[] = "M3D2 Exporter - Data Error";

}  // namespace

// 0x1005be44 (124 bytes). The Mac formats into a 1,032 byte buffer with sprintf; the host bounds the write. The world's map is at
// +0x2d8 (plain arithmetic: the Mac does not test the world).
bool LoadLevelAttributeMap() {
    char buffer[1032];
    char* themePath = get_theme_path();
    std::snprintf(buffer, sizeof buffer, "%s:Terrain:Base.map", themePath);
    const char* path = FindFilePath(buffer, 1, nullptr);
    return gGameWorld->GetMap()->LoadAttribMap(path) != 0;
}

// 0x1005c7e8 (60 bytes): 12 bytes for each of gPolySearchCount, zeroed (flag 1).
void SetupForPolySearch() { gPolySearchTable = MReleaseAllocF(gPolySearchCount * 0xc, 1); }

// 0x1005c850 (60 bytes).
void SetdownFromPolySearch() {
    MReleaseFreeF(gPolySearchTable);
    gPolySearchTable = nullptr;
}

// 0x1005f6f4 (24 bytes): 0.02f (the constant 0x3ca3d70a) into both words, the one at 0x102c1e0c first.
void SetupHeightmapSystem() {
    const float scale = 0.02f;
    gHeightmapScaleA = scale;
    gHeightmapScaleB = scale;
}

// 0x10061448 (40 bytes).
void ClosedownRideHoardings() { DMFreeTextures(&gRideHoardingTextures); }

// 0x1006446c (280 bytes). Type 2 takes out the directional light and then runs the point light's part too (the record's
// pointer is NULL, so it unlinks NULL and deletes nothing); 3 only the point light; 1 sets GlobalDiffuse to (1, 0, 0, 0) first and
// then does what 2 does (both pointers are NULL: two unlinks of NULL); 0 reports and leaves; a negative type or 4 and above
// does nothing. A light is unlinked (its link base, NULL for none) from the map's list 0x70, then deleted through its virtual
// deleting destructor (flag 1).
void DestroyLevelLights() {
    sGlobalLight* record = gGlobalLight;
    const int32_t type = record->mType;
    bool directional = false;
    if (type == 2) {
        directional = true;
    } else if (type > 1) {
        if (type > 3) return;
    } else {
        if (type == 0) {
            DataErrorHandler(gInvalidLightText);
            return;
        }
        if (type < 0) return;
        GlobalDiffuse[0] = 1.0f;
        const float zero = 0.0f;
        GlobalDiffuse[1] = zero;
        GlobalDiffuse[2] = zero;
        GlobalDiffuse[3] = zero;
        directional = true;
    }
    if (directional) {
        CMapLinkBase* link = nullptr;
        if (record->mDirectional != nullptr) link = record->mDirectional;
        gei_MapWho->Unlink(link, 0x70);
        if (record->mDirectional != nullptr) delete record->mDirectional;
    }
    CMapLinkBase* link = nullptr;
    if (record->mPoint != nullptr) link = record->mPoint;
    gei_MapWho->Unlink(link, 0x70);
    if (record->mPoint != nullptr) delete record->mPoint;
}

// 0x1006d1d0 (104 bytes, Debug.cpp): 12 bytes for each face (gMaxMeshFaces4 + gMaxMeshF3E), 4 for each node.
void CreateHighlightColourBuffers() {
    gHighlightFaceColours = MReleaseAllocF((gMaxMeshFaces4 + gMaxMeshF3E) * 0xc, 1);
    gHighlightNodeColours = MReleaseAllocF(gMaxMeshNodes << 2, 1);
}

// 0x1006d26c (88 bytes, Debug.cpp): both are freed before either is cleared.
void DestroyHighlightColourBuffers() {
    MReleaseFreeF(gHighlightFaceColours);
    MReleaseFreeF(gHighlightNodeColours);
    gHighlightFaceColours = nullptr;
    gHighlightNodeColours = nullptr;
}

// 0x10042478 (72 bytes, TPCoaster.cpp).
void InitialiseCoasterTypes() {
    CreateExtrudeFaceStorage(gExtrudeFaceCountA, gExtrudeFaceCountB);
    SetupObjectInstanceTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest);
}

// 0x1003acf0 (472 bytes, TPCoaster.cpp). Per type: the track sets' blocks (the one at +0x300 first, the others when they are
// not that set; +0x2fc and +0x304 may be the same set, whose block is then freed twice, the second time as NULL), +0x308, the
// texture set's first (count - gCoasterTextureBase) entries (the dynamic ones: CreateCoasterFromSamFile, src/TPCoaster_sam.cpp,
// puts the gCoasterTextureBase static entries LAST, so theirs are never freed; unit w138), the textures and the mesh set, then
// the texture set's entry array and itself, +0x18, +0x14, the record block (not cleared), +0x90 to +0x98, the record table and
// +0x78. The table entry is cleared last. The count of types is read again at every step and cleared at the end.
void DestroyCoasterTypes() {
    for (uint32_t index = 0; index < gCoasterTypeCount; ++index) {
        tag_sCoasterType* type = gCoasterTypes[index];
        sCarType* setB = type->mCarTypeB;
        sCarType* setA = type->mCarTypeA;
        sCarType* setC = type->mCarTypeC;
        MReleaseFreeF(setB->mBlock);
        setB->mBlock = nullptr;
        if (setA != setB) {
            MReleaseFreeF(setA->mBlock);
            setA->mBlock = nullptr;
        }
        if (setC != setB) {
            MReleaseFreeF(setC->mBlock);
            setC->mBlock = nullptr;
        }
        MReleaseFreeF(type->mCarTypes);
        type->mCarTypes = nullptr;
        const uint32_t base = gCoasterTextureBase;
        const uint32_t count = static_cast<uint32_t>(type->mTextures->mCount) - base;
        sDTextureEntry* entry = type->mTextures->mEntries;
        for (uint32_t i = 0; i < count; ++i) {
            MReleaseFreeF(entry->mBlock);
            entry->mBlock = nullptr;
            ++entry;
        }
        DMFreeTextures(type->mTextures);
        DMFreeMesh(type->mMeshSet);
        MReleaseFreeF(type->mTextures->mEntries);
        type->mTextures->mEntries = nullptr;
        MReleaseFreeF(type->mTextures);
        type->mTextures = nullptr;
        MReleaseFreeF(type->mMeshSet);
        type->mMeshSet = nullptr;
        MReleaseFreeF(type->mF14);
        type->mF14 = nullptr;
        MReleaseFreeF(type->mRecords[type->mRecordIndex].mBlock);
        MReleaseFreeF(type->mF90);
        MReleaseFreeF(type->mF94);
        MReleaseFreeF(type->mF98);
        type->mF90 = nullptr;
        type->mF94 = nullptr;
        type->mF98 = nullptr;
        MReleaseFreeF(type->mRecords);
        type->mRecords = nullptr;
        MReleaseFreeF(type->mF78);
        type->mF78 = nullptr;
        gCoasterTypes[index] = nullptr;
    }
    gCoasterTypeCount = 0;
}

// 0x100b9834 (72 bytes, rsse.cpp). The Mac's box has OK and Cancel (flags 0x31) and answers false for Cancel; the port logs the
// text and answers true, as a player clicking OK.
bool DataErrorHandler(char* text) {
    SamsUtilities::UDebug("%s: %s", kDataErrorCaption, text);
    return true;
}
