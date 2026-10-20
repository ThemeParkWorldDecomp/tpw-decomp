// Test library for the hoarding edit (unit w206; libtpw_unit_HoardingEdit.dylib, tests/test_HoardingEdit.py), main app:
//   src/LLCellSystem_hoardedit.cpp  LLCC_FillInsideOfPark and LLCC_IsHoardingEditLegal
//   src/TrackCell_isflag.cpp, src/MapCell_kinds.cpp  CTrackCell::IsFlag and CMapCell::UnSetFlag, IsWater, IsFreeform
//   src/TrackCell_flags.cpp, src/MapCell_flags.cpp (unit w201), src/TrackCell_shape.cpp (unit w204), src/CellBase.cpp, src/TrackCell.cpp, src/MapDelta.cpp
//   (landed): the flag and kind tests, the neighbour counts, the cell equivalents, GetNeighbouringCell of the track cell and the compass offsets
// Every other callee is a fake that records the event the PowerPC stub records (a cell is named by its index in its block, never by its address). The
// globals of other files are defined here, and so are the four tiny callees that run for real on the PowerPC side: OPPOSITE, CMap::IsXYValid,
// MapDelta::operator* and CMapCell::GetNeighbouringCell (the host copies their bodies from src/MapDelta_map.cpp).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ActionRecorder.h"
#include "CellBase.h"
#include "Debug.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "SaveLoad.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"
#include "calllog.h"

using namespace calllog;

// ---- globals of other files (src/LLCellSystem.cpp's words, src/LLCellSystem_hoarding.cpp's extents, the recorder and the balance)
CWorld* gGameWorld;
CWorld* gWorld;
CMap* gWorldMap;
uint8_t gLLCC4818, gLLCCE0CC;
int32_t gLLCCWidth, gLLCCHeight, gLLCC47CC, gLLCCC204;
int32_t gHoardingLeft, gHoardingTop, gHoardingRight, gHoardingBottom;
CActionRecorder gActionRecorder;
// gMainBalance is raw storage under the symbol's name (the class has a constructor and a vtable the unit does not link; only one value is read here).
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");

namespace {
char* gWorldStore;               // the world block (gGameWorld, gWorld and gWorldMap point into it)
alignas(16) char gBankStore[16];   // the token the fake CWorld::GetBankAccount hands out
}

// ---- the small callees that run for real on the PowerPC side: bodies as in src/MapDelta_map.cpp (0x10102500, 0x101012f4, 0x101037d0, 0x101036b0)
uint8_t OPPOSITE(uint8_t d) {
    if (d == 0) return 0;
    if (d < 0x10) return static_cast<uint8_t>((d << 4) & 0xf0);
    return static_cast<uint8_t>((d >> 4) & 0xff);
}
int32_t CMap::IsXYValid(int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < 0x80 && y < 0x80; }
MapDelta MapDelta::operator*(int32_t k) const {
    MapDelta result;
    result.mX = mX * k;
    result.mY = mY * k;
    result.mFlag = 0;
    return result;
}
CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    const MapDelta scaled = (*delta) * flag;
    const uint32_t i = static_cast<uint32_t>(mCellId) - 1u;
    if (!CMap::IsXYValid(static_cast<int32_t>(i & 0x7fu) + scaled.mX, static_cast<int32_t>(i >> 7) + scaled.mY)) return nullptr;
    const uint16_t id = static_cast<uint16_t>(delta->mX + (delta->mY << 7) + mCellId);
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44);
}

// ---- the serialisers of the landed cell code are linked but never run: their file and log callees only have to exist
TbError LbFile_Read(void*, void*, uint32_t, uint32_t*) { return TbError(-1); }
TbError LbFile_Write(void*, const void*, uint32_t, uint32_t*) { return TbError(-1); }
CSaveLoad gSaveLoad;
void CSaveLoad::WriteSerialiseLog(int32_t, const char*, LogfileState) {}
CMap* CWorldSavvy::GetMap() const { return gWorldMap; }

namespace {
CTrackCell* TrackCell(int32_t id) {
    return reinterpret_cast<CTrackCell*>(gWorldStore + 0x1102d8 + (static_cast<int64_t>(id) - 1) * 0x28);
}
CMapCell* MapCell(int32_t id) {
    return reinterpret_cast<CMapCell*>(gWorldStore + 0x2d8 + (static_cast<int64_t>(id) - 1) * 0x44);
}
// The index of a cell in its block (the log never holds an address).
int64_t TrackIndex(const CTrackCell* cell) { return (reinterpret_cast<const char*>(cell) - (gWorldStore + 0x1102d8)) / 0x28; }
int64_t MapIndex(const CMapCell* cell) { return (reinterpret_cast<const char*>(cell) - (gWorldStore + 0x2d8)) / 0x44; }
}  // namespace

// ---- the fakes
// The stand-ins for the deletion of a hoarding cell (the Mac ignores their answers), and the tile of a map cell.
void CTrackCell::Delete(int32_t a, int32_t b, bool c) { ev("Delete", {TrackIndex(this), a, b, c ? 1 : 0}); }
void CTrackCell::QuickDelete() { ev("QuickDelete", {TrackIndex(this)}); }
void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {MapIndex(this)}); }
// The image functions of unit w201: only the arguments are recorded; the legality answer is scripted per case (the queue "SoIsItLegal").
void LLCC_CopyCurrentToNewImage() { ev("CopyCurrentToNewImage"); }
void LLCC_AddNewAreaToImage(int32_t x1, int32_t y1, int32_t x2, int32_t y2) { ev("AddNewAreaToImage", {x1, y1, x2, y2}); }
int32_t LLCC_SoIsItLegal() {
    ev("SoIsItLegal");
    return static_cast<int32_t>(ret("SoIsItLegal", 0));
}
// The bank: GetBankAccount answers a token (label 1 in the log, never an address); GetBalance answers the case's balance (the queue "GetBalance").
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {reinterpret_cast<char*>(this) == gWorldStore ? 1 : 0});
    return reinterpret_cast<CBankAccount*>(gBankStore);
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance", 0));
}

// ---- trace and assertion (GetNeighbouringCell's bad direction path)
void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%d") != nullptr) {
        va_list args;
        va_start(args, format);
        const int value = va_arg(args, int);
        va_end(args);
        ev("CTRACE", {level, value}, {format});
    } else {
        ev("CTRACE", {level}, {format});
    }
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    char shown[160];
    std::snprintf(shown, sizeof shown, "%.159s", format);
    ev("GAME_ASSERTMSG", {condition}, {shown});
}

extern "C" {

// The host address of a global by name (the Python module reads and writes them with the width it knows).
uint64_t tpw_hed_gaddr(const char* name) {
#define G(n) if (std::strcmp(name, #n) == 0) return reinterpret_cast<uintptr_t>(&n)
    G(gLLCC4818); G(gLLCCE0CC); G(gLLCCWidth); G(gLLCCHeight); G(gLLCC47CC); G(gLLCCC204);
    G(gHoardingLeft); G(gHoardingTop); G(gHoardingRight); G(gHoardingBottom); G(gLLCCEdgeMarks);
#undef G
    if (std::strcmp(name, "edge0") == 0) return reinterpret_cast<uintptr_t>(&gLLCCEdgeCount[0]);
    if (std::strcmp(name, "edge1") == 0) return reinterpret_cast<uintptr_t>(&gLLCCEdgeCount[1]);
    if (std::strcmp(name, "edge2") == 0) return reinterpret_cast<uintptr_t>(&gLLCCEdgeCount[2]);
    if (std::strcmp(name, "edge3") == 0) return reinterpret_cast<uintptr_t>(&gLLCCEdgeCount[3]);
    // gMainBalance value slot 369 (the 32-bit integer) and the action recorder's recording word
    if (std::strcmp(name, "cost") == 0) return reinterpret_cast<uintptr_t>(&reinterpret_cast<CMainBalance*>(gMainBalanceStorage)->mValues[369].mInteger);
    if (std::strcmp(name, "recording") == 0) return reinterpret_cast<uintptr_t>(&gActionRecorder.mRecording);
    if (std::strcmp(name, "world") == 0) return reinterpret_cast<uintptr_t>(gWorldStore);
    if (std::strcmp(name, "bank") == 0) return reinterpret_cast<uintptr_t>(gBankStore);
    return 0;
}

// Once per process: the world (zero, never constructed); the three world pointers.
void tpw_hed_init() {
    if (gWorldStore == nullptr) gWorldStore = static_cast<char*>(std::calloc(1, 0x1b1000));
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStore);
    gWorld = reinterpret_cast<CWorld*>(gWorldStore);
    gWorldMap = reinterpret_cast<CMap*>(gWorldStore + 0x2d8);
}

int32_t tpw_hed_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mHoardingNeighbours));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mOverlapCounter));
    out[n++] = static_cast<int64_t>(sizeof(gLLCCEdgeCount));
    out[n++] = static_cast<int64_t>(sizeof(gLLCCEdgeMarks));
    return n;
}

// ---- the functions. A cell is passed by its 1 based id.
void tpw_hed_FillInside() { LLCC_FillInsideOfPark(); }
int32_t tpw_hed_EditLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t charge) {
    return LLCC_IsHoardingEditLegal(x1, y1, x2, y2, charge != 0);
}
int32_t tpw_hed_TrackIsFlag(int32_t id, int32_t flag) { return TrackCell(id)->IsFlag(static_cast<uint16_t>(flag)); }
int32_t tpw_hed_UnSetFlag(int32_t id, int32_t flag) {
    MapCell(id)->UnSetFlag(static_cast<uint16_t>(flag));
    return 0;
}
int32_t tpw_hed_IsFreeform(int32_t id) { return MapCell(id)->IsFreeform(); }
int32_t tpw_hed_IsWater(int32_t id) { return MapCell(id)->IsWater(); }

}  // extern "C"
