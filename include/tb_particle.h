// tb_particle.h: the engine's particle object (engine library; Particle_Init 0x1001f21c to
// CParticle::GetLinkType 0x1001fc08, which the inventory files under tb_profiler.cpp).
//
// The engine draws particles through hooks that the game installs (its PAR_* functions,
// set by EngineStartup_Final); the object is also something the map lists can link to,
// through its virtual base. 0xac bytes on the Mac: +0 the pointer to the CMapLinkBase at
// +0xa4, +0xa0 the vtable; the host's compiler lays the class out its own way.
#pragma once
#include <cstdint>

#include "MapWho.h"

struct PARPOINT3D;
struct PTCL;

class CParticle : public virtual CMapLinkBase {
public:
    enum : int32_t { kEmitters = 0x78 };

    CParticle();                       // inline in Particle_Init: also registers the link base
    ~CParticle() override;             // 0x1001f3e4
    int32_t Prepare() override;        // 0x1001f4ac (src/tb_particle_frame.cpp)
    int32_t GetLinkType() override;    // 0x1001fc08: 4
    void* GetData() override;          // 0x1001fbd8: NULL
    // 0 on failure: CMapWho::RenderWindow tests the result, so the declaration's void became
    // int32_t (nothing defined or called it before).
    int32_t PlotParticles();           // 0x1001f754 (src/tb_particle_frame.cpp)

    int32_t mF04;                      // 0x04  -1
    int32_t mF08;                      // 0x08  -1
    uint8_t mClip[kEmitters];          // 0x0c  Prepare: each emitter's clipping result
    void (*mGetEmitterBoundingBox)(uint32_t emitter, PARPOINT3D* min, PARPOINT3D* max);   // 0x84
    void (*mInitParticleSystem)(void* owner, int32_t density, int32_t maxParticles);      // 0x88
    void (*mCloseParticleSystem)();                                                        // 0x8c
    void (*mLoadLibrary)(char* path);                                                      // 0x90
    PTCL* (*mBuildDrawingList)(int32_t list);                                              // 0x94
    void (*mFreeDrawingList)(PTCL* list);                                                  // 0x98
    int32_t (*mGetEmitterParticleCount)(uint32_t emitter);                                 // 0x9c
};
extern CParticle* gei_Particle;   // engine export (0x100b0508)

// What the game's particle hooks hand the frame members (src/tb_particle_frame.cpp). Added with
// Prepare and PlotParticles; the layouts are the Mac's, with native pointers.
//
// A corner of an emitter's bounding box, in 1/1024 world units (all zero: no box).
struct PARPOINT3D {
    int32_t x, y, z;   // 0x00
};
class CTextureEntry;
// The texture a particle is drawn with: the entry (null while it is not loaded) and the ratio
// of its width to its height (a Mac word at +8).
struct PTCLTexture {
    CTextureEntry* mEntry;   // 0x00
    int32_t mF04;            // 0x04  not read by the frame members
    float mAspect;           // 0x08  scales the sprite's half width
};
// One particle of a drawing list (0x1c bytes on the Mac).
struct PTCL {
    int32_t mX, mY, mZ;      // 0x00  position, in 1/1024 world units
    uint32_t mColour;        // 0x0c  its low byte decides the blending of an untextured particle
    PTCLTexture* mTexture;   // 0x10  or kNoParticleTexture
    uint16_t mSize;          // 0x14  the sprite's height, in 1/1024 world units
    uint16_t mAngle;         // 0x16  the turn of the sprite about the vertical, in 1/65536 of a turn
    uint32_t mFlags;         // 0x18  4 translucent, 0x2000, 0x4000, 0x10000, 0x20000, 0x40000
};
static_assert(sizeof(void*) != 4 || sizeof(PTCL) == 0x1c, "PTCL is 0x1c bytes on the Mac");
// The texture pointer of a particle that has none (the Mac's 0xffffffff).
inline PTCLTexture* ParticleNoTexture() { return reinterpret_cast<PTCLTexture*>(~static_cast<uintptr_t>(0)); }

int32_t Particle_Init();    // 0x1001f21c
int32_t Particle_Close();   // 0x1001f35c
