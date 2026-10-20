// Test library for CSubscriptionHeader::Save and ::Load (unit v5729; libtpw_unit_CSubscriptionHeader_Load.dylib,
// tests/test_CSubscriptionHeader_Load.py), main app:
//   src/MessageCentre_post_CSubscriptionHeader_Load.cpp   the two functions
//
// The callees outside the unit are fakes that record what the real ones read: LbFile_Read / LbFile_Write, CSubscription::Load / Save and
// SubscriptionTree::insert_one. The tree walk (begin, increment) is the program's own code, linked from src/.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "CSubscriptionHeader.h"
#include "LbFile.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    (void)transferred;
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), length});
    *static_cast<uint32_t*>(buffer) = static_cast<uint32_t>(ret("LbFile_Read"));
    return TbError(0);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    (void)transferred;
    ev("LbFile_Write", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), length, *static_cast<const uint32_t*>(buffer)});
    return TbError(0);
}

void CSubscription::Load(void*& file) {
    ev("CSub_Load", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))});
    mThingId = static_cast<uint16_t>(ret("CSub_Load"));
}

void CSubscription::Save(void*& file) const {
    ev("CSub_Save", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), mThingId});
}

SubscriptionInsertResult SubscriptionTree::insert_one(const CSubscription& key) {
    ev("insert_one", {lab(this), key.mThingId});
    SubscriptionInsertResult result;
    result.mNode = nullptr;
    result.mInserted = ret("insert_one") != 0;
    return result;
}

extern "C" {

uint64_t tpw_csh_addr(const char* name) {
    (void)name;
    return 0;
}

uint64_t tpw_csh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Host layout: count offset, root offset, node value offset, node size.
int32_t tpw_csh_layout(int64_t* out) {
    int32_t n = 0;
    SubscriptionTree::node node;
    out[n++] = static_cast<int64_t>(offsetof(SubscriptionTree, mCount));
    out[n++] = static_cast<int64_t>(offsetof(SubscriptionTree, mRoot));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&node.mValue) - reinterpret_cast<char*>(&node));
    out[n++] = static_cast<int64_t>(sizeof(CSubscriptionHeader));
    return n;
}

uint32_t tpw_csh_save(uint64_t header, uint64_t slot) {
    return static_cast<uint32_t>(reinterpret_cast<CSubscriptionHeader*>(header)->Save(*reinterpret_cast<void**>(slot)));
}

uint32_t tpw_csh_load(uint64_t header, uint64_t slot) {
    return static_cast<uint32_t>(reinterpret_cast<CSubscriptionHeader*>(header)->Load(*reinterpret_cast<void**>(slot)));
}

}  // extern "C"
