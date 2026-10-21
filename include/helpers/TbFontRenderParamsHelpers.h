// TbFontRenderParamsHelpers.h: file-local helpers that src/ltms/TbFontRenderParams.cpp shares with TbITable.cpp and
// TbIUnicodeToMB.cpp. Before the regroup, the functions using them shared a source file (1 file), which had one copy of
// each; that copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TbIResourceIndex.h"

// What is left of the file after the header.
static inline uint32_t Remaining(bfl::FileSystem::CFile& file) {
    return static_cast<uint32_t>(file.GetSize()) - static_cast<uint32_t>(file.GetPos());
}
