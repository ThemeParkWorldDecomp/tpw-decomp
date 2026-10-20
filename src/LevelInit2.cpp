// The park's second initialisation and its teardown (main application, TPRide.cpp range; unit w110): LevelInit2 and LevelDeInit,
// with the four height field accessors of CWorld they read the level's place and size through.
#include "Advisor.h"
#include "HLSprite.h"
#include "LevelInit2.h"
#include "MainBalance.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"
#include "tb_hfield.h"

// The height field's place and size (names invented).
int32_t gHeightfieldXStart;        // 0x102c6c70
int32_t gHeightfieldYStart;        // 0x102c6c6c
int32_t gHeightfieldWidth;         // 0x102c6c68
int32_t gHeightfieldHeight;        // 0x102c6c64
float gHeightfieldXOrigin;         // 0x102c6c60
float gHeightfieldYOrigin;         // 0x102c6c5c
int32_t gHeightfieldRangeExtraX;   // 0x102c6c58
int32_t gHeightfieldRangeExtraY;   // 0x102c6c54

namespace {
// The main balance's value slots for the four height field rows: the Mac's byte offsets 0x544, 0x548, 0x54c and 0x550, less the
// table's 8 bytes of header, over four (src/ChunkFile.cpp reads the first two with the same numbers).
constexpr int kBalanceHeightfieldXStart = (0x544 - 8) / 4;
constexpr int kBalanceHeightfieldYStart = (0x548 - 8) / 4;
constexpr int kBalanceHeightfieldWidth = (0x54c - 8) / 4;
constexpr int kBalanceHeightfieldHeight = (0x550 - 8) / 4;
}  // namespace

// 0x1013e89c, 0x1013e8dc, 0x1013e91c and 0x1013e95c (12 bytes each): the world is not used, the answer is a word of gMainBalance.
int32_t CWorld::GetHeightfieldXStart() const { return gMainBalance.mValues[kBalanceHeightfieldXStart].mInteger; }
int32_t CWorld::GetHeightfieldYStart() const { return gMainBalance.mValues[kBalanceHeightfieldYStart].mInteger; }
int32_t CWorld::GetHeightfieldWidth() const { return gMainBalance.mValues[kBalanceHeightfieldWidth].mInteger; }
int32_t CWorld::GetHeightfieldHeight() const { return gMainBalance.mValues[kBalanceHeightfieldHeight].mInteger; }

// 0x100639b0 (428 bytes). The level's attribute map is loaded, then the four numbers of the height field are asked of the world
// (height first, then width, y start, x start) and stored in the opposite order. With a height field header that has a height
// field, the two origins are the field's scale words (mF08[2] and mF08[3], at +0x10 and +0x14) times the starts, each start
// taken as an unsigned number (the 0x43300000 idiom: exact in double, rounded to single, then the multiply in single). With
// options bit 0x2000 the procedural textures' range is set to the level's rectangle, widened by two words nothing writes.
// Then the dynamic cell meshes, the poly search, the highlight and scratch buffers (the scratch buffer is made small), the
// heightmap system, the coaster types, the low-level cell system (128 by 128) and the script system. Answers 1. The world the
// accessors are called on is *gGameWorldRef (the Mac loads it twice); they never use it, so it may be NULL.
int32_t LevelInit2() {
    LoadLevelAttributeMap();
    const int32_t height = gGameWorldRef->GetHeightfieldHeight();
    const int32_t width = gGameWorldRef->GetHeightfieldWidth();
    const int32_t yStart = gGameWorldRef->GetHeightfieldYStart();
    const int32_t xStart = gGameWorldRef->GetHeightfieldXStart();
    gHeightfieldXStart = xStart;
    gHeightfieldYStart = yStart;
    gHeightfieldWidth = width;
    gHeightfieldHeight = height;
    tag_sM3D2Header* header = gHeightfieldHeader;
    if (header != nullptr && header->mHField != nullptr) {
        const float xOrigin = static_cast<float>(static_cast<double>(static_cast<uint32_t>(xStart)));
        const float yOrigin = static_cast<float>(static_cast<double>(static_cast<uint32_t>(yStart)));
        gHeightfieldXOrigin = header->mHField->mF08[2] * xOrigin;
        gHeightfieldYOrigin = header->mHField->mF08[3] * yOrigin;
    }
    if ((gGameOptions & 0x2000) != 0) {
        const int32_t farX = gHeightfieldXStart + (gHeightfieldWidth + gHeightfieldRangeExtraX);
        const int32_t farY = gHeightfieldYStart + (gHeightfieldHeight + gHeightfieldRangeExtraY);
        PT_SetRange(gHeightfieldXStart, gHeightfieldYStart, farX, farY);
    }
    DC_SetupDynamicCellMeshes();
    SetupForPolySearch();
    DestroyHighlightColourBuffers();
    DestroyScratchGeomBuffer();
    CreateScratchGeomBuffer(0);
    CreateHighlightColourBuffers();
    SetupHeightmapSystem();
    InitialiseCoasterTypes();
    LLCC_SystemInit(0x80, 0x80);
    SCR_InitScriptSystem();
    return 1;
}

// 0x10063b80 (284 bytes). `keep` (tested as its low byte) is set when the same level is started again: the poly search, the
// sprites, the height field, the ride types and the highlight buffers are then kept. The ride instances are walked by number
// (1 up, the sets table is indexed from 1) while the number is below the count of instances plus free slots, which
// DestroyRideInstance changes, so the sum is read again each time.
void LevelDeInit(bool keep) {
    DC_FreeDynamicCellMeshes();
    DestroyScratchGeomBuffer();
    ADV_Close();
    DestroyAdvisor(1);
    DestroyAdvisor(0);
    ClosedownRideHoardings();
    DestroyCoasterInstances();
    DestroyCoasterTypes();
    DestroyLevelLights();
    if (!keep) SetdownFromPolySearch();
    LLCC_SystemDeInit();
    if (!keep) ESP_RemoveSprites();
    for (uint32_t i = 0; i < gRideCountB + gRideCountA; ++i) {
        if (gRideAnimSets[i + 1] != nullptr) DestroyRideInstance(i + 1);
    }
    if (!keep) {
        DestroyLevelHeightfield();
        DestroyAllRideTypes();
        DestroyHighlightColourBuffers();
    }
    DestroyPathAllocs();
}
