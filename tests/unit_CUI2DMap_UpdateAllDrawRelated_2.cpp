// Test library for CUI2DMap::UpdateAllDrawRelated (unit v5965; libtpw_unit_CUI2DMap_UpdateAllDrawRelated_2.dylib,
// tests/test_CUI2DMap_UpdateAllDrawRelated_2.py), main app:
//   src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_UpdateAllDrawRelated_2.cpp
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs): the CDraw
// constructor, SetCurrent and destructor, GAME_ASSERTMSG, CObject's three queries, UI2DMReturnScaleColour, DrawCoasterTracks,
// CUI2DMapSprite::UpdateVisibilityAndColour and the render context slots (an implementation of CUI2DMapDrawOps).
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "ThingReceivers.h"
#include "TbTList.h"
#include "World.h"
#include "ui_main.h"
#include "InterfaceWindow.h"

using namespace calllog;

// ---- globals of other files the unit reads
CThingArrayEntry gThingArray[kThingArraySize];
CUI2DMapColourEntry gUI2DMapColours[23];

namespace {

bfl::draw::CDraw* gDraw = nullptr;
uint64_t gMapBase = 0;
bool gUseCtx = false;

using SpriteList = TbTList<CUI2DMapSprite*, CUI2DMapSprite*>;

uint32_t HashWord(uint32_t index, uint32_t i) { return index * 0x9E3779B1u + i * 0x85EBCA6Bu + 0x1234567u; }

struct FakeOps : CUI2DMapDrawOps {
    void Colour(const void* colour) {
        uint32_t w[9];
        std::memcpy(w, colour, sizeof(w));
        ev("col", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8]});
    }
    void Line(const char* name, const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) {
        ev(name, {1, viewPort == &gDraw->mViewPort ? 1 : 0, clip == &gDraw->mClipRect ? 1 : 0, flags, x, y, length});
        Colour(colour);
    }
    void Rect(const char* name, const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) {
        ev(name, {1, viewPort == &gDraw->mViewPort ? 1 : 0, clip == &gDraw->mClipRect ? 1 : 0, flags, rect[0], rect[1], rect[2], rect[3]});
        Colour(colour);
    }
    void Slot10(const void* v, const void* c, int32_t f, int32_t x, int32_t y, int32_t l, const void* col) override { Line("s10", v, c, f, x, y, l, col); }
    void Slot14(const void* v, const void* c, int32_t f, int32_t x, int32_t y, int32_t l, const void* col) override { Line("s14", v, c, f, x, y, l, col); }
    void Slot24(const void* v, const void* c, int32_t f, const int32_t* r, const void* col) override { Rect("s24", v, c, f, r, col); }
    void Slot28(const void* v, const void* c, int32_t f, const int32_t* r, const void* col) override { Rect("s28", v, c, f, r, col); }
};

FakeOps gOps;

template <class T>
size_t Ofs(size_t (*)(), T*) { return 0; }

}  // namespace

#define OFS(T, m) (reinterpret_cast<size_t>(&reinterpret_cast<T*>(0x1000)->m) - 0x1000)

// ---- the fakes
TbRenderArea::TbRenderArea() {}   // the CDraw fake's members need it (the real one is another unit's)

namespace bfl {
namespace draw {
CDraw::CDraw() {
    gDraw = this;
    mRenderContext = nullptr;
    ev("draw_new", {});
}
CDraw::~CDraw() { ev("draw_del", {}); }
void CDraw::SetCurrent(TbRenderArea* area) {
    // the host map's render area is at CUI2DMap::kHostAreaOffset, the Mac's at 0x2c; the two are swapped so that
    // a host build drawing into the Mac words at 0x2c shows up as a mismatch
    const int64_t host = static_cast<int64_t>(CUI2DMap::kHostAreaOffset);
    const int64_t offset = static_cast<int64_t>(reinterpret_cast<uint64_t>(area) - gMapBase);
    ev("setcur", {offset == host ? 0x2c : offset == 0x2c ? host : offset});
    mRenderContext = gUseCtx ? static_cast<void*>(static_cast<CUI2DMapDrawOps*>(&gOps)) : nullptr;
}
}  // namespace draw
}  // namespace bfl

// The context the SetCurrent fake sets is a CUI2DMapDrawOps already (the host's adapter is not linked here).
CUI2DMapDrawOps* UI2DMapDrawOps(void* context) { return static_cast<CUI2DMapDrawOps*>(context); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t sum = 0;
    for (const char* p = format; *p; p++) sum += static_cast<uint8_t>(*p);
    ev("assert", {condition & 0xff, sum & 0xffff});
}

static uint32_t ObjId(const void* object) {
    uint32_t id;
    std::memcpy(&id, object, 4);
    return id;
}

uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("exc", {ObjId(this), flag ? 1 : 0});
    return static_cast<uint8_t>(ret("exc"));
}

int32_t CObject::GetCustomerSatisfaction() const {
    ev("sat", {ObjId(this)});
    return static_cast<int32_t>(ret("sat"));
}

bool CObject::IsClosed() const {
    ev("closed", {ObjId(this)});
    return ret("closed") != 0;
}

bfl::draw::CColour UI2DMReturnScaleColour(int32_t index, const bfl::draw::CColour& a, const bfl::draw::CColour& b, const bfl::draw::CColour& c) {
    uint32_t wa, wb, wc;
    std::memcpy(&wa, &a, 4);
    std::memcpy(&wb, &b, 4);
    std::memcpy(&wc, &c, 4);
    ev("scale", {index, wa, wb, wc});
    uint32_t w[9];
    for (uint32_t i = 0; i < 9; i++) w[i] = HashWord(static_cast<uint32_t>(index), i);
    bfl::draw::CColour out;
    static_assert(sizeof(out) == sizeof(w), "CColour is nine words");
    std::memcpy(&out, w, sizeof(w));
    return out;
}

void CUI2DMap::DrawCoasterTracks() { ev("tracks", {static_cast<int64_t>(reinterpret_cast<uint64_t>(this) - gMapBase)}); }

void CUI2DMapSprite::UpdateVisibilityAndColour(int32_t mask, int32_t second, int32_t third) {
    ev("uvc", {ObjId(this), mask, second, third});
}

extern "C" {

uint64_t tpw_ui2dm_addr(const char* name) {
    if (std::strcmp(name, "things") == 0) return reinterpret_cast<uintptr_t>(gThingArray);
    if (std::strcmp(name, "colours") == 0) return reinterpret_cast<uintptr_t>(gUI2DMapColours);
    return 0;
}

uint64_t tpw_ui2dm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ui2dm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(OFS(CObject, mF40));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(OFS(CUI2DMapSprite, mKind));
    out[n++] = static_cast<int64_t>(OFS(CUI2DMapSprite, mMode));
    out[n++] = static_cast<int64_t>(OFS(CUI2DMapSprite, mMatch));
    out[n++] = static_cast<int64_t>(OFS(CUI2DMapSprite, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    out[n++] = static_cast<int64_t>(OFS(SpriteList, mHead));
    out[n++] = static_cast<int64_t>(OFS(SpriteList::CNode, mNext));
    out[n++] = static_cast<int64_t>(sizeof(SpriteList));
    out[n++] = static_cast<int64_t>(sizeof(SpriteList::CNode));
    return n;
}

void tpw_ui2dm_run(uint64_t map, int32_t useCtx) {
    gMapBase = map;
    gUseCtx = useCtx != 0;
    reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->UpdateAllDrawRelated();
}

}  // extern "C"
