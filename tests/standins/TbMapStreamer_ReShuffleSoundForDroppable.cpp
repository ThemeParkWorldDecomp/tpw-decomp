// The weak stand-ins of the unit library tpw_unit_TbMapStreamer_ReShuffleSoundForDroppable
// (cmake/units/TbMapStreamer_ReShuffleSoundForDroppable.cmake), as
// src/TbMapStreamer_TbMapStreamer_ReShuffleSoundForDroppable.cpp of before the regroup (v5L30b) defined them,
// unchanged. The regroup put each function into the src file of its original source file, where these defaults would
// sit next to the real definitions, so it dropped them; the unit library compiles them from here, so that it builds
// what it built before.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "TbError.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundManager.h"
#include "platform/samsutils.h"

namespace TbMapStreamerFixup {
namespace {
char*& Text(void* ustring) { return static_cast<SamsUtilities::UString*>(ustring)->mText; }
}  // namespace
__attribute__((weak)) char* StringText(void* ustring) { return Text(ustring); }
}  // namespace TbMapStreamerFixup
