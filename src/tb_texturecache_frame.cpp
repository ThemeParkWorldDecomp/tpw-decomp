#include "tpw_probe.h"
// tb_texturecache_frame.cpp (engine library of SimTheme Park 2.04b3, the texture cache's frame
// calls; the inventory files the range under Wavelet.cpp): CTextureCache::Refresh 0x10034204
// and Execute 0x10033688.
//
// Refresh runs at the start of every window's frame. It ages every entry on every page (the
// frame's use count goes out of the entry's place record and the record's score halves),
// forgets what was asked for the last frame and, when the view or the lights changed (the
// static pool is drawn again), unlocks every entry and page so they can be placed anew.
// Execute runs after the window's links were prepared: when pictures were asked for it sorts
// them, balances the pages and generates the pages.
//
// Refresh is void in the original; it answers 1 here because the declaration says int32_t.
#include "tb_texturecache.h"

#include "tb_error.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

uint32_t gFrequencyIndex = 0;

namespace {

// An entry's flags that Refresh clears: 0x2000 (asked for this frame; also cleared on the
// entries still waiting) and 0x40000000.
constexpr uint32_t kEntryFlags2000 = 0x00002000;
constexpr uint32_t kEntryFlags40000000 = 0x40000000;

// Texture settings flag 0x80: Execute first writes the cache's state to the log.
constexpr uint32_t kSettingsDumpStream = 0x00000080;

// Execute's error when no page could be generated.
constexpr int32_t kErrGeneratePages = static_cast<int32_t>(0xb0754011);

}  // namespace

int32_t CTextureCache::Refresh() {
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0)
        mFlags |= kUnlocking;
    else
        mFlags &= ~static_cast<uint32_t>(kUnlocking);
    CycleFrequencyTable();
    for (int32_t size = 0; size < kSizes; size++) {
        for (int32_t page = 0; page < mPageCount[size]; page++) {
            CTextureLink* link = mPages[size][page];
            for (CTextureEntry* entry = link->mEntries; entry != nullptr; entry = entry->mNext) {
                entry->mFlags &= ~kEntryFlags2000;
                // The place record keeps a count per frame (0..15): this frame's goes out of
                // the total and starts again from 0.
                STexturePlace* place = entry->mPlace;
                place->mF10 -= place->mF00[gFrequencyIndex];
                place->mF00[gFrequencyIndex] = 0;
                place->mF14 >>= 1;
                entry->mFlags &= ~kEntryFlags40000000;
                entry->mPriority = 1.0f;
                if ((mFlags & kUnlocking) != 0) entry->Unlock();
            }
            if ((mFlags & kUnlocking) != 0) link->mTexture->PositionUnlock();
        }
    }
    for (CTextureEntry* entry = mF18AC; entry != nullptr; entry = entry->mNext) entry->mFlags &= ~kEntryFlags2000;
    mF18AC = nullptr;
    return 1;
}

int32_t CTextureCache::Execute() {
    if ((gei_TextureSettings.mFlags & kSettingsDumpStream) != 0) DumpTCStreamToLog_Execute();
    if (mF18AC == nullptr) return 1;
    tpw_probe::Hit(tpw_probe::kTexExecute);
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0)
        BalanceCache_FreeLockedTextureSpace();
    SortTextures();
    SortEntries();
    BalanceCache_LevelSurfaceUsage();
    if (GenerateTexturePages() == 0) {
        gei_ErrorHandler.Push(2, kErrGeneratePages);
        return 0;
    }
    return 1;
}
