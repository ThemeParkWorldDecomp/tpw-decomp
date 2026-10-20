// The particle object's start-up and shutdown (engine library of SimTheme Park 2.04b3,
// 0x1001f21c..0x1001fc08; the inventory files the range under tb_profiler.cpp). PARTIAL:
// CParticle::Prepare and PlotParticles are still to do.
#include "tb_particle.h"

#include <new>

#include "tb_error.h"

CParticle* gei_Particle = nullptr;

int32_t Particle_Init() {
    if (gei_Particle != nullptr) return 0;
    gei_Particle = new (std::nothrow) CParticle;
    if (gei_Particle == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t Particle_Close() {
    if (gei_Particle == nullptr) return 0;
    delete gei_Particle;
    gei_Particle = nullptr;
    return 1;
}

CParticle::CParticle() {
    mF04 = -1;
    mF08 = -1;
    mInitParticleSystem = nullptr;
    mCloseParticleSystem = nullptr;
    mLoadLibrary = nullptr;
    mBuildDrawingList = nullptr;
    mFreeDrawingList = nullptr;
    mGetEmitterParticleCount = nullptr;
    mGetEmitterBoundingBox = nullptr;
    RegisterLinkBase(this);
}

CParticle::~CParticle() { FreeLinkBase(this); }

int32_t CParticle::GetLinkType() { return 4; }

void* CParticle::GetData() { return nullptr; }
