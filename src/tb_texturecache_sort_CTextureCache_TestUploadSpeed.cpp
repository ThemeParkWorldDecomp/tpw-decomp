#include <cstddef>
#include <cstdint>
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_qsort.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tb_profiler.h"
#include "tb_resource_texture.h"

// Bytes per pixel by texture format (engine data 0x10051eb0, indexed by the settings' mFormat).
static const int32_t kUploadBytes[8] = {2, 2, 2, 4, 2, 4, 0xc0, 3};

// Times `count` locks and unlocks of the first page of `size`; answers pixels per clock tick
// (0 when a lock or unlock fails or no page of that size exists).
double CTextureCache::TestUploadSpeed(int32_t size, int32_t count) {
    int32_t flag = ConvertSurfaceSizeToFlag(size);
    if (mPageCount[flag] <= 0) return 0.0f;
    int32_t bytes = kUploadBytes[gei_TextureSettings.mFormat];
    CTextureBase* texture = mPages[flag][0]->mTexture;
    gei_TextureSettings.mFlags |= 0x20000000u;
    int32_t total = count * (size * size * bytes);
    int32_t start = gProfiler.GetClockCycleCount10h();
    for (int32_t n = count;;) {
        bool last = (n == 0);
        n--;
        if (last) {
            int32_t end = gProfiler.GetClockCycleCount10h();
            gei_TextureSettings.mFlags &= 0xdfffffffu;
            int32_t ticks = (end - start) * 16;
            return static_cast<float>(static_cast<float>(total) / static_cast<float>(ticks));
        }
        if (texture->LockSurface() == 0) {
            gei_TextureSettings.mFlags &= 0xdfffffffu;
            return 0.0f;
        }
        if (texture->UnlockSurface() == 0) {
            gei_TextureSettings.mFlags &= 0xdfffffffu;
            return 0.0f;
        }
    }
}
