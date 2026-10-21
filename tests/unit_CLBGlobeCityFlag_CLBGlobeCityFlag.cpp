// Test library for the online globe's city flag (unit v5546; libtpw_unit_CLBGlobeCityFlag_CLBGlobeCityFlag.dylib,
// tests/test_CLBGlobeCityFlag_CLBGlobeCityFlag.py), main app:
//   src/game/online/lobby_globe.cpp   the constructor and destructor, gGlobeNoCityName
//
// The lobby meshes' InstanceMeshType, the mesh instance's Show/SetDirection/SetPosition/Scale, _wcsicmp and the globe's two virtuals
// are recording fakes. Objects are static buffers so the log's labels are fixed addresses.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "lobby.h"
#include "platform/macdoze.h"

using namespace calllog;

CLBLobbyMeshes* gLobbyMeshes = nullptr;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 33u + static_cast<uint8_t>(*s);
    return h;
}

int64_t HashW(const char16_t* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 33u + static_cast<uint32_t>(*s);
    return h;
}

alignas(16) uint8_t gMeshesBuf[64];
alignas(16) uint8_t gMeshBuf[64];
alignas(16) uint8_t gFlagBuf[64];
alignas(16) uint8_t gCityBuf[0x68];
char16_t gNameBuf[40];

class FakeGlobe : public CLBGlobeFlagOwner {
public:
    void AddFlag(CLBGlobeCityFlag* flag) override { ev("vcall", {lab(this), lab(flag), 0x40}); }
    void RemoveFlag(CLBGlobeCityFlag* flag) override { ev("vcall", {lab(this), lab(flag), 0x44}); }
};
FakeGlobe gGlobe;

}  // namespace

int32_t NS_MacDoze::_wcsicmp(const char16_t* a, const char16_t* b) {
    ev("wcsicmp", {HashW(a), b != nullptr ? 1 : 0});
    return static_cast<int32_t>(ret("wcsicmp"));
}

CLBMeshInstance* CLBLobbyMeshes::InstanceMeshType(const char* folder, const char* name, int32_t overMapRange, uint32_t a,
                                                  const char16_t* sign1, const char16_t* sign2) {
    ev("InstanceMeshType", {lab(this), Hash(folder), Hash(name), overMapRange, a, lab(sign1), lab(sign2)});
    return reinterpret_cast<CLBMeshInstance*>(gMeshBuf);
}

void CLBMeshInstance::Show() { ev("Show", {lab(this)}); }

void CLBMeshInstance::SetDirection(const s3DPoint& p) {
    ev("SetDirection", {lab(this), CanonBits(p.x), CanonBits(p.y), CanonBits(p.z)});
}

void CLBMeshInstance::SetPosition(const s3DPoint& p) {
    ev("SetPosition", {lab(this), CanonBits(p.x), CanonBits(p.y), CanonBits(p.z)});
}

void CLBMeshInstance::Scale(float x, float y, float z) {
    ev("Scale", {lab(this), CanonBits(x), CanonBits(y), CanonBits(z)});
}

#pragma clang diagnostic ignored "-Winvalid-offsetof"

extern "C" {

uint64_t tpw_cf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"meshes", gMeshesBuf}, {"mesh", gMeshBuf}, {"flag", gFlagBuf}, {"city", gCityBuf},
        {"globe", &gGlobe},     {"name", gNameBuf}, {"gLobbyMeshes", &gLobbyMeshes},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

int32_t tpw_cf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLBGlobeCityFlag, mMesh));
    out[n++] = static_cast<int64_t>(offsetof(CLBGlobeCityFlag, mGlobe));
    out[n++] = static_cast<int64_t>(offsetof(CLBGlobeCityFlag, mCity));
    out[n++] = static_cast<int64_t>(sizeof(CLBGlobeCityFlag));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mName));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mValue));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mPosX));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mPosY));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mPosZ));
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlineCity));
    return n;
}

// The constructor on the static flag buffer; answers the object.
uint64_t tpw_cf_ctor() {
    auto* globe = reinterpret_cast<const CLBScreenComponentOnlineGlobe*>(static_cast<CLBGlobeFlagOwner*>(&gGlobe));
    auto* city = reinterpret_cast<const CFSOnlineCity*>(gCityBuf);
    CLBGlobeCityFlag* flag = new (gFlagBuf) CLBGlobeCityFlag(globe, city);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(flag));
}

// The host object's vptr, taken from a throwaway construction (called before the case's log starts).
static uint64_t gSavedVptr = 0;
void tpw_cf_prep() {
    if (gSavedVptr != 0) return;
    alignas(16) static uint8_t scratch[64];
    alignas(16) static uint8_t city[0x68];
    gLobbyMeshes = reinterpret_cast<CLBLobbyMeshes*>(gMeshesBuf);
    std::memset(city, 0, sizeof(city));
    reinterpret_cast<CFSOnlineCity*>(city)->mName = gNameBuf;
    CLBGlobeCityFlag* f = new (scratch) CLBGlobeCityFlag(
        reinterpret_cast<const CLBScreenComponentOnlineGlobe*>(static_cast<CLBGlobeFlagOwner*>(&gGlobe)),
        reinterpret_cast<const CFSOnlineCity*>(city));
    std::memcpy(&gSavedVptr, f, 8);
}

// The destructor as the Mac's delete-flag version does: NULL is skipped, the storage is released when flag > 0 (a fake event here).
uint64_t tpw_cf_dtor(int32_t isNull, int16_t flag) {
    if (isNull) return 0;
    auto* p = reinterpret_cast<CLBGlobeCityFlag*>(gFlagBuf);
    std::memcpy(gFlagBuf, &gSavedVptr, 8);
    p->~CLBGlobeCityFlag();
    if (flag > 0) ev("delete", {lab(p)});
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

}  // extern "C"
