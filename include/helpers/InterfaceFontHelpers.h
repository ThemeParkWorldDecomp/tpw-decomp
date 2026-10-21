// InterfaceFontHelpers.h: file-local helpers that src/game/ui/InterfaceFont.cpp shares with OnlineParser.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "ui_main.h"

// The Mac's links are 32-bit words: offsets from the arena base on the host (Ptr32.h).
static inline uint8_t* NodeAt(uint32_t word) { return tpw::gArenaBase + word; }

static inline uint32_t WordOf(const void* p) { return tpw::ArenaOffset(p); }
