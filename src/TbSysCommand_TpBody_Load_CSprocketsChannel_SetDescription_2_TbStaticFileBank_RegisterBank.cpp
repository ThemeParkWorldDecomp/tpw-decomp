// Unit v51045: TbStaticFileBank::RegisterBank and three static initialisers of the sound library.
#include <cstdint>
#include <cstring>
#include <new>
#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "snd_commands.h"
#include "CSprocketsModule.h"
#include "CMpegBase.h"
#include "platform/samsutils.h"

// 0x100130e8 (252)
// RegisterBank is defined inline in include/TbSoundBank.h.

// Mac data the initialisers register (their addresses are in the names).
uint32_t gSinit_1003f2a8[1], gSinit_1003f2ac[3], gSinit_1003f284[1], gSinit_1003f288[3];
uint32_t gSinitDtor_10033d3c[2], gSinitDtor_10033d34[2], gSinitDtor_1003352c[2], gSinitDtor_100336dc[2];
uint32_t gSinit_1003ebe8[1], gSinit_1003ebec[3], gSinit_1003ebf8[3], gSinit_1003ebc8[3];
uint8_t gPoolQueueBuilt;     // 0x1003e329
uint8_t gPoolCleanupBuilt;   // 0x1003e328

using Pool = SamsUtilities::MpegFilePlayerPool;

// 0x1000f8d4 (68)
void __sinit_TbSoundSamplePipe_cpp() {
    __register_global_object(gSinit_1003f2a8, gSinitDtor_10033d3c, gSinit_1003f2ac);
    __register_global_object(gSinit_1003f284, gSinitDtor_10033d34, gSinit_1003f288);
}

// 0x10009934 (120)
void __sinit_TbMMFileBank_cpp() {
    if (static_cast<int8_t>(gPoolQueueBuilt) == 0) {
        new (&Pool::gPoolQueue) Pool::Queue();
        gPoolQueueBuilt = 1;
    }
    if (static_cast<int8_t>(gPoolCleanupBuilt) == 0) {
        __register_global_object(&Pool::gPoolCleanup, gSinitDtor_1003352c, gSinit_1003ebf8);
        gPoolCleanupBuilt = 1;
    }
}

// 0x10008760 (140)
void __sinit_StreamableMpeg2_cpp() {
    __register_global_object(gSinit_1003ebe8, gSinitDtor_100336dc, gSinit_1003ebec);
    if (static_cast<int8_t>(gPoolQueueBuilt) == 0) {
        new (&Pool::gPoolQueue) Pool::Queue();
        gPoolQueueBuilt = 1;
    }
    if (static_cast<int8_t>(gPoolCleanupBuilt) == 0) {
        __register_global_object(&Pool::gPoolCleanup, gSinitDtor_1003352c, gSinit_1003ebc8);
        gPoolCleanupBuilt = 1;
    }
}
