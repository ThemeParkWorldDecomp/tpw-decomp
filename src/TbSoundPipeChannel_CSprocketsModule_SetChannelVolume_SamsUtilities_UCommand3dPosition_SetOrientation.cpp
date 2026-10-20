// Unit v51053: the position commands of CSprocketsModule.cpp, the small GetTag answers, the speaker kinds and the static initialiser.
#include "CSprocketsModule.h"

#include <cstring>
#include <new>

#include "CMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundPosition.h"

uint32_t gSinit_1003f3b8[3];   // the __register_global_object record at 0x1003f3b8

namespace {
// The command bytes are the Mac's (offsets in comments); a double may sit at a 4-byte aligned offset.
void StoreDouble(SamsUtilities::UCommand* command, uint32_t offset, double value) {
    std::memcpy(command->mStorage + offset, &value, sizeof(value));
}
void StoreWord(SamsUtilities::UCommand* command, uint32_t offset, uint32_t value) {
    std::memcpy(command->mStorage + offset, &value, sizeof(value));
}
}  // namespace

namespace SamsUtilities {

// 0x1002cb7c (20): neg, addic -1, subfe: 1 when the value is not 0
bool UStatus::IsSet() const { return mValue != 0; }

// 0x1002cf68 (12)
UStatus::UStatus() { mValue = 0; }

// 0x1002ce54 (12)
UStatus& UStatus::operator=(const UStatus& other) {
    mValue = other.mValue;
    return *this;
}

// 0x1002cbc8 (56)
void UCommand3dPosition::SetLocation(double x, double y, double z) {
    StoreDouble(this, 0xc, x);
    StoreDouble(this, 0x14, y);
    StoreDouble(this, 0x1c, z);
    UHasFlags_SetFlag(mStorage + 8, 1);
}

// 0x1002cc4c (56)
void UCommand3dPosition::SetVelocity(double x, double y, double z) {
    StoreDouble(this, 0x24, x);
    StoreDouble(this, 0x2c, y);
    StoreDouble(this, 0x34, z);
    UHasFlags_SetFlag(mStorage + 8, 2);
}

// 0x1002d420 (56)
void UCommand3dPosition::SetOrientation(double x, double y, double z) {
    StoreDouble(this, 0x3c, x);
    StoreDouble(this, 0x44, y);
    StoreDouble(this, 0x4c, z);
    UHasFlags_SetFlag(mStorage + 8, 4);
}

// 0x1002d39c (56)
void UCommand3dPosition::SetUpVector(double x, double y, double z) {
    StoreDouble(this, 0x54, x);
    StoreDouble(this, 0x5c, y);
    StoreDouble(this, 0x64, z);
    UHasFlags_SetFlag(mStorage + 8, 8);
}

// 0x1002cd48 (64): the argument is not used
UCommand3dPosition::UCommand3dPosition(uint32_t) {
    UCommand_ConstructBase(this);
    StoreWord(this, 4, 0x1003dd88);
    UHasFlags_Construct(mStorage + 8);
}

// 0x1002d1a0 (56)
UCommandListenerPosition::UCommandListenerPosition(uint32_t on) : UCommand3dPosition(on) { StoreWord(this, 4, 0x1003dd2c); }

// 0x1002ccd0 (56)
UCommandPosition::UCommandPosition(uint32_t on) : UCommand3dPosition(on) { StoreWord(this, 4, 0x1003ddb8); }

// 0x1002ceac (56)
UCommandPreroll::UCommandPreroll(uint32_t) {
    UCommand_ConstructBase(this);
    StoreWord(this, 4, 0x1003dd54);
}

uint32_t UCommandSpeakerType::GetTag() { return 0x73706b72; }     // 0x1002d884 'spkr'
uint32_t UCommandListenerPosition::GetTag() { return 0x6c697370; }   // 0x1002d8d4 'lisp'
uint32_t UCommandPreroll::GetTag() { return 0x70726572; }         // 0x1002d928 'prer'
uint32_t UCommandVolume::GetTag() { return 0x766f6c75; }          // 0x1002d974 'volu'
uint32_t UCommandPosition::GetTag() { return 0x706f736e; }        // 0x1002d9c0 'posn'
uint32_t UCommandPause::GetTag() { return 0x70617573; }           // 0x1002da0c 'paus'
uint32_t UCommandStop::GetTag() { return 0x73746f70; }            // 0x1002da58 'stop'
uint32_t UCommandSoundDistance::GetTag() { return 0x73726364; }   // 0x1002daa0 'srcd'
uint32_t UCommandListenerScale::GetTag() { return 0x6c697373; }   // 0x1002daf4 'liss'
uint32_t UCommandCPULoad::GetTag() { return 0x6370756c; }         // 0x1002db48 'cpul'
uint32_t UCommandSoundType::GetTag() { return 0x7372636d; }       // 0x1002dc48 'srcm'
uint32_t UCommandLoadSound::GetTag() { return 0x6c736e64; }       // 0x1002dc98 'lsnd'
uint32_t UCommandPlay::GetTag() { return 0x706c6179; }            // 0x1002dce8 'play'

}  // namespace SamsUtilities

uint32_t CSprocketsSpeaker::Stereo() const { return 0; }       // 0x1002db94
int32_t CSprocketsSpeaker::Headphones() const { return 2; }    // 0x1002dbd0
int32_t CSprocketsSpeaker::Mono() const { return 1; }          // 0x1002dc10

// SetSpeedOfSound and SetListenerVelocity are weak: platform/sound_module.cpp holds the host versions the launcher
// links (as SetListenerDirection in the unit v5997 file), the unit test library takes these.
// 0x1002d034 (36)
__attribute__((weak)) TbError CSprocketsModule::SetSpeedOfSound(float) { return TbError(0); }

// 0x1002d4a4 (192): like SetListenerPosition, but the velocity setter
__attribute__((weak)) TbError CSprocketsModule::SetListenerVelocity(const TbSoundPosition& velocity) {
    CSprocketsChannel* ch = Channels()[0];
    if (ch == nullptr) return TbError(-1);
    float x, y, z;
    velocity.Get(x, y, z);
    SamsUtilities::UCommandListenerPosition command(1);
    command.SetVelocity(x, y, z);
    SamsUtilities::UStatus status = SamsUtilities::SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002dd30 (120): the same two guarded objects as __sinit_TbMMFileBank_cpp
void __sinit_CSprocketsModule_cpp() {
    using Pool = SamsUtilities::MpegFilePlayerPool;
    if (static_cast<int8_t>(gPoolQueueBuilt) == 0) {
        new (&Pool::gPoolQueue) Pool::Queue();
        gPoolQueueBuilt = 1;
    }
    if (static_cast<int8_t>(gPoolCleanupBuilt) == 0) {
        __register_global_object(&Pool::gPoolCleanup, gSinitDtor_1003352c, gSinit_1003f3b8);
        gPoolCleanupBuilt = 1;
    }
}
