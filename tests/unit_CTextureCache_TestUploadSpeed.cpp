// Test library for CTextureCache::TestUploadSpeed (unit v5668).
// Callees are fakes recording the same events as the PowerPC stubs; the cache and its fake texture are built by tpw_tus_setup.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "tb_profiler.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"

using namespace calllog;

CTextureSettings::CTextureSettings() {}   // not the original's: the test writes the fields it reads
CProfiler gProfiler;
CTextureSettings gei_TextureSettings;

CTextureBase::~CTextureBase() {}

namespace {

class FakeTexture : public CTextureBase {
public:
    int32_t Create(int32_t, int32_t, int32_t, int32_t) override { return 0; }
    int32_t Destroy() override { return 0; }
    void GetTexture() override {}
    int32_t GetPixelCount() override { return 0; }
    int32_t LockSurface() override {
        ev("Lock", {lab(this)});
        return static_cast<int32_t>(ret("Lock"));
    }
    int32_t UnlockSurface() override {
        ev("Unlock", {lab(this)});
        return static_cast<int32_t>(ret("Unlock"));
    }
    int32_t MipmapsUsed() override { return 0; }
    int32_t Download(CTextureEntry*, int32_t, int32_t) override { return 0; }
    double TestUploadSpeed() override { return 0; }
};

CTextureCache* gCache = nullptr;
CTextureLink* gLink = nullptr;
FakeTexture* gTexture = nullptr;

}  // namespace

int32_t ConvertSurfaceSizeToFlag(int32_t size) {
    ev("Convert", {size});
    return static_cast<int32_t>(ret("Convert"));
}

int32_t CProfiler::GetClockCycleCount10h() {
    ev("Clock", {this == &gProfiler ? 1 : 0});
    return static_cast<int32_t>(ret("Clock"));
}

extern "C" {

uint64_t tpw_tus_addr(const char* name) {
    if (std::strcmp(name, "settings") == 0) return reinterpret_cast<uintptr_t>(&gei_TextureSettings);
    return 0;
}

// Builds a fresh cache whose page list of size `flag` holds one page with a fake texture (pages count `pages`);
// answers the texture's address.
uint64_t tpw_tus_setup(int32_t flag, int32_t pages) {
    gCache = static_cast<CTextureCache*>(tpw::ArenaAllocZeroed(sizeof(CTextureCache)));
    gLink = static_cast<CTextureLink*>(tpw::ArenaAllocZeroed(sizeof(CTextureLink)));
    void* mem = tpw::ArenaAllocZeroed(sizeof(FakeTexture));
    gTexture = new (mem) FakeTexture();
    gLink->mTexture = gTexture;
    if (flag >= 0 && flag < CTextureCache::kSizes) {
        gCache->mPages[flag][0] = gLink;
        gCache->mPageCount[flag] = pages;
    }
    return reinterpret_cast<uintptr_t>(gTexture);
}

uint64_t tpw_tus_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tus_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mFormat));
    return n;
}

double tpw_tus_call(int32_t size, int32_t count) { return gCache->TestUploadSpeed(size, count); }

}  // extern "C"
