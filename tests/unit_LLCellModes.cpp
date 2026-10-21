// Test library for the cell system's modes (unit w185; libtpw_unit_LLCellModes.dylib, tests/test_LLCellModes.py), main app:
//   src/game/park/LLCellSystem.cpp  the 23 functions (modes, ride words, cursor, costs, click stack, FixFirstBlock)
//   src/game/park/LLCellSystem.cpp (landed): the cost words and the click stack's count
//   src/game/Mouse.cpp       (landed) AssignMouseMode, CMMDefault: they run for real
// Every other callee is a fake that records the event the PowerPC stub records. The globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "CellBase.h"
#include "Debug.h"
#include "FS_Online.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files (src/game/park/LLCellSystem.cpp's words, the world and the recorder); gMouse and the mouse words are src/game/Mouse.cpp's
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
UIInterface gUIInterface;
uint8_t gLLCC47C4;
int32_t gLLCC47CC, gLLCC4814, gLLCC4810, gLLCC480C, gLLCC4808, gLLCC4804, gLLCC4800;
uint16_t gLLCCDE0E2;
int32_t gLLCCC208, gLLCCC204, gLLCCC200;
int32_t gLLCC47EC, gLLCC47E8, gLLCCDB6C, gLLCC47C8;

namespace {
char* gWorldStore;              // the world block (gGameWorld points to it)
CAdvisor* gAdvisorStore;        // label 30
const void* gLastRequest;       // the last request constructed
alignas(16) unsigned char gMouseStore[16];

// A fake mode (label 9): the destructor, GetMode, OnAssign and OnRelease record.
class FakeMode : public CMouseMode {
public:
    ~FakeMode() override { ev("Mode.dtor", {lab(this)}); }
    int32_t GetMode() override {
        ev("Mode.GetMode", {lab(this)});
        return static_cast<int32_t>(ret("ModeGetMode", 0));
    }
    void OnAssign() override { ev("Mode.OnAssign", {lab(this), lab(gMouseMode)}); }
    void OnRelease() override { ev("Mode.OnRelease", {lab(this), lab(gMouseMode)}); }
};

CTrackCell* TrackCell(int32_t k) {
    return reinterpret_cast<CTrackCell*>(gWorldStore + 0x1102d8 + (static_cast<int64_t>(k) - 1) * 0x28);
}
}  // namespace

CGameType::CGameType() { mType = kGameTypeNormal; }
CMMOnline::CMMOnline() { ev("CMMOnline.ctor"); }   // not reached: the game type is the normal one
CMMOnline::~CMMOnline() {}
void CMMOnline::OnLeftClick(int32_t, int32_t) {}
void CMMOnline::OnRightClick(int32_t, int32_t) {}
void CMMOnline::OnDoubleClick(int32_t, int32_t) {}
int32_t CMMOnline::GetMode() { return 7; }
void CMMOnline::OnAssign() {}

// ---- the nothrow new: the answer is the queue "New" (0 a NULL). The size is recorded as the Mac's.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = static_cast<int64_t>(size);
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(CMMDefault)) mac = 8;
    void* block = nullptr;
    if (ret("New", 1) != 0) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        newlab(block);
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}

// ---- the recorder, the advisor, the cursor, the mouse context
void CActionRecorder::RecordAction(int32_t action, uint16_t value) { ev("RecordAction", {action, value}); }
void CActionRecorder::RecordAction(int32_t action, uint16_t value, uint8_t extra) { ev("RecordAction3", {action, value, extra}); }
CAdvisor* CWorld::GetAdvisor() { ev("GetAdvisor"); return gAdvisorStore; }
int32_t CAdvisor::CancelMessage(AdvisorMessage message) { ev("CancelMessage", {lab(this), static_cast<int32_t>(message)}); return 0; }
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    gLastRequest = this;
    ev("CAdvisorRequest", {static_cast<int32_t>(message), thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
}
int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {lab(this), &request == gLastRequest ? 1 : 0});
    return 0;
}
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }
void UIInterface::CheckMouseContext(int context) { ev("CheckMouseContext", {context}); }

// ---- the world's calls
void AbortMovePylon() { ev("AbortMovePylon"); }
void LLCM_SetIllegalPlacement(int32_t value) { ev("LLCM_SetIllegalPlacement", {value}); }
int32_t LLCM_GetIllegalPlacement() { ev("LLCM_GetIllegalPlacement"); return static_cast<int32_t>(ret("GetIllegal", 0)); }
void SetCoasterEdit(void* edit, int32_t flag) { ev("SetCoasterEdit", {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(edit)), flag}); }
void CWorld::FlagItemAsSelected(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) { ev("FlagItemAsSelected", {id.mIndex}); }
int32_t CMap::IsXYValid(int32_t x, int32_t y) { ev("IsXYValid", {x, y}); return static_cast<int32_t>(ret("IsXYValid", 0)); }

// ---- the cells
int32_t CTrackCell::IsTrack() const { ev("IsTrack", {lab(this)}); return static_cast<int32_t>(ret("IsTrack", 0)); }
int32_t CTrackCell::IsTrackStartBlock() const { ev("IsTrackStartBlock", {lab(this)}); return static_cast<int32_t>(ret("IsStart", 0)); }
uint8_t CTrackCell::IsCorner() const { ev("IsCorner", {lab(this)}); return static_cast<uint8_t>(ret("IsCorner", 0)); }
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {lab(this)}); }
uint8_t CCellBase::GetNeighbours() const { ev("GetNeighbours", {lab(this)}); return static_cast<uint8_t>(ret("Neighbours", 0)); }
void CCellBase::ModifyDirection(uint8_t d) { ev("ModifyDirection", {lab(this), d}); }
void CCellBase::ModifyNeighbours(uint8_t d) { ev("ModifyNeighbours", {lab(this), d}); }
void CCellBase::RemoveNeighbour(uint8_t d) { ev("RemoveNeighbour", {lab(this), d}); }
void CCellBase::RemoveDirection(uint8_t d) { ev("RemoveDirection", {lab(this), d}); }
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("GetNeighbouringCell", {lab(this), direction, offset});
    const int32_t k = static_cast<int32_t>(ret("Cell", 0));
    return k == 0 ? nullptr : TrackCell(k);
}

// ---- OPPOSITE and ADD_DIRECTION run for real on the PowerPC side; the bodies are src/game/park/Map.cpp's (0x10102500, 0x1010254c)
uint8_t OPPOSITE(uint8_t d) {
    if (d == 0) return 0;
    if (d < 0x10) return static_cast<uint8_t>((d << 4) & 0xf0);
    return static_cast<uint8_t>((d >> 4) & 0xff);
}
uint8_t ADD_DIRECTION(uint8_t a, uint8_t b) {
    int32_t n = 0;
    int32_t first = a;
    while (static_cast<uint8_t>(first) > 1) {
        first = first >> 1;
        ++n;
    }
    const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(b) << n);
    if (v > 0x80) return static_cast<uint8_t>((v >> 8) & 0xff);
    return static_cast<uint8_t>(v & 0xff);
}

// ---- trace and assertion
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
uint64_t tpw_llm_gaddr(const char* name) {
#define G(n) if (std::strcmp(name, #n) == 0) return reinterpret_cast<uintptr_t>(&n)
    G(gLLCC4800); G(gLLCC4804); G(gLLCC4808); G(gLLCC480C); G(gLLCC4810); G(gLLCC4814); G(gLLCC47C4); G(gLLCC47C8); G(gLLCC47CC);
    G(gLLCC47D0); G(gLLCC47D4); G(gLLCC47D8); G(gLLCC47DC); G(gLLCC47E8); G(gLLCC47EC); G(gLLCCC200); G(gLLCCC204); G(gLLCCC208);
    G(gLLCCDB6C); G(gLLCCDE0E2); G(gLLCCE0DC); G(gLLCCE0E0); G(gLLCC9F14); G(gLLCC9F24); G(gLLCC9F26); G(gLLCCE288); G(gLLCCE28C);
    G(gLLCCDB58); G(gLLCCDB5A); G(gLLCCFirstBlockX); G(gLLCCFirstBlockY); G(gLLCCClickStackCount); G(gLLCCClickCopyIndex);
    G(gLLCCClickCopyCount); G(gLLCCClickStack); G(gLLCCClickStackCopy);
#undef G
    if (std::strcmp(name, "world") == 0) return reinterpret_cast<uintptr_t>(gWorldStore);
    return 0;
}

// Once per process: the world (zero, never constructed) and the advisor block.
void tpw_llm_init() {
    if (gWorldStore == nullptr) gWorldStore = static_cast<char*>(std::calloc(1, 0x1b1000));
    if (gAdvisorStore == nullptr) gAdvisorStore = static_cast<CAdvisor*>(std::calloc(1, 0x2000));
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStore);
}

// A block for a test object (the pool is reused). Label 8 is the mouse; label 30 the advisor.
uint64_t tpw_llm_obj(int32_t label, uint32_t size) {
    static void* pool[128];
    if (label == 8) { std::memset(gMouseStore, 0xa5, sizeof gMouseStore); return reinterpret_cast<uintptr_t>(gMouseStore); }
    if (label == 30) return reinterpret_cast<uintptr_t>(gAdvisorStore);
    const uint32_t want = size < 0x2000 ? 0x2000 : size;
    if (pool[label] == nullptr) pool[label] = std::calloc(1, want);
    std::memset(pool[label], 0, want);
    return reinterpret_cast<uintptr_t>(pool[label]);
}

uint64_t tpw_llm_fake_mode() { return reinterpret_cast<uintptr_t>(new FakeMode); }

// Key 1 gMouse, 2 gMouseMode, 3 and 4 the mouse words.
void tpw_llm_set(int32_t key, int64_t value) {
    switch (key) {
        case 1: gMouse = reinterpret_cast<CMouse*>(value); break;
        case 2: gMouseMode = reinterpret_cast<CMouseMode*>(value); break;
        case 3: gMouseWord1E0 = static_cast<int32_t>(value); break;
        case 4: gMouseWord1E4 = static_cast<int32_t>(value); break;
        default: break;
    }
}
void tpw_llm_dump(int64_t* out) {
    out[0] = lab(gMouse);
    out[1] = lab(gMouseMode);
    out[2] = gMouseWord1E0;
    out[3] = gMouseWord1E4;
}

int32_t tpw_llm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(LLCCClick));
    return n;
}

// ---- the functions
void tpw_llm_SetRideRotation(int32_t rotation, int32_t flag) { LLCC_SetRideRotation(rotation, flag != 0); }
int32_t tpw_llm_GetRideRotation() { return LLCC_GetRideRotation(); }
void tpw_llm_SystemSetMode(int32_t mode, int32_t flag) { LLCC_SystemSetMode(mode, flag != 0); }
void tpw_llm_SystemSetModeNR(int32_t mode, int32_t flag) { LLCC_SystemSetModeNR(mode, flag != 0); }
int32_t tpw_llm_SystemGetLastMode() { return LLCC_SystemGetLastMode(); }
int32_t tpw_llm_SystemIsMode(int32_t mode) { return LLCC_SystemIsMode(mode); }
void tpw_llm_SystemSetRide(int32_t id, int32_t track) {
    const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag{static_cast<uint16_t>(id)};
    LLCC_SystemSetRide(tag, track);
}
void tpw_llm_SystemSetCursor() { LLCC_SystemSetCursor(); }
void tpw_llm_SystemSetRideQueue(int32_t q) { LLCC_SystemSetRideQueue(q); }
int32_t tpw_llm_SystemGetRideQueue() { return LLCC_SystemGetRideQueue(); }
void tpw_llm_SystemSetRideTrack(int32_t t) { LLCC_SystemSetRideTrack(t); }
int32_t tpw_llm_SystemGetRideTrack() { return LLCC_SystemGetRideTrack(); }
int32_t tpw_llm_GetPathCost() { return LLCC_SystemGetPathCost(); }
int32_t tpw_llm_GetQueueCost() { return LLCC_SystemGetQueueCost(); }
uint32_t tpw_llm_GetKartTrackCost() { return LLCC_SystemGetKartTrackCost(); }
uint32_t tpw_llm_GetWaterTrackCost() { return LLCC_SystemGetWaterTrackCost(); }
void tpw_llm_SetSessionCost(int32_t c) { LLCC_SystemSetSessionCost(c); }
void tpw_llm_AddToSessionCost(int32_t c) { LLCC_SystemAddToSessionCost(c); }
int32_t tpw_llm_GetSessionCost() { return LLCC_SystemGetSessionCost(); }
int32_t tpw_llm_GetClickStackEntries() { return LLCC_SystemGetClickStackEntries(); }
int32_t tpw_llm_AddToClickStack(int32_t x, int32_t y) { return LLCC_SystemAddToClickStack(x, y); }
void tpw_llm_MakeClickStackCopy() { LLCC_SystemMakeClickStackCopy(); }
void tpw_llm_FixFirstBlock(int32_t flag) { LLCC_FixFirstBlock(flag != 0); }

}  // extern "C"
