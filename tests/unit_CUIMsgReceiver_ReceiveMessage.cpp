// Test library for CUIMsgReceiver::ReceiveMessage (unit v5666; libtpw_unit_CUIMsgReceiver_ReceiveMessage.dylib), main app:
//   src/game/ui/vector2d.cpp
//
// The UIInterface callees are fakes that record the arguments the real ones get. gUIInterface is defined here. The message is a test
// subclass: its GetType answers from the queue "GetType"; its fields sit where the casts of the translation read them.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "UIInterface.h"

using namespace calllog;

UIInterface gUIInterface;

namespace {

// Host layout: vptr 0, mFlags 8, then the fields the casts read (uint16 or int at 12, int at 16).
struct TestMsg : CMsg {
    uint32_t f12 = 0;
    uint32_t f16 = 0;
    int32_t GetType() const override { return static_cast<int32_t>(ret("GetType")); }
};

}  // namespace

void UIInterface::EnableResearchButton(bool on) { ev("EnableResearchButton", {lab(this), on ? 1 : 0}); }
void UIInterface::ResearchCategoryComplete(ResearchCat category) { ev("ResearchCategoryComplete", {lab(this), static_cast<int32_t>(category)}); }
void UIInterface::RideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("RideDeleted", {lab(this), id.mIndex}); }
void UIInterface::RideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("RideBrokenDown", {lab(this), id.mIndex}); }
void UIInterface::RideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("RideCondemned", {lab(this), id.mIndex}); }
void UIInterface::ThingDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("ThingDeath", {lab(this), id.mIndex}); }
void UIInterface::KidArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("KidArrived", {lab(this), id.mIndex}); }
void UIInterface::StaffRoomEvent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id, int32_t arriving) {
    ev("StaffRoomEvent", {lab(this), id.mIndex, arriving});
}

extern "C" {

uint64_t tpw_uimsg_addr(const char* name) {
    if (std::strcmp(name, "ui") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gUIInterface));
    return 0;
}

uint64_t tpw_uimsg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uimsg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = 1;
    return n;
}

// kind 1: the word at Mac +8 is a uint16 id (the high half of w8) and wc an int; kind 0: w8 is an int.
void tpw_uimsg_receive(uint32_t kind, uint32_t w8, uint32_t wc) {
    TestMsg msg;
    msg.mFlags = 0;
    msg.f12 = kind ? (w8 >> 16) : w8;
    msg.f16 = wc;
    alignas(16) static char storage[64];   // the receiver's body is never read; no constructor is linked
    reinterpret_cast<CUIMsgReceiver*>(storage)->ReceiveMessage(msg);
}

}  // extern "C"
