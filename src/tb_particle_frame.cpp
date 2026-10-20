// The particle object's frame members (engine library of SimTheme Park 2.04b3, in the
// inventory's tb_profiler.cpp range):
//
//   0x1001f4ac CParticle::Prepare       (640 bytes)
//   0x1001f754 CParticle::PlotParticles (1,108)
//
// The game's particle system (the PAR_* hooks installed by EngineStartup_Final) owns the
// particles: 0x78 emitters, each with a count, a bounding box and, on request, a drawing list
// of PTCL records in 1/1024 world units. Prepare (once per frame, before drawing) asks each
// emitter for its box, works out whether the box is in view and queues the textures its
// particles use; PlotParticles (CMapWho::RenderWindow) draws the particles of the emitters
// that are in view as camera-facing sprites, once for each frame.
#include "tb_particle.h"

#include <cstring>

#include "m3d2.h"
#include "tb_clipping.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// Positions and sizes are integers in 1/1024 of a world unit.
constexpr float kUnit = 1.0f / 1024.0f;   // 0x3a800000
constexpr float kHalf = 0.5f;
constexpr float kAngleStep = 1.0f / 65536.0f;   // 0x37800000: the angle is in 1/65536 of a turn

// What the clipper answers for an emitter without a box: in view, all codes set (the low bits
// of the answer are the outside codes, 0x40 means "in view").
constexpr uint8_t kNoBox = 0x7c;
constexpr uint8_t kInView = 0x40;

// A corner of the sprite: x, y, z and w (0x10 bytes, as RotateVertex_Y and the renderer read them).
struct SCorner {
    float mX, mY, mZ, mW;
};

// Blending modes of the sprites: an untextured particle with a colour that is not nearly opaque
// is blended (0x556), otherwise it is drawn as it is (0x1256); the particle's own flags replace
// or add to the mode.
constexpr uint32_t kBlendFaint = 0x556;
constexpr uint32_t kBlendOpaque = 0x1256;
constexpr uint32_t kBlendTranslucent = 0x200522;
constexpr uint32_t kBlendTranslucentLit = 0x200552;

// The particle's flags.
constexpr uint32_t kParticleTranslucent = 0x00000004;
constexpr uint32_t kParticleLit = 0x00002000;       // with translucent: the brighter mode
constexpr uint32_t kParticleMode20000 = 0x00020000;   // adds 0x20000 to the blending mode
constexpr uint32_t kParticleMode800 = 0x00004000;     // adds 0x800 to the blending mode
constexpr uint32_t kParticleSpriteFlag2 = 0x00010000; // the sprite's last word is 2, not 0
constexpr uint32_t kParticleViewSpace = 0x00040000;   // the position is already in view space

}  // namespace

// Once per frame, before anything is drawn: for each emitter that has particles, the clipping
// code of its box (mClip, read by PlotParticles) and the texture cache's request for every
// texture its particles use, at a distance of one half. An emitter whose box is all zero counts as
// in view with every code set. Always answers 1.
int32_t CParticle::Prepare() {
    for (int32_t emitter = 0; emitter < kEmitters; ++emitter) {
        const int32_t count = mGetEmitterParticleCount(static_cast<uint32_t>(emitter));
        if (count == 0) continue;

        PARPOINT3D lowest;
        PARPOINT3D highest;
        mGetEmitterBoundingBox(static_cast<uint32_t>(emitter), &lowest, &highest);
        const bool empty = lowest.x == 0 && lowest.y == 0 && lowest.z == 0 && highest.x == 0 && highest.y == 0 &&
                           highest.z == 0;
        if (empty) {
            mClip[emitter] = kNoBox;
        } else {
            sAlignedBox box;
            box.mMin[0] = static_cast<float>(lowest.x) * kUnit;
            box.mMin[1] = static_cast<float>(lowest.y) * kUnit;
            box.mMin[2] = static_cast<float>(lowest.z) * kUnit;
            box.mMax[0] = static_cast<float>(highest.x) * kUnit;
            box.mMax[1] = static_cast<float>(highest.y) * kUnit;
            box.mMax[2] = static_cast<float>(highest.z) * kUnit;
            mClip[emitter] = gei_Clipping->CheckBoundingBox(box, *gei_FinalMatrix);
        }

        PTCL* list = mBuildDrawingList(emitter);
        if (list != nullptr) {
            const PTCL* particle = list;
            for (int32_t i = 0; i < count; ++i, ++particle) {
                if (particle->mTexture != ParticleNoTexture()) gei_TCache->Prepare(particle->mTexture->mEntry, kHalf);
            }
            mFreeDrawingList(list);
        }
    }
    return 1;
}

// Draws the particles of the emitters Prepare found in view (flag 0x40 of their clipping code),
// each as one sprite of four corners through the draw primitive's DrawSprite, unless the display
// has not moved on to another frame since the last call (its frame count and its word at +0x84
// are both the ones remembered). It stops as soon as DrawSprite fails, without freeing the
// emitter's list, and answers 1 all the same: the original has no way to answer 0 (the header's
// "0 on failure" is what RenderWindow tests, and it never sees one).
//
// A sprite is the particle's size high (half above and half below its position), its width scaled
// by the display's aspect, or by the texture's own when it has one. The sprite parameters are
// one record for the whole call, so a texture flagged 0x100000 sets the sprite flag 4 for every
// later particle too. The record's word at +0x14 is not set (the host sets it to zero).
int32_t CParticle::PlotParticles() {
    SCorner corners[4];
    SSpriteDrawParameters sprite;
    sprite.mFlags = 0;
    sprite.mVertices = corners;
    sprite.mF14 = 0;

    const int32_t frame = gei_Display.mFrameCount;
    const int32_t other = gei_Display.mF84;
    if (mF04 == frame && mF08 == other) return 1;
    mF04 = frame;
    mF08 = other;

    for (int32_t emitter = 0; emitter < kEmitters; ++emitter) {
        const int32_t count = mGetEmitterParticleCount(static_cast<uint32_t>(emitter));
        if (count <= 0 || (mClip[emitter] & kInView) == 0) continue;
        PTCL* list = mBuildDrawingList(emitter);
        if (list == nullptr) continue;

        const PTCL* particle = list;
        for (int32_t i = 0; i < count; ++i, ++particle) {
            // Half the sprite's height: 0.5 * (size / 1024).
            const float halfHeight = kHalf * (kUnit * static_cast<float>(particle->mSize));
            float halfWidth;
            PTCLTexture* slot = particle->mTexture;
            if (slot == ParticleNoTexture()) {
                sprite.mTexture = nullptr;
                sprite.mU0 = sprite.mV0 = sprite.mU1 = sprite.mV1 = 0.0f;
                const uint32_t alpha = particle->mColour & 0xff;
                sprite.mBlendingMode = (alpha != 0 && alpha < 0xf0) ? kBlendFaint : kBlendOpaque;
                halfWidth = halfHeight * gei_Display.mF04;
            } else {
                CTextureEntry* entry = slot->mEntry;
                if (entry == nullptr) {
                    sprite.mTexture = nullptr;
                    sprite.mBlendingMode = kBlendOpaque;
                    sprite.mU0 = sprite.mV0 = sprite.mU1 = sprite.mV1 = 0.0f;
                } else {
                    sprite.mTexture = (entry->mFlags & CTextureEntry::kOnPage) != 0 ? entry->mTexture : nullptr;
                    sprite.mBlendingMode = entry->GetBlendingMode();
                    sprite.mU0 = entry->mUOffset;
                    sprite.mV0 = entry->mVOffset;
                    sprite.mU1 = sprite.mU0 + entry->mUSize;
                    sprite.mV1 = sprite.mV0 + entry->mVSize;
                    if ((entry->mFlags & 0x100000) != 0) sprite.mFlags |= 4;
                }
                halfWidth = halfHeight * slot->mAspect;
            }
            if ((particle->mFlags & kParticleTranslucent) != 0) {
                sprite.mBlendingMode = (particle->mFlags & kParticleLit) != 0 ? kBlendTranslucentLit : kBlendTranslucent;
            }
            if ((particle->mFlags & kParticleMode20000) != 0) sprite.mBlendingMode |= 0x20000;
            if ((particle->mFlags & kParticleMode800) != 0) sprite.mBlendingMode |= 0x800;

            // The position, in view space (transformed unless the particle says it is there already).
            float position[3];
            position[0] = static_cast<float>(particle->mX) * kUnit;
            position[1] = static_cast<float>(particle->mY) * kUnit;
            position[2] = static_cast<float>(particle->mZ) * kUnit;
            float view[4];
            if ((particle->mFlags & kParticleViewSpace) != 0) {
                view[0] = position[0];
                view[1] = position[1];
                view[2] = position[2];
                view[3] = 1.0f;
            } else {
                gei_FinalMatrix->TransformVertex(view, position, 0);
            }
            const float left = view[0] - halfWidth;
            const float right = view[0] + halfWidth;
            const float bottom = view[1] - halfHeight;
            const float top = view[1] + halfHeight;
            corners[0] = {left, bottom, view[2], view[3]};
            corners[1] = {right, bottom, view[2], view[3]};
            corners[2] = {right, top, view[2], view[3]};
            corners[3] = {left, top, view[2], view[3]};
            if (particle->mAngle != 0) {
                RotateVertex_Y(reinterpret_cast<SVertex*>(corners), kAngleStep * static_cast<float>(particle->mAngle),
                               nullptr);
            }
            sprite.mF08 = static_cast<int32_t>(particle->mColour);
            sprite.mF28 = (particle->mFlags & kParticleSpriteFlag2) != 0 ? 2 : 0;
            if (gei_DrawPrimitive->DrawSprite(&sprite, -1) == 0) return 1;
        }
        mFreeDrawingList(list);
    }
    return 1;
}
