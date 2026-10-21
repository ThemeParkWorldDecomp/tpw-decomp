// LbDrawHelpers.h: file-local helpers that src/bullfrog/LbDraw.cpp shares with LbDrawText.cpp. Before the regroup, the
// functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "LbDraw.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"

template <typename F>
static inline void Context(F&& call) {
    const int32_t depth = _lbpRenderBase ? _lbpRenderBase->mDepth : 8;
    if (depth == 16) call(reinterpret_cast<RenderContext16*>(_lbpRenderBase));
    else if (depth == 24) call(reinterpret_cast<RenderContext24*>(_lbpRenderBase));
    else if (depth == 32) call(reinterpret_cast<RenderContext32*>(_lbpRenderBase));
    else call(reinterpret_cast<RenderContext8*>(_lbpRenderBase));
}
