// TPInstanceHelpers.h: file-local helpers that src/game/TPInstance.cpp shares with TPRide.cpp. Before the regroup, the
// functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TPEngine.h"

// The node of a mesh header with index `index`: the full nodes first, then the mini nodes.
static inline tag_sMiniNode* NodeByIndex(tag_sM3D2Header* header, uint32_t index) {
    if (index < header->mNodeCount) return header->mNodes.get() + index;
    return header->mMiniNodes.get() + (index - header->mNodeCount);
}
