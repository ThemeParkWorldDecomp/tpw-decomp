// host_particle_texture.cpp: checks that a sprite of the particle sprite file (SSpriteEngineData, include/tb_starfield.h)
// reads as the PTCLTexture the particle frame members draw with (include/tb_particle.h): ESP_GetParticleSprite hands out a
// pointer into the file's array of sprites, and CParticle::PlotParticles makes each sprite its half height times the
// record's mAspect wide. On a 64-bit host the record's second word was an int32_t, so mAspect was read from the upper half
// of the sprite's pixel pointer: 0 for a sprite loaded from a file, and every textured particle (the gold star over a
// sideshow when a guest wins among them) was drawn 0 wide. No game, no files. Run by ctest ("particle_texture"); exits
// nonzero on the first failure.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tb_particle.h"
#include "tb_starfield.h"

static int gChecks = 0;

#define CHECK(condition)                                                                \
    do {                                                                                \
        ++gChecks;                                                                      \
        if (!(condition)) {                                                             \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            std::exit(1);                                                               \
        }                                                                               \
    } while (0)

namespace {
constexpr int kSprites = 3;
// The file's array, as CSpriteFileIO::Load leaves it (the sprites are not constructed here: their fields are written where
// the sprite has them).
alignas(SSpriteEngineData) unsigned char gArray[kSprites * sizeof(SSpriteEngineData)];

SSpriteEngineData* Sprites() { return reinterpret_cast<SSpriteEngineData*>(gArray); }

template <typename T>
void Put(int index, size_t offset, T value) {
    std::memcpy(gArray + index * sizeof(SSpriteEngineData) + offset, &value, sizeof value);
}
}  // namespace

int main() {
    std::memset(gArray, 0xa5, sizeof gArray);
    CTextureEntry* const entries[kSprites] = {reinterpret_cast<CTextureEntry*>(static_cast<uintptr_t>(0x1000)),
                                              reinterpret_cast<CTextureEntry*>(static_cast<uintptr_t>(0x2000)), nullptr};
    // The particle file's own sprites have no pixel block (mData null); the last one has one, as a sprite of Init(int, int).
    void* const pixels[kSprites] = {nullptr, nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(0x123456789abcULL))};
    const float aspects[kSprites] = {0.75f, 1.5f, 2.0f};
    for (int i = 0; i < kSprites; ++i) {
        Put(i, offsetof(SSpriteEngineData, mTexture), entries[i]);
        Put(i, offsetof(SSpriteEngineData, mData), pixels[i]);
        Put(i, offsetof(SSpriteEngineData, mAspect), aspects[i]);
    }

    for (int i = 0; i < kSprites; ++i) {
        // ESP_GetParticleSprite's answer: the record at the index, read as a particle texture.
        const PTCLTexture* texture = reinterpret_cast<const PTCLTexture*>(Sprites() + i);
        CHECK(texture->mEntry == entries[i]);
        CHECK(texture->mF04 == pixels[i]);
        CHECK(texture->mAspect == aspects[i]);
        // PlotParticles: the half width of a particle of size 4096 (half height 2.0) on this texture.
        const float halfHeight = 0.5f * (4096.0f / 1024.0f);
        CHECK(halfHeight * texture->mAspect == 2.0f * aspects[i]);
    }
    std::printf("particle_texture: %d checks passed\n", gChecks);
    return 0;
}
