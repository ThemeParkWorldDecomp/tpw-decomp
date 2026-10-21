// Test library for OnlineConfig::~OnlineConfig (unit v5368; libtpw_unit_OnlineConfig_Dtor_OnlineConfig.dylib,
// tests/test_OnlineConfig_Dtor_OnlineConfig.py), main app:
//   src/game/MslSupport.cpp   online_config_mac::Destroy
//
// operator delete (0x10238218) is a fake that records the pointer as a label.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace online_config_callee {
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
}  // namespace online_config_callee

extern "C" {

uint64_t tpw_oc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_oc_layout(int64_t* out) {
    (void)out;
    return 0;
}

uint64_t tpw_oc_dtor(uint64_t self, int32_t flag) {
    MslCharString* r = online_config_mac::Destroy(reinterpret_cast<MslCharString*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
