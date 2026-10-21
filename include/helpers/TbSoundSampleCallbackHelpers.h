// TbSoundSampleCallbackHelpers.h: file-local helpers that src/sound/TbSoundSampleCallback.cpp shares with
// AdvisorScoring.cpp, TbSoundSamplePipe.cpp and UserCallback.cpp. Before the regroup, the functions using them shared a
// source file (1 file), which had one copy of each; that copy now lives here, static inline, so each file that includes
// this header keeps its own internal copy. The helpers it has in common with StreamableMpeg2Helpers.h come from that
// header, which a file of both groups includes too.
#pragma once

#include "StreamableMpeg2Helpers.h"   // the shared helpers
