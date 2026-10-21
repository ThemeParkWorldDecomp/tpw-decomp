// TbSoundSampleWAVHelpers.h: file-local helpers that src/sound/TbSoundSampleWAV.cpp shares with AdvisorScoring.cpp,
// TbSoundCodecLoop.cpp, TbSoundSampleWAVFile.cpp and TimerCodec.cpp. Before the regroup, the functions using them
// shared a source file (1 file), which had one copy of each; that copy now lives here, static inline, so each file that
// includes this header keeps its own internal copy. The helpers it has in common with StreamableMpeg2Helpers.h come
// from that header, which a file of both groups includes too.
#pragma once

#include "StreamableMpeg2Helpers.h"   // the shared helpers

// The Mac's LB_MIN<int,int> (0x1000e7d8, 0x100112f8 and 0x10011a98, 24 bytes each): `a < b ? a : b` on references, signed.
// (Unit w79a adds LB_MIN<unsigned long> to a header of its own; this one stays private to this file.)
template <class T>
static inline const T& LB_MIN(const T& a, const T& b) {
    return a < b ? a : b;
}
