// TbSysCommand.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSysCommand code, named after it.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "bfl_FileSystem.h"
#include "LbTime.h"
#include "PlaceHolderTypes.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

// The system command constructors of the sound library (unit w79b, sound symbols shared): each stores its vptr (the Mac
// vtable addresses are in include/snd_commands.h) and at most two words. The Execute bodies are unit w79d's.

namespace SystemCommand {

UpdateAllVolumes::UpdateAllVolumes() : Command() {}

Start::Start(void* handle) : HandleCommand(handle) {}

Process::Process() : Command() {}

StopAll::StopAll() : Command() {}

PauseAll::PauseAll() : Command() {}

RestartAll::RestartAll() : Command() {}

}  // namespace SystemCommand

// TbSoundManager.cpp: the sound manager the game holds (unit w79e, sound symbols shared, file CSprocketsModule.cpp and
// AudioPlaceHolder.cpp in the library). See include/TbSoundManager.h for the layout, the vtable and the calling convention. The
// commands' Execute bodies (unit w79d), the sample store, the device module (p-sound) and TbMapStreamer::SetDroppable
// (0x100231fc) are other units'.

TbError TbSoundManager::ToggleVolumeControl(uint8_t index, uint32_t on) {
    if (index < 4) {
        mControlOn[index] = static_cast<uint8_t>(on);
        return TbError(0);
    }
    return TbError(-1);
}

TbError TbSoundManager::SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& front,
                                               const TbSoundPosition& top, const TbSoundPosition& velocity) {
    TbSoundSystemInfo info;
    mListenerPosition = &position;
    info.SetListenerPosition(position);
    info.SetListenerDirection(front, top);
    info.SetListenerVelocity(velocity);
    SystemCommand::SetSystemInfo command(info);
    return SystemCommand(command);
}

void* TbSoundManager::ConstructSample(void* handle, void*& control, TbSoundSampleInfo& info) {
    control = mAudioSystem->NewControl(handle, info);
    if ((info.mFlags & 0x2000) != 0 && control == nullptr) return nullptr;
    if (control != nullptr) handle = control;
    return mAudioSystem->NewSample(handle, info);
}

void* TbSoundManager::PlaySample(TbSoundSampleInfo& info, void*& control) {
    if ((mOptions & 8) != 0 && (mOptions & 0x100) != 0) info.mField0C |= 2;
    info.mField0C |= 0x2000;
    void* sample = ConstructSample(nullptr, control, info);
    if (sample != nullptr) {
        if (control == nullptr) control = sample;
        SystemCommand::Start command(control);
        TbError error = SystemCommand(command);
        if (static_cast<int32_t>(error) >= 0) return sample;
    }
    return nullptr;
}
// The string goes first, then ~TbSoundCore (the members and bases of C++ run in that order).
TbSoundManager::~TbSoundManager() {}

// TbSysCommand.cpp: the Execute bodies of the sound library's system commands (unit w79h, "sound symbols shared", Code at 0x10000000,
// TOC r2 = 0x1003abf0; the first 41 are in CSprocketsModule.cpp at 0x10027fd4 to 0x1002a0c4, the rest follow the module's static
// initialiser at 0x1002de28 to 0x1002e348), with the five payload accessors of the command base classes. The classes are declared in
// include/TbAudioSystem.h, where the game builds them; TbAudioBody::SystemCommand (unit w79f) hands each one the body.
//
// Calling convention: every Execute returns its TbError through a hidden r3 buffer (the listing's `this`, the command in r4, the body
// in r5); a body that ends with a call to the sound manager or a virtual of the body answers that call's TbError through the same
// buffer. The manager is reached through the body (GetSoundManager 0x10028110), the placeholder list too (GetPlaceHolderList
// 0x1002814c); neither is tested for NULL here (TbAudioBody::SystemCommand tests both before it runs the command), except by
// GetSampleLength for the list.
//
// Findings (kept as the original does them):
//  - GetMaxNumberOfChannels, SetDroppable, SetDistanceMapping, Process, Tick and the answers-0 commands answer 0 whatever the
//    manager or the body answered.
//  - Process runs the placeholders' tick (ProcessPlaceHolders) BEFORE it sends the Process command to the manager.
//  - The volume commands send (float)(uint)v / 100.0f (the listing's bias trick: a double minus 2^52 rounded to a single by fsubs, then
//    an fdivs by the single 100.0f at 0x100332d0), so the game's 0..100 is an unsigned word (101 is allowed, 0xffffffff is not
//    clamped); the control index is a register the decompiler drops (0 master, 1 music, 2 speech and user 1, 3 user 2 and sfx).
//  - SetHandleVolume's holder is changed even when the sample handle is NULL (answers 0 then).
//  - SetNumberOfChannels answers -1 when the count is unchanged (compared with the list's count BEFORE the list clamps it).

namespace {

// A handle is a 32-bit word on the Mac and a pointer on the host: zero-extended, as the list's handles are.
void* HandleOf(int32_t handle) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(handle)));
}

// The listing's volume conversion: the unsigned word as a single, divided by the single 100.0f.
float VolumeOf(uint32_t value) {
    return static_cast<float>(value) / 100.0f;
}

// The volume controls of the manager (the register the decompiler drops).
constexpr uint8_t kMasterControl = 0;
constexpr uint8_t kMusicControl = 1;
constexpr uint8_t kSpeechControl = 2;   // also user 1
constexpr uint8_t kSfxControl = 3;      // also user 2

}  // namespace

namespace TbSysCommand {

// ---- pass-through to the manager --------------------------------------------------------------------------------------------

// 0x10027fd4 (136)
TbError Process::Execute(const TbAudioBody* body) const {
    body->GetPlaceHolderList()->ProcessPlaceHolders();
    const ::SystemCommand::Process command;
    body->GetSoundManager()->SystemCommand(command);
    return TbError(0);
}

// 0x100281a8 (128)
TbError StopAll::Execute(const TbAudioBody* body) const {
    body->GetPlaceHolderList()->StopAllPlaceHolders(0);
    const ::SystemCommand::StopAll command;
    return body->GetSoundManager()->SystemCommand(command);
}

// ---- the commands that do nothing ---------------------------------------------------------------------------------------------

// 0x100282dc (36)
TbError StopAllWithFade::Execute(const TbAudioBody*) const {
    return TbError(-1);
}

// 0x10028350 (108)
TbError PauseAll::Execute(const TbAudioBody* body) const {
    const ::SystemCommand::PauseAll command;
    return body->GetSoundManager()->SystemCommand(command);
}

// 0x10028474 (108)
TbError RestartAll::Execute(const TbAudioBody* body) const {
    const ::SystemCommand::RestartAll command;
    return body->GetSoundManager()->SystemCommand(command);
}

// 0x1002859c (104): the multiplier is the payload read as a float; the manager's own answer is dropped
TbError SetDistanceMapping::Execute(const TbAudioBody* body) const {
    const float multiplier = GetFloatSysValue();
    body->GetSoundManager()->SetDistanceMapping(multiplier);
    return TbError(0);
}

// 0x10028658: lfs f1, 4(r3), the single widened in f1
float SetValue::GetFloatSysValue() const {
    return mFloat;
}

// 0x100286a4 (164): -1 for a NULL handle; the rectangle is the four words at +8
TbError SetBoundingRectangle::Execute(const TbAudioBody* body) const {
    void* handle = GetTbHandleSysValue();
    if (handle == nullptr) return TbError(-1);
    const TbRect rect{mLeft, mTop, mRight, mBottom};
    return body->GetPlaceHolderList()->SetBoundingRectangle(handle, rect);
}

// 0x100287d8
void* SetValue::GetTbHandleSysValue() const {
    return HandleOf(mHandle);
}

// 0x10028824 (216): -1 when the count equals the list's; else the list takes it (clamped, the clamped count comes back) and the
// device gets a system info with that many buffers to use
TbError SetNumberOfChannels::Execute(const TbAudioBody* body) const {
    const uint32_t channels = GetSysValue();
    if (channels == body->GetPlaceHolderList()->GetNumberOfChannels()) return TbError(-1);
    const uint32_t used = body->GetPlaceHolderList()->SetNumberOfChannels(channels);
    TbSoundSystemInfo info;
    info.SetBuffersToUse(used);
    const ::SystemCommand::SetSystemInfo command(info);
    return body->GetSoundManager()->SystemCommand(command);
}

// ---- the payload accessors (8 bytes each: lwz r3, 4(r3); blr) ---------------------------------------------------------------

// 0x1002899c
uint32_t SetValue::GetSysValue() const {
    return static_cast<uint32_t>(mValue);
}

// 0x100289e0 (112): the manager's own answer is dropped
TbError GetMaxNumberOfChannels::Execute(const TbAudioBody* body) const {
    uint32_t* out = reinterpret_cast<uint32_t*>(GetSysValue());
    body->GetSoundManager()->GetMaxNumberOfChannels(*out);
    return TbError(0);
}

// 0x10028aa8
int32_t* GetValue::GetSysValue() const {
    return mOut;
}

// 0x10028aec (192): the request word of the info (+8) asks for bit 0x200; -1 when the device does not answer it
TbError GetNumberOfPlayingChannels::Execute(const TbAudioBody* body) const {
    TbSoundSystemInfo info;
    info.mField8 = 0x200;
    body->GetSoundManager()->GetSystemInfo(info);
    if ((*info.GetMask() & 0x200) == 0) return TbError(-1);
    const uint32_t playing = info.GetSamplesPlaying();
    *GetSysValue() = static_cast<int32_t>(playing);
    return TbError(0);
}

// 0x10028c4c (88): the body's virtual +0x38
TbError FlushCatagory::Execute(const TbAudioBody* body) const {
    const uint32_t category = GetSysValue();
    return body->DeregisterCatagoryBanks(category);
}

// 0x10028cf0 (128): the info made from the speaker configuration (its second constructor)
TbError SetSpeakerConfig::Execute(const TbAudioBody* body) const {
    TbSoundSystemInfo info(*mConfig);
    const ::SystemCommand::SetSystemInfo command(info);
    return body->GetSoundManager()->SystemCommand(command);
}

// 0x10028dc0 (36)
TbError SetFades::Execute(const TbAudioBody*) const {
    return TbError(-1);
}

// 0x10028e2c (36)
TbError SetHalfRate::Execute(const TbAudioBody*) const {
    return TbError(-1);
}

// 0x10028e9c (120): three words (the listing shows two arguments; r7 is the third)
TbError SetQSoundInfo::Execute(const TbAudioBody* body) const {
    const uint32_t qsound = GetSysValue();
    return body->GetSoundManager()->SetQSoundInfo(qsound, static_cast<uint32_t>(mRear), static_cast<uint32_t>(mCrossover));
}

// 0x10028f60 (36)
TbError SetEAX::Execute(const TbAudioBody*) const {
    return TbError(-1);
}

// 0x10028fc8 (120): the payload as a flag; the manager's answer is dropped
TbError SetDroppable::Execute(const TbAudioBody* body) const {
    const bool droppable = GetSysValue() != 0;
    body->GetSoundManager()->SetDroppable(droppable);
    return TbError(0);
}

// 0x1002908c (36)
TbError SetGeometryInfo::Execute(const TbAudioBody*) const {
    return TbError(-1);
}

// 0x10029100 (108): the body's virtual +0x34; -1 for a NULL handle
TbError StopHandle::Execute(const TbAudioBody* body) const {
    void* handle = GetTbHandleSysValue();
    if (handle == nullptr) return TbError(-1);
    return body->StopHandle(handle);
}

// ---- the placeholders of one handle ---------------------------------------------------------------------------------------

// 0x100291b8 (260): -1 without a placeholder; the placeholder takes the volume; with a sound handle the device gets it too
TbError SetHandleVolume::Execute(const TbAudioBody* body) const {
    void* handle = GetTbHandleSysValue();
    CAudioPlaceHolder* holder = nullptr;
    void* sample = body->GetPlaceHolderList()->GetPlaceholderAndSoundHandle(handle, holder);
    if (holder == nullptr) return TbError(-1);
    holder->SetVolume(static_cast<uint32_t>(mVolume));
    if (sample == nullptr) return TbError(0);
    TbSoundSampleInfo info;
    info.SetVolume(static_cast<uint32_t>(mVolume));
    const ::SystemCommand::SetSampleInfo command(sample, info);
    return body->GetSoundManager()->SystemCommand(command);
}

// 0x1002930c (280): the same with the pitch
TbError SetHandleVolumeAndPitch::Execute(const TbAudioBody* body) const {
    void* handle = GetTbHandleSysValue();
    CAudioPlaceHolder* holder = nullptr;
    void* sample = body->GetPlaceHolderList()->GetPlaceholderAndSoundHandle(handle, holder);
    if (holder == nullptr) return TbError(-1);
    holder->SetVolumeAndPitch(static_cast<uint32_t>(mVolume), mPitch);
    if (sample == nullptr) return TbError(0);
    TbSoundSampleInfo info;
    info.SetVolume(static_cast<uint32_t>(mVolume));
    info.SetPitch(mPitch);
    const ::SystemCommand::SetSampleInfo command(sample, info);
    return body->GetSoundManager()->SystemCommand(command);
}

// 0x1002947c (36)
TbError StopHandleWithFade::Execute(const TbAudioBody*) const {
    return TbError(-1);
}

// ---- the volume controls ---------------------------------------------------------------------------------------------------

// 0x100294f4 (132)
TbError SetMasterVolume::Execute(const TbAudioBody* body) const {
    const float volume = VolumeOf(GetSysValue());
    return body->GetSoundManager()->UpdateVolumeControl(kMasterControl, volume);
}

// 0x100295c8 (104)
TbError ToggleMaster::Execute(const TbAudioBody* body) const {
    const uint32_t on = GetSysValue();
    return body->GetSoundManager()->ToggleVolumeControl(kMasterControl, on);
}

// 0x1002967c (132)
TbError SetSpeechVolume::Execute(const TbAudioBody* body) const {
    const float volume = VolumeOf(GetSysValue());
    return body->GetSoundManager()->UpdateVolumeControl(kSpeechControl, volume);
}

// 0x10029750 (104)
TbError ToggleSpeech::Execute(const TbAudioBody* body) const {
    const uint32_t on = GetSysValue();
    return body->GetSoundManager()->ToggleVolumeControl(kSpeechControl, on);
}

// 0x10029804 (132)
TbError SetUser1Volume::Execute(const TbAudioBody* body) const {
    const float volume = VolumeOf(GetSysValue());
    return body->GetSoundManager()->UpdateVolumeControl(kSpeechControl, volume);
}

// 0x100298d8 (104)
TbError ToggleUser1::Execute(const TbAudioBody* body) const {
    const uint32_t on = GetSysValue();
    return body->GetSoundManager()->ToggleVolumeControl(kSpeechControl, on);
}

// 0x1002998c (132)
TbError SetUser2Volume::Execute(const TbAudioBody* body) const {
    const float volume = VolumeOf(GetSysValue());
    return body->GetSoundManager()->UpdateVolumeControl(kSfxControl, volume);
}

// 0x10029a60 (104)
TbError ToggleUser2::Execute(const TbAudioBody* body) const {
    const uint32_t on = GetSysValue();
    return body->GetSoundManager()->ToggleVolumeControl(kSfxControl, on);
}

// 0x10029b14 (132)
TbError SetSFXVolume::Execute(const TbAudioBody* body) const {
    const float volume = VolumeOf(GetSysValue());
    return body->GetSoundManager()->UpdateVolumeControl(kSfxControl, volume);
}

// 0x10029be4 (104)
TbError ToggleSFX::Execute(const TbAudioBody* body) const {
    const uint32_t on = GetSysValue();
    return body->GetSoundManager()->ToggleVolumeControl(kSfxControl, on);
}

// 0x10029c94 (132)
TbError SetMusicVolume::Execute(const TbAudioBody* body) const {
    const float volume = VolumeOf(GetSysValue());
    return body->GetSoundManager()->UpdateVolumeControl(kMusicControl, volume);
}

// 0x10029d68 (104)
TbError ToggleMusic::Execute(const TbAudioBody* body) const {
    const uint32_t on = GetSysValue();
    return body->GetSoundManager()->ToggleVolumeControl(kMusicControl, on);
}

// 0x10029e1c (100): answers the manager's TbError
TbError GetDirectSoundObject::Execute(const TbAudioBody* body) const {
    uint32_t* out = reinterpret_cast<uint32_t*>(GetSysValue());
    return body->GetSoundManager()->GetDirectSoundObject(*out);
}

// 0x10029ed4 (152): the sample's word at +8 (its length); -1 without a list, a placeholder or a sample
TbError GetSampleLength::Execute(const TbAudioBody* body) const {
    CAudioPlaceHolderList* list = body->GetPlaceHolderList();
    if (list != nullptr) {
        CAudioPlaceHolder* holder = list->FindPlaceHolder(HandleOf(mHandle));
        if (holder != nullptr && holder->mSample != nullptr) {
            const uint32_t length = holder->mSample->mF08;
            *GetSysValue() = static_cast<int32_t>(length);
            return TbError(0);
        }
    }
    return TbError(-1);
}

// 0x10029fbc (120): the sfx volume (control 3) times the single 100.0f, converted to an unsigned word
TbError GetSFXVolume::Execute(const TbAudioBody* body) const {
    TbSoundManager* manager = body->GetSoundManager();
    const float volume = manager->GetVolume(kSfxControl);
    int32_t* out = GetSysValue();
    *out = static_cast<int32_t>(CvtDblUl(static_cast<double>(100.0f * volume)));
    return TbError(0);
}

// 0x1002a0c4 (120): the same for the music volume (control 1)
TbError GetMusicVolume::Execute(const TbAudioBody* body) const {
    TbSoundManager* manager = body->GetSoundManager();
    const float volume = manager->GetVolume(kMusicControl);
    int32_t* out = GetSysValue();
    *out = static_cast<int32_t>(CvtDblUl(static_cast<double>(100.0f * volume)));
    return TbError(0);
}

}  // namespace TbSysCommand

// TbAudioBody.cpp: the audio object the game holds, decompiled from the sound library of SimTheme Park 2.04b3 ("sound symbols
// shared", Code at 0x10000000, TOC r2 = 0x1003abf0; file AudioPlaceHolder.cpp, with GetSoundManager, GetPlaceHolderList and
// CreateAudioObject in CSprocketsModule.cpp and the factory in LbMpeg.cpp). See include/TbAudioBody.h for the layout and the
// slots.

// 0x10028110 (8)
TbSoundManager* TbAudioBody::GetSoundManager() const {
    return mManager;
}

// 0x1002814c (36): the CAutoPtr's operator T* (0x100172b4)
CAudioPlaceHolderList* TbAudioBody::GetPlaceHolderList() const {
    return mPlaceHolders;
}

// AudioPlaceHolderList.cpp: CAudioPlaceHolderList, the pool and the priority list of the sound placeholders (unit w79g1, "sound
// symbols shared", file CSprocketsModule.cpp of the library). See include/AudioPlaceHolderList.h for the layout, the handles and
// the pool; the function addresses are in the header.
//
// Conventions of this file. The listing's operation order is kept (the call logs of the test compare it). The virtual calls on a
// placeholder are `lwz r12, 0(this); lwz r12, SLOT(r12); bl 0x1002f154` on the Mac: Release is slot +0x14, PlaySample +0x1c, Stop
// +0x20, Update +0x30, UpdateParameter +0x34 (all declared by include/AudioPlaceHolder.h in that order). Answers a virtual
// returns through a hidden TbError buffer are dropped where the listing drops them. The file's virtuals are Read +0x10, Write
// +0x14, Seek +0x18 and GetPos +0x20 (bfl::FileSystem::CFile). The words of a save block are written with
// SamsUtilities::USwap32 (include/TbSoundBank.h: the file is little-endian, so the host writes native words). The runtime's
// allocator pair: 0x1002ebfc is operator new[] and 0x1002ebcc operator delete[] (CMemBlock::CreateObject allocates the block with the
// first and the list's destructor frees it with the second, without running ~CMemBlock); 0x1002eaac is LbTime_GetClock.

// 0x10028950 (8)
uint32_t CAudioPlaceHolderList::GetNumberOfChannels() const {
    return mNumberOfChannels;
}

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// 0x10028c08 (8 bytes)
uint32_t TbSoundSystemInfo::GetSamplesPlaying() const {
    return mSamplesPlaying;
}

float TbSoundManager::GetVolume(uint8_t index) {
    return mVolumes[index];
}
