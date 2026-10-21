// PARTIAL HLSprite.cpp. Only the array represented by __arraydtor$3071 is translated.
#pragma once
#include <cstddef>
#include <cstdint>
#include "tb_starfield.h"
extern CSpriteList gHLSpriteLists[16]; // main app 0x102dbdfc, original name unknown

// The rest of the HLSprite.cpp range: the sprite set loading and the flies (FLY_*).
// PARTIAL; untranslated.
void ESP_InitialiseAndLoadSprites(bool a, bool lobby);   // 0x1007ace0
void ESP_RemoveSprites();                                // 0x1007c790 (src/game/HLSprite.cpp)
void FLY_Init();               // 0x100752d4
// 0x1007a0b8 (756 bytes, untranslated; the inventory files it under HLSprite.cpp): loads the level's object file (CGame::InitLevel_Load calls it
// with FindFilePath's answer); it tests the path for NULL itself.
void OBJECT_Load(char* path);
void FLY_SetFlyZone();         // 0x100755a0
void FLY_Tick();               // 0x1007567c
void FLY_Draw(float step);     // 0x10077ae4
void FLY_Close();              // 0x100783ec

// Button highlight, effect 2 (src/game/HLSprite.cpp).
void BTN_HighLight_On(float x, float y, float width, float height); // 0x1007a934
void BTN_HighLight_Move(float x, float y);                         // 0x1007ab1c
void BTN_HighLight_Kill();                                        // 0x1007ab60
void BTN_HighLight_Off();                                         // 0x1007ac14
extern float gHLBaseArea;       // 0x102dab0c
extern int8_t gHLBaseAreaSet;   // 0x102dab08
extern float gHLAreaScale;      // 0x102dab04
extern int8_t gHLAreaScaleSet;  // 0x102dab00
extern int32_t gHLMinEmitters;  // 0x1029d30c, initially 2
extern int32_t gHLMaxEmitters;  // 0x1029d308, initially 5
// 0x100783ec (src/game/rides/FlyingRide.cpp): frees the sine table and removes the bases 1 to 99, when FLY_Init has run.
// 0x100781f4 (464 bytes, src/game/rides/FlyingRide.cpp, unit w114): removes one fly base (1 to 99, when its slot is set): for each of its 20 slots
// that is active, disables the ride heads the slot had enabled, kills its emitter, destroys its ride instance, counts the slot out of the
// base and stops its sound; then frees the base and clears the slot of gFlyBases. The layout of a base is below.
void FLY_RemoveBase(int32_t index);
// The flies' globals (the original names are unknown; src/game/rides/FlyingRide.cpp).
extern int32_t gFlyActive;         // 0x102daaf4  FLY_Init sets it to 1; FLY_Close leaves it
extern int32_t gFlyInitialised;    // 0x102daaf8  nonzero between FLY_Init and FLY_Close
extern void* gFlySinTable;         // 0x102daafc  the 0x4000-byte sine table FLY_Init makes (a 32-bit pointer on the Mac)
extern void* gFlyBases[100];       // 0x102da964  the fly bases FLY_RemoveBase indexes (slot 0 is never removed); not touched by FLY_Close

// The layout of a fly base, from FLY_RemoveBase and FLY_CreateBase (0x100753e8, untranslated; names invented): LbMem_Alloc(0x122c) bytes,
// only 32-bit words and no pointers, so the host layout is the Mac's. 20 slots start at +0xdc, 0xe4 bytes apart, and the words the
// translated code reads are in the first 0x64 bytes of each (the last slot ends exactly at 0x122c: 0xdc + 19 * 0xe4 + 0x64). The words
// nothing translated reads are left unnamed.
struct sFlySlot {
    uint32_t mActive;      // 0x00  nonzero while the slot is in use (FLY_RemoveBase never clears it)
    uint32_t mF04[2];      // 0x04
    int32_t mCount;        // 0x0c  how many times FLY_RemoveBase asks the ride set for a head to disable (its absolute value counts down)
    uint32_t mF10[12];     // 0x10
    uint32_t mRide;        // 0x40  the ride instance number (an index of gRideAnimSets), 0 for none
    uint32_t mEmitter;     // 0x44  the particle emitter handle, 0 for none
    uint32_t mF48[4];      // 0x48
    int32_t mOffset;       // 0x58  added to the count to pick the head
    int32_t mDivisor;      // 0x5c  the head number is (count + offset) mod divisor, plus 1
    uint32_t mSound;       // 0x60  the sound effect handle, 0 for none
};
static_assert(sizeof(sFlySlot) == 0x64, "sFlySlot layout");
static_assert(offsetof(sFlySlot, mCount) == 0x0c && offsetof(sFlySlot, mRide) == 0x40 && offsetof(sFlySlot, mEmitter) == 0x44 &&
                  offsetof(sFlySlot, mOffset) == 0x58 && offsetof(sFlySlot, mDivisor) == 0x5c && offsetof(sFlySlot, mSound) == 0x60,
              "sFlySlot layout");
constexpr uint32_t kFlySlotStride = 0xe4;   // the slots' distance on the Mac
constexpr int32_t kFlySlotCount = 20;
struct sFlyBase {
    uint32_t mF00[3];      // 0x00
    int32_t mLive;         // 0x0c  the slots in use: FLY_RemoveBase counts one down per active slot
    uint32_t mF10[(0xdc - 0x10) / 4];
    uint8_t mSlotArea[0x122c - 0xdc];   // 0xdc  the slots, kFlySlotStride apart
    sFlySlot* Slot(int32_t i) { return reinterpret_cast<sFlySlot*>(mSlotArea + static_cast<uint32_t>(i) * kFlySlotStride); }
};
static_assert(sizeof(sFlyBase) == 0x122c, "sFlyBase layout");
static_assert(offsetof(sFlyBase, mSlotArea) == 0xdc, "sFlyBase layout");

// The script sprites (HLSprite.cpp; unit w132's SCR_BuildScriptSpriteList calls them; untranslated).
struct tagSCRIPT;
class CThing;
CThing* LLCC_SystemGetPointedPeepThing();      // 0x10096bc8 (772 bytes, untranslated)
// 0x102dbd38 (name invented, src/game/HLSprite.cpp): ResetHLSprites sets it to 1; DrawHLSprites copies the cell counts when set.
extern int32_t gHLSpritesEnabled;
void ResetHLSprites();                         // 0x1007af24
void DrawHLSprites();                          // 0x1007b02c
void ESP_AddHLSpriteFromScript(tagSCRIPT* s);  // 0x1007ca9c
// Host only (src/game/park/analyser.cpp): the host's SSpriteGameData of a script block, which ESP_AddHLSpriteFromScript fills
// from the block's embedded Mac-layout record and lists (made zeroed at the first call for a block); whether `sprite` is it.
SSpriteGameData* ESP_ScriptHostSprite(const tagSCRIPT* script);
bool ESP_IsScriptHostSprite(const tagSCRIPT* script, const void* sprite);
enum ESP_Sprites : int32_t;
int32_t ESP_GetNumberOfCustomAnims(ESP_Sprites set, int32_t index);   // 0x1007c990 (40 bytes, untranslated)
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set);   // 0x1007c900 (84 bytes, untranslated)
// FLY_SetFlyZone (src/game/rides/FlyingRide.cpp, unit w133) writes the fly zone's four limits (initialised data, names invented).
extern int32_t gFlyZoneB;   // 0x1029d2b0  (initially 0x11170)
extern int32_t gFlyZoneA;   // 0x1029d2b4  (initially 0x222e0)
extern int32_t gFlyZoneD;   // 0x1029d2ac  (initially 0x2e630)
extern int32_t gFlyZoneC;   // 0x1029d2b8  (initially 0x7530)

// FLY_Draw (src/game/rides/FlyingRide.cpp): its working registers (bss, names invented).
extern float gFlyDrawPosX;      // 0x102da960
extern float gFlyDrawPosY;      // 0x102da95c
extern float gFlyDrawPosZ;      // 0x102da958
extern float gFlyDrawAngA;      // 0x102da954
extern float gFlyDrawAngB;      // 0x102da950
extern float gFlyDrawAngC;      // 0x102da94c
extern uint32_t gFlyDrawOrigin[3];   // 0x1029d2f8
// 0x10237cd8: turns a float into an integer angle (name and exact rule unknown; FLY_Draw masks the answer with 0xfff).
int32_t FLY_FloatToInt(float value);

// Unit v5198: GetNextWaypoint (src/game/rides/FlyingRide.cpp). Only the fields it touches are named (Mac offsets).
struct FLYBOY {
    int32_t m0;            // 0x00
    int32_t mX4;           // 0x04 position x
    int32_t m8;            // 0x08 height base
    int32_t mZC;           // 0x0c position z
    uint8_t mPad10[0x3c];  // 0x10
    int32_t mX4c;          // 0x4c target x
    int32_t mH50;          // 0x50 target height
    int32_t mZ54;          // 0x54 target z
    int32_t mPrevX58;      // 0x58 previous x
    int32_t mPrevH5c;      // 0x5c previous height
    int32_t mPrevZ60;      // 0x60 previous z
};
static_assert(offsetof(FLYBOY, mX4c) == 0x4c && offsetof(FLYBOY, mPrevZ60) == 0x60, "FLYBOY layout");
extern int32_t gFlyMinHeight;   // 0x1029d2c0 (initially 0x2ee0, name invented)
int32_t HeightAtPos_NAV(int32_t x, int32_t z);   // 0x10074230 (512 bytes, untranslated)
void GetNextWaypoint(FLYBOY* fly);               // 0x100745ec

// Unit v5209 (src/game/rides/FlyingRide.cpp): the fly file loader, the mesh restore and the object file loader.
// 0x10079aa8 (1220 bytes): loads the fly file (magic 0x5f594c46) from an open file; answers 1 for a bad magic, else 0.
int32_t FLY_Load(void* file);
// 0x10079f8c (260 bytes): re-instances the ride of every active slot of every fly base.
void FLY_RestoreMeshes();
// 0x100de828 (148 bytes, untranslated): sets the sound rectangle of a handle.
void SOUND_SetRect(int32_t handle, int32_t a, int32_t b, int32_t c, int32_t d);

// Unit v5216: GetNextHoldingWaypoint (src/game/rides/FlyingRide.cpp). Reads the fly base's fields by Mac offset
// (+0x14 x, +0x18 height, +0x1c z) and the state word at fly +0x88.
extern int32_t gFlyHoldMax;                      // 0x1029d2bc (initially 0x36b0, name invented)
void GetNextHoldingWaypoint(FLYBOY* fly);        // 0x10074458

// Unit v5648 (src/game/HLSprite.cpp): 0x10079740 (480 bytes), SamsUtilities::USwapBlockStruct<FLYBOY>.
// Byte-swaps `count` fly bases of 0xe4 bytes each (the Mac record; FLYBOY above names only its first 0x64 bytes).
namespace SamsUtilities {
void USwapBlockStruct(FLYBOY* records, int32_t count);
}

// Unit v5236 (src/game/rides/FlyingRide.cpp): FLY_CreateBase 0x100753e8 is declared in rsse.h.
// 0x10074d9c (1208 bytes): starts a car in the first free slot of the fly base `index`; answers 1, or 0 when all 20 slots are used.
int32_t EmployEric(int32_t index);

// Unit v5880 (src/game/rides/FlyingRide.cpp): FLY_Tick 0x1007567c (declared above). The lowest target heights it uses (initialised
// data, names invented, each initially 0x36b0).
extern int32_t gFlyApproachMinHeight;   // 0x1029d2c4  state 4 to 5
extern int32_t gFlyTurnMinHeight;       // 0x1029d2c8  state 4 to 6
extern int32_t gFlyTakeOffMinHeight;    // 0x1029d2cc  state 1 to 2

// Unit v51046 (src/game/HLSprite.cpp)
// 0x1007d53c, HLSprite.cpp's static initialiser: constructs gHLSpriteLists (16 lists of 0x28 bytes on the Mac), gESPGlobalList and
// gESPFileInfo and registers three destructors. The records and glue below are stand-ins for the Mac's words (names invented).
void __sinit_HLSprite_cpp();
void HLSpriteListCtorGlue(void* list);   // 0x103bca78 (import __ct__11CSpriteListFv); not called here
void HLSpriteListDtorGlue(void* list);   // 0x103bc914 (import __dt__11CSpriteListFv)
extern uint32_t gHLSpriteArrayDtor[2];   // 0x102661f8 TVector of __arraydtor$3071 (0x1007d614)
extern uint32_t gESPFileInfoDtor[2];     // 0x10265730 TVector of CFileInfo::~CFileInfo (0x10004994)
extern uint8_t gHLSpriteArrayNode[12];   // 0x102dc07c  registration record of gHLSpriteLists
extern uint8_t gESPGlobalListNode[12];   // 0x102dbd70  registration record of gESPGlobalList
extern uint8_t gESPFileInfoNode[12];     // 0x102db514  registration record of gESPFileInfo
