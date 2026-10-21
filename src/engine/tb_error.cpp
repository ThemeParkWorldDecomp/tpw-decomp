// tb_error.cpp: name proven by __sinit_tb_error_cpp in the engine library.

#include "helpers/tb_errorHelpers.h"
#include "MapWho.h"
#include "tb_clipping.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include <cstddef>
#include <cstring>

// tb_drawprimitive_addsprite.cpp (engine library of SimTheme Park 2.04b3; the original file is
// tb_drawprimitive.cpp, the inventory files the range under tb_error.cpp): the draw primitive's
// batching sprite and the vertex record's four small members it builds its quad with.
//
//   0x10008230 CDrawPrimitive::AddSprite     (1,392 bytes)
//   0x100087e8 SFVF_TLCTEX::ReturnPtr        (12)
//   0x10008824 SFVF_TLCTEX::SetUV            (20)
//   0x10008864 SFVF_TLCTEX::SetDiffuse       (8)
//   0x1000889c SFVF_TLCTEX::SetVertex        (52)
//
// DrawSprite (src/tb_drawprimitive_sprite.cpp) makes a primitive of its own for each sprite and
// executes it. AddSprite is what CSpriteManager::RenderSprites uses (through SSpriteGameData::
// AddRegular): it adds the four corners to the primitive being built, so that every sprite of
// one texture and blending mode shares a primitive, and the caller executes the batch when the
// kind changes. It starts a primitive only when there is none.

// ---- SFVF_TLCTEX ----------------------------------------------------------------------------------

SFVF_TLCTEX* SFVF_TLCTEX::ReturnPtr(int32_t size, int32_t index) {
    const uint32_t bytes = static_cast<uint32_t>(index) * static_cast<uint32_t>(size);
    return reinterpret_cast<SFVF_TLCTEX*>(Moved(this, bytes));
}

void SFVF_TLCTEX::SetUV(int32_t set, float newU, float newV) {
    SFVF_TLCTEX* where = reinterpret_cast<SFVF_TLCTEX*>(Moved(this, static_cast<uint32_t>(set) << 3));
    where->u = newU;
    where->v = newV;
}

void SFVF_TLCTEX::SetDiffuse(uint32_t newColour) { colour = newColour; }

void SFVF_TLCTEX::SetVertex(SVertex vertex) {
    x = vertex.x;
    y = vertex.y;
    z = vertex.z;
    rhw = vertex.w;
}

// tb_error.cpp (decompiled from the engine library of SimTheme Park 2.04b3).
// PARTIAL: CErrorHandler::Push (0x10008904), Pop (0x10008964), the static initialiser
// (0x10008a7c) and SError's inline constructor, translated with tb_capture.cpp;
// GetAltTabStatus (0x100089bc), translated with tb_resource_system.cpp.

CErrorHandler gei_ErrorHandler;

void CErrorHandler::Push(int32_t level, int32_t code) {
    if (mCount >= 64) return;
    mStack[mCount].mLevel = level;
    mStack[mCount].mCode = code;
    mCount++;
}

SError* CErrorHandler::Pop() {
    if (mCount <= 0) return nullptr;
    mCount--;
    return &mStack[mCount];
}

int32_t CErrorHandler::GetAltTabStatus() {
    // Removes every lost-surface error (each is replaced by the current top entry, walking
    // down from the top) and reports whether there was one.
    bool found = false;
    SError* p = &mStack[mCount];
    for (int32_t i = mCount; i != 0; i--) {
        p--;
        if (static_cast<uint32_t>(p->mCode) == kErrSurfaceLost) {
            found = true;
            mCount--;
            *p = mStack[mCount];
        }
    }
    if (!found) return 0;
    gei_System.mFlags |= CSystem::kAltTab;
    return 1;
}
