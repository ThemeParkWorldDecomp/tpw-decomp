// Test library for CCellBase and CTrackCell (unit w179; libtpw_unit_CellBase.dylib, tests/test_CellBase.py), main app:
//   src/CellBase.cpp        the neighbour, direction and overlap members, the four equivalents, CCellBase::Serialise and the four instances
//   src/TrackCell.cpp       CTrackCell::HasBeenModifiedFrom, Serialise and the two GetNeighbouringCell
//   src/Serialise_basic.cpp the basic LOADVAR / SAVEVAR instances (REAL: the file is the fake)
//   src/MapDelta.cpp        the eight compass deltas
//
// The fake file: LbFile_Read hands out the test's bytes in order (a cursor), LbFile_Write records the bytes as hex; the queues "LbFile_Read",
// "LbFile_Read:xfer", "LbFile_Write" and "LbFile_Write:xfer" replace the result and the transferred count. Every other callee is a fake that
// records what the real one reads and answers from its queue: CWorldSavvy::GetMap (logs the cell as its offset in the cell block, answers the
// map base the test set), MapDelta::operator* (the wrapping products), CMap::IsXYValid, CSaveLoad::WriteSerialiseLog and GAME_ASSERTMSG.
// Nothing dereferences a map or a world: the equivalents and neighbours are pointer arithmetic, so the bases are plain numbers.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#define TPW_CALLLOG_EVENTS 20000
#define TPW_CALLLOG_LABELS 64
#define TPW_CALLLOG_QUEUE_VALUES 4000
#include "calllog.cpp"
#include "CellBase.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CWorld* gWorld;
CMap* gWorldMap;
CSaveLoad gSaveLoad;
uint8_t gLLCC4818;

namespace {

constexpr uint32_t kGuard = 0x40;
alignas(16) uint8_t gCellBlock[kGuard + sizeof(CTrackCell) + kGuard];   // 0x40 guard bytes, the cell, 0x40 guard bytes
char gFile;                       // the fake file's address (a label for the log)
uintptr_t gSavvyMap;              // what CWorldSavvy::GetMap answers
std::vector<uint8_t> gFileData;   // the bytes the reads hand out, in order
std::size_t gCursor = 0;

uint8_t* Cell() { return gCellBlock + kGuard; }

// A cell's offset in the cell block (the guards included), -1 for anything else.
int64_t Off(const void* p) {
    const intptr_t d = static_cast<const uint8_t*>(p) - Cell();
    return (d >= -static_cast<intptr_t>(kGuard) && d < static_cast<intptr_t>(sizeof(CTrackCell) + kGuard)) ? d : -1;
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

}  // namespace

// ---- the fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CMap* CWorldSavvy::GetMap() const {
    ev("CWorldSavvy::GetMap", {Off(this)});
    return reinterpret_cast<CMap*>(gSavvyMap);
}

// The products wrap in 32 bits; the flag byte is written 0 (the original never writes it).
MapDelta MapDelta::operator*(int32_t k) const {
    ev("MapDelta::operator*", {mX, mY, k});
    MapDelta result;
    result.mX = static_cast<int32_t>(static_cast<uint32_t>(mX) * static_cast<uint32_t>(k));
    result.mY = static_cast<int32_t>(static_cast<uint32_t>(mY) * static_cast<uint32_t>(k));
    result.mFlag = 0;
    return result;
}

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("CMap::IsXYValid", {x, y});
    return static_cast<int32_t>(ret("CMap::IsXYValid"));
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

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_cbase_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gSaveLoad", &gSaveLoad},
        {"file", &gFile},
        {"cellblock", gCellBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The globals the functions read: the cell system's flag byte, gWorld, gWorldMap and what CWorldSavvy::GetMap answers (plain numbers).
void tpw_cbase_set_globals(uint32_t flag, uint64_t world, uint64_t worldMap, uint64_t savvyMap) {
    gLLCC4818 = static_cast<uint8_t>(flag);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(worldMap));
    gSavvyMap = static_cast<uintptr_t>(savvyMap);
}

void tpw_cbase_set_file(const char* data, int32_t size) {
    gFileData.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size);
    gCursor = 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cbase_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(sizeof(CCellBase::CELL_IndexType));
    out[n++] = static_cast<int64_t>(sizeof(CELL_MODES));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mTileData));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mSegmentNumber));
    out[n++] = static_cast<int64_t>(sizeof(MapDelta));
    return n;
}

// One entry for the 23 functions, by the module's index (OPS in the module). `cell` is the cell's address; a, b, c are the integer arguments, p
// the name of the instances. Pointers answer as numbers, narrow values widened the way the module compares them.
int64_t tpw_cbase_call(uint32_t op, uint64_t cell, int64_t a, int64_t b, int64_t c, uint64_t p) {
    CCellBase* const base = reinterpret_cast<CCellBase*>(static_cast<uintptr_t>(cell));
    CTrackCell* const track = reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(cell));
    const CCellBase* const constBase = base;
    uint8_t* const bytes = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(cell));
    const char* const name = reinterpret_cast<const char*>(static_cast<uintptr_t>(p));
    switch (op) {
        case 0: base->ModifyNeighbours(static_cast<uint8_t>(a)); return 0;
        case 1: return constBase->GetNeighbours();
        case 2: return constBase->NumberOfNeighbours();
        case 3: base->RemoveDirection(static_cast<uint8_t>(a)); return 0;
        case 4: return constBase->NumberOfHVNeighbours();
        case 5: return constBase->GetDirection();
        case 6: return constBase->NumberOfDirections();
        case 7: base->DecOverlapCounter(); return 0;
        case 8: base->SetOverlapCounter(static_cast<int32_t>(a)); return 0;
        case 9: return constBase->GetOverlapCounter();
        case 10: return static_cast<int64_t>(reinterpret_cast<uintptr_t>(constBase->GetTrackCellEquiv()));
        case 11: return static_cast<int64_t>(reinterpret_cast<uintptr_t>(base->GetTrackCellEquiv()));
        case 12: return static_cast<int64_t>(reinterpret_cast<uintptr_t>(constBase->GetMapCellEquiv()));
        case 13: return static_cast<int64_t>(reinterpret_cast<uintptr_t>(base->GetMapCellEquiv()));
        case 14: return base->Serialise(&gFile, static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<uint32_t>(c));
        case 15: {
            auto* tile = reinterpret_cast<CCellBase::CELL_IndexType*>(bytes + offsetof(CCellBase, mTileData));
            return SAVEVARSTRUCT(&gFile, tile, name, static_cast<int32_t>(a));
        }
        case 16: {
            auto* tile = reinterpret_cast<CCellBase::CELL_IndexType*>(bytes + offsetof(CCellBase, mTileData));
            return LOADVARSTRUCT(&gFile, tile, name, static_cast<int32_t>(a));
        }
        case 17: return LOADVAR(&gFile, reinterpret_cast<CELL_MODES*>(bytes + offsetof(CCellBase, mType)), name, static_cast<int32_t>(a));
        case 18: return SAVEVAR(&gFile, reinterpret_cast<CELL_MODES*>(bytes + offsetof(CCellBase, mType)), name, static_cast<int32_t>(a));
        case 19: return track->HasBeenModifiedFrom(*track);
        case 20: return track->Serialise(&gFile, static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<uint32_t>(c));
        case 21: {
            MapDelta delta;
            delta.mX = static_cast<int32_t>(a);
            delta.mY = static_cast<int32_t>(b);
            delta.mFlag = 0;
            return static_cast<int64_t>(reinterpret_cast<uintptr_t>(track->GetNeighbouringCell(delta, static_cast<int32_t>(c))));
        }
        case 22: return static_cast<int64_t>(reinterpret_cast<uintptr_t>(track->GetNeighbouringCell(static_cast<uint8_t>(a), static_cast<int32_t>(b))));
        default: return 0;
    }
}

}  // extern "C"
