// Test library for sAdvisorStore::~sAdvisorStore (unit v5704; libtpw_unit_sAdvisorStore_Dtor_sAdvisorStore.dylib), main app:
//   src/TPAdvisor_model_sAdvisorStore_Dtor_sAdvisorStore.cpp
//
// The delete the original calls is recorded as the event "delete"; the CLight destructor chain the original inlines
// is faked as empty here (the host compiler calls it, the original inlines its vtable stores).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPAdvisor.h"

using namespace calllog;

CMapLinkBase::~CMapLinkBase() {}
CLight::~CLight() {}
int32_t CLight::Prepare() { return 0; }
int32_t CLight::GetLinkType() { return 1; }
void* CLight::GetData() { return nullptr; }

extern "C" {

uint64_t tpw_sas_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The destructor as the PowerPC call sees it: this, then the hidden short flag; a positive flag also deletes.
uint32_t tpw_sas_dtor(uint64_t object, int32_t flag) {
    sAdvisorStore* p = reinterpret_cast<sAdvisorStore*>(static_cast<uintptr_t>(object));
    if (p) {
        p->~sAdvisorStore();
        if (static_cast<int16_t>(flag) > 0) ev("delete", {lab(p)});
    }
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

}  // extern "C"
