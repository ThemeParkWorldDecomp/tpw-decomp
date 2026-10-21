// Test library for the eight functions of unit v51049 (libtpw_unit_std_locale_Dtor_locale.dylib, tests/test_std_locale_Dtor_locale.py), main app:
//   src/game/HLSprite.cpp, src/game/TPInstance.cpp, src/game/TPPointer.cpp, src/game/advisor/Advisor.cpp   LocaleStdDestroy, BasicFilebufChar::imbue / pbackfail, DisableRideHighlight, ESP_GetRandomSpritesFrom,
//                                    ToggleProceduralTexturing, DestroyDynamicMeshSetInstance, CAdvisor::SetPlayingFor
//
// The callees outside the unit are fakes that record what the PowerPC stubs record; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "Clock.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "World.h"
#include "keytable_actions.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "tb_hfield.h"

using namespace calllog;

// ---- globals of other files (the program's definitions are in files this library does not compile)
uint32_t gGameOptions;
CWorld* gGameWorld;
// gClock (a CClock whose constructor lives in another file): raw storage under the same symbol.
alignas(16) unsigned char gClockRaw[sizeof(CClock)] __asm__("_gClock");
tag_sRideAnimSet* gHighlightedRide;
ESPSpriteGroup gESPGroups[kESPGroups];
// gPlayerSettings (a CPlayerSettings whose constructor lives in another file): raw storage under the same symbol.
alignas(16) unsigned char gPlayerSettingsRaw[0x100] __asm__("_gPlayerSettings");

namespace {
uint32_t gImp, gTmpCount, gFacet;   // what the Getloc / UseFacetCodecvt fakes hand out
inline void* P(uint64_t w) { return reinterpret_cast<void*>(static_cast<uintptr_t>(w)); }
inline void* O(uint32_t w) { return w != 0 ? tpw::gArenaBase + w : nullptr; }   // a 32-bit word of the Mac's objects: an arena offset
inline uint64_t U(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// ---- the fakes
void UnhighlightRide() { ev("Unhighlight", {}); }
void PT_Enable() { ev("PT_Enable", {}); }
void PT_Disable() { ev("PT_Disable", {}); }
void CorrectOrientationFlagsForDiffProceduralMode(int32_t procedural) { ev("Correct", {procedural}); }
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {lab(mesh)}); }
void DestroyEngineMesh(tag_sMesh* mesh) { ev("DestroyEngineMesh", {lab(mesh)}); }
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
int32_t CClock::ReadFixed() {
    ev("ReadFixed", {this == &gClock ? 1 : 0});
    return static_cast<int32_t>(ret("ReadFixed"));
}

namespace filebuf_char_callee {
void Getloc(void* out, void* streambuf) {
    ev("Getloc", {lab(streambuf)});
    uint32_t* o = static_cast<uint32_t*>(out);
    o[0] = gImp;
    o[1] = gTmpCount;
}
void* UseFacetCodecvt(void* locale) {
    const uint32_t* l = static_cast<const uint32_t*>(locale);
    ev("UseFacet", {lab(O(l[0])), lab(O(l[1]))});
    return O(gFacet);
}
int32_t CodecvtAlwaysNoconv(void* cvt) {
    ev("Noconv", {lab(cvt)});
    return static_cast<int32_t>(ret("Noconv"));
}
}  // namespace filebuf_char_callee
namespace filebuf_wclose_callee {
void LocaleImpDtor(void* impl, int32_t flag) { ev("ImpDtor", {lab(impl), flag}); }
}  // namespace filebuf_wclose_callee
namespace locale_imp_callee {
void Delete(void* p) { ev("Delete", {lab(p)}); }
}  // namespace locale_imp_callee
namespace streambuf_callee {
void LocaleAssign(void* dst, const void* src) { ev("LocaleAssign", {lab(dst), lab(src)}); }
}  // namespace streambuf_callee

extern "C" {

uint64_t tpw_loc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rideflag", &gRideHighlightDisabled}, {"hl", &gHighlightedRide}, {"groups", &gESPGroups[0]}, {"world", &gGameWorld},
        {"opts", &gGameOptions},               {"settings", gPlayerSettingsRaw},
    };
    if (std::strcmp(name, "arenabase") == 0) return U(tpw::gArenaBase);
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return U(entry.address);
    return 0;
}

uint64_t tpw_loc_alloc(uint32_t size) { return U(tpw::ArenaAllocZeroed(size)); }

// What Getloc and UseFacetCodecvt hand out in this case (arena offsets: the 32-bit words of the Mac's objects).
void tpw_loc_set(uint64_t imp, uint64_t tmpCount, uint64_t facet) {
    gImp = static_cast<uint32_t>(imp);
    gTmpCount = static_cast<uint32_t>(tmpCount);
    gFacet = static_cast<uint32_t>(facet);
}

// The host offsets and sizes the module relies on.
int32_t tpw_loc_layout(int64_t* out) {
    int32_t n = 0;
    auto* grp = &gESPGroups[0];
    out[n++] = static_cast<int64_t>(sizeof(ESPSpriteGroup));                                                // 0
    out[n++] = reinterpret_cast<const char*>(&grp->mCount) - reinterpret_cast<const char*>(grp);            // 1
    alignas(16) static char setRaw[sizeof(tag_sDMeshSet)];
    alignas(16) static char meshRaw[sizeof(tag_sMesh)];
    alignas(16) static char sharedRaw[sizeof(sDMeshShared)];
    alignas(16) static char advRaw[sizeof(CAdvisor)];
    auto* set = reinterpret_cast<tag_sDMeshSet*>(setRaw);
    auto* mesh = reinterpret_cast<tag_sMesh*>(meshRaw);
    auto* shared = reinterpret_cast<sDMeshShared*>(sharedRaw);
    auto* adv = reinterpret_cast<CAdvisor*>(advRaw);
    out[n++] = reinterpret_cast<char*>(&set->mMesh) - setRaw;             // 2
    out[n++] = reinterpret_cast<char*>(&set->mAllocation) - setRaw;       // 3
    out[n++] = reinterpret_cast<char*>(&set->mShared) - setRaw;           // 4
    out[n++] = reinterpret_cast<char*>(&mesh->mF00) - meshRaw;            // 5
    out[n++] = reinterpret_cast<char*>(&mesh->mAllocation) - meshRaw;     // 6
    out[n++] = reinterpret_cast<char*>(&shared->mInstances) - sharedRaw;  // 7
    out[n++] = static_cast<int64_t>(sizeof(CAdvisor));                    // 8
    out[n++] = reinterpret_cast<char*>(&adv->mPlayStartTime) - advRaw;    // 9
    out[n++] = reinterpret_cast<char*>(&adv->mPlayDuration) - advRaw;     // 10
    out[n++] = static_cast<int64_t>(sizeof(tag_sDMeshSet));               // 11
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));                   // 12
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufChar));            // 13
    return n;
}

// ---- the functions
uint64_t tpw_loc_dtor(uint64_t self, int32_t flag) {
    return U(LocaleStdDestroy(static_cast<StdLocaleWords*>(P(self)), static_cast<int16_t>(flag)));
}
void tpw_loc_imbue(uint64_t fb, uint64_t loc) { static_cast<BasicFilebufChar*>(P(fb))->imbue(P(loc)); }
int32_t tpw_loc_pbackfail(uint64_t fb, int32_t c) { return static_cast<BasicFilebufChar*>(P(fb))->pbackfail(c); }
void tpw_loc_disable(int32_t flag) { DisableRideHighlight(flag); }
int32_t tpw_loc_esp(int32_t set) { return ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(set)); }
int32_t tpw_loc_toggle() { return ToggleProceduralTexturing(); }
int32_t tpw_loc_destroy(uint64_t set, uint64_t heap, uint32_t flags) {
    DestroyDynamicMeshSetInstance(static_cast<tag_sDMeshSet*>(P(set)), static_cast<sHeapInfo*>(P(heap)), flags);
    return 1;   // the original answers 1 in r3 unconditionally
}
void tpw_loc_playing(uint64_t adv, uint32_t length) { static_cast<CAdvisor*>(P(adv))->SetPlayingFor(length); }

}  // extern "C"
