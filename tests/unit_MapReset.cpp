// Test library for the map reset and the attribute map (libtpw_unit_MapReset.dylib; see tests/test_MapReset.py): src/MapReset.cpp
// (LLCC_ResetMap, the two cell resets, HFieldCellStatus, RemoveNeighbour, ModifyDirection), src/ChunkFile.cpp (the chunk file reader,
// its two handler classes, the list members and CMap::LoadAttribMap) and src/ScriptCheckTable.cpp, all real.
//
// Everything they call outside those files is a fake that records through calllog: the three untranslated cell members
// (CalculateNeighbours, SetHFieldTile, ModifyType; each records the cell and a hash of its bytes at the call, and may write into
// the cell to show a later store), TPFile_Open, the file object (GetSize, Close and Read, answering the case's queues), LbMem_Alloc
// and LbMem_Free, DEBUG_MESSAGE and CExitError::SetError. Operator new and delete are counted and recorded by calllog.cpp, which this
// file includes (the list nodes, the reader and the check table are heap objects).
//
// The world is raw storage of a CWorld's size (nothing constructs it): the cell arrays are at the Mac's offsets, so the test loads and
// dumps the bytes it likes. The cells a single-cell test uses are map cell 3 and track cell 3 of that storage, labelled 1 and 2; any
// other cell is reported by its index (track cells from 0x10000).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
bool ObserveRelease(void* block, bool array);
}  // namespace
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_RELEASE_OBSERVER ObserveRelease
#define TPW_CALLLOG_EVENTS 52000   // LLCC_ResetMap over the whole map: up to three fake calls for each of 16,384 places
#define TPW_CALLLOG_LABELS 512
#include "calllog.cpp"

#include "CellBase.h"
#include "ChunkFile.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "LevelSystems.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "fake_file_slots.h"
#include "m3d2.h"
#include "platform/arena.h"

using calllog::ev;
using calllog::forget;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

// ------------------------------------------------------------------ the main balance (raw storage under the symbol's name)
#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};

// ------------------------------------------------------------------ globals of other files
CWorld* gGameWorld;
tag_sM3D2Header* gHeightfieldHeader;
uint8_t gLLCC4818;
CExitError gExitError;

namespace {

constexpr uint32_t kBufferCap = 0x4000;    // the most bytes a fake buffer holds (and a fake read fills)
alignas(16) unsigned char gWorldBytes[sizeof(CWorld)];
uint8_t gFileBytes[kBufferCap];            // what the case's file holds
uint32_t gFileLength;
uint8_t* gBuffer;                          // the last block LbMem_Alloc made, and its capacity
uint32_t gBufferCapacity;
CMapCell* gSingleMap;                      // map cell 3 and track cell 3 of the world, labelled 1 and 2
CTrackCell* gSingleTrack;

CWorld* World() { return reinterpret_cast<CWorld*>(gWorldBytes); }

// The heap traffic is part of the log: the kind of each block (sizes differ from the Mac's, except for arrays).
void ObserveAllocation(std::size_t size, bool array, void*) {
    if (array) {
        ev("new[]", {static_cast<int64_t>(size)});
    } else {
        ev("new");
    }
}
bool ObserveRelease(void* block, bool array) {
    if (block != nullptr) ev(array ? "delete[]" : "delete");
    return false;
}

// A cell in the shape both sides report: its label when it has one (the single cells), else its index in the world's arrays.
int64_t CellId(const void* cell) {
    const int32_t label = lab(cell);
    if (label != -1) return label;
    const uint8_t* const a = static_cast<const uint8_t*>(cell);
    const uint8_t* const map = gWorldBytes + 0x2d8;
    const uint8_t* const track = gWorldBytes + 0x1102d8;
    if (a >= map && a < map + 0x110000) return (a - map) / 0x44;
    if (a >= track && a < track + 0xa0000) return 0x10000 + (a - track) / 0x28;
    return -1;
}

// FNV-1a over the cell's bytes (the Python side hashes the same bytes re-packed little endian), cut to 62 bits.
int64_t Snapshot(const void* cell, std::size_t size) {
    uint64_t hash = 1469598103934665603ull;
    const uint8_t* bytes = static_cast<const uint8_t*>(cell);
    for (std::size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
    return static_cast<int64_t>(hash & 0x3fffffffffffffffull);
}

// ---- the file: GetSize, Close and Read record and answer the queues "size", "read" (default: the case's length and the bytes asked)
class FakeFile : public bfl::FileSystem::CFile {
public:
    int32_t GetSize() override {
        ev("CFile::GetSize", {lab(this)});
        return static_cast<int32_t>(ret("size", static_cast<int32_t>(gFileLength)));
    }
    void Close() override { ev("CFile::Close", {lab(this)}); }
    int32_t Read(void* dest, uint32_t size) override {
        ev("CFile::Read", {lab(this), lab(dest), size});
        if (dest != nullptr) std::memcpy(dest, gFileBytes, std::min<uint32_t>({size, gFileLength, gBufferCapacity}));
        return static_cast<int32_t>(ret("read", static_cast<int32_t>(size)));
    }
};
FakeFile gFile;

// ---- the handlers a case adds to a reader: each claims the ids of its list
struct FakeHandler : public CIndividualChunkHandler {
    FakeHandler() : CIndividualChunkHandler(0) {}
    bool IsThisMyChunk(uint32_t chunkId) override {
        bool claimed = false;
        for (int32_t i = 0; i < mClaimCount; ++i) claimed = claimed || mClaims[i] == chunkId;
        ev("IsThisMyChunk", {lab(this), chunkId, claimed});
        return claimed;
    }
    void ParseChunk(void* data, uint32_t size) override {
        const int64_t offset = gBuffer != nullptr ? static_cast<uint8_t*>(data) - gBuffer : -1;
        uint32_t words[2] = {0, 0};
        if (offset >= 0 && offset + 8 <= static_cast<int64_t>(gBufferCapacity)) std::memcpy(words, data, 8);
        ev("ParseChunk", {lab(this), offset, size, words[0], words[1]});
    }
    uint32_t mClaims[4] = {};
    int32_t mClaimCount = 0;
};
FakeHandler gHandlers[4];

// The reader (or list) a case works on; made by tpw_mr_object or tpw_mr_storage.
void* gObject;
alignas(16) unsigned char gObjectStorage[256];

using HandlerList = TbTList<CIndividualChunkHandler*, CIndividualChunkHandler*>;

}  // namespace

// ------------------------------------------------------------------ the file system and the allocator (fakes)

bfl::FileSystem::CFile::CFile() : mImplementation(nullptr) {}
bfl::FileSystem::CFile::~CFile() {}
void bfl::FileSystem::CFile::Close() { ev("CFile::Close (base)", {lab(this)}); }
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t bfl::FileSystem::CFile::GetSize() {
    ev("CFile::GetSize (base)", {lab(this)});
    return -1;
}

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    return ret("open", 0) != 0 ? &gFile : nullptr;
}

void* LbMem_Alloc(uint32_t size) {
    if (ret("alloc", 1) == 0) {
        ev("LbMem_Alloc", {size, 0});
        return nullptr;
    }
    gBufferCapacity = std::min<uint32_t>(size, kBufferCap) + 0x100;
    gBuffer = static_cast<uint8_t*>(std::calloc(1, gBufferCapacity));
    ev("LbMem_Alloc", {size, newlab(gBuffer)});
    return gBuffer;
}

void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    forget(block);
    std::free(block);
}

void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {flags}, {message}); }

void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    ev("SetError", {this == &gExitError ? 1 : 0, static_cast<int64_t>(type), value}, {text});
}

// ------------------------------------------------------------------ the untranslated cell members (stand-ins in the game)

// Each records the cell and its bytes at the call. The queues "cn flags" / "cn type" / "mt type" make the callee write the cell (a
// negative or absent answer writes nothing), which shows a store that Reset makes before or after the call.
void CMapCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b, bool flag) {
    ev("CalculateNeighbours", {CellId(this), static_cast<int32_t>(mode), a, b, flag, Snapshot(this, sizeof(CMapCell))});
    const int64_t flags = ret("cn flags", -1);
    if (flags >= 0) mFlags = static_cast<uint16_t>(flags);
    const int64_t type = ret("cn type", -1);
    if (type >= 0) mType = static_cast<CELL_MODES>(type);
}
void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {CellId(this), Snapshot(this, sizeof(CMapCell))}); }
void CTrackCell::ModifyType(CELL_MODES mode) {
    ev("ModifyType", {CellId(this), static_cast<int32_t>(mode), Snapshot(this, sizeof(CTrackCell))});
    const int64_t type = ret("mt type", -1);
    if (type >= 0) mType = static_cast<CELL_MODES>(type);
}

// ------------------------------------------------------------------ test entry points

extern "C" {

// ---- the world and the cells
void tpw_mr_world_setup() {
    gGameWorld = World();
    gSingleMap = &World()->mMapCells[3];
    gSingleTrack = &World()->mTrackCells[3];
}
void tpw_mr_load(uint32_t offset, const uint8_t* bytes, uint32_t n) { std::memcpy(gWorldBytes + offset, bytes, n); }
void tpw_mr_dump(uint32_t offset, uint8_t* out, uint32_t n) { std::memcpy(out, gWorldBytes + offset, n); }
void* tpw_mr_addr(const char* name) {
    if (std::strcmp(name, "map1") == 0) return gSingleMap;
    if (std::strcmp(name, "track1") == 0) return gSingleTrack;
    if (std::strcmp(name, "file") == 0) return &gFile;
    if (std::strcmp(name, "object") == 0) return gObject;
    if (name[0] == 'h' && name[1] >= '0' && name[1] <= '3') return &gHandlers[name[1] - '0'];
    return nullptr;
}
// The offsets of the members the Mac's layout names, in the order of the test module's table.
void tpw_mr_layout(int32_t* out) {
    alignas(16) static unsigned char storage[sizeof(CMapCell)];   // (the constructor is not part of this unit)
    CMapCell& cell = *reinterpret_cast<CMapCell*>(storage);
    CTrackCell& t = *reinterpret_cast<CTrackCell*>(storage);
    const uint8_t* b = storage;
    auto off = [&](const void* p) { return static_cast<int32_t>(static_cast<const uint8_t*>(p) - b); };
    int32_t i = 0;
    out[i++] = off(&cell.mCellId);
    out[i++] = off(&cell.mKind);
    out[i++] = off(&cell.mMeshInstance);
    out[i++] = off(&cell.mType);
    out[i++] = off(&cell.mNeighbours);
    out[i++] = off(&cell.mDirection);
    out[i++] = off(&cell.mFlags);
    out[i++] = off(&cell.mParentID);
    out[i++] = off(&cell.mTileData[0]);
    out[i++] = off(&cell.mTileData[2]);
    out[i++] = off(&cell.mOverlapCounter);
    out[i++] = off(&cell.mHoardingNeighbours);
    out[i++] = off(&cell.mWho);
    out[i++] = off(&cell.mStatusFlags);
    out[i++] = off(&cell.mF3C);
    out[i++] = off(&t.mSegmentNumber);
    out[i++] = static_cast<int32_t>(sizeof(CMapCell));
    out[i++] = static_cast<int32_t>(sizeof(CTrackCell));
}

// ---- the height field and the globals
void tpw_mr_set_globals(uint32_t flag, int32_t xStart, int32_t yStart) {
    gLLCC4818 = static_cast<uint8_t>(flag);
    CMainBalance* const balance = reinterpret_cast<CMainBalance*>(gBalanceStorage);
    balance->mValues[335].mInteger = xStart;
    balance->mValues[336].mInteger = yStart;
}
uint32_t tpw_mr_get_flag() { return gLLCC4818; }
// mode 0: no header; 1: a header without a field; 2: a header with a field of the given size and `count` quads (two half words each).
void tpw_mr_field(int32_t mode, uint32_t width, uint32_t height, int32_t count, const uint16_t* halves) {
    if (gHeightfieldHeader != nullptr) {
        if (gHeightfieldHeader->mHField != nullptr) {
            sHField* const field = gHeightfieldHeader->mHField;
            if (field->mQuads != nullptr) tpw::ArenaFree(field->mQuads.get());
            tpw::ArenaFree(field);
        }
        tpw::ArenaFree(gHeightfieldHeader);
        gHeightfieldHeader = nullptr;
    }
    if (mode == 0) return;
    gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    if (mode == 1) return;
    sHField* const field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    field->mF18[0] = width;
    field->mF18[1] = height;
    sHFieldQuad* const quads = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(sizeof(sHFieldQuad) * (count > 0 ? count : 1)));
    for (int32_t i = 0; i < count; ++i) {
        quads[i].mF00 = halves[2 * i];
        quads[i].mF02 = halves[2 * i + 1];
    }
    field->mQuads = quads;
    gHeightfieldHeader->mHField = field;
}

// ---- the file and the handlers
void tpw_mr_set_file(const uint8_t* bytes, uint32_t n) {
    gFileLength = std::min(n, kBufferCap);
    std::memcpy(gFileBytes, bytes, gFileLength);
}
void tpw_mr_set_claims(int32_t index, const uint32_t* ids, int32_t n) {
    gHandlers[index].mClaimCount = n;
    for (int32_t i = 0; i < n && i < 4; ++i) gHandlers[index].mClaims[i] = ids[i];
}

// Makes the object a case works on (recording is off): kind 0 a bare list, 1 a reader, both on the heap (a destructor with a
// positive flag deletes them; the others leave the memory, which the library never gives back). The handlers named by `indexes` are
// added in order, as AddChunkHandler / AddHead would.
void* tpw_mr_object(int32_t kind, uint32_t magic, const int32_t* indexes, int32_t n) {
    if (kind == 1) {
        gObject = new CChunkFileReader(magic);
    } else {
        gObject = new HandlerList;
    }
    for (int32_t i = 0; i < n; ++i) static_cast<HandlerList*>(gObject)->AddHead(&gHandlers[indexes[i]]);
    return gObject;
}
// The block the list constructor is run on (op 11), filled with one byte first.
void* tpw_mr_storage(int32_t fill) {
    std::memset(gObjectStorage, fill, sizeof gObjectStorage);
    gObject = gObjectStorage;
    return gObject;
}
// After op 11 (the list constructor): the head, the tail and the count; and whether the bytes the list object does not cover (up to
// the 0x40 the Mac side checks) are still the fill (bit 0) and the vtable pointer is set (bit 1).
void tpw_mr_storage_words(uint64_t* out) {
    const HandlerList* const list = reinterpret_cast<const HandlerList*>(gObjectStorage);
    out[0] = reinterpret_cast<uintptr_t>(list->mHead);
    out[1] = reinterpret_cast<uintptr_t>(list->mTail);
    out[2] = static_cast<uint32_t>(list->mCount);
}
int32_t tpw_mr_storage_state(int32_t fill) {
    int32_t state = 1;
    for (std::size_t i = sizeof(HandlerList); i < 0x40; ++i)
        if (gObjectStorage[i] != static_cast<uint8_t>(fill)) state = 0;
    void* vptr;
    std::memcpy(&vptr, gObjectStorage, sizeof vptr);
    return state | (vptr != nullptr ? 2 : 0);
}
// The list as a case sees it: out[0] the count, out[1] the number of nodes reached from the head, then for each node (up to 16) the
// label of its handler and the index of its previous node (-1: none), and last the index of the tail node (-1: none).
void tpw_mr_list(void* list, int64_t* out) {
    HandlerList* const l = static_cast<HandlerList*>(list);
    out[0] = l->mCount;
    int64_t n = 0;
    HandlerList::CNode* nodes[16] = {};
    for (HandlerList::CNode* node = l->mHead; node != nullptr && n < 16; node = node->mNext) nodes[n++] = node;
    out[1] = n;
    for (int64_t i = 0; i < n; ++i) {
        out[2 + 2 * i] = lab(nodes[i]->mData);
        int64_t prev = -1;
        for (int64_t k = 0; k < n; ++k)
            if (nodes[k] == nodes[i]->mPrev) prev = k;
        out[3 + 2 * i] = nodes[i]->mPrev == nullptr ? -1 : prev;
    }
    int64_t tail = -1;
    for (int64_t k = 0; k < n; ++k)
        if (nodes[k] == l->mTail) tail = k;
    out[2 + 2 * n] = l->mTail == nullptr ? -1 : tail;
}
uint32_t tpw_mr_magic() { return static_cast<CChunkFileReader*>(gObject)->mMagic; }

// ---- the script check table
void tpw_mr_script(const uint32_t* words, int32_t extraBytes, int32_t wordCount, const uint32_t* commands, int32_t commandCount,
                   uint32_t first) {
    static uint32_t wordStore[4096];
    static uint32_t commandStore[256];
    std::memcpy(wordStore, words, sizeof(uint32_t) * wordCount);
    std::memcpy(commandStore, commands, sizeof(uint32_t) * commandCount);
    commandStore[commandCount] = 0;
    gScriptCodeWords = wordStore;
    gScriptCodeWordsEnd = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(wordStore) + 4 * wordCount + extraBytes);
    gScriptCommandList = commandStore;
    gScriptCommandFirst = first;
}
// A table the case starts with (SCR_InitCheckTable frees it): `present` makes a heap block of 4 bytes, else none.
void tpw_mr_table_set(int32_t present) {
    gScriptCheckTable = present != 0 ? new uint8_t[4] : nullptr;
}
int32_t tpw_mr_table_get(uint8_t* out, int32_t n) {
    if (gScriptCheckTable == nullptr) return 0;
    std::memcpy(out, gScriptCheckTable, n);
    return 1;
}

// ---- the calls
int64_t tpw_mr_call(int32_t op, int64_t a, int64_t b, int64_t c) {
    switch (op) {
    case 0: LLCC_ResetMap(static_cast<int32_t>(a), static_cast<int32_t>(b)); return 0;
    case 1: gSingleMap->Reset(a != 0); return 0;
    case 2: gSingleTrack->Reset(); return 0;
    case 3: return HFieldCellStatus(static_cast<int32_t>(a), static_cast<int32_t>(b));
    // b selects the cell: 0 the map cell, 1 the track cell (both are CCellBase).
    case 4: (b != 0 ? static_cast<CCellBase*>(gSingleTrack) : gSingleMap)->RemoveNeighbour(static_cast<uint8_t>(a)); return 0;
    case 5: (b != 0 ? static_cast<CCellBase*>(gSingleTrack) : gSingleMap)->ModifyDirection(static_cast<uint8_t>(a)); return 0;
    case 6: return World()->GetMap()->LoadAttribMap(reinterpret_cast<const char*>(a));
    case 7: static_cast<CChunkFileReader*>(gObject)->AddChunkHandler(&gHandlers[a]); return 0;
    case 8: return static_cast<CChunkFileReader*>(gObject)->ReadFile(reinterpret_cast<const char*>(a));
    case 9: return static_cast<CChunkFileReader*>(gObject)->ReadFile(gFile, reinterpret_cast<char*>(a));
    case 10: {
        // ~CChunkFileReader(flag): the Mac code does nothing for a NULL this and deletes the object for a positive flag.
        CChunkFileReader* const reader = a != 0 ? static_cast<CChunkFileReader*>(gObject) : nullptr;
        if (reader != nullptr) {
            if (static_cast<int16_t>(b) > 0) {
                delete reader;
            } else {
                reader->~CChunkFileReader();
            }
        }
        return 0;
    }
    case 11: new (gObjectStorage) HandlerList; return 0;
    case 12: {
        HandlerList* const list = a != 0 ? static_cast<HandlerList*>(gObject) : nullptr;
        if (list != nullptr) {
            if (static_cast<int16_t>(b) > 0) {
                delete list;
            } else {
                list->~HandlerList();
            }
        }
        return 0;
    }
    case 13: static_cast<HandlerList*>(gObject)->AddHead(&gHandlers[a]); return 0;
    case 14: static_cast<HandlerList*>(gObject)->RemoveAll(); return 0;
    case 15: SCR_InitCheckTable(); return 0;
    case 16: SCR_CloseCheckTable(); return 0;
    case 17: return gHandlers[0].CIndividualChunkHandler::IsThisMyChunk(static_cast<uint32_t>(a));
    case 18: {
        CMapChunkHandler handler(static_cast<uint32_t>(b), reinterpret_cast<CMap*>(gWorldBytes + 0x2d8));
        handler.ParseChunk(reinterpret_cast<void*>(a), static_cast<uint32_t>(c));
        return 0;
    }
    default: return -1;
    }
}

// The handler object test 17 asks: its id.
void tpw_mr_set_handler_id(int32_t index, uint32_t id) { gHandlers[index].mChunkId = id; }

}  // extern "C"
