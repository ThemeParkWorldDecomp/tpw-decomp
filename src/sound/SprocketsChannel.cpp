// SprocketsChannel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "helpers/CSprocketsModuleHelpers.h"
#include "snd_commands.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbGraphicFile.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include <cstdint>
#include <cstring>

// TbAudioBody_create.cpp: CreateAudioObject (0x1002a18c), the sound library's entry point, decompiled from SimTheme Park 2.04b3
// ("sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0; file CSprocketsModule.cpp).
//
// Why it is alone in its file: the launcher's libtpw.a is a static archive and launcher/pending_manual.cpp defines an interim
// CreateAudioObject (it answers -1, so SOUND_Init fails cleanly) that is always linked. This archive member is therefore never
// pulled in while the interim exists, and no duplicate symbol arises. When the interim is deleted, the real one links by itself.
// The test unit (cmake/units/AudioBody.cmake) lists this file next to src/TbAudioBody.cpp.

// 0x1002a18c (196): the sound library's entry point. A new body is stored in *audio before it is started; a failed start deletes
// it (Release, virtual slot +8) and clears *audio; the answer is 0 or -1. (The Mac does not test the allocation for NULL before
// the virtual call.)
TbError CreateAudioObject(const char* name, TbAudioSystem** audio, const CAudioConfig& config) {
    TbAudioBody* body = new TbAudioBody();
    *audio = body;
    if ((*audio)->Init(name, config) < 0) {
        (*audio)->Release();
        *audio = nullptr;
        return TbError(-1);
    }
    return TbError(0);
}

// Unit v51107: the small Sound Sprockets pieces of CSprocketsModule.cpp (sound 0x1002879c .. 0x1002c4d8). The Mac objects are raw
// words; a vtable word is the TOC (r2 = 0x1003abf0) plus an offset, stored native-endian.
namespace {
constexpr uint32_t kToc = 0x1003abf0;

}  // namespace

void UHasFlags_ClearFlag(void* flags, uint32_t flag);

void UHasFlags_SetFlag(void* flags, uint32_t flag);

void UHasFlags_Construct(void* flags);

namespace SamsUtilities {

// 0x1002a5c4 (56): the low byte decides.
void UCommand::SetImmediate(uint8_t immediate) {
    if (immediate != 0)
        UHasFlags_SetFlag(this, 1);
    else
        UHasFlags_ClearFlag(this, 1);
}

void UCommand_Construct(UCommand* command);

// 0x1002a6e0 (8)
template <>
UReferenceCounter<USoundDescription>::operator USoundDescription*() const {
    return mObject;
}

// 0x1002a774 (56)
UCommandPlay::UCommandPlay() {
    UCommand_Construct(this);
    StoreWord(this, 4, kToc + 0x3420);
}

// 0x1002adfc (20)
uint32_t UReferenceCounted::AddReference() { return ++mCount; }

}  // namespace SamsUtilities

// SamsUtilities::UCommandLoadSound constructor (0x1002ac30) and destructor with the hidden delete flag (0x1002ab8c).

namespace SamsUtilities {

UCommandLoadSound::UCommandLoadSound(const UReferenceCounter<USoundDescription>& description) {
    UCommand_Construct(reinterpret_cast<UCommand*>(this));
    mVtable4 = 0x1003dfdc;
    USoundDescriptionCounter_Copy(&mDescription, &description);
}

UCommandLoadSound* UCommandLoadSound::Destruct(int16_t deleteFlag) {
    if (this != nullptr) {
        mVtable4 = 0x1003dfdc;
        USoundDescriptionCounter_Destruct(&mDescription, -1);
        if (deleteFlag > 0) USoundChannel_Free(this);
    }
    return this;
}

}  // namespace SamsUtilities

// TpBody::Load / Save (the save-game block of the listener and the placeholder list), the Sound Sprockets command constructors
// and UReferenceCounted::RemoveReference. Unit v51014.

namespace SamsUtilities {

// 0x1002af28 (120): one reference less; at zero the object disposes itself (virtual +0xc) and is deleted (deleting destructor, +8,
// flag 1); answers 0 then, else the new count.
uint32_t UReferenceCounted::RemoveReference() {
    mCount = mCount - 1;
    if (mCount == 0) {
        Dispose();
        delete this;
        return 0;
    }
    return mCount;
}

UCommandSoundType::UCommandSoundType(SoundType type) {   // 0x1002b158
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003dfb4);
    Put32(mStorage, 8, static_cast<uint32_t>(type));
}

}  // namespace SamsUtilities
