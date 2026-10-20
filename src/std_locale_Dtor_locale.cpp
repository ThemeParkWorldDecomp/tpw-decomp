// Unit v51049: std::locale::~locale 0x10070d44, std::basic_filebuf<char>::imbue 0x10070bf4 and pbackfail 0x100715b4,
// DisableRideHighlight 0x100672e4, ESP_GetRandomSpritesFrom 0x1007c900, ToggleProceduralTexturing 0x100634f8,
// DestroyDynamicMeshSetInstance 0x10063264, CAdvisor::SetPlayingFor 0x1000c7fc.
#include <cstddef>
#include <cstdint>

#include "AdvisorQueue.h"
#include "Clock.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "World.h"
#include "keytable_actions.h"
#include "platform/arena.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "tb_hfield.h"

namespace {
// The 32-bit words of the Mac's objects are arena offsets here (include/Ptr32.h): 0 is NULL.
inline uint32_t W(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
inline void* P(uint32_t w) { return w != 0 ? tpw::gArenaBase + w : nullptr; }
}  // namespace

// 0x102d1f04: DisableRideHighlight's flag (zero at start, like the Mac's bss).
int32_t gRideHighlightDisabled;

// ---- std::locale::~locale (0x10070d44): the last reference destroys the impl and the count; flag > 0 (signed 16 bit) deletes the locale.
StdLocaleWords* LocaleStdDestroy(StdLocaleWords* self, int16_t flag) {
    if (self) {
        uint32_t count = self->mCount;
        if (count) {
            int32_t* c = static_cast<int32_t*>(P(count));
            int32_t v = *c - 1;
            *c = v;
            if (v == 0) {
                filebuf_wclose_callee::LocaleImpDtor(P(self->mImp), 1);
                locale_imp_callee::Delete(P(self->mCount));
            }
        }
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

// ---- std::basic_filebuf<char>::imbue (0x10070bf4)
void BasicFilebufChar::imbue(const void* loc) {
    streambuf_callee::LocaleAssign(&mLocale, loc);
    uint32_t tmp[2];
    filebuf_char_callee::Getloc(tmp, this);
    void* facet = filebuf_char_callee::UseFacetCodecvt(tmp);
    mCodecvt = W(facet);
    uint32_t count = tmp[1];
    if (count) {
        int32_t* c = static_cast<int32_t*>(P(count));
        int32_t v = *c - 1;
        *c = v;
        if (v == 0) {
            filebuf_wclose_callee::LocaleImpDtor(P(tmp[0]), 1);
            locale_imp_callee::Delete(P(tmp[1]));
        }
    }
    mUnbuffered = static_cast<uint8_t>(filebuf_char_callee::CodecvtAlwaysNoconv(P(mCodecvt)));
}

// ---- std::basic_filebuf<char>::pbackfail (0x100715b4): step the get pointer back (unsigned compare with the start).
int32_t BasicFilebufChar::pbackfail(int32_t c) {
    if (mFile == 0 || !(mGptr > mEback)) return -1;
    mGptr = mGptr - 1;
    if (c != -1) {
        *static_cast<char*>(P(mGptr)) = static_cast<char>(c);
    }
    if (c == -1) c = 0;
    return c;
}

// ---- DisableRideHighlight (0x100672e4)
void DisableRideHighlight(int32_t flag) {
    if (flag == 0) {
        gRideHighlightDisabled = 0;
    } else {
        if (gHighlightedRide != nullptr) UnhighlightRide();
        gRideHighlightDisabled = 1;
    }
}

// ---- ESP_GetRandomSpritesFrom (0x1007c900): a random index below the group's count (the count is never 0 in the game;
// the divide would trap here).
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    uint32_t count = static_cast<uint32_t>(gESPGroups[static_cast<int32_t>(set)].mCount);
    uint32_t r = gGameWorld->GetRand() >> 2;
    return static_cast<int32_t>(r % count);
}

// ---- ToggleProceduralTexturing (0x100634f8): bit 0x2000 = available, 0x4000 = on.
int32_t ToggleProceduralTexturing() {
    uint32_t options = gGameOptions;
    if (options & 0x2000) {
        if (options & 0x4000) {
            gGameOptions = options & ~0x4000u;
            PT_Disable();
            CorrectOrientationFlagsForDiffProceduralMode(0);
        } else {
            int32_t renderer = static_cast<int32_t>(gPlayerSettings.mCurrent.mRenderer);
            if (renderer == 1) {
                gGameOptions = options | 0x4000u;
                PT_Enable();
                CorrectOrientationFlagsForDiffProceduralMode(1);
            } else if (renderer == 0) {
                gGameOptions = options & ~0x4000u;
                gGameOptions = gGameOptions & ~0x2000u;
            }
        }
    }
    return 1;
}

// ---- DestroyDynamicMeshSetInstance (0x10063264); the heap and flags arguments are unused.
void DestroyDynamicMeshSetInstance(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags) {
    (void)heap;
    (void)flags;
    tag_sMesh* mesh = set->mMesh;
    if (mesh->mF00 & 1) DisableMeshDraw(mesh);
    DestroyEngineMesh(mesh);
    MReleaseFreeF(mesh->mAllocation);
    set->mShared->mInstances = set->mShared->mInstances - 1;
    MReleaseFreeF(set->mAllocation);   // the original also answers 1 in r3; callers ignore it
}

// ---- CAdvisor::SetPlayingFor (0x1000c7fc)
void CAdvisor::SetPlayingFor(uint32_t length) {
    mPlayStartTime = static_cast<uint32_t>(gClock.ReadFixed());
    mPlayDuration = length;
}
