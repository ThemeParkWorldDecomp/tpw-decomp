// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit w133: TickRideHighlights (0x1006cff8, 212 bytes) and TickFlashObjects
// (0x1006d5fc, 244 bytes), which write colour words into the points of ride meshes. Both read a node's points as an array of 32-bit
// colour words (the node's pointer at +0x64, which tag_sNode types as s3DPoint*, read here as uint32_t*).
#include "Debug.h"

#include <cmath>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

float gFlashPhase = 0.0f;                    // 0x102d1efc
tag_sRideAnimSet* gFlashObjects = nullptr;   // 0x102d1f00

// While a ride is highlighted: every node of its mesh that does not have flag 0x4000 gets white (0xffffffff) in every one of its colour
// words. The original also reads the absolute time and divides it by 1000 (a helper, 0x10237d2c) and drops the answer; the call is kept.
void TickRideHighlights() {
    if (gHighlightedRide == nullptr) return;
    tag_sM3D2Header* header = gHighlightedRide->mHeads->mHeader;
    tag_sNode* node = header->mNodes.get();
    static_cast<void>(SamsUtilities::UTimer::GetAbsolute());
    for (uint32_t n = 0; n < header->mNodeCount; ++n) {
        for (uint32_t i = 0; i < node->mF5E; ++i) {
            if ((node->mFlags & 0x4000) == 0) reinterpret_cast<uint32_t*>(node->mPoints.get())[i] = 0xffffffffu;
        }
        ++node;
    }
}

// The flash colour: the phase (folded to 2 - phase above 1) times 255, converted to an unsigned (CodeWarrior's helper), is the red, green
// and blue byte of the word 0xff000000 | b << 16 | b << 8 | 0xff (b is not masked). The phase then advances by 2 * the frame time
// (gTPGlobals.mF4014, one fused multiply-add in single precision) and is taken modulo 2 in double. Every node of every set in the list
// gFlashObjects (linked through mFD0) gets the word in each of its colour words.
void TickFlashObjects() {
    float level = gFlashPhase;
    if (1.0f < level) level = 2.0f - level;
    const uint32_t b = cw_cvt_fp2unsigned(static_cast<double>(255.0f * level));
    gFlashPhase = ppc_fmadds(2.0f, gTPGlobals.mF4014, gFlashPhase);
    gFlashPhase = static_cast<float>(std::fmod(static_cast<double>(gFlashPhase), 2.0));
    for (tag_sRideAnimSet* set = gFlashObjects; set != nullptr; set = set->mFD0) {
        tag_sM3D2Header* header = set->mHeads->mHeader;
        tag_sNode* node = header->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            uint32_t* word = reinterpret_cast<uint32_t*>(node->mPoints.get());
            for (uint32_t i = node->mF5E; i != 0; --i) {
                *word = (b << 16) | 0xff000000u | (b << 8) | 0xff;
                ++word;
            }
            ++node;
        }
    }
}
