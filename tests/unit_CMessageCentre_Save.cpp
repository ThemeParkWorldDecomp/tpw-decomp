// Test library for CMessageCentre::Save, Load and UnsubscribeAll (unit v5760; libtpw_unit_CMessageCentre_Save.dylib,
// tests/test_CMessageCentre_Save.py), main app:
//   src/ThingLoop_CMessageCentre_Save.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: LbFile_Read,
// LbFile_Write, CSubscriptionHeader::Save and Load, the subscription tree's clear, find and erase, CThing::GetThingId.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CMessageCentre.h"
#include "LbFile.h"
#include "MslSubscriptionTree.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

namespace {

CMessageCentre* gCentre = nullptr;
MslRedBlackTree::node_base gFoundNode;   // what the fake find answers when the id is "found" (never the end node)

int64_t IndexOf(const void* header) {
    return (reinterpret_cast<const char*>(header) - reinterpret_cast<const char*>(gCentre)) / 16;
}

}  // namespace

// ---- the fakes
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    ev("Write", {lab(file), static_cast<int64_t>(length), transferred != nullptr ? 1 : 0, b[0], b[1], b[2], b[3]});
    return TbError(0);
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("Read", {lab(file), static_cast<int64_t>(length), buffer != nullptr ? 1 : 0, transferred != nullptr ? 1 : 0});
    return TbError(0);
}

int32_t CSubscriptionHeader::Save(void*& file) const {
    ev("HSave", {IndexOf(this), lab(file)});
    return static_cast<int32_t>(ret("HSave"));
}

int32_t CSubscriptionHeader::Load(void*& file) {
    ev("HLoad", {IndexOf(this), lab(file)});
    return static_cast<int32_t>(ret("HLoad"));
}

void SubscriptionTree::clear() { ev("Clear", {IndexOf(this)}); }

SubscriptionTree::node_base* SubscriptionTree::find(const CSubscription& key) {
    ev("Find", {IndexOf(this), key.mThingId});
    return ret("Find") != 0 ? &gFoundNode : end();
}

void SubscriptionTree::erase(node_base* position) { ev("Erase", {IndexOf(this), position == &gFoundNode ? 1 : 0}); }


tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("Id"));
    return id;
}

extern "C" {

uint64_t tpw_cmc_addr(const char*) { return 0; }

uint64_t tpw_cmc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cmc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    return n;
}

int32_t tpw_cmc_save(uint64_t centre, uint64_t file) {
    gCentre = reinterpret_cast<CMessageCentre*>(centre);
    void* f = reinterpret_cast<void*>(file);
    return gCentre->Save(f);
}

int32_t tpw_cmc_load(uint64_t centre, uint64_t file) {
    gCentre = reinterpret_cast<CMessageCentre*>(centre);
    void* f = reinterpret_cast<void*>(file);
    return gCentre->Load(f);
}

int32_t tpw_cmc_unsub(uint64_t centre, uint64_t thing) {
    gCentre = reinterpret_cast<CMessageCentre*>(centre);
    return gCentre->UnsubscribeAll(*reinterpret_cast<const CThing*>(thing));
}

}  // extern "C"
