// Test library for TbIResourceIndex::ReleaseResource / ReleaseItem (unit v5260).
// The resource is a fake TbResource whose ManagerRelease records an event.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbIResourceIndex.h"

using namespace calllog;

// TbResource's virtuals live in other files; defined here.
int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}

namespace {
class FakeRes : public TbResource {
public:
    int32_t ManagerRelease() override {
        ev("ManagerRelease", {lab(this)});
        return 0;
    }
};
FakeRes gObj;
alignas(16) unsigned char gIndexStorage[256];
TbIResourceIndex* Index() { return reinterpret_cast<TbIResourceIndex*>(gIndexStorage); }
}  // namespace

extern "C" {

uint64_t tpw_rr_addr(const char* name) {
    if (std::strcmp(name, "obj") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gObj));
    return 0;
}

uint64_t tpw_rr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbIResourceIndex::resourceIndexItem, mResource));
    return n;
}

void tpw_rr_release_resource(uint64_t item) { Index()->ReleaseResource(reinterpret_cast<void*>(static_cast<uintptr_t>(item))); }
void tpw_rr_release_item(uint64_t item) {
    Index()->ReleaseItem(reinterpret_cast<TbIResourceIndex::resourceIndexItem*>(static_cast<uintptr_t>(item)));
}

}  // extern "C"
