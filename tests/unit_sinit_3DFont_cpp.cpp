// Test library for the 3DFont.cpp static initialiser (unit v5109; libtpw_unit_sinit_3DFont_cpp.dylib), main app:
//   src/game/ui/3DFont.cpp   __sinit_3DFont_cpp and its storage
//
// Fakes: CLight::CLight, SetNormal, SetColor, __register_global_object, each recording what the PowerPC stub records.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Light.h"

using namespace calllog;

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
}  // namespace

CMapLinkBase::~CMapLinkBase() {}
CLight::CLight() { ev("CLight", {lab(this), 0}); }
CLight::~CLight() {}
int32_t CLight::Prepare() { return 0; }
int32_t CLight::GetLinkType() { return 0; }
void* CLight::GetData() { return nullptr; }
void CLight::SetNormal(float x, float y, float z) {
    ev("SetNormal", {lab(this), CanonBits(x), CanonBits(y), CanonBits(z)});
}
void CLight::SetColor(float a, float r, float g, float b) {
    ev("SetColor", {lab(this), CanonBits(a), CanonBits(r), CanonBits(g), CanonBits(b)});
}
void UIMain_DestroyLight(void*) {}
extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"global", gSinit3DFontLight},
        {"node", gSinit3DFontNode},
        {"dtor", reinterpret_cast<const void*>(&UIMain_DestroyLight)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Where the CLight part and its mFlags sit in a CLightParallel on the host.
int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    CLightParallel probe;
    CLight* light = &probe;
    out[n++] = reinterpret_cast<char*>(light) - reinterpret_cast<char*>(&probe);
    out[n++] = reinterpret_cast<char*>(&probe.mFlags) - reinterpret_cast<char*>(light);
    return n;
}

void tpw_sinit_run() { __sinit_3DFont_cpp(); }

}  // extern "C"
