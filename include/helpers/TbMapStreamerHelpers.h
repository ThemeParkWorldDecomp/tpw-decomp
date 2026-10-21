// TbMapStreamerHelpers.h: file-local helpers that src/sound/TbMapStreamer.cpp shares with CSprocketsModule.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TbSoundBank.h"

namespace TbMapStreamerFixup {
static inline char*& Text(void* ustring) { return static_cast<SamsUtilities::UString*>(ustring)->mText; }

}  // namespace TbMapStreamerFixup
