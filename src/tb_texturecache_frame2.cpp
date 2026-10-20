#include "tpw_probe.h"
// tb_texturecache_frame2.cpp (engine library of SimTheme Park 2.04b3, the texture cache's
// per-frame calls other than Refresh and Execute; the inventory files the range under
// Wavelet.cpp): CTextureCache::Prepare 0x10032ddc, ReInitialize 0x1003447c,
// DumpTCStreamToLog_Prepare 0x10034bc4, DumpTCStreamToLog_Execute 0x10034d50,
// GenerateTexturePages 0x10037494, UploadToVRAM 0x1003820c and CTextureLink::PageUpdated
// 0x100385f4.
//
// The frame asks for pictures with Prepare, which queues every entry that is on no page
// (CTextureCache::mF18AC); Execute sorts the queue and GenerateTexturePages packs it onto
// pages, PackTextures calling UploadToVRAM and PageUpdated for each entry and page it fills.
// The file "tcache.log" is a debug stream of the requests (texture settings flag 0x80).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

int32_t gTexturePageUpdates = 0;

namespace {

// The entry's flags (CTextureEntry::mFlags) this file reads; the named ones are in
// tb_textureentry.h.
constexpr uint32_t kEntryWanted = 0x00000030;      // the pair 0xc0 is copied down into it when an entry is queued or dropped
constexpr uint32_t kEntryQueued = 0x00002000;      // Prepare queued it this frame; UploadToVRAM clears it
constexpr uint32_t kEntrySortLocked = 0x00004000;  // the sort key gets 0x20000000
constexpr uint32_t kEntrySortBlend = 0x00080000;   // the sort key gets 0x40000000
constexpr uint32_t kEntryWrapU = 0x00010000;       // part of kWrapAny: UploadToVRAM's offset along u is 0
constexpr uint32_t kEntryWrapV = 0x00020000;       // the same along v
constexpr uint32_t kEntryFlag400 = 0x00000400;     // with no second block (mF38) a wrapping image is placed like any other
constexpr uint32_t kEntryFlag4000000 = 0x04000000; // UploadToVRAM takes the settings' four fixups
constexpr uint32_t kEntryNoPrepare = 0x02000000;   // Prepare ignores the entry
constexpr uint32_t kEntryAsked = 0x40000000;       // Prepare was called for it this frame
constexpr uint32_t kEntryCentred = 0x80000000;     // UploadToVRAM centres the image in a larger slot, once

// The entry's place record: this frame's use count is the byte gFrequencyIndex, mF10 their
// sum and mF14 the entry's score (0x8000 marks "used").
constexpr int32_t kPlaceUsed = 0x00008000;

// Cache flag: the stream file has been started.
constexpr uint32_t kCacheStreamStarted = 0x40000000;
// Texture settings flag: the requests are written to the stream file.
constexpr uint32_t kSettingsDumpStream = 0x00000080;

// The stream file's name (in the working directory).
const char kStreamName[] = "tcache.log";

// An entry's sort key: the priority (nearer is larger) in bits 16..27, the cache size
// (width * 256 + height) below it, and a bit each for a locked entry and for the 0x80000
// kind. CTextureCache::Remove builds the same key.
void RebuildSortKey(CTextureEntry* entry) {
    entry->mSortKey = static_cast<int32_t>(
        (static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & kEntrySortLocked) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & kEntrySortBlend) != 0) entry->mSortKey |= 0x40000000;
}

// An entry the packing gave up on: its page bits (0x1000..0x8000) go and its "wanted" pair
// falls back to the pair above it, its size is rounded up to 8 and its key rebuilt. This is
// CTextureCache::Remove's ending with 7 in place of the cache's size mask. (The locked bit
// 0x4000 is cleared with the page bits, so the key's locked test is dead, as in Remove.)
void ForgetPlacement(CTextureEntry* entry) {
    entry->mFlags &= 0xffff0fcfu;
    entry->mFlags &= ~kEntryWanted;
    entry->mFlags |= (entry->mFlags >> 2) & kEntryWanted;
    entry->mCacheWidth = static_cast<int16_t>((entry->mWidth + 7) & 0xfff8);
    entry->mCacheHeight = static_cast<int16_t>((entry->mHeight + 7) & 0xfff8);
    RebuildSortKey(entry);
}

// What the debug stream holds for one request, as the Mac wrote it after a one-byte tag:
// the entry's address (32 bits), flags, size and priority.
struct SStreamRecord {
    uint32_t mEntry;
    uint32_t mFlags;
    int16_t mWidth;
    int16_t mHeight;
    float mPriority;
};
static_assert(sizeof(SStreamRecord) == 0x10, "SStreamRecord layout");

}  // namespace

void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    tpw_probe::Hit(tpw_probe::kTexPrepare);
    if (entry == nullptr) { tpw_probe::Hit(tpw_probe::kTexPrepareNull); return; }
    // Both asserts return, and the function carries on.
    if (!(entry->mWidth > 0 && entry->mCacheWidth != 0)) {
        msl_assertion_failed("(src->GetSrcWidth()>0) && (src->GetDestWidth())", "tb_texturecache_core.cpp", 0x23);
    }
    if ((entry->mFlags & CTextureEntry::kWrapAny) != 0 && entry->mWidth != entry->mHeight) {
        msl_assertion_failed("(!src->IsWrapping()) || (src->GetSrcWidth()==src->GetSrcHeight())",
                             "tb_texturecache_core.cpp", 0x26);
    }
    if ((entry->mFlags & kEntryNoPrepare) != 0) { tpw_probe::Hit(tpw_probe::kTexPrepareIgnored); return; }
    entry->mFlags |= kEntryAsked;

    // This frame's use count (a byte, which stops at 255), the sum of the counts and the
    // score's "used" bit.
    STexturePlace* place = entry->mPlace;
    if (place->mF00[gFrequencyIndex] != 0xff) {
        ++place->mF00[gFrequencyIndex];
        ++place->mF10;
    }
    place->mF14 |= kPlaceUsed;

    if ((entry->mFlags & (CTextureEntry::kOnPage | kEntryQueued)) == 0) {
        // On no page and not queued yet: queue it, size and key as the cache holds them.
        entry->mPriority = distance;
        entry->mFlags |= kEntryQueued;
        entry->mFlags &= ~kEntryWanted;
        entry->mFlags |= (entry->mFlags >> 2) & kEntryWanted;
        entry->mCacheWidth = static_cast<int16_t>(~mSizeMask & (entry->mWidth + mSizeMask));
        entry->mCacheHeight = static_cast<int16_t>(~mSizeMask & (entry->mHeight + mSizeMask));
        RebuildSortKey(entry);
        entry->mNext = mF18AC;
        mF18AC = entry;
        tpw_probe::Hit(tpw_probe::kTexQueued);
    } else {
        tpw_probe::Hit(tpw_probe::kTexPrepareHeld);
        if (distance < entry->mPriority) entry->mPriority = distance;
        // When the view or the lights changed a locked entry on a page locks its page.
        if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0) {
            if ((entry->mFlags & CTextureEntry::kOnPage) != 0 && (entry->mFlags & CTextureEntry::kLocked) != 0) {
                entry->mPage->mTexture->PositionLock();
            }
        }
    }
    // A further plane of the same image is the next entry of the array.
    if ((entry->mFlags & CTextureEntry::kNextIsPlane) != 0) Prepare(entry + 1, distance);
    if ((gei_TextureSettings.mFlags & kSettingsDumpStream) != 0) DumpTCStreamToLog_Prepare(entry);
}

int32_t CTextureCache::ReInitialize(int32_t format) {
    if ((mFlags & kReady) != 0) Close();
    if (format != -1) gei_TextureSettings.mFormat = format;
    return AutoCreateSurfaces();
}

// The first call starts the file over ("w+b") with the texture settings and the first word
// of the system and display objects (their flags); later calls append. Then one record: a tag
// byte 1 and the entry.
void CTextureCache::DumpTCStreamToLog_Prepare(CTextureEntry* entry) {
    const uint8_t tag = 1;
    FILE* file;
    if ((mFlags & kCacheStreamStarted) == 0) {
        mFlags |= kCacheStreamStarted;
        file = std::fopen(kStreamName, "w+b");
        if (file != nullptr) {
            std::fwrite(&gei_TextureSettings, 1, 0x50, file);
            std::fwrite(&gei_System, 1, 4, file);
            std::fwrite(&gei_Display, 1, 4, file);
        }
    } else {
        file = std::fopen(kStreamName, "a");
    }
    if (file != nullptr) {
        SStreamRecord record;
        record.mEntry = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry));
        record.mFlags = entry->mFlags;
        record.mWidth = entry->mWidth;
        record.mHeight = entry->mHeight;
        record.mPriority = entry->mPriority;
        std::fwrite(&tag, 1, 1, file);
        std::fwrite(&record, 1, sizeof record, file);
        std::fflush(file);
        std::fclose(file);
    }
}

// The marker for "Execute ran". The Mac stores the word 0x10 and writes its FIRST byte,
// which is 0 on a big-endian machine (the byte 0x10 is the fourth), so that is what the
// stream holds there.
void CTextureCache::DumpTCStreamToLog_Execute() {
    const uint8_t tag = 0;
    FILE* file = std::fopen(kStreamName, "a");
    if (file != nullptr) {
        std::fwrite(&tag, 1, 1, file);
        std::fflush(file);
        std::fclose(file);
    }
}

int32_t CTextureCache::GenerateTexturePages() {
    std::memset(mF1830, 0, sizeof mF1830);
    int32_t result = 1;
    // PackTextures takes entries off the queue; -1 means the first one fits nowhere, so
    // it is dropped back to the state of an entry on no page.
    while (mF18AC != nullptr && result != 0) {
        result = PackTextures(&mF18AC);
        if (result == -1) {
            ForgetPlacement(mF18AC);
            result = 1;
            mF18AC = mF18AC->mNext;
        }
    }
    // After a failure the rest of the queue is dropped the same way.
    for (CTextureEntry* entry = mF18AC; entry != nullptr; entry = entry->mNext) ForgetPlacement(entry);
    mF18AC = nullptr;
    return result;
}

int32_t CTextureCache::UploadToVRAM(CTextureLink* link, SAvailableQuad* quad, CTextureEntry* entry) {
    tpw_probe::Hit(tpw_probe::kUploadToVRAM);
    if (link == nullptr) msl_assertion_failed("dest_link != NULL", "tb_texturecache_upload.cpp", 0x56f);
    if (entry == nullptr) entry = quad->mEntry;
    CTextureBase* texture = link->mTexture;
    texture->Download(entry, quad->mX, quad->mY);

    // The slot's corner and the size the image takes in it. An entry flagged to be centred
    // that is smaller than its cache size sits in the middle of the slot (once).
    int32_t width = entry->mCacheWidth;
    int32_t height = entry->mCacheHeight;
    int32_t x = quad->mX;
    int32_t y = quad->mY;
    if ((entry->mFlags & kEntryCentred) != 0) {
        if (width > entry->mWidth) {
            x += (width - entry->mWidth) >> 1;
            width = entry->mWidth;
        }
        if (height > entry->mHeight) {
            y += (height - entry->mHeight) >> 1;
            height = entry->mHeight;
        }
        entry->mFlags &= ~kEntryCentred;
    }
    entry->mFlags &= ~kEntryQueued;
    entry->mFlags |= CTextureEntry::kOnPage;
    entry->mPage = link;
    if (gei_System.mRenderer != CSystem::kRendererOpenGL) return 1;

    const CTextureSettings& settings = gei_TextureSettings;
    const float scale = mF18E8;
    const uint32_t flags = entry->mFlags;
    if ((flags & CTextureEntry::kWrapAny) != 0 && !((flags & kEntryFlag400) != 0 && entry->mF38 == nullptr)) {
        // A wrapping image covers the whole page: the offset is 0 along a direction that wraps
        // and mFloat18 texels (0.5) along the other; the size is what is left (1 - 2 * offset).
        float u = (flags & kEntryWrapU) != 0 ? 0.0f : settings.mFloat18;
        float v = (flags & kEntryWrapV) != 0 ? 0.0f : settings.mFloat18;
        u = u * (scale / static_cast<float>(texture->mWidth));
        v = v * (scale / static_cast<float>(texture->mHeight));
        entry->mUOffset = u;
        entry->mVOffset = v;
        entry->mUSize = ppc_fnmsubs(2.0f, u, 1.0f);
        entry->mVSize = ppc_fnmsubs(2.0f, v, 1.0f);
    } else {
        // The image's corner and size in fractions of the page, moved by the settings' fixups
        // when flag 0x4000000 asks (else by mFloat18, mFloat18, 0 and 0). The offsets are summed
        // as x + fixU first and lastU after (the decompiled C shows (lastU + x) + fixU, which
        // rounds differently).
        float fixU, fixV, lastU, lastV;
        if ((flags & kEntryFlag4000000) != 0) {
            fixU = settings.mUVFixup[0];
            fixV = settings.mUVFixup[1];
            lastU = settings.mUVFixup[2];
            lastV = settings.mUVFixup[3];
        } else {
            fixU = settings.mFloat18;
            fixV = fixU;
            lastU = 0.0f;
            lastV = lastU;
        }
        entry->mUOffset = scale * ((lastU + (static_cast<float>(x) + fixU)) / static_cast<float>(texture->mWidth));
        entry->mVOffset = scale * ((lastV + (static_cast<float>(y) + fixV)) / static_cast<float>(texture->mHeight));
        entry->mUSize =
            scale * (ppc_fnmsubs(2.0f, fixU, static_cast<float>(width)) / static_cast<float>(texture->mWidth));
        entry->mVSize =
            scale * (ppc_fnmsubs(2.0f, fixV, static_cast<float>(height)) / static_cast<float>(texture->mHeight));
    }
    // GetTexture is an empty function on the Mac (the OpenGL page's only), so its answer is
    // the object it was called on: the whole COpenGLTexture, which SetTexture takes back as
    // the page. (The field's type is the page's CTextureBase; the pointer is the complete
    // object's, as on the Mac.)
    texture->GetTexture();
    entry->mTexture = reinterpret_cast<CTextureBase*>(dynamic_cast<void*>(texture));
    return 1;
}

int32_t CTextureLink::PageUpdated() {
    ++gTexturePageUpdates;
    mF14 = gei_Display.mFrameCount;
    return mTexture->UnlockSurface();
}
