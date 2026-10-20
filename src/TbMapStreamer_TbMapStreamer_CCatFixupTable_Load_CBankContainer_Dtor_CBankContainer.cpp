#include <new>
#include "TbMapStreamer.h"
#include "TbError.h"
#include "TbSoundBank.h"   // SamsUtilities::USwapInplace32(uint32_t&), the sound library's file-endian swap

// 0x10023afc: CBankContainer::~CBankContainer is defined inline in TbMapStreamer.h (it stores the vtable, then
// destroys mStorage (+0x24), mName (+0xc) and the TbRefRelease base; the hidden delete flag is handled by the compiler).
