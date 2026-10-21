// TPCamera.cpp: name proven by __sinit_TPCamera_cpp in the main application.

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Clock.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "helpers/BumperRideHelpers.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Light.h"
#include "lobby.h"
#include "m3d2.h"
#include "MapCell.h"
#include "MapWho.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "Quickload.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_system.h"
#include "ThingReceivers.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// TPAdvisor.cpp (main app of SimTheme Park 2.04b3): the advisor's model, 0x10031bc0..0x1003228c, and the slots'
// constructor 0x10032454. (The file's text reader is in src/TPAdvisor.cpp, which two other unit libraries link, so the
// model is kept apart.)
//
// The game keeps four slots (gAdvisorStore), each a ride animation set loaded from "Advisor<variant>.md2" in the global
// advisor folder, an instance of it that is drawn and animated (group 5 holds his animations), a matrix to stand on and a
// light. The instance is drawn in the interface's space, scaled down, lit by the slot's directional light, which is linked
// into map list 0x10. LoadAdvisor makes all that; ADV_Tick and DressAdvisor (src/Advisor.cpp) drive it.

// ---- the advisor's model

// __sinit_TPAdvisor_cpp (0x1003228c) constructs the four slots with this and registers their destruction at exit. The Mac
// constructor (0x10032454) makes the slot's CLight (flags 0), sets its flags to 2 (a directional light), then calls
// CLight::SetNormal(0, 0, 1) and CLight::SetColor(1, 1, 1, 1), each of which tells the engine the lights changed
// (CSystem::UpdateLights) and then stores: the normal; the four colour floats and the colour word, each byte the float
// times 255 (0xffffffff for white). The two are not translated (nothing else calls them) and are written out here.
// The set, the instance and the matrix are not touched (the storage is zero).
sAdvisorStore::sAdvisorStore() {
    mLight.mFlags = 2;
    gei_System.UpdateLights();
    mLight.mNormal = {0.0f, 0.0f, 1.0f};
    gei_System.UpdateLights();
    mLight.mColor[0] = 1.0f;
    mLight.mColor[1] = 1.0f;
    mLight.mColor[2] = 1.0f;
    mLight.mColor[3] = 1.0f;
    mLight.mColorWord = 0xffffffffu;
}

// TPCamera.cpp range of the main app (SimTheme Park 2.04b3): InitCamera (0x100334d4, 344 bytes) and Load1stPersonMesh
// (0x10032558, 4 bytes). The park camera's globals that these set are defined here.

// Initialised data (0x1029cc68..0x1029cc87) and BSS (0x102b5a90..0x102b5bf4).
float gCameraF68 = 110.0f;
float gCameraStartPosition[3] = {475.0f, 0.0f, 175.0f};
float gCameraPosition[3] = {0.0f, 0.0f, 0.0f};
float gCameraFov = 1.5707963705062866f;
uint32_t gCameraClockStamp;
uint32_t gCameraModeA;
uint32_t gCameraModeB;
uint32_t gCameraF5AAC;
sMatrix gCameraMatrixA;
sMatrix gCameraMatrixB;
sMatrix gCameraMatrix;
float gCameraF5BB4[3];
float gCameraF5BF4;

void Load1stPersonMesh() {}

// 0x10032584 (100 bytes): while the first-person ride bit is up, leaves the ride (the remembered instance) and, when that returned
// the player to walking, moves the camera over the ride's exit.
void StopRidingRideIn1stPerson() {
    if ((gFirstPersonFlags & kFirstPersonRideBit) != 0) {
        int32_t out;   // uninitialised in the original; ToggleRideRideIn1stPerson sets it
        ToggleRideRideIn1stPerson(static_cast<int32_t>(gFirstPersonRideInstance), 0, 0, &out, 1, 0);
        if (out != 0) MoveCameraOverRideExit();
    }
}

// Unit v51013: GetThingAtPosnIfEntrance(int, int), MoveCameraOverRideExit (camera over a ride's exit) and the collision object
// helpers of the advisor world (FreeTrackForRide, BMP_StartColObject, BMP_StartColObjectForRide).

namespace {

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

float FloatFromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

}  // namespace

// 0x10032618 (308 bytes): the grid square's cell, when it is an entrance, names (halfword +0x10) the cell of the ride whose list of things
// (first id at +0x24, next ids at thing +0x0a, ended by gThingListEnd) is walked for the first thing of kind 3 whose ride record has 0 at +0x118.
CObject* GetThingAtPosnIfEntrance(int32_t gridX, int32_t gridZ) {
    const uint16_t index = static_cast<uint16_t>(gridX + (gridZ << 7) + 1);
    char* const map = reinterpret_cast<char*>(gGameWorldRef) + 0x2d8;
    CMapCell* const cell = reinterpret_cast<CMapCell*>(map + (static_cast<int32_t>(index) - 1) * 0x44);
    if (cell->IsEntrance() != 0) {
        const uint16_t link = At<uint16_t>(cell, 0x10);
        char* const target = map + (static_cast<int32_t>(link) - 1) * 0x44;
        if (target != nullptr) {
            uint16_t id = At<uint16_t>(target, 0x24);
            while (id != gThingListEnd) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZeroMessage);
                char* const thing = static_cast<char*>(gThingArray[id].mThing);
                if (At<uint8_t>(thing, 2) == 3) {
                    const RideInfo* info = reinterpret_cast<CObject*>(thing)->GetDBInfo();
                    // The CannotRide slot (the Mac's +0x118) by name: the host's record is laid out wider.
                    if (info->Value(kRideSlot_CannotRide).mInteger == 0) return reinterpret_cast<CObject*>(thing);
                }
                id = At<uint16_t>(thing, 0xa);
            }
        }
    }
    return nullptr;
}

// 0x1003277c (140 bytes): the entrance object at a place; the place in tenths is clamped into the height field's grid.
CObject* GetThingAtPosnIfEntrance(float x, float z) {
    int32_t gx = ppc_fctiwz(static_cast<double>(x / 10.0f));
    int32_t gz = ppc_fctiwz(static_cast<double>(z / 10.0f));
    if (gx < 0) gx = 0;
    if (gz < 0) gz = 0;
    if (gx >= gHeightfieldWidth) gx = gHeightfieldWidth - 1;
    if (gz >= gHeightfieldHeight) gz = gHeightfieldHeight - 1;
    return GetThingAtPosnIfEntrance(gx, gz);
}

// 0x10032838 (356 bytes): when the camera stands over an entrance's thing, moves it to the middle of the thing's cell (10 units a cell, plus
// 5) and turns it to the cell's direction (bit 0: pi, bit 2: pi / 2, bit 4: 0, bit 6: 3 pi / 2; the other bits leave the angle).
void MoveCameraOverRideExit() {
    CObject* const object = GetThingAtPosnIfEntrance(gCameraPosition[0], gCameraPosition[2]);
    if (object == nullptr) return;
    const uint16_t cellIndex = At<uint16_t>(object, 0x34);
    const int32_t column = static_cast<int32_t>((static_cast<int32_t>(cellIndex) - 1) & 0x7f);
    const int32_t row = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(cellIndex) - 1) >> 7);
    const float x = ppc_fmadds(10.0f, static_cast<float>(column), 5.0f);
    const float z = ppc_fmadds(10.0f, static_cast<float>(row), 5.0f);
    gCameraPosition[0] = x;
    gCameraPosition[2] = z;
    char* const map = reinterpret_cast<char*>(gGameWorldRef) + 0x2d8;
    const CMapCell* const cell = reinterpret_cast<const CMapCell*>(map + (static_cast<int32_t>(cellIndex) - 1) * 0x44);
    switch (cell->GetDirection() & 0x55) {
        case 0x10: gCameraF5BF4 = FloatFromBits(0x00000000); break;
        case 0x04: gCameraF5BF4 = FloatFromBits(0x3fc90fdb); break;
        case 0x01: gCameraF5BF4 = FloatFromBits(0x40490fdb); break;
        case 0x40: gCameraF5BF4 = FloatFromBits(0x4096cbe4); break;
        default: break;
    }
}

// The way out of the lobby (main app): the key sparkle when the player enters an island (ui_islandLobbyController.cpp
// 0x101e1460), the interface's lobby tear-down (ui_main.cpp 0x10186434, ui_ErrorDisplay.cpp 0x101ef7a4) and the first-person
// ride test (TPCamera.cpp 0x100329cc).

// 1 when the ride `instance` is the one the player rides in first person: the first-person ride flag (bit 4 of the flags) is up,
// a ride set is current and the remembered instance, which must not be 0, is the argument.
int32_t IsRideFirstPersonRide(uint32_t instance) {
    if ((gFirstPersonFlags & kFirstPersonRideBit) != 0 && gFirstPersonRideSet != nullptr && gFirstPersonRideInstance != 0 &&
        gFirstPersonRideInstance == instance)
        return 1;
    return 0;
}

// 0x10032a44 (248 bytes): 1 when the object (words: +0x20 its script, +0x24 its camera mesh, either may be 0) or the ride animSet
// has a first-person camera: a bump camera mesh, a flying tour's camera mesh, a coaster's first-person car, or a data node 0x1000.
int32_t IsRideRideable(void* object, uint32_t animSet) {
    if (object != nullptr) {
        const uint8_t* bytes = static_cast<const uint8_t*>(object);
        int32_t mesh, script;
        std::memcpy(&script, bytes + 0x20, 4);
        std::memcpy(&mesh, bytes + 0x24, 4);
        if (mesh != 0 && BMP_GetCameraMesh(mesh) != 0) return 1;
        int32_t tour = RSSE_GetTourBaseID(RSSE_GetScriptPointer(script));
        if (tour != 0 && FLY_GetCameraMesh(tour) != 0) return 1;
    }
    if (IsCoaster(animSet) != 0 && GetCoaster1stPersonCar(animSet) != 0) return 1;
    return FindDataNodeIndexByID(gRideAnimSets[animSet], 0x1000, 1) != 0xffffffffu ? 1 : 0;
}

// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).

// 0x10032300: the array destructor of gAdvisorStore.
void __arraydtor_3112() {
    __destroy_arr(gAdvisorStore, gAdvisorStoreDtorVector, 0xbc, 4);
}
unsigned char gAdvisorStoreDtorVector[16];   // stand-in for 0x10266010

// ---- the camera
// 0x10032b64: the ridden ride's instance while the first person view rides one, else 0.
uint32_t GetFirstPersonRideID() {
    if ((gFirstPersonFlags & 4) != 0 && gFirstPersonRideSet != nullptr) return gFirstPersonRideInstance;
    return 0;
}

// 0x10032bc4
int32_t ResetCameraPosition() {
    gCameraModeA |= 0x100;
    return 1;
}

// TPCamera.cpp (main app of SimTheme Park 2.04b3): the camera's small callees, unit w140: GetNextGridsquare (0x10033e14, 1,948 bytes),
// MatrixDifferenceTiny (0x10034784), ApplyHeadsOnRidesBumpage (0x100336f4), RoundRotationToNearest90Degrees (0x100346a0), SetCameraPOI
// (0x10033650), RideRideIfRequired (0x100345f8) and AllowFullCameraZoomout (0x10032c08). The two height lookups are in
// src/TPRide_height.cpp.
//
// Every float is a single in the listing's order. The fused multiply-adds of the listing are ppc_fmadds (GetNextGridsquare 10,
// ApplyHeadsOnRidesBumpage 9) and ppc_fnmsubs (GetNextGridsquare 4: `x -= x * t`). GetNextGridsquare calls __cvt_fp2unsigned 18 times
// (cw_cvt_fp2unsigned: a negative number gives 0, a huge one 0xFFFFFFFF), modf 4 times (only the fraction is used) and MAP_wall_in_way
// up to twice (the translation: once per call). The callees MAP_wall_in_way, GetThingAtPosnIfEntrance, CObject::GoFirstPerson and
// CRideDatabase::GetRideInfo are not translated.

namespace {
constexpr float Bits(uint32_t b) { return __builtin_bit_cast(float, b); }
constexpr float kHalfPi = Bits(0x3fc90fdbu);       // 0x102646d0
constexpr float kQuarterPi = Bits(0x3f490fdbu);    // 0x10264680, and its negative 0xbf490fdb
constexpr double kHalfPiD = __builtin_bit_cast(double, 0x3ff921fb60000000ull);   // 0x10264688: the double of the float pi / 2
constexpr float kCell = 10.0f;                      // 0x102646dc: the grid square's size
constexpr float kNoStep = 2.0f;                     // 0x10264698: "this axis never reaches a square edge"
}  // namespace

// Initialised data of the camera (0x1029cc5c..0x1029cc98; names invented).
float gCameraMatrixEpsilon = Bits(0x3727c5acu);   // 0x1029cc5c  1e-5
float gCameraPOIValue = Bits(0x3f800000u);        // 0x1029cc88  1.0
float gGridEdgeOffset = Bits(0x411ffbe7u);        // 0x1029cc90  9.999
float gGridStepScale = Bits(0x3f8147aeu);         // 0x1029cc94  1.01
float gGridNudge = Bits(0x3a83126fu);             // 0x1029cc98  0.001

int32_t AllowFullCameraZoomout() {
    if ((gCameraModeA & 0x200) != 0) gCameraModeA = gCameraModeA & ~0x200u;
    else gCameraModeA = gCameraModeA | 0x200u;
    return 1;
}

// TPCamera.cpp (main app of SimTheme Park 2.04b3), unit w141: the camera modes and the first person walk and ride.
//   __sinit_TPCamera_cpp 0x10036518 (36 bytes)  copies the start position over the camera position (a file-scope dynamic initialiser)
//   SetCameraMode 0x1003384c (792), ToggleFirstPersonWalkAround 0x10033b90 (508), Setup1stPersonRideMeshes 0x10032c64 (428),
//   SetupRideRideMatrixAndSuch 0x10032e40 (384), ToggleRideRideIn1stPerson 0x10032ff4 (408), SaveCamera 0x10035f4c (680),
//   LoadCamera 0x10036218 (732).
//
// gFirstPersonFlags: 2 walking, 4 riding a ride, 0x10 (with the others, see TPCamera.h), 0x200 procedural texturing was on, 0x400 a ride
// entered from walking (leaving it returns to walking), 0x800 the look angles were reset, 0x1000 cleared at the end of SetCameraMode.
// SaveCamera and LoadCamera swap the globals in place, as the original does (the file's words are little-endian).

tag_sRideAnimSet* gCameraHeadsRideSet;
uint32_t* gFirstPersonRideFlagWord;
void* gFirstPersonRideNode;
float gCameraSavedPosition[3];
float gCameraSavedYaw;
int8_t gRideRideOnceGuard;
int8_t gRideRideOnceValue;

namespace {
constexpr double kHalfPiD_2 = 1.5707963705062866;    // the float pi / 2 as a double (0x10264688)
constexpr float kHalfPi_2 = 1.57079637f;             // 0x3fc90fdb
constexpr float kQuarterPi_2 = 0.785398185f;         // 0x3f490fdb

// The Mac swaps the word in place (4 bytes reversed): the file is little-endian, so on the little-endian host the word already is
// the file's (the same convention as serialise::SwapWord in include/Serialise.h): the bytes are taken low to high.
uint32_t Swap(uint32_t v) {
    uint8_t bytes[4];
    for (int i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(v >> (8 * i));
    std::memcpy(&v, bytes, 4);
    return v;
}

void SwapWords(void* words, int32_t count) {
    uint32_t* w = static_cast<uint32_t*>(words);
    for (int32_t i = 0; i < count; ++i) w[i] = Swap(w[i]);
}

// Sets or clears the 0x8000 bit of the node of the embedded entry.
void SetNodeBit(tag_sM3D2Header* header, uint32_t entry, bool on) {
    tag_sMiniNode* const node = EmbeddedNode(header, entry);
    if (on) node->mFlags |= 0x8000;
    else node->mFlags &= ~0x8000u;
}
}  // namespace

// 0x10036518: the dynamic initialiser of TPCamera.cpp.
static void InitCameraPosition() {
    std::memcpy(gCameraPosition, gCameraStartPosition, sizeof gCameraPosition);
}
static const int kCameraPositionInit = (InitCameraPosition(), 0);

void Setup1stPersonRideMeshes(int32_t ride) {
    const uint32_t last = gRideCountB + gRideCountA;
    for (uint32_t s = 1; s <= last; ++s) {
        tag_sRideAnimSet* const set = gRideAnimSets[s];
        if (set == nullptr) continue;
        tag_sM3D2Header* const header = set->mHeads->mHeader;
        const sEmbeddedData* record = header->mEmbedded.get();
        for (uint32_t i = 0; i < header->mEmbeddedCount; ++i, ++record) {
            if ((gFirstPersonFlags & 0x16) == 0) SetNodeBit(header, i, false);
            else if ((record->mFlags & 0x4000000) != 0) SetNodeBit(header, i, true);
            else if ((record->mFlags & 0x8000000) != 0) SetNodeBit(header, i, false);
        }
    }
    if (ride != 0 && (gFirstPersonFlags & 4) != 0 && gRideAnimSets[ride] != nullptr) {
        tag_sM3D2Header* const header = gRideAnimSets[ride]->mHeads->mHeader;
        const sEmbeddedData* record = header->mEmbedded.get();
        for (uint32_t i = 0; i < header->mEmbeddedCount; ++i, ++record) {
            if ((record->mFlags & 0x10000000) != 0) SetNodeBit(header, i, true);
            else if ((record->mFlags & 0x20000000) != 0) SetNodeBit(header, i, false);
        }
    }
}

int32_t SetupRideRideMatrixAndSuch(int32_t ride, int32_t node) {
    if (ride == 0) return 0;
    if ((gFirstPersonFlags & 4) != 0) {
        if (gFirstPersonRideFlagWord != nullptr) *gFirstPersonRideFlagWord &= ~4u;
        gFirstPersonRideInstance = 0;
        gFirstPersonRideNode = nullptr;
        gFirstPersonRideFlagWord = nullptr;
        gFirstPersonRideMatrix = nullptr;
        return 1;
    }
    tag_sRideAnimSet* const set = gRideAnimSets[ride];
    if (set == nullptr) return 0;
    uint32_t index = FindDataNodeIndexByID(set, 0x1000, static_cast<uint32_t>(node));
    if (node != 1 && index == 0xffffffffu) index = FindDataNodeIndexByID(set, 0x1000, 1);
    if (index == 0xffffffffu) return 0;
    tag_sMesh* const mesh = set->mHeads;
    gFirstPersonRideInstance = static_cast<uint32_t>(ride);
    gFirstPersonRideNode = EmbeddedNode(mesh->mHeader, index);
    sEmbeddedRecord* const record = &static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords[index];
    gFirstPersonRideFlagWord = &record->mFlags;
    gFirstPersonRideMatrix = reinterpret_cast<sMatrix*>(record->mBlock);
    gFirstPersonRideSet = set;
    record->mFlags |= 4;
    return 1;
}

// TPCamera.cpp (main app): Will_HandleCameraMessages 0x100331c4 (732 bytes), the park camera's share of the game window's
// messages (Engine_GameWinMsg and Engine_GameFirstPersonMsg pass every message on). Translated by the maintainer 2026-10-14
// from the listing; module test_CameraMessages.
//
// The pointer's position in a message is in the UI's 2048 x 1536 space (x in the high half, y in the low half); the camera
// keeps it in screen pixels in gPointerScreenXY, which HandleCamera reads for the edge scroll and the right-button drag. Bit
// 0x100 of gFirstPersonFlags stops the edge scroll: set while the pointer is outside the game window.

float gPointerScreenXY[2];          // 0x102b5a94
int8_t gCameraYawDragScaleOnce;     // 0x102b5a18
float gCameraYawDragScale;          // 0x102b5a1c
int8_t gCameraMessagesF5A10Once;    // 0x102b5a10
float gCameraMessagesF5A14;         // 0x102b5a14

namespace {
constexpr int32_t kMsgFrame = 0x1e;
constexpr int32_t kMsgModeOff = 0x20;
constexpr int32_t kMsgReset = 0x22;
constexpr int32_t kMsgPointerEnter = 0x10001;
constexpr int32_t kMsgPointerLeave = 0x10002;
constexpr int32_t kMsgPointerMove = 0x10003;
constexpr int32_t kMsgButtonUp = 0x10004;
constexpr int32_t kMsgButtonDown = 0x10005;
constexpr int32_t kMsgWheel = 0x1000d;

// The Mac's int-to-float: the magic double minus 2^52 + 2^31, rounded to single by fsubs.
float FromInt(int32_t v) { return static_cast<float>(static_cast<double>(v)); }

// The camera mode bit of a mouse button (0 left, 1 right, 2 middle); 0 for any other value.
uint32_t ButtonBit(int32_t button) {
    if (button == 0) return 1;
    if (button == 1) return 4;
    if (button == 2) return 2;
    return 0;
}
}  // namespace

void Will_HandleCameraMessages(int32_t message, int32_t a, int32_t b) {
    (void)b;
    // Both scaled positions are worked out before the switch (32-bit products; the divisions truncate toward zero).
    const int32_t width = gUIRealScreenWidth;
    const int32_t height = gUIRealScreenHeight;
    const int32_t x = static_cast<int32_t>((static_cast<uint32_t>(a) >> 16) * static_cast<uint32_t>(width)) / 2048;
    const int32_t y = static_cast<int32_t>((static_cast<uint32_t>(a) & 0xffffu) * static_cast<uint32_t>(height)) / 1536;

    // Function statics: the Mac gives them their values on the first call.
    if (gCameraYawDragScaleOnce == 0) {
        gCameraYawDragScale = 1.0f / 2048.0f;   // 0x3a000000
        gCameraYawDragScaleOnce = 1;
    }
    if (gCameraMessagesF5A10Once == 0) {
        gCameraMessagesF5A14 = 0.0f;
        gCameraMessagesF5A10Once = 1;
    }

    switch (message) {
    case kMsgPointerEnter:
        gFirstPersonFlags &= ~0x100u;
        [[fallthrough]];
    case kMsgPointerMove:
        gPointerScreenXY[0] = FromInt(x);
        gPointerScreenXY[1] = FromInt(y);
        break;
    case kMsgPointerLeave:
        gFirstPersonFlags |= 0x100u;
        break;
    case kMsgButtonUp:
        gCameraModeA &= ~ButtonBit(a);
        break;
    case kMsgButtonDown:
        gCameraModeA |= ButtonBit(a);
        break;
    case kMsgWheel:
        // Wheel back (a negative delta) zooms out by 10, forward zooms in.
        if (a < 0) gCameraF68 = gCameraF68 + 10.0f;
        else gCameraF68 = gCameraF68 - 10.0f;
        break;
    case kMsgFrame:
        // With the middle button held the key yaw turns by the pointer's place across the screen: x / width - 0.5 radians
        // a frame (0 in the middle), worked as (2048 x / width - 1024) / 2048 with one fused multiply-add.
        if ((gCameraModeA & 2) != 0) {
            const float across = (2048.0f * gPointerScreenXY[0]) / FromInt(width) - 1024.0f;
            gCameraF5BF4 = ppc_fmadds(gCameraYawDragScale, across, gCameraF5BF4);
        }
        break;
    case kMsgModeOff:
        gCameraModeA &= ~0x78u;
        break;
    case kMsgReset:
        gCameraModeA &= ~0x7fu;
        gFirstPersonFlags |= 0x100u;
        break;
    default:
        break;
    }
}

// The home position is copied as raw words (lwz/stw) over the position, and from there into the row 3 of the camera matrix as
// floats (lfs/stfs). The field of view is 57.2957763671875 (degrees per radian, as a float) times gCameraFov, a single
// precision multiply; the projection gets it with the near plane 0.1 and the far plane 1000.0.
void InitCamera() {
    gCameraF68 = 110.0f;
    gCameraF5BF4 = 0.0f;
    gFirstPersonFlags = 0;
    std::memcpy(gCameraPosition, gCameraStartPosition, sizeof gCameraPosition);
    SetMatrixIdentity(&gCameraMatrix);
    SetMatrixIdentity(&gCameraMatrixB);
    SetMatrixIdentity(&gCameraMatrixA);
    gCameraMatrix.m[3][0] = gCameraPosition[0];
    gCameraMatrix.m[3][1] = gCameraPosition[1];
    gCameraMatrix.m[3][2] = gCameraPosition[2];
    gei_ViewMatrix->Load(gCameraMatrix);
    const float fov = 57.2957763671875f * gCameraFov;
    gei_ProjectionMatrix.Projection(0.1f, 1000.0f, fov);
    gei_Gamut->Init(fov);
    const uint32_t mode = (gCameraModeA & 0x200) | 0x80;
    gCameraModeA = mode;
    gCameraModeB = mode;
    gCameraClockStamp = static_cast<uint32_t>(gClock.ReadFixed());
    std::memset(gCameraF5BB4, 0, sizeof gCameraF5BB4);
    gCameraF5AAC = 1;
    gFirstPersonFlags = 0;
}

// The camera looks at a place: the velocity stops; each non-null input is copied. The listing tests the first pointer a second time
// where it reads the second one (so a null z with a non-null x dereferences null, as on the Mac).
void SetCameraPOI(float* x, float* z, float* zoom, float* yaw, float* value) {
    gCameraF5BB4[0] = 0.0f;
    gCameraF5BB4[1] = 0.0f;
    gCameraF5BB4[2] = 0.0f;
    if (x != nullptr) gCameraPosition[0] = *x;
    if (x != nullptr) gCameraPosition[2] = *z;
    if (zoom != nullptr) gCameraF68 = *zoom;
    if (yaw != nullptr) gCameraF5BF4 = *yaw;
    if (value != nullptr) gCameraPOIValue = *value;
}

// out = the ride matrix * (1 - w) + the previous one * w with w = (1000 - dt clamped to 1..1000) / 1000 / 2, for the nine rotation
// words; the position (row 3, x y z) is the ride's own.
void ApplyHeadsOnRidesBumpage(sMatrix* ride, sMatrix* previous, float dt, sMatrix* out) {
    float t = dt;
    if (!(t <= 1000.0f)) t = 1000.0f;
    if (!(t >= 1.0f)) t = 1.0f;
    const float fraction = (1000.0f - t) / 1000.0f;
    const float w = 0.5f * fraction;
    const float rest = 1.0f - w;
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            const float scaled = previous->m[row][column] * w;
            out->m[row][column] = ppc_fmadds(ride->m[row][column], rest, scaled);
        }
    }
    out->m[3][0] = ride->m[3][0];
    out->m[3][1] = ride->m[3][1];
    out->m[3][2] = ride->m[3][2];
}

int32_t SetCameraMode(int32_t mode, int32_t toggle, int32_t ride, int32_t force, int32_t* out, int32_t resetTurn) {
    bool enter = false;
    bool leave = false;
    int32_t result = 0;
    gFirstPersonFlags &= ~0x800u;
    const uint32_t previous = gFirstPersonFlags & 0x16;
    if (out != nullptr) *out = 0;
    if (mode == 2) {
        if ((gFirstPersonFlags & 0x84) == 0 || force != 0) {
            gFirstPersonFlags &= 0xffffff43u;
            if (toggle != 0) {
                if ((gFirstPersonFlags & 2) != 0) gFirstPersonFlags &= ~2u;
                else gFirstPersonFlags |= 2;
            } else {
                gFirstPersonFlags |= 2;
            }
            if ((gFirstPersonFlags & 2) != 0) {
                enter = previous == 0;
                result = 1;
            } else if (previous != 0) {
                leave = true;
            }
        }
    } else if (mode == 4) {
        if ((gFirstPersonFlags & 0x92) == 0 || force != 0) {
            gFirstPersonFlags &= 0xffffff45u;
            if (toggle != 0) {
                if ((gFirstPersonFlags & 4) != 0) gFirstPersonFlags &= ~4u;
                else gFirstPersonFlags |= 4;
            } else {
                gFirstPersonFlags |= 4;
            }
            if ((gFirstPersonFlags & 4) != 0) {
                if (previous == 0) {
                    enter = true;
                    if (resetTurn != 0) {
                        gFirstPersonFlags |= 0x800;
                        gFirstPersonTurn = 0.0f;
                        gFirstPersonPitch = 0.0f;
                    }
                } else {
                    gFirstPersonFlags |= 0x400;
                }
                result = 1;
            } else if (previous != 0) {
                if ((gFirstPersonFlags & 0x400) != 0) {
                    gFirstPersonFlags &= ~0x400u;
                    gFirstPersonFlags |= 2;
                    if (out != nullptr) *out = 1;
                } else {
                    leave = true;
                }
            }
        }
    } else if (mode == 0x40) {
        gFirstPersonFlags &= 0xffffff41u;
        leave = previous != 0;
        result = 1;
    }
    if (enter) {
        DisableRideHighlight(1);
        gUIInterface.ShowFirstPersonWindow(1);
        if (IsProceduralTexturingActive() != 0) {
            gFirstPersonFlags |= 0x200;
            DisableProceduralTexturing();
        }
        tag_sRideAnimSet* const set = gCameraHeadsRideSet;
        if (set != nullptr) {
            if ((set->mFlags & 0x4000) != 0) EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
            else EnableMeshDraw(set->mHeads);
        }
    } else if (leave) {
        DisableRideHighlight(0);
        gUIInterface.ShowFirstPersonWindow(0);
        if ((gFirstPersonFlags & 0x200) != 0) {
            gFirstPersonFlags &= ~0x200u;
            EnableProceduralTexturing();
        }
    }
    Setup1stPersonRideMeshes(ride);
    gFirstPersonFlags &= ~0x1000u;
    return result;
}

void ToggleRideRideIn1stPerson(int32_t ride, int32_t a, int32_t b, int32_t* out, int32_t node, int32_t c) {
    if (gRideRideOnceGuard == 0) {
        gRideRideOnceValue = 0;
        gRideRideOnceGuard = 1;
    }
    if (out != nullptr) *out = 0;
    if (gFirstPersonRideFlagWord != nullptr) *gFirstPersonRideFlagWord &= ~4u;
    gFirstPersonRideNode = nullptr;
    gFirstPersonRideMatrix = nullptr;
    gFirstPersonRideFlagWord = nullptr;
    if ((gFirstPersonFlags & 4) != 0 && gFirstPersonRideSet != nullptr) gFirstPersonRideSet->mFlags &= ~0x2000u;
    gFirstPersonRideSet = nullptr;
    if (IsCoaster(static_cast<uint32_t>(ride)) != 0) ride = static_cast<int32_t>(GetCoaster1stPersonCar(static_cast<uint32_t>(ride)));
    if (SetupRideRideMatrixAndSuch(ride, node) != 0) {
        if (SetCameraMode(4, 1, ride, b, out, c) != 0) {
            tag_sMesh* const mesh = gRideAnimSets[ride]->mHeads;
            if (mesh->mF28 != nullptr) TPProcessEmbeddedData(mesh);
            gRideAnimSets[ride]->mFlags |= 0x2000;
        }
    }
    if (a == 1 || (gFirstPersonFlags & 2) != 0) ESP_SwitchToFirstPersonSprites();
    else ESP_SwitchToThirdPersonSprites();
}

void ToggleFirstPersonWalkAround() {
    bool allowed = (gFirstPersonFlags & 0x16) != 0;
    if (!allowed) {
        int32_t x = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.x / 10.0f));
        int32_t z = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.z / 10.0f));
        if (x < 0) x = 0;
        if (z < 0) z = 0;
        if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
        if (z >= gHeightfieldHeight) z = gHeightfieldHeight - 1;
        const uint16_t id = static_cast<uint16_t>(x + (z << 7) + 1);
        unsigned char* const base = reinterpret_cast<unsigned char*>(gGameWorldRef) + 0x2d8;
        const CMapCell* const cell = reinterpret_cast<const CMapCell*>(base + (static_cast<int32_t>(id) - 1) * 0x44);
        allowed = cell->IsWalkable() != 0 || cell->IsPath() != 0 || cell->IsQueue() != 0 || cell->IsGrass() != 0;
    }
    if (!allowed) return;
    SetCameraMode(2, 1, 0, 0, nullptr, 0);
    if ((gFirstPersonFlags & 0x16) != 0) {
        std::memcpy(gCameraSavedPosition, gCameraPosition, sizeof gCameraSavedPosition);
        gCameraSavedYaw = gCameraF5BF4;
        gCameraPosition[0] = gPointerWorldPosn.x;
        gCameraPosition[1] = 0.0f;
        gCameraPosition[2] = gPointerWorldPosn.z;
        ESP_SwitchToFirstPersonSprites();
    } else {
        std::memcpy(gCameraPosition, gCameraSavedPosition, sizeof gCameraPosition);
        gCameraF5BF4 = gCameraSavedYaw;
        ESP_SwitchToThirdPersonSprites();
    }
}

// Only the external-camera path is needed by the lobby. HandleCamera's park branches
// remain visibly pending in the launcher; they are not counted as translated code.

uint32_t gFirstPersonFlags;
sMatrix gExternalCameraMatrix;
sMatrix gInverseCameraMatrix;
sMatrix* gCurrentCameraMatrix;

void SetExternalCamera(int32_t enabled) {
    if (enabled) gFirstPersonFlags |= 0x80;
    else gFirstPersonFlags &= ~0x80u;
}

bool HandleExternalCamera() {
    if ((gFirstPersonFlags & 0x80) == 0) return false;
    gCurrentCameraMatrix = &gExternalCameraMatrix;
    gei_ViewMatrix->Load(gExternalCameraMatrix);
    InvertMatrix(&gInverseCameraMatrix, gCurrentCameraMatrix);
    return true;
}

// Nonzero when the entrance of a ride stands at the point: the camera then rides it in first person. The ride database's answer is
// not used (the Mac passes the type id in the high half of r4).
int32_t RideRideIfRequired(s3DPoint* point) {
    CObject* const object = GetThingAtPosnIfEntrance(point->x, point->z);
    if (object == nullptr) return 0;
    gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType});
    object->GoFirstPerson(1);
    return 1;
}

// Rounds the key yaw (gCameraF5BF4) to a multiple of a quarter turn: the remainder of fmod(yaw, pi / 2) is taken off, or topped up to
// the next multiple when it is past pi / 4 (and mirrored for a negative one).
void RoundRotationToNearest90Degrees() {
    const float rest = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kHalfPiD));
    if (rest >= 0.0f) {
        if (rest > kQuarterPi) gCameraF5BF4 = gCameraF5BF4 + (kHalfPi - rest);
        else gCameraF5BF4 = gCameraF5BF4 - rest;
    } else if (rest >= -kQuarterPi) {
        gCameraF5BF4 = gCameraF5BF4 - rest;
    } else {
        gCameraF5BF4 = gCameraF5BF4 - (kHalfPi + rest);
    }
}

// 1 when the two matrices are almost the same: the sum of the absolute differences of the 12 words of rows 0 to 3 (x y z each; the w
// column is not read), each difference taken in double and rounded to a single, is below 1e-5. A NaN sum answers 1 as well.
int32_t MatrixDifferenceTiny(sMatrix* a, sMatrix* b) {
    float sum = 0.0f;
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            const double difference = static_cast<double>(a->m[row][column]) - static_cast<double>(b->m[row][column]);
            const float magnitude = static_cast<float>(std::fabs(difference));
            sum = sum + magnitude;
        }
    }
    return !(sum >= gCameraMatrixEpsilon) ? 1 : 0;
}

namespace {
// A point pushed to the edge of square `index` on the side it walks to: 10 * index when it walks down, 10 * index + 9.999 otherwise.
float EdgeOf(float step, uint32_t index) {
    const float first = static_cast<float>(index);
    if (step >= 0.0f) return ppc_fmadds(kCell, first, gGridEdgeOffset);
    return kCell * first;
}

// The time (in steps) an axis takes to the next square edge: the fraction of the square left (1 - fraction when walking up) over the step per
// square, as a positive number; an axis with no step takes 2.0 (more than any square).
float TimeToEdge(float position, float step) {
    if (step == 0.0f) return kNoStep;
    double whole = 0.0;
    const float fraction = static_cast<float>(std::modf(static_cast<double>(position / kCell), &whole));
    const float perSquare = step / kCell;
    if (step >= 0.0f) {
        const float left = 1.0f - fraction;
        return std::fabs(left / perSquare);
    }
    return std::fabs(fraction / perSquare);
}
}  // namespace

// Walks the point `from` by the step `step` to the next grid square edge of 10 units (the squares are 10 by 10). `out` is the point
// reached (y 0); `step` is left with what remains to walk (the whole step when nothing is in the way, less when an edge was reached
// first, 0 on an axis stopped by a wall). The axis that reaches an edge first (tx and tz are the times to the edges, 2.0 for a zero
// step) is moved to the edge; if a wall (MAP_wall_in_way, mode 2) stands on that side the point stops at the edge and that axis' step
// is cleared, else it is pushed 0.001 over the edge. The other axis moves the same fraction and is snapped to its square when that
// leaves it. A step that reaches no edge (both times 1.0 or more) is applied whole. Answers 1 while something remains to walk.
// A tie (tx == tz) with both steps nonzero makes the x step 1.01 times smaller and its time 1.01 times larger, so the axes differ.
int32_t GetNextGridsquare(s3DPoint* from, s3DPoint* step, s3DPoint* out) {
    uint32_t ix = cw_cvt_fp2unsigned(static_cast<double>(from->x / kCell));
    uint32_t iz = cw_cvt_fp2unsigned(static_cast<double>(from->z / kCell));
    out->y = 0.0f;
    float tx = TimeToEdge(from->x, step->x);
    float tz = TimeToEdge(from->z, step->z);
    if (tx == tz) {
        const float stepX = step->x;
        if (0.0f != stepX) {
            const float stepZ = step->z;
            if (0.0f != stepZ) {
                if (stepX == 0.0f) {
                    step->z = stepZ / gGridStepScale;
                    tz = tz * gGridStepScale;
                } else {
                    step->x = stepX / gGridStepScale;
                    tx = tx * gGridStepScale;
                }
            }
        }
    }
    uint32_t cx = 0;
    uint32_t cz = 0;
    if (tx >= tz) {
        // z reaches its edge first
        if (!(tz >= 1.0f)) {
            const uint8_t direction = step->z >= 0.0f ? 2 : 0;
            if (MAP_wall_in_way(static_cast<uint8_t>(ix), static_cast<uint8_t>(iz), direction, static_cast<NavMode>(2)) != 0) {
                out->z = EdgeOf(step->z, iz);
                step->z = 0.0f;
                cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
            } else {
                out->z = ppc_fmadds(step->z, tz, from->z);
                step->z = ppc_fnmsubs(step->z, tz, step->z);
                cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
                if (iz == cz) {
                    if (step->z >= 0.0f) out->z = out->z + gGridNudge;
                    else out->z = out->z - gGridNudge;
                    cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
                }
            }
            out->x = ppc_fmadds(step->x, tz, from->x);
            step->x = ppc_fnmsubs(step->x, tz, step->x);
            cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            if (ix != cx) {
                out->x = EdgeOf(step->x, ix);
                cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            }
            return (step->x != 0.0f || step->z != 0.0f) ? 1 : 0;
        }
    } else if (!(tx >= 1.0f)) {
        // x reaches its edge first
        const uint8_t direction = step->x >= 0.0f ? 1 : 3;
        if (MAP_wall_in_way(static_cast<uint8_t>(ix), static_cast<uint8_t>(iz), direction, static_cast<NavMode>(2)) != 0) {
            out->x = EdgeOf(step->x, ix);
            step->x = 0.0f;
            cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
        } else {
            out->x = ppc_fmadds(step->x, tx, from->x);
            step->x = ppc_fnmsubs(step->x, tx, step->x);
            cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            if (ix == cx) {
                if (step->x >= 0.0f) out->x = out->x + gGridNudge;
                else out->x = out->x - gGridNudge;
                cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            }
        }
        out->z = ppc_fmadds(step->z, tx, from->z);
        step->z = ppc_fnmsubs(step->z, tx, step->z);
        cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
        if (iz != cz) {
            out->z = EdgeOf(step->z, iz);
            cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
        }
        return (step->x != 0.0f || step->z != 0.0f) ? 1 : 0;
    }
    // no edge is reached: the whole step is taken, and a coordinate that left its square is put on the edge it crossed
    out->x = from->x + step->x;
    out->z = from->z + step->z;
    cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
    cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
    if (ix != cx) out->x = EdgeOf(step->x, ix);
    if (iz != cz) out->z = EdgeOf(step->z, iz);
    cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
    cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
    (void)cx;
    (void)cz;
    return 0;
}

// TPCamera.cpp (main app of SimTheme Park 2.04b3): HandleCamera (0x1003491c, 5,644 bytes), unit w135: the park camera's frame.
//
// Every float is a single and every operation is done in single precision in the listing's order; a fused multiply-add of the listing is
// one of ppc_fmadds / ppc_fmsubs / ppc_fnmsubs below (27 in the listing: 23 fmadds, 1 fmsubs, 3 fnmsubs; the source has the same 27 calls).
// The constants are written as their bit patterns from the listing's constant pool. The
// C library's sin, cos, sqrt and fmod are the host's. The callees GetState, DetermineHFieldHeight, GetNextGridsquare, RideRideIfRequired,
// MatrixDifferenceTiny and ApplyHeadsOnRidesBumpage are not translated (declared in TPCamera.h and keyboard.h).

// Globals (BSS 0x102b59f8..0x102b5bf0 and initialised data 0x1029cc60..0x1029ccb8; names invented).
int8_t gCameraWalkOnce;
float gCameraF59FC;
int8_t gCameraWideZoomMaxOnce;
float gCameraWideZoomMax;
int8_t gCameraWideZoomOnce;
float gCameraWideZoomMin;
float gCameraGroundHeight;
sMatrix* gFirstPersonRideMatrix;
float gCameraTilt[2];
float gCameraPitch;
float gCameraPointerAnchor[2];
float gFirstPersonTurn;
float gFirstPersonPitch;
float gCameraYaw;
float gCameraYawSnap = 5.0000002374872565e-4f;       // 0x3a03126f
float gCameraStopSpeed = 9.99999974737875e-5f;       // 0x38d1b717
float gCameraYawRateIso = 200.0f;
float gCameraYawRate = 140.0f;
float gCameraPitchMin = 0.0f;
float gCameraPitchMax = __builtin_bit_cast(float, 0x3fbc430eu);
float gCameraTiltMinX = 0.0f;
float gCameraTiltMaxX = 0.0f;
float gCameraTiltMinZ = 0.0f;
float gCameraTiltMaxZ = 0.0f;

namespace {

constexpr float kK0012 = Bits(0x3a9d4952u);        // 0.0012
constexpr float kK0002 = Bits(0x3b03126fu);        // 0.002
constexpr float kK005 = Bits(0x3d4ccccdu);         // 0.05
constexpr float kK001 = Bits(0x3c23d70au);         // 0.01
constexpr float kK002 = Bits(0x3ca3d70au);         // 0.02
constexpr float kK0001 = Bits(0x3a83126fu);        // 0.001
constexpr float kOneAndAHalfPitch = Bits(0x3f91361eu);   // 1.134464
constexpr double kTwoPiD = __builtin_bit_cast(double, 0x401921fb60000000ull);   // 0x10264600
inline s3DPoint* AsPoint(float* p) { return reinterpret_cast<s3DPoint*>(p); }

inline bool Key(int16_t id) { return gKeyTableCamera->GetState(id) != 0; }

// The exact conversion of the listing: a 32-bit unsigned value through the 2^52 trick, rounded to single by the fsubs.
inline float FromU32(uint32_t v) { return static_cast<float>(v); }

}  // namespace

void HandleCamera() {
    const uint32_t isometric = static_cast<uint8_t>(gPlayerSettings.mCurrent.mIsometricOn);
    if (HandleExternalCamera()) return;

    const uint32_t now = static_cast<uint32_t>(gClock.ReadFixed());
    float dt;
    if (gCameraF5AAC != 0) {
        if (isometric != 0) {
            // The first frame in the isometric view: the key yaw is made a multiple of a quarter turn.
            const float y = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kHalfPiD));
            if (!(y < 0.0f)) {
                if (y > kQuarterPi) gCameraF5BF4 = gCameraF5BF4 + (kHalfPi - y);
                else gCameraF5BF4 = gCameraF5BF4 - y;
            } else if (!(y < -kQuarterPi)) {
                gCameraF5BF4 = gCameraF5BF4 - y;
            } else {
                gCameraF5BF4 = gCameraF5BF4 - (kHalfPi + y);
            }
        }
        dt = 0.0f;
        gCameraF5AAC = 0;
    } else {
        dt = FromU32(now + 1u) - FromU32(gCameraClockStamp);
    }
    if (dt > 200.0f) dt = 100.0f;
    if (dt <= 0.0f) dt = 1.0f;

    // A button that went down since the last frame remembers where the pointer was.
    if (gCameraModeB != gCameraModeA && (gCameraModeB & 4) == 0 && (gCameraModeA & 4) != 0) {
        std::memcpy(gCameraPointerAnchor, gPointerScreenXY, sizeof gCameraPointerAnchor);
    }
    gCameraModeA &= ~0x78u;
    if (Key(6) || Key(0x12)) gCameraModeA |= 8;
    if (Key(7) || Key(0x13)) gCameraModeA |= 0x10;
    if (Key(0) || Key(0x15)) gCameraModeA |= 0x20;
    if (Key(1) || Key(0x16)) gCameraModeA |= 0x40;

    float f30 = 0.0f;
    float f31 = 0.0f;   // a first-person view's key pitch
    if ((gFirstPersonFlags & 0x16) != 0) {
        f31 = 0.0f;
        if ((gFirstPersonFlags & 0x3c) == 0) {
            const uint32_t mode = gCameraModeA;
            if (mode & 8) gCameraF5BF4 = ppc_fnmsubs(kK0012, dt, gCameraF5BF4);
            if (mode & 0x10) gCameraF5BF4 = ppc_fmadds(kK0012, dt, gCameraF5BF4);
            if (mode & 0x40) f31 = f31 + Bits(0xbdcccccdu);
            if (mode & 0x20) f31 = f31 + Bits(0x3dcccccdu);
        }
    } else {
        if (isometric != 0) {
            const uint32_t mode = gCameraModeA;
            if ((mode & 8) && !(gCameraModeB & 8)) gCameraF5BF4 = gCameraF5BF4 + kHalfPi;
            if ((mode & 0x10) && !(gCameraModeB & 0x10)) gCameraF5BF4 = gCameraF5BF4 - kHalfPi;
        } else {
            const uint32_t mode = gCameraModeA;
            if (mode & 8) gCameraF5BF4 = ppc_fmadds(kK0002, dt, gCameraF5BF4);
            if (mode & 0x10) gCameraF5BF4 = ppc_fnmsubs(kK0002, dt, gCameraF5BF4);
        }
        const uint32_t mode = gCameraModeA;
        if (mode & 0x40) gCameraF68 = ppc_fmadds(kK005, dt, gCameraF68);
        if (mode & 0x20) gCameraF68 = ppc_fnmsubs(kK005, dt, gCameraF68);
    }

    if (gCameraModeA & 0x100) {
        // Reset: the home zoom, no key yaw, the home position.
        gCameraF68 = Bits(0x42dc0000u);
        gCameraF5BF4 = 0.0f;
        std::memcpy(gCameraPosition, gCameraStartPosition, sizeof gCameraPosition);
        gCameraModeA &= ~0x100u;
        gFirstPersonFlags &= ~0x1000u;
    }
    gCameraModeB = gCameraModeA;

    // The pointer's offset from the middle of the game window, in half windows (up is positive).
    float f28 = (gPointerScreenXY[0] - gGameWindowCentreX) / gGameWindowHalfWidth;
    float f27 = (-(gPointerScreenXY[1] - gGameWindowCentreY)) / gGameWindowHalfHeight;
    int32_t edge;       // the pointer is at a window edge with edge scrolling on
    int32_t firstOk;    // a first-person view that is not held back
    int32_t keyScroll = 0;
    if ((gFirstPersonFlags & 0x16) != 0) {
        edge = 0;
        firstOk = (gFirstPersonFlags & 0x800) == 0 ? 1 : 0;
    } else {
        if ((gTPWFlags & 8) != 0 && (gFirstPersonFlags & 0x100) == 0 &&
            (f28 < Bits(0xbf6e147bu) || f28 > Bits(0x3f6e147bu) || f27 < Bits(0xbf6e147bu) || f27 > Bits(0x3f6e147bu))) {
            edge = 1;
        } else {
            edge = 0;
        }
        firstOk = 0;
        if (Key(0xd) || Key(0xe) || Key(0xf) || Key(0x10)) keyScroll = 1;
    }

    // Which sides of the map the position is outside of: 1 x < 0, 4 z < 0, 2 x past the end, 8 z past the end.
    uint32_t bounds = 0;
    {
        const sHField* field = gHeightfieldHeader->mHField;
        const float width = field->mF08[2] * static_cast<float>(field->mF18[0]);
        const float depth = field->mF08[3] * static_cast<float>(field->mF18[1]);
        const float px = gCameraPosition[0];
        const float pz = gCameraPosition[2];
        if (px < 0.0f) bounds |= 1;
        if (pz < 0.0f) bounds |= 4;
        if (px > width) bounds |= 2;
        if (pz > depth) bounds |= 8;
    }

    float* const vel = gCameraF5BB4;
    if ((gCameraModeA & 4) == 0 && edge == 0 && firstOk == 0 && keyScroll == 0) {
        // Path A: nothing pushes the camera. The velocity dies away and the yaw eases towards the key yaw.
        const float scale = ppc_fmadds(kK001, dt, 1.0f);
        vel[0] = vel[0] / scale;
        vel[1] = vel[1] / scale;
        vel[2] = vel[2] / scale;
        const float target = gCameraF5BF4;
        float f3;
        if (isometric != 0) {
            const float least = gCameraYawRateIso < dt ? gCameraYawRateIso : dt;
            f3 = least / Bits(0x43480000u);
        } else {
            const float least = gCameraYawRate < dt ? gCameraYawRate : dt;
            f3 = least / Bits(0x430c0000u);
        }
        gCameraYaw = ppc_fmadds(gCameraYaw, 1.0f - f3, target * f3);
        if (static_cast<float>(std::fabs(static_cast<double>(gCameraYaw) - static_cast<double>(target))) < gCameraYawSnap) gCameraYaw = target;
    } else {
        // Path B: something pushes the camera: f30 sideways, f31 forward (or up the screen).
        if ((gFirstPersonFlags & 0x16) != 0) {
            f30 = 0.0f;
            if ((gFirstPersonFlags & 0x3c) != 0) {
                if (f28 < Bits(0xbe4ccccdu)) gFirstPersonTurn = Bits(0x40600000u) * (f28 - Bits(0xbe4ccccdu));
                else if (f28 > Bits(0x3e4ccccdu)) gFirstPersonTurn = Bits(0x40600000u) * (f28 - Bits(0x3e4ccccdu));
                else gFirstPersonTurn = f30;
            } else {
                if (f28 < Bits(0xbecccccdu)) gFirstPersonTurn = Bits(0x40000000u) * (f28 - Bits(0xbecccccdu));
                else if (f28 > Bits(0x3ecccccdu)) gFirstPersonTurn = Bits(0x40000000u) * (f28 - Bits(0x3ecccccdu));
                else gFirstPersonTurn = f30;
            }
            if (f27 < Bits(0xbecccccdu)) gFirstPersonPitch = (-f27) - Bits(0x3ecccccdu);
            else if (f27 > Bits(0x3e4ccccdu)) gFirstPersonPitch = Bits(0x3f99999au) * (Bits(0x3e4ccccdu) - f27);
            else gFirstPersonPitch = 0.0f;
            if ((gCameraModeA & 4) != 0 && (gFirstPersonFlags & 0x3c) == 0) f31 = f31 + Bits(0x3dcccccdu);
        } else {
            const bool rmbOff = gPlayerSettings.mCurrent.mRMBScrollOn == 0;
            if ((gCameraModeA & 4) != 0 && !rmbOff) {
                gFirstPersonFlags &= ~0x1000u;
                f30 = f28;
                f31 = f27;
            } else {
                f30 = 0.0f;
                f31 = f30;
                if (!rmbOff) {
                    f28 = f30;
                    f27 = f30;
                }
                if (Key(0xd)) f27 = 1.0f;
                if (Key(0xe)) f27 = -1.0f;
                if (Key(0xf)) f28 = -1.0f;
                if (Key(0x10)) f28 = 1.0f;
                if (f28 < Bits(0xbf6e147bu)) {
                    f30 = f28 - Bits(0xbf6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f30 = Bits(0x41200000u) * f30;
                } else if (f28 > Bits(0x3f6e147bu)) {
                    f30 = f28 - Bits(0x3f6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f30 = Bits(0x41200000u) * f30;
                }
                if (f27 < Bits(0xbf6e147bu)) {
                    f31 = f27 - Bits(0xbf6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f31 = Bits(0x41200000u) * f31;
                } else if (f27 > Bits(0x3f6e147bu)) {
                    f31 = f27 - Bits(0x3f6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f31 = Bits(0x41200000u) * f31;
                }
            }
        }

        if ((gFirstPersonFlags & 0x16) != 0) {
            if ((gFirstPersonFlags & 0x3c) == 0) {
                // Walking: the key yaw turns with the pointer, and the push is turned by it.
                const float turnStep = kK0012 * gFirstPersonTurn;
                gCameraF5BF4 = ppc_fmadds(turnStep, dt, gCameraF5BF4);
                const float s = static_cast<float>(std::sin(static_cast<double>(gCameraF5BF4)));
                const float c = static_cast<float>(std::cos(static_cast<double>(gCameraF5BF4)));
                const float a = f31 * s;
                const float n = ppc_fneg(f30);
                const float b = f31 * c;
                f30 = ppc_fmadds(f30, c, a);
                f31 = ppc_fmadds(n, s, b);
            }
            gCameraYaw = gCameraF5BF4;
        } else {
            const float target = gCameraF5BF4;
            float f3;
            if (isometric != 0) {
                const float least = gCameraYawRateIso < dt ? gCameraYawRateIso : dt;
                f3 = least / Bits(0x43480000u);
            } else {
                const float least = gCameraYawRate < dt ? gCameraYawRate : dt;
                f3 = least / Bits(0x430c0000u);
            }
            gCameraYaw = ppc_fmadds(gCameraYaw, 1.0f - f3, target * f3);
            if (static_cast<float>(std::fabs(static_cast<double>(gCameraYaw) - static_cast<double>(target))) < gCameraYawSnap) gCameraYaw = target;
            const float s = static_cast<float>(std::sin(static_cast<double>(gCameraYaw)));
            const float c = static_cast<float>(std::cos(static_cast<double>(gCameraYaw)));
            const float a = f31 * s;
            const float n = ppc_fneg(f30);
            const float b = f31 * c;
            f30 = ppc_fmadds(f30, c, a);
            f31 = ppc_fmadds(n, s, b);
        }

        // The push changes the velocity, which is halved each frame; against a map edge a velocity that points out decays instead.
        const float root = static_cast<float>(std::sqrt(static_cast<double>(gCameraF68)));
        const float f3 = kK002 * root;
        const float oldX = vel[0];
        const float oldZ = vel[2];
        vel[0] = ppc_fmadds(f30, f3, vel[0]) * 0.5f;
        vel[1] = 0.0f;
        vel[2] = ppc_fmadds(f31, f3, vel[2]) * 0.5f;
        if ((bounds & 1) != 0 && vel[0] < 0.0f) vel[0] = oldX / ppc_fmadds(kK001, dt, 1.0f);
        if ((bounds & 4) != 0 && vel[2] < 0.0f) vel[2] = oldZ / ppc_fmadds(kK001, dt, 1.0f);
        if ((bounds & 2) != 0 && vel[0] > 0.0f) vel[0] = oldX / ppc_fmadds(kK001, dt, 1.0f);
        if ((bounds & 8) != 0 && vel[2] > 0.0f) vel[2] = oldZ / ppc_fmadds(kK001, dt, 1.0f);
    }

    // A velocity this small in all three axes is nothing.
    if (static_cast<float>(std::fabs(static_cast<double>(vel[0]))) < gCameraStopSpeed &&
        static_cast<float>(std::fabs(static_cast<double>(vel[1]))) < gCameraStopSpeed &&
        static_cast<float>(std::fabs(static_cast<double>(vel[2]))) < gCameraStopSpeed) {
        vel[0] = 0.0f;
        vel[1] = 0.0f;
        vel[2] = 0.0f;
    }

    // The move. Walking on the ground goes grid square by grid square (the ground and the rides can stop it); the rest just adds.
    if ((gFirstPersonFlags & 0x16) != 0 && (gFirstPersonFlags & 0x3c) == 0) {
        s3DPoint step;
        step.x = vel[0] * dt;
        step.y = vel[1] * dt;
        step.z = vel[2] * dt;
        float* const parts[3] = {&step.x, &step.y, &step.z};
        for (float* part : parts) {
            if (*part >= Bits(0xb8d1b717u) && *part <= Bits(0x38d1b717u)) *part = 0.0f;
        }
        if (step.x != 0.0f || step.z != 0.0f) {
            s3DPoint out;
            out.x = out.y = out.z = 0.0f;
            int32_t more;
            do {
                more = GetNextGridsquare(AsPoint(gCameraPosition), &step, &out);
                if (RideRideIfRequired(&out) != 0) more = 0;
                std::memcpy(gCameraPosition, &out, sizeof out);
            } while (more != 0);
        }
    } else {
        gCameraPosition[0] = ppc_fmadds(vel[0], dt, gCameraPosition[0]);
        gCameraPosition[2] = ppc_fmadds(vel[2], dt, gCameraPosition[2]);
    }

    if ((gCameraModeA & 0x80) != 0) {
        if (gCameraWideZoomOnce == 0) {
            gCameraWideZoomMin = Bits(0x42200000u);
            gCameraWideZoomOnce = 1;
        }
        if (gCameraWideZoomMaxOnce == 0) {
            gCameraWideZoomMax = Bits(0x43960000u);
            gCameraWideZoomMaxOnce = 1;
        }
        if ((gCameraModeA & 0x200) != 0) {
            if (gCameraF68 < gCameraWideZoomMin) gCameraF68 = gCameraWideZoomMin;
            else if (gCameraF68 > gCameraWideZoomMax) gCameraF68 = gCameraWideZoomMax;
        } else {
            if (gCameraF68 < Bits(0x428c0000u)) gCameraF68 = Bits(0x428c0000u);
            else if (gCameraF68 > Bits(0x43020000u)) gCameraF68 = Bits(0x43020000u);
        }
    }
    std::memcpy(gCameraTarget, gCameraPosition, sizeof gCameraTarget);

    // The view point: where the camera is relative to the target (x +0, y +4, z +8 of gCameraViewPoint).
    if ((gFirstPersonFlags & 0x16) != 0) {
        if (gCameraWalkOnce == 0) {
            gCameraF59FC = 0.0f;
            gCameraWalkOnce = 1;
        }
        gCameraViewPoint.x = 0.0f;
        gCameraViewPoint.z = 0.0f;
        float f26 = 0.0f;   // not set in the original when there is no height field (a stale register); 0 here
        float f25 = 0.0f;
        const tag_sM3D2Header* header = gHeightfieldHeader;
        if (header != nullptr && header->mHField != nullptr && header->mHField->mHeights != nullptr &&
            header->mHField->mQuads != nullptr && (gFirstPersonFlags & 0x3c) == 0) {
            f26 = gCameraTarget[0] + gCameraViewPoint.x;
            f25 = gCameraTarget[2] + gCameraViewPoint.z;
        }
        if ((gFirstPersonFlags & 0x3c) == 0) {
            gCameraViewPoint.y = Bits(0x40a00000u) + DetermineHFieldHeight(f26, f25, 1);
        }
    } else {
        gCameraViewPoint.x = 0.0f;
        gCameraViewPoint.y = 0.0f;
        gCameraViewPoint.z = -gCameraF68;
        const float f4 = gCameraF68 - Bits(0x428c0000u);
        const float t = f4 / Bits(0x42700000u);
        gCameraPitch = ppc_fmadds(kQuarterPi, 1.0f - t, kOneAndAHalfPitch * t);
        gCameraPitch = gCameraPitch >= gCameraPitchMin ? gCameraPitch : gCameraPitchMin;
        gCameraPitch = gCameraPitchMax >= gCameraPitch ? gCameraPitch : gCameraPitchMax;
        const float s = static_cast<float>(std::sin(static_cast<double>(gCameraPitch)));
        const float c = static_cast<float>(std::cos(static_cast<double>(gCameraPitch)));
        const float y = gCameraViewPoint.y;
        const float z = gCameraViewPoint.z;
        gCameraViewPoint.y = ppc_fmsubs(y, c, z * s);
        gCameraViewPoint.z = ppc_fmadds(y, s, z * c);
        // The ground under the target, averaged with the last frame's, lifts the view point.
        gCameraGroundHeight = gCameraGroundHeight + DetermineHFieldHeight(gCameraTarget[0], gCameraTarget[2], 1);
        gCameraGroundHeight = gCameraGroundHeight * 0.5f;
        gCameraViewPoint.y = gCameraViewPoint.y + gCameraGroundHeight;
        // The tilt from the velocity: the square root of its size, scaled.
        if (vel[0] < 0.0f) {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(-vel[0])));
            gCameraTilt[0] = ppc_fneg(root / Bits(0x43480000u) - kK0001);
            gCameraTilt[0] = gCameraTilt[0] < gCameraTiltMinX ? gCameraTilt[0] : gCameraTiltMinX;
        } else {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(vel[0])));
            gCameraTilt[0] = root / Bits(0x43480000u) - kK0001;
            gCameraTilt[0] = gCameraTiltMaxX < gCameraTilt[0] ? gCameraTilt[0] : gCameraTiltMaxX;
        }
        if (vel[2] < 0.0f) {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(-vel[2])));
            gCameraTilt[1] = ppc_fneg(root / Bits(0xc3480000u) - kK0001);
            gCameraTilt[1] = gCameraTilt[1] < gCameraTiltMinZ ? gCameraTilt[1] : gCameraTiltMinZ;
        } else {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(vel[2])));
            gCameraTilt[1] = root / Bits(0xc3480000u) - kK0001;
            gCameraTilt[1] = gCameraTiltMaxZ < gCameraTilt[1] ? gCameraTilt[1] : gCameraTiltMaxZ;
        }
    }

    // The view point turned by the yaw and moved to the target.
    {
        const float s = static_cast<float>(std::sin(static_cast<double>(gCameraYaw)));
        const float c = static_cast<float>(std::cos(static_cast<double>(gCameraYaw)));
        const float x = gCameraViewPoint.x;
        const float n = ppc_fneg(x);
        gCameraViewPoint.x = ppc_fmadds(x, c, gCameraViewPoint.z * s);
        gCameraViewPoint.z = ppc_fmadds(n, s, gCameraViewPoint.z * c);
        gCameraViewPoint.x = gCameraViewPoint.x + gCameraTarget[0];
        gCameraViewPoint.y = gCameraViewPoint.y + gCameraTarget[1];
        gCameraViewPoint.z = gCameraViewPoint.z + gCameraTarget[2];
    }

    // The camera matrix.
    if ((gFirstPersonFlags & 0x16) != 0) {
        if ((gFirstPersonFlags & 0x3c) != 0) {
            // Riding: the ride's matrix, bumped by the ride, then turned by the pointer.
            std::memcpy(&gCameraMatrix, gFirstPersonRideMatrix, sizeof(sMatrix));
            ApplyHeadsOnRidesBumpage(gFirstPersonRideMatrix, &gCameraMatrixA, dt, &gCameraMatrix);
            std::memcpy(&gCameraMatrixA, gFirstPersonRideMatrix, sizeof(sMatrix));
            sMatrix local;
            SetMatrixIdentity(&local);
            float angles[2] = {gFirstPersonPitch, gFirstPersonTurn};
            RotateLMatrixXYZNoTrans(&local, kRotateXY, angles);
            CombineMatricesNoWDest2nd(&local, &gCameraMatrix);
        } else {
            SetLMatrixOrientationHi(&gCameraMatrix, gFirstPersonPitch, gCameraYaw);
        }
    } else {
        SetLMatrixOrientationHi(&gCameraMatrix, gCameraPitch, gCameraYaw);
        RotateLMatrixXYZNoTransHi(&gCameraMatrix, kRotateZX, gCameraTilt);
    }
    if ((gFirstPersonFlags & 0x3c) != 0) {
        gCameraMatrix.m[3][0] = gFirstPersonRideMatrix->m[3][0];
        gCameraMatrix.m[3][1] = gFirstPersonRideMatrix->m[3][1];
        gCameraMatrix.m[3][2] = gFirstPersonRideMatrix->m[3][2];
    } else {
        gCameraMatrix.m[3][0] = gCameraViewPoint.x;
        gCameraMatrix.m[3][1] = gCameraViewPoint.y;
        gCameraMatrix.m[3][2] = gCameraViewPoint.z;
    }
    if (MatrixDifferenceTiny(&gCameraMatrix, &gCameraMatrixB) == 0) {
        gei_ViewMatrix->Load(gCameraMatrix);
    } else {
        gCameraF5BF4 = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kTwoPiD));
        gCameraYaw = static_cast<float>(std::fmod(static_cast<double>(gCameraYaw), kTwoPiD));
    }
    gCurrentCameraMatrix = &gCameraMatrix;
    std::memcpy(&gCameraMatrixB, &gCameraMatrix, sizeof(sMatrix));
    gCameraClockStamp = now;
    InvertMatrix(&gInverseCameraMatrix, gCurrentCameraMatrix);
}

int32_t SaveCamera(void* file) {
    uint32_t count = 0;
    SwapWords(&gCameraF68, 1);
    LbFile_Write(file, &gCameraF68, 4, &count);
    SwapWords(&gCameraF68, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(&gCameraF5BF4, 1);
    LbFile_Write(file, &gCameraF5BF4, 4, &count);
    SwapWords(&gCameraF5BF4, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(&gFirstPersonFlags, 1);
    LbFile_Write(file, &gFirstPersonFlags, 4, &count);
    SwapWords(&gFirstPersonFlags, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(gCameraPosition, 3);
    LbFile_Write(file, gCameraPosition, 12, &count);
    SwapWords(gCameraPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(gCameraSavedPosition, 3);
    LbFile_Write(file, gCameraSavedPosition, 12, &count);
    SwapWords(gCameraSavedPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(&gCameraSavedYaw, 1);
    LbFile_Write(file, &gCameraSavedYaw, 4, &count);
    SwapWords(&gCameraSavedYaw, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

int32_t LoadCamera(void* file) {
    uint32_t count = 0;
    LbFile_Read(file, &gCameraF68, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gCameraF68, 1);
    LbFile_Read(file, &gCameraF5BF4, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gCameraF5BF4, 1);
    LbFile_Read(file, &gFirstPersonFlags, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gFirstPersonFlags, 1);
    LbFile_Read(file, gCameraPosition, 12, &count);
    SwapWords(gCameraPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    LbFile_Read(file, gCameraSavedPosition, 12, &count);
    SwapWords(gCameraSavedPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    LbFile_Read(file, &gCameraSavedYaw, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gCameraSavedYaw, 1);
    gFirstPersonFlags = 0;
    gCameraYaw = gCameraF5BF4;
    if (gPlayerSettings.mCurrent.mIsometricOn) {
        const float f = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kHalfPiD_2));
        if (!(f < 0.0f)) {
            if (!(f > kQuarterPi_2)) gCameraF5BF4 = gCameraF5BF4 - f;
            else gCameraF5BF4 = gCameraF5BF4 + (kHalfPi_2 - f);
        } else {
            if (!(f < -kQuarterPi_2)) gCameraF5BF4 = gCameraF5BF4 - f;
            else gCameraF5BF4 = gCameraF5BF4 - (kHalfPi_2 + f);
        }
    }
    return 1;
}

// 0x10032358. The original inlines the destruction of the embedded CLight (its virtual base CMapLinkBase
// vtable stores, which only rewrite vptrs and offsets) and then deletes the object when the hidden
// "delete storage" flag is positive. Both are what the compiler generates for the member destruction
// and for a delete expression, so the body is empty. The destructor is defined inline in
// include/TPAdvisor.h so every library that uses sAdvisorStore links.
