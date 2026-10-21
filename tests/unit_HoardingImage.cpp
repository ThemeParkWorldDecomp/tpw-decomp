// Test library for the park's hoarding (unit w201; libtpw_unit_HoardingImage.dylib, tests/test_HoardingImage.py), main app:
//   src/game/park/LLCellSystem.cpp  the 12 LLCC functions (the walk, the lock, the redraw, the image, the fill, the legality checks)
//   src/game/park/TrackCell.cpp, src/game/park/MapCell.cpp  the six CTrackCell members and CMapCell::IsFlag
//   src/game/park/CellBase.cpp, src/game/buildings/TrainingCentre.cpp, src/game/park/MapDelta.cpp (landed): the neighbour counts, the cell equivalents, GetNeighbouringCell and the compass
//   src/game/park/LLCellSystem.cpp (landed): the click stack's count
// Every other callee is a fake that records the event the PowerPC stub records. The globals of other files are defined here, and so are the two tiny
// callees that run for real on the PowerPC side: OPPOSITE and CMap::IsXYValid (the host copies their bodies from src/game/park/Map.cpp).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CellBase.h"
#include "Debug.h"
#include "HoardingsBalance.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "SaveLoad.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"
#include "calllog.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files (src/game/park/LLCellSystem.cpp's words, src/game/park/LLCellSystem.cpp's extents, src/game/park/LLCellSystem.cpp's click stack)
CWorld* gGameWorld;
CWorld* gWorld;
CMap* gWorldMap;
uint8_t gLLCC4818, gLLCC47C4, gLLCCE0CC;
int32_t gLLCCWidth, gLLCCHeight, gLLCC4804, gLLCC4800;
int32_t gHoardingLeft, gHoardingTop, gHoardingRight, gHoardingBottom;
LLCCClick gLLCCClickStack[0x400];

namespace {
char* gWorldStore;   // the world block (gGameWorld, gWorld and gWorldMap point into it)
}

// ---- the two small callees that run for real on the PowerPC side: bodies as in src/game/park/Map.cpp (0x10102500, 0x101012f4)
uint8_t OPPOSITE(uint8_t d) {
    if (d == 0) return 0;
    if (d < 0x10) return static_cast<uint8_t>((d << 4) & 0xf0);
    return static_cast<uint8_t>((d >> 4) & 0xff);
}
int32_t CMap::IsXYValid(int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < 0x80 && y < 0x80; }
// 0x101037d0 (src/game/park/MapDelta.cpp): both words times k (the host writes 0 into the flag byte; GetNeighbouringCell never reads it).
MapDelta MapDelta::operator*(int32_t k) const {
    MapDelta result;
    result.mX = mX * k;
    result.mY = mY * k;
    result.mFlag = 0;
    return result;
}

// ---- the serialisers of the landed cell code are linked but never run: their file and log callees only have to exist
TbError LbFile_Read(void*, void*, uint32_t, uint32_t*) { return TbError(-1); }
TbError LbFile_Write(void*, const void*, uint32_t, uint32_t*) { return TbError(-1); }
CSaveLoad gSaveLoad;
void CSaveLoad::WriteSerialiseLog(int32_t, const char*, LogfileState) {}
CMap* CWorldSavvy::GetMap() const { return gWorldMap; }

// ---- the fakes
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LeftMouseButtonUp", {x, y, shift, flag}); }
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("SystemSetMode", {mode, flag}); }
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }
int32_t LLCC_BuildHoardingDB(int32_t right, int32_t bottom, int32_t left, int32_t top, char mode, bool flag) {
    ev("BuildHoardingDB", {right, bottom, left, top, static_cast<uint8_t>(mode), flag});
    return static_cast<int32_t>(ret("BuildHoardingDB", 0));
}
void LLCC_FillInsideOfPark() { ev("FillInsideOfPark"); }
void RecalculateParkDarkening() { ev("RecalculateParkDarkening"); }

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

namespace {
CTrackCell* TrackCell(int32_t id) {
    return reinterpret_cast<CTrackCell*>(gWorldStore + 0x1102d8 + (static_cast<int64_t>(id) - 1) * 0x28);
}
CMapCell* MapCell(int32_t id) {
    return reinterpret_cast<CMapCell*>(gWorldStore + 0x2d8 + (static_cast<int64_t>(id) - 1) * 0x44);
}
}  // namespace

extern "C" {

// The host address of a global by name (the Python module reads and writes them with the width it knows).
uint64_t tpw_hbi_gaddr(const char* name) {
#define G(n) if (std::strcmp(name, #n) == 0) return reinterpret_cast<uintptr_t>(&n)
    G(gLLCC4818); G(gLLCC47C4); G(gLLCCE0CC); G(gLLCCWidth); G(gLLCCHeight); G(gLLCC4804); G(gLLCC4800);
    G(gHoardingLeft); G(gHoardingTop); G(gHoardingRight); G(gHoardingBottom); G(gLLCCReviseAnswer);
    G(gLLCCClickStackCount); G(gLLCCClickStack); G(gLLCCHoardingImages);
#undef G
    if (std::strcmp(name, "world") == 0) return reinterpret_cast<uintptr_t>(gWorldStore);
    return 0;
}

// Once per process: the world (zero, never constructed); the three world pointers.
void tpw_hbi_init() {
    if (gWorldStore == nullptr) gWorldStore = static_cast<char*>(std::calloc(1, 0x1b1000));
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStore);
    gWorld = reinterpret_cast<CWorld*>(gWorldStore);
    gWorldMap = reinterpret_cast<CMap*>(gWorldStore + 0x2d8);
}

int32_t tpw_hbi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(LLCCClick));
    out[n++] = static_cast<int64_t>(sizeof(gLLCCHoardingImages));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mHoardingNeighbours));
    return n;
}

// ---- the functions. A cell is passed by its 1 based id.
void tpw_hbi_ConstructClickStack() { LLCC_SystemConstructHoardingClickStack(); }
void tpw_hbi_LockHoarding(int32_t lock) { LLCC_LockHoarding(lock != 0); }
void tpw_hbi_DrawDefaultHoardings() { LLCC_DrawDefaultHoardings(); }
// The balance table is a raw block (the class is not constructed): its 130 values (mValues[0] to [129]) as 32-bit integers.
void tpw_hbi_DrawFromBalanceFile(const int32_t* values) {
    alignas(16) static char store[sizeof(CHoardingsBalance)];
    CHoardingsBalance* balance = reinterpret_cast<CHoardingsBalance*>(store);
    for (int32_t i = 0; i < 130; ++i) {
        balance->mValues[i].mBits = 0;
        balance->mValues[i].mInteger = values[i];
    }
    LLCC_DrawHoardingsFromBalanceFile(balance);
}
void tpw_hbi_ReviseHoarding(int32_t a, int32_t b, int32_t c, int32_t d) { LLCC_ReviseHoarding(a, b, c, d); }
void tpw_hbi_FloodFill(int32_t x, int32_t y, int32_t oldValue, int32_t fill) {
    LLCC_FloodFill(x, y, static_cast<uint8_t>(oldValue), static_cast<uint8_t>(fill));
}
void tpw_hbi_CreateCurrentImage() { LLCC_CreateCurrentHoardingImage(); }
void tpw_hbi_CopyCurrentToNew() { LLCC_CopyCurrentToNewImage(); }
void tpw_hbi_AddNewArea(int32_t a, int32_t b, int32_t c, int32_t d) { LLCC_AddNewAreaToImage(a, b, c, d); }
int32_t tpw_hbi_SoIsItLegal() { return LLCC_SoIsItLegal(); }
int32_t tpw_hbi_IsClearLandLegal(int32_t a, int32_t b, int32_t c, int32_t d) { return LLCC_IsClearLandLegal(a, b, c, d); }
int32_t tpw_hbi_IsPatrolAreaLegal(int32_t a, int32_t b, int32_t c, int32_t d) { return LLCC_IsPatrolAreaLegal(a, b, c, d) ? 1 : 0; }
int32_t tpw_hbi_GetFlag(int32_t id) { return TrackCell(id)->GetFlag(); }
void tpw_hbi_SetFlag(int32_t id, int32_t flags) { TrackCell(id)->SetFlag(static_cast<uint16_t>(flags)); }
void tpw_hbi_UnSetFlag(int32_t id, int32_t flags) { TrackCell(id)->UnSetFlag(static_cast<uint16_t>(flags)); }
int32_t tpw_hbi_IsHoarding(int32_t id) { return TrackCell(id)->IsHoarding(); }
int32_t tpw_hbi_IsTrackX(int32_t id) { return TrackCell(id)->IsTrackX(); }
int32_t tpw_hbi_IsCorner(int32_t id) { return TrackCell(id)->IsCorner(); }
int32_t tpw_hbi_IsFlag(int32_t id, int32_t flag) { return MapCell(id)->IsFlag(static_cast<uint16_t>(flag)); }

}  // extern "C"
