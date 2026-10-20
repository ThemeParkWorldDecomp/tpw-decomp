// Test library for the save and litter half of MapDelta.cpp (unit w146; libtpw_unit_MapSave.dylib, tests/test_MapSave.py), main app:
//   src/MapDelta_save.cpp   CMapCell::PlaceLitter, CMap::Serialise, CMapCell::Serialise, CMacroAI::Serialise, LOADVAR/SAVEVAR<LitterType>
//   src/Serialise_basic.cpp the basic LOADVAR / SAVEVAR instances (REAL: the file is the fake)
//   src/CellInit.cpp        the three cell constructors (REAL on both sides: the locals of CMap::Serialise)
//
// The fake file: LbFile_Read hands out the test's bytes in order (a cursor), LbFile_Write records the bytes as hex, LbFile_GetPosition answers
// from its queue; the queues "LbFile_Read", "LbFile_Read:xfer", "LbFile_Write", "LbFile_Write:xfer" replace the result and the transferred count
// as in unit_Serialise.cpp. Every other callee is a fake that records the arguments the real one reads (objects as offsets from the map block, or
// -1 for a local) and answers from its queue: the cell, staff, time and arrival serialisers, the three "modified" tests, the two cell resets,
// CTRACE, GAME_ASSERTMSG, the sprite script, the emitter, the world's generator and the regional effector. The map is one static block
// (tpw_mw_block); the world pointer is shifted back by 0x2d8 as in the real layout (CWorld + 0x2d8 is the map).
#define TPW_CALLLOG_EVENTS 200000
#define TPW_CALLLOG_LABELS 512
#define TPW_CALLLOG_QUEUE_VALUES 16500
#include "calllog.cpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "CellBase.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "LbFile.h"
#include "ObjectParts.h"
#include "MapCell.h"
#include "RegionEffectCell.h"
#include "SaveLoad.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gWorld;
CMap* gWorldMap;
const CMapCell* gMapGridBase;
const CMapCell* gMapGridEnd;
int32_t gMapGridCellSize;
const CTrackCell* gTrackGridBase;
const CTrackCell* gTrackGridEnd;
int32_t gTrackGridCellSize;
CSaveLoad gSaveLoad;

namespace {

constexpr uint32_t kBlockSize = 0x1db000;
alignas(16) unsigned char gBlock[kBlockSize];
char gFileHandle;                 // the fake file's address (a label for the log)
std::vector<uint8_t> gFileData;   // the bytes the reads hand out, in order
std::size_t gCursor = 0;

// An object's offset from the map block (the world is 0x2d8 before it), -1 for anything else (a local of the code under test).
int64_t Off(const void* p) {
    const intptr_t d = static_cast<const unsigned char*>(p) - gBlock;
    return (d >= -0x400 && d < static_cast<intptr_t>(kBlockSize)) ? d : -1;
}

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    const auto* p = static_cast<const uint8_t*>(data);
    std::string out;
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

// The number of arguments a format takes ("%%" takes none).
int Args(const char* format) {
    int n = 0;
    for (const char* p = format; *p; ++p) {
        if (*p != '%') continue;
        if (p[1] == '%') {
            ++p;
            continue;
        }
        ++n;
    }
    return n;
}

}  // namespace

// ---- the fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t values[3] = {0, 0, 0};
    const int n = Args(format);
    va_list ap;
    va_start(ap, format);
    for (int i = 0; i < n && i < 3; ++i) values[i] = static_cast<int64_t>(va_arg(ap, uint32_t));
    va_end(ap);
    if (n == 0) ev("CTRACE", {level}, {format});
    else if (n == 1) ev("CTRACE", {level, values[0]}, {format});
    else if (n == 2) ev("CTRACE", {level, values[0], values[1]}, {format});
    else ev("CTRACE", {level, values[0], values[1], values[2]}, {format});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const std::size_t available = gFileData.size() > gCursor ? gFileData.size() - gCursor : 0;
    uint32_t count = available < length ? static_cast<uint32_t>(available) : length;
    const std::size_t taken = count;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        std::vector<uint8_t> bytes(length, 0);
        std::memcpy(bytes.data(), gFileData.data() + (gFileData.size() > gCursor ? gCursor : gFileData.size()), taken < length ? taken : length);
        std::memcpy(buffer, bytes.data(), length);
    }
    gCursor += taken;
    return TbError(result);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), length}, {Hex(buffer, length).c_str()});
    const int64_t forced = ret("LbFile_Write:xfer", -1);
    *transferred = forced >= 0 ? static_cast<uint32_t>(forced) : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetPosition"));
}

CGameType::CGameType() : mType(kGameTypeNormal) {}

int32_t CCellBase::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CCellBase::Serialise", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CCellBase::Serialise"));
}
int32_t CTrackCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CTrackCell::Serialise", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CTrackCell::Serialise"));
}
int32_t CRegionEffectCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CRegionEffectCell::Serialise", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CRegionEffectCell::Serialise"));
}
int32_t CStaffDB::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CStaffDB::Serialise", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CStaffDB::Serialise"));
}
int32_t CParkTime::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CParkTime::Serialise", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CParkTime::Serialise"));
}
int32_t CArrival::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CArrival::Serialise", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CArrival::Serialise"));
}

// The "modified" tests and resets log what the real ones would read of the cells: the id and the attribute byte.
int32_t CMapCell::HasBeenModifiedFrom(const CMapCell& other) const {
    ev("CMapCell::HasBeenModifiedFrom", {Off(this), Off(&other), other.mCellId, other.mStatusFlags});
    return static_cast<int32_t>(ret("CMapCell::HasBeenModifiedFrom"));
}
int32_t CTrackCell::HasBeenModifiedFrom(const CTrackCell& other) {
    ev("CTrackCell::HasBeenModifiedFrom", {Off(this), Off(&other), other.mCellId});
    return static_cast<int32_t>(ret("CTrackCell::HasBeenModifiedFrom"));
}
bool CRegionEffectCell::operator==(const CRegionEffectCell& other) {
    ev("CRegionEffectCell::operator==", {Off(this), Off(&other)});
    return ret("CRegionEffectCell::operator==") != 0;
}
void CMapCell::Reset(bool full) { ev("CMapCell::Reset", {Off(this), full ? 1 : 0, mCellId, mStatusFlags}); }
void CTrackCell::Reset() { ev("CTrackCell::Reset", {Off(this), mCellId}); }

void CMap::AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::AddRegionalEffector", {Off(this), static_cast<int64_t>(effector), cell.mIndex});
}
void CWorld::SeedRand(uint32_t seed) { ev("CWorld::SeedRand", {Off(this), seed}); }
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {Off(this)});
    return static_cast<uint32_t>(ret("CWorld::GetRand"));
}

int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    const uint32_t* w = static_cast<const uint32_t*>(block);
    ev("ESP_SpawnScript", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], static_cast<int64_t>(sprite), a, static_cast<int64_t>(acts)});
    ev("ESP_SpawnScript.pos", {fbits(x), fbits(y), fbits(z)});
    return static_cast<int32_t>(ret("ESP_SpawnScript"));
}
void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    ev("SCR_SetScriptPosition", {handle, fbits(x), fbits(y), fbits(z), mode});
}
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}

extern "C" {

// The host address of a global by the module's name for it: "file" and "gSaveLoad" (the log's labels).
uint64_t tpw_mw_addr(const char* name) {
    if (std::strcmp(name, "file") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gFileHandle));
    if (std::strcmp(name, "gSaveLoad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad));
    return 0;
}

// The map block (0x1db000 bytes, zeroed); the world pointer is the block less 0x2d8, as at CWorld + 0x2d8.
uint64_t tpw_mw_block() {
    std::memset(gBlock, 0, sizeof gBlock);
    gWorldMap = reinterpret_cast<CMap*>(gBlock);
    gWorld = reinterpret_cast<CWorld*>(gBlock - 0x2d8);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gBlock));
}

void tpw_mw_set_file(const char* data, int32_t size) {
    gFileData.assign(data, data + size);
    gCursor = 0;
}

void tpw_mw_set_gametype(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }

// The eight compass deltas in __sinit_MapDelta_cpp's order (x, y, flag each).
void tpw_mw_deltas(int32_t* out) {
    const MapDelta* const all[8] = {&gMapDeltaMinusY, &gMapDeltaPlusXMinusY, &gMapDeltaPlusX, &gMapDeltaPlusXPlusY,
                                    &gMapDeltaPlusY, &gMapDeltaMinusXPlusY, &gMapDeltaMinusX, &gMapDeltaMinusXMinusY};
    for (int i = 0; i < 8; ++i) {
        out[3 * i] = all[i]->mX;
        out[3 * i + 1] = all[i]->mY;
        out[3 * i + 2] = all[i]->mFlag;
    }
}

int32_t tpw_mw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mStatusFlags));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterScript));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitter));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterCollector));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mTimeMarkedForLitterCollection));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterEmitter));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mPylonIndex));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(sizeof(CRegionEffectCell));
    return n;
}

// ---- the functions (the cell, the map and the world as addresses)
int32_t tpw_mw_cell_serialise(uint64_t cell, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->Serialise(&gFileHandle, save, version, unused);
}
int32_t tpw_mw_map_serialise(uint64_t map, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CMap*>(static_cast<uintptr_t>(map))->Serialise(&gFileHandle, save, version, unused);
}
int32_t tpw_mw_macro_serialise(uint64_t world, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CMacroAI*>(static_cast<uintptr_t>(world))->Serialise(&gFileHandle, save, version, unused);
}
void tpw_mw_place_litter(uint64_t cell, uint32_t type) {
    reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->PlaceLitter(static_cast<CMapCell::LitterType>(type));
}

}  // extern "C"
