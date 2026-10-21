// CSprocketsModule.cpp: name proven by __sinit_CSprocketsModule_cpp in the sound library.

#include "../platform/sound_module.h"
#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "helpers/CSprocketsModuleHelpers.h"
#include "helpers/TbMapStreamerHelpers.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbError.h"
#include "TbGraphicFile.h"
#include "TbMapStreamer.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundBank.h"   // SamsUtilities::USwapInplace32(uint32_t&), the sound library's file-endian swap
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundPosition.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

// CSprocketsModule, second part (unit v5997): the constructor, Close, Tick, StreamData, SetChannelInfo and SetListenerDirection.
// The Mac answers through a hidden r3 TbError buffer; the host returns the TbError.

using namespace SamsUtilities;

// 0x1002b234 (108): the base constructor, the vtable, then a CSprocketsSpeaker (4 bytes from the sound library's allocator,
// constructed only when the allocation worked) handed to the speaker configuration.
__attribute__((weak)) CSprocketsModule::CSprocketsModule() {
    CSprocketsSpeaker* speaker = static_cast<CSprocketsSpeaker*>(UAllocatorMake(4));
    if (speaker != nullptr) new (speaker) CSprocketsSpeaker();
    TbSpeaker::ConfigSetModuleSpeaker(reinterpret_cast<TbSpeaker::Module*>(speaker));
}

// CSprocketsModule::Init (unit v5L24), sound symbols shared 0x1002b408 (416). The Mac answers through a hidden r3 TbError
// buffer; the host returns the TbError.

// Init is weak: platform/sound_module.cpp holds the host version (the SDL device) the launcher links, as v51053 did for
// SetSpeedOfSound and SetListenerVelocity; the unit test library takes this one.
// 0x1002b408 (416): the base's Init (its answer dropped), then the channel array (mChannels, +0x14), -1 when it cannot be made.
// Three commands are built once: UCommandSoundDistance(400.0) and UCommandListenerScale(0.01f, 0, 0), both immediate, and
// UCommandCPULoad with the info's double at +0xc (not immediate). Every channel is allocated (0x4c bytes) and constructed;
// one that cannot be allocated leaves its slot as the allocation left it and gets nothing. The first channel that was made
// (not necessarily channel 0) gets the CPU load and the listener scale; every one gets the sound distance. The speaker
// configuration is a TbSpeaker::Stereo, sent through the virtual SetSpeakerConfig (answer dropped); the MPEG file player pool is
// set to be kept full and Tick runs. The commands have no destructor calls.
__attribute__((weak)) TbError CSprocketsModule::Init(const TbSoundSystemInfo& info) {
    TbSoundSystemModule::Init(info);
    // Host: the array holds native pointers (the Mac's was mBuffers << 2 bytes), and mChannels is the host record's.
    SetChannels(static_cast<CSprocketsChannel**>(AllocBuffer_(mBuffers * static_cast<uint32_t>(sizeof(CSprocketsChannel*)))));
    if (Channels() == nullptr) return TbError(-1);

    UCommandSoundDistance distance(400.0);
    UCommandListenerScale scale(0.01f, 0.0, 0.0);   // the Mac's constant is the float 0.01 widened (0x3f847ae140000000)
    scale.SetImmediate(1);
    // The info's +0xc and +0x10 words are one double on the Mac (TbAudioBody::Init stores the quality there).
    const uint64_t loadBits = (static_cast<uint64_t>(info.mFieldC) << 32) | info.mField10;
    double load;
    std::memcpy(&load, &loadBits, sizeof load);
    UCommandCPULoad cpuLoad(load);
    distance.SetImmediate(1);

    bool first = true;
    for (uint32_t i = 0; i < mBuffers; i++) {
        // Host: the native object's size (the Mac's is 0x4c).
        CSprocketsChannelMac* channel = static_cast<CSprocketsChannelMac*>(UAllocatorMake(sizeof(CSprocketsChannelMac)));
        if (channel != nullptr) new (channel) CSprocketsChannelMac();
        if (channel == nullptr) continue;
        // Host: the array's element type is the platform's channel class (platform/sound_module.h owns the name).
        Channels()[i] = reinterpret_cast<CSprocketsChannel*>(channel);
        if (first) {
            SendToChannel(channel, cpuLoad);
            SendToChannel(channel, scale);
            first = false;
        }
        SendToChannel(channel, distance);
    }

    uint32_t stereo;   // the Mac's 4-byte TbSpeaker::Stereo: its vtable word
    TbSpeaker::StereoConstruct(&stereo);
    SetSpeakerConfig(*reinterpret_cast<const TbSpeaker::Config*>(&stereo));
    MpegFilePlayerPool::SetPoolKeepFull(1);
    Tick();
    return TbError(0);
}

// The sound library's MPEG file player (unit w80c; program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp):
// CMpegMacFilePlayerUnpooled, its pool SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10> and the pool's queue
// UInterruptSafeQueue<unsigned char[28004]>. See include/CMpegBase.h for the layout.
//
// How the host differs from the Mac:
//  - The Mac started a File Manager read (PBReadAsync) into the buffer that was not being decoded and trusted it to be complete
//    by the time the decoder had used up the other one. The host has no asynchronous File Manager: RequestBuffer performs the
//    read at once through tpw::ReadFileReference (platform/file_handles.cpp, the open file named by ioRefNum), which is the only
//    order the original was correct under. The result is what the Mac's completion would have stored: ioActCount the bytes
//    read, ioPosOffset advanced by them, ioResult 0, -39 (eofErr) for a short read or -51 (rfNumErr) for an unknown reference.
//    The positioning mode is taken as fsFromStart, which is the one the player sets.
//  - Enqueue and Dequeue (the Mac OS queue calls) are written out below. The host has no interrupts, but the players are
//    returned to the pool from the audio thread, so they lock.
//  - DebugStr and SamsUtilities::UDebug are the platform layer's (they log). The sound library's own UDebug (0x10003950, 36
//    bytes) returns at once; the platform's is the one the host links, so it has no definition here.
//  - __sinit_LbMpeg_cpp (0x10003acc) constructs the file's static objects in this order: CMpegBase_DecodeMethod at 0x1003e370
//    and _CS_CMpegBase at 0x1003e360 (both in src/TbMpegBase.cpp), then gPoolQueue and gPoolCleanup (below), and registers their
//    destructors to run at exit. On the host these are the file's static objects, and the destructors run at exit.

using Pool = SamsUtilities::MpegFilePlayerPool;
using Queue = Pool::Queue;

// ---------------------------------------------------------------- the queue

namespace SamsUtilities {

// 0x10007ffc: for the player's size, the first element of the queue whose player has no read in flight, taken off the queue;
// NULL when there is none (no heap fallback). Any other size goes to the heap.
// (The game tests the answer: NULL is a pool with no idle player, not an allocation failure.)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnew-returns-null"
#pragma clang diagnostic pop

// 0x1002b5ec.
template <>
void Pool::SetPoolKeepFull(uint8_t flag) { gPoolKeepFull = flag; }

// 0x1002b964. Fill the queue up to the target (one element, or, with keep-full, until it is there or an element cannot be
// made); then, while it holds more than the target, free idle elements from the head (one only, unless keep-full).
template <>
void Pool::MaintainPoolLevel() {
    while (static_cast<uint32_t>(gPoolQueue.GetSize()) < gPoolTarget) {
        Queue::Element* element = MakeNewPoolElement();
        if (element == nullptr) break;
        gPoolQueue.Add(element);
        if (gPoolKeepFull == 0) break;
    }
    Queue::Element* element = gPoolQueue.GetHead();
    while (element != nullptr) {
        if (static_cast<uint32_t>(gPoolQueue.GetSize()) <= gPoolTarget) break;
        Queue::Element* next = element->GetNext();
        void* player = element->GetData();
        if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
            ::operator delete(element);
            if (gPoolKeepFull == 0) return;
        }
        element = next;
    }
}

// 0x1002bb08: an element (0x6d6c bytes on the Mac: 8 and the player's 0x6d64) with its player cleared through the allocator
// (UManager<UAllocator>::operator-> makes the allocator on first use, 0x1002bbe4).
template <>
Pool::Queue::Element* Pool::MakeNewPoolElement() {
    Queue::Element* element = static_cast<Queue::Element*>(::operator new(sizeof(Queue::Element) + sizeof(CMpegMacFilePlayerUnpooled)));
    if (element != nullptr) {
        ::new (element) Queue::Element();
        void* data = element->GetData();
        if (gMemory == nullptr) gMemory = new UAllocator;
        gMemory->Clear(data, sizeof(CMpegMacFilePlayerUnpooled));
    }
    return element;
}

}  // namespace SamsUtilities

// TpBody::Load / Save (the save-game block of the listener and the placeholder list), the Sound Sprockets command constructors
// and UReferenceCounted::RemoveReference. Unit v51014.

namespace SamsUtilities {

namespace {
void PutDouble(uint8_t* base, size_t offset, double value) { std::memcpy(base + offset, &value, 8); }
}  // namespace

UCommandCPULoad::UCommandCPULoad(double load) {   // 0x1002b700
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003de80);
    PutDouble(mStorage, 8, load);
}

UCommandListenerScale::UCommandListenerScale(double x, double y, double z) {   // 0x1002b788
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003de58);
    PutDouble(mStorage, 8, x);
    PutDouble(mStorage, 0x10, y);
    PutDouble(mStorage, 0x18, z);
}

UCommandVolume::UCommandVolume(double volume) {   // 0x1002cdcc
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003dd7c);
    PutDouble(mStorage, 8, volume);
    PutDouble(mStorage, 0x10, volume);
}

// The Mac object: the UCommand base (+0, its own constructor), the vtable word at +4 and the fields. The host keeps the words in the
// base's storage at the Mac offsets (the vtable word is the Mac address of the table).
UCommandSpeakerType::UCommandSpeakerType(SpeakerType type, int32_t flags) {   // 0x1002d7cc
    UCommand_Construct(this);
    Put32(mStorage, 4, 0x1003dce0);
    Put32(mStorage, 8, static_cast<uint32_t>(type));
    Put32(mStorage, 0xc, static_cast<uint32_t>(flags));
}

// The key function of the class (the Mac's destructor lives elsewhere): empty body here, the subclasses own the real ones.
UReferenceCounted::~UReferenceCounted() {}

}  // namespace SamsUtilities

// Unit v51107: the small Sound Sprockets pieces of CSprocketsModule.cpp (sound 0x1002879c .. 0x1002c4d8). The Mac objects are raw
// words; a vtable word is the TOC (r2 = 0x1003abf0) plus an offset, stored native-endian.
namespace {
constexpr uint32_t kToc = 0x1003abf0;

}  // namespace

// 0x1002a63c (16)
void UHasFlags_ClearFlag(void* flags, uint32_t flag) { StoreWord(flags, 0, LoadWord(flags, 0) & ~flag); }

// 0x1002a690 (16)
void UHasFlags_SetFlag(void* flags, uint32_t flag) { StoreWord(flags, 0, LoadWord(flags, 0) | flag); }

// 0x1002a858 (12)
void UHasFlags_Construct(void* flags) { StoreWord(flags, 0, 0); }

// 0x1002c4d8 (8)
void UHasFlags_ConstructWith(void* flags, uint32_t value) { StoreWord(flags, 0, value); }

namespace SamsUtilities {

// 0x1002a7e8 (56)
void UCommand_Construct(UCommand* command) {
    UHasFlags_Construct(command);
    StoreWord(command, 4, kToc + 0x33f8);
}

// 0x1002c468 (56): the same, the flags passed through to UHasFlags(unsigned long).
void UCommand_ConstructFlags(UCommand* command, uint32_t flags) {
    UHasFlags_ConstructWith(command, flags);
    StoreWord(command, 4, kToc + 0x33f8);
}

// 0x1002b838 (72)
UCommandSoundDistance::UCommandSoundDistance(double distance) {
    UCommand_Construct(this);
    StoreWord(this, 4, kToc + 0x3240);
    std::memcpy(static_cast<uint8_t*>(static_cast<void*>(this)) + 8, &distance, 8);
}

// 0x1002c1c4 (56)
UCommandStop::UCommandStop() {
    UCommand_Construct(this);
    StoreWord(this, 4, kToc + 0x3218);
}

// 0x1002c3f4 (56): the argument goes to UCommand(unsigned long) as the flags.
UCommandPause::UCommandPause(uint32_t on) {
    UCommand_ConstructFlags(this, on);
    StoreWord(this, 4, kToc + 0x31f0);
}

// 0x1002afec (64): keeps the object and counts a reference unless it is NULL. (The Mac answers the counter itself.)
void USoundDescriptionCounter_Make(UReferenceCounter<USoundDescription>* self, USoundDescription* description) {
    self->mObject = description;
    if (self->mObject != nullptr) reinterpret_cast<UReferenceCounted*>(self->mObject)->AddReference();
}

// 0x1002879c's neighbour: the status test, ok when the word is 0.
int32_t UStatus::IfOk(TbLegacyError ok, TbLegacyError failed) const {
    if (mValue == 0) return ok.mValue;
    return failed.mValue;
}

}  // namespace SamsUtilities

// 0x1002a94c (8), 0x1002a99c (36), 0x1002b1fc (8)
void CSprocketsChannelMac::SetPipe(void* pipe) { mPipe = pipe; }

SamsUtilities::USoundDescription* CSprocketsChannelMac::GetDescription() { return mDescription; }

uint8_t CSprocketsChannelMac::Is3D() { return m3D; }

// 0x1002b2d0 (56)
CSprocketsSpeaker::CSprocketsSpeaker() {
    TbSpeaker::ModuleConstruct(this);
    StoreWord(this, 0, kToc + 0x3394);
}

namespace TbSpeaker {
// 0x1002b338 (12)
void ModuleConstruct(void* module) { StoreWord(module, 0, kToc + 0x336c); }

// 0x1002b6c4 (12)
void ConfigConstruct(void* config) { StoreWord(config, 0, kToc + 0x32a4); }

// 0x1002b65c (56)
void StereoConstruct(void* stereo) {
    ConfigConstruct(stereo);
    StoreWord(stereo, 0, kToc + 0x1cdc);
}
}  // namespace TbSpeaker

// Lead 2026-10-20 (v51107): the three CSprocketsModule functions below are weak, and so are the class's vtable and typeinfo,
// which the destructor (the key function) emits here: platform/sound_module.cpp holds the host versions the launcher links
// (as v51053 did for SetSpeedOfSound and SetListenerVelocity), the unit test library takes these.
asm(".weak_definition __ZTV16CSprocketsModule\n.weak_definition __ZTI16CSprocketsModule\n.weak_definition __ZTS16CSprocketsModule");

// 0x1002b374 (100): the Mac stores its vtable, runs the base destructor with the flag 0 and deletes when the delete flag is
// above 0 (the C++ destructor leaves that to the caller). The NULL test is the caller's.
__attribute__((weak)) CSprocketsModule::~CSprocketsModule() {}

// 0x1002b374 (100) is not translated yet: the destructor (the vtable's key function) is defined by platform/sound_module.cpp
// in the launcher and by the test library's unit file.

// 0x1002b8c4 (112): every pool is topped up, then the import.
__attribute__((weak)) void CSprocketsModule::Tick() {
    _TbSoundSampleMPEG2::TopUp();
    _TbSoundSampleCallback::TopUp();
    CMpegBase::TopUp();
    _TbSoundSampleMPEG2File::TopUp();
    TbTimerCodec::TopUp();
    TbSoundUserCallback::TopUp();
    TbSoundSamplePipe::TopUp();
    _TbSoundCodecLoop::TopUp();
    _TbSoundSampleCompletedCallback::TopUp();
    MpegFilePlayerPool::MaintainPoolLevel();
    SoundTickImport_();
}

namespace SamsUtilities {

// 0x1002bc7c: the object is only initialised when the allocation succeeded; the pointer is stored either way.
template <>
void UManager<UAllocator>::Allocate() {
    void* object = UAllocatorMake(8);
    if (object != nullptr) UAllocatorInit(object);
    mObject = object;
}

}  // namespace SamsUtilities

// 0x1002bd90 (36), 0x1002beb8 (36): both answer TbError(0).
__attribute__((weak)) TbError CSprocketsModule::SetOptions(uint32_t) { return TbError(0); }

// 0x1002bde8 (160): the file player pool is emptied, then every channel is deleted (the deleting destructor, flag 1) and the
// array is freed. The array pointer is not cleared and is read again at every step, as the original did.
__attribute__((weak)) void CSprocketsModule::Close() {
    MpegFilePlayerPool::ClearPool(1);
    if (Channels() != nullptr) {
        for (uint32_t i = 0; i < mBuffers; i++) {
            CSprocketsChannel* channel = Channels()[i];
            if (channel != nullptr) delete channel;
        }
        DisposeBuffer_(Channels());
    }
}

__attribute__((weak)) TbError CSprocketsModule::Set3DDSPFunctionality(uint32_t, uint32_t) { return TbError(0); }

namespace SamsUtilities {
// 0x1002bbe4 (64)
template <>
void* UManager<UAllocator>::operator->() {
    if (mObject == nullptr) Allocate();
    return mObject;
}
}  // namespace SamsUtilities

// 0x1002879c (20)
void TbRect_Construct(TbRect* rect, int32_t left, int32_t top, int32_t right, int32_t bottom) {
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}

// 0x1002bf20 (188): a NULL pipe is ok; otherwise the pipe's channel must exist and have a description. GetSampleFlags is
// called and its answer dropped.
__attribute__((weak)) TbError CSprocketsModule::StreamData(TbSoundPipeChannel* pipe) {
    if (pipe != nullptr) {
        uint32_t index = pipe->GetIndex();
        pipe->GetSampleFlags();
        CSprocketsChannel* channel = Channels()[index];
        if (channel == nullptr) return TbError(-1);
        if (!channel->GetDescription()) return TbError(-1);
        channel->Play();
    }
    return TbError(0);
}

// The sound library's pipe channel (unit w79c, program "sound symbols shared"): TbSoundPipeChannel, the head of a sample's pipe
// chain (TbSoundCodecLoop.cpp, LbMpeg.cpp, TbSoundSampleCallback.cpp, TbSoundSampleWAV.cpp, TimerCodec.cpp and
// CSprocketsModule.cpp hold its out-of-line copies). See include/TbSoundPipeChannel.h for the layout and the slot order.

uint32_t TbSoundPipeChannel::GetSampleFlags() {
    return mSampleFlags;
}

// CSprocketsModule: the channel commands the Mac sent through Sound Sprockets (unit v5250). Each function reads the channel from the
// module's channel array (mChannels, +0x14, no bounds test), builds a SamsUtilities command, hands it to the channel's virtual
// send (the glue 0x1002f154 through slot +0x2c) and answers the status as a TbError.

// 0x1002c068 (164)
TbError CSprocketsModule::StopChannel(uint32_t channel) {
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    UCommandStop command;
    command.SetImmediate(1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002c238 (172)
TbError CSprocketsModule::StopAll() {
    UCommandStop command;
    command.SetImmediate(1);
    for (uint32_t i = 0; i < mBuffers; i++) {
        CSprocketsChannel* ch = Channels()[i];
        if (ch != nullptr) SendToChannel(ch, command);
    }
    return TbError(0);
}

// 0x1002c314 (168): the command is built before the channel is tested
TbError CSprocketsModule::PauseChannel(uint32_t channel) {
    UCommandPause command(1);
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// Unit v51113: the small sound-library functions at the edges of CSprocketsModule.cpp (sound 0x10023310 to 0x1002e8fc) and the
// float square roots / cleanup objects of the codec files.

namespace TbMapStreamerFixup {
// 0x10023674 (8): UString::operator char* answers the text word (+8).
char* StringText(void* ustring) { return static_cast<SamsUtilities::UString*>(ustring)->mText; }
}  // namespace TbMapStreamerFixup

namespace SamsUtilities {
// 0x1002c5fc (56): UCommand's constructor with `on`, then the vtable word 0x1003e010 at +4.
UCommandPlay::UCommandPlay(uint32_t on) {
    UCommand_ConstructOn(this, on);
    const uint32_t vtable = 0x1003e010;
    std::memcpy(mStorage + 4, &vtable, 4);
}
}  // namespace SamsUtilities

// Lead 2026-10-20 (v51113): RestartChannel and RestartAll are weak: platform/sound_module.cpp holds the host versions the
// launcher links (as v51053 did for SetSpeedOfSound and SetListenerVelocity), the unit test library takes these.
// 0x1002c51c (168): the twin of PauseChannel with a play command.
__attribute__((weak)) TbError CSprocketsModule::RestartChannel(uint32_t channel) {
    UCommandPlay command(1);
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002c670 (164)
TbError CSprocketsModule::PauseAll() {
    UCommandPause command(1);
    for (uint32_t i = 0; i < mBuffers; i++) {
        CSprocketsChannel* ch = Channels()[i];
        if (ch != nullptr) SendToChannel(ch, command);
    }
    return TbError(0);
}

// 0x1002c748 (164): the twin of PauseAll with a play command.
__attribute__((weak)) TbError CSprocketsModule::RestartAll() {
    UCommandPlay command(1);
    for (uint32_t i = 0; i < mBuffers; i++) {
        CSprocketsChannel* ch = Channels()[i];
        if (ch != nullptr) SendToChannel(ch, command);
    }
    return TbError(0);
}

// 0x1002c820 (760). The flags tested in the sample info: +0x0c (mField0C) 0x2000 attaches the pipe, 0x20 is the 3D test; the
// mask +0x14 (mFlags) says what to send. A handled bit is cleared; the answer is -1 while a bit is left or the status is set.
// Bug kept: the 3D test ANDs the channel's 3D byte (0 or 1 after Set3D) with 0x20, so it only passes when the byte has bit 5;
// and the velocity branch clears 0x800000 before the position branch tests it again, so a position always zeroes the velocity.
__attribute__((weak)) TbError CSprocketsModule::SetChannelInfo(uint32_t channelIndex, TbSoundPipeChannel* pipe, TbSoundSampleInfo& info) {
    UStatus status{};
    CSprocketsChannel* channel = Channels()[channelIndex];
    if (channel == nullptr) return TbError(-1);
    if ((info.mField0C & 0x2000) != 0) {
        uint32_t sampleFlags = pipe->GetSampleFlags();
        channel->SetPipe(pipe);
        channel->SetDescription(*pipe->GetSampleHeader());
        channel->Set3D((sampleFlags & 1) == 0);
        UCommandPreroll preroll(1);
        status = SendToChannel(channel, preroll);
    }
    if ((info.mFlags & 2) != 0) {
        float volume = static_cast<float>(static_cast<double>(info.mVolume)) / 127.0f;
        UCommandVolume command(volume);
        command.SetImmediate(1);
        status = SendToChannel(channel, command);
        info.mFlags &= ~2u;
    }
    if ((info.mFlags & 0x400000) != 0) info.mFlags &= ~0x400000u;
    if ((info.mFlags & 0x800000) != 0 || (info.mFlags & 1) != 0) {
        UCommandPosition command(1);
        float x, y, z;
        if ((info.mFlags & 0x800000) != 0) {
            if ((channel->Is3D() & (info.mField0C & 0x20)) != 0) {
                info.mVelocity->Get(x, y, z);
                command.SetVelocity(x, y, z);
            }
            info.mFlags &= ~0x800000u;
        }
        if ((info.mFlags & 1) != 0) {
            if ((channel->Is3D() & (info.mField0C & 0x20)) != 0) {
                info.mPosition->Get(x, y, z);
                if ((info.mFlags & 0x800000) == 0) command.SetVelocity(0.0, 0.0, 0.0);
                command.SetLocation(x, y, z);
            }
            info.mFlags &= ~1u;
        }
        status = SendToChannel(channel, command);
    }
    if ((info.mFlags & 0x200) != 0) info.mFlags &= ~0x200u;
    if ((info.mFlags & 0x20) != 0) info.mFlags &= ~0x20u;
    if (status.IsSet() || info.mFlags != 0) return TbError(-1);
    return TbError(0);
}

// Unit v51053: the position commands of CSprocketsModule.cpp, the small GetTag answers, the speaker kinds and the static initialiser.

uint32_t gSinit_1003f3b8[3];   // the __register_global_object record at 0x1003f3b8

namespace {
// The command bytes are the Mac's (offsets in comments); a double may sit at a 4-byte aligned offset.
void StoreDouble(SamsUtilities::UCommand* command, uint32_t offset, double value) {
    std::memcpy(command->mStorage + offset, &value, sizeof(value));
}
void StoreWord_2(SamsUtilities::UCommand* command, uint32_t offset, uint32_t value) {
    std::memcpy(command->mStorage + offset, &value, sizeof(value));
}
}  // namespace

namespace SamsUtilities {

// 0x1002cb7c (20): neg, addic -1, subfe: 1 when the value is not 0
bool UStatus::IsSet() const { return mValue != 0; }

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

// 0x1002ccd0 (56)
UCommandPosition::UCommandPosition(uint32_t on) : UCommand3dPosition(on) { StoreWord_2(this, 4, 0x1003ddb8); }

// 0x1002cd48 (64): the argument is not used
UCommand3dPosition::UCommand3dPosition(uint32_t) {
    UCommand_ConstructBase(this);
    StoreWord_2(this, 4, 0x1003dd88);
    UHasFlags_Construct(mStorage + 8);
}

// 0x1002ce54 (12)
UStatus& UStatus::operator=(const UStatus& other) {
    mValue = other.mValue;
    return *this;
}

// 0x1002ceac (56)
UCommandPreroll::UCommandPreroll(uint32_t) {
    UCommand_ConstructBase(this);
    StoreWord_2(this, 4, 0x1003dd54);
}

// 0x1002cf68 (12)
UStatus::UStatus() { mValue = 0; }

// 0x1002d1a0 (56)
UCommandListenerPosition::UCommandListenerPosition(uint32_t on) : UCommand3dPosition(on) { StoreWord_2(this, 4, 0x1003dd2c); }

// 0x1002d39c (56)
void UCommand3dPosition::SetUpVector(double x, double y, double z) {
    StoreDouble(this, 0x54, x);
    StoreDouble(this, 0x5c, y);
    StoreDouble(this, 0x64, z);
    UHasFlags_SetFlag(mStorage + 8, 8);
}

// 0x1002d420 (56)
void UCommand3dPosition::SetOrientation(double x, double y, double z) {
    StoreDouble(this, 0x3c, x);
    StoreDouble(this, 0x44, y);
    StoreDouble(this, 0x4c, z);
    UHasFlags_SetFlag(mStorage + 8, 4);
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

TbSampleHeader* TbSoundPipeChannel::GetSampleHeader() {
    return &mSampleHeader;
}

// 0x1002cfa8 (68): only tests that the channel exists
TbError CSprocketsModule::GetPosition(uint32_t channel, TbSoundPosition& position) {
    (void)position;
    if (Channels()[channel] == nullptr) return TbError(-1);
    return TbError(0);
}

// SetSpeedOfSound and SetListenerVelocity are weak: platform/sound_module.cpp holds the host versions the launcher
// links (as SetListenerDirection in the unit v5997 file), the unit test library takes these.
// 0x1002d034 (36)
__attribute__((weak)) TbError CSprocketsModule::SetSpeedOfSound(float) { return TbError(0); }

// 0x1002d090 (192): sent to channel 0
TbError CSprocketsModule::SetListenerPosition(const TbSoundPosition& position) {
    CSprocketsChannel* ch = Channels()[0];
    if (ch == nullptr) return TbError(-1);
    float x, y, z;
    position.Get(x, y, z);
    UCommandListenerPosition command(1);
    command.SetLocation(x, y, z);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002d220 (280): channel 0 gets a listener command with the orientation and the up vector.
__attribute__((weak)) TbError CSprocketsModule::SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top) {
    UStatus status{};
    CSprocketsChannel* channel = Channels()[0];
    if (channel == nullptr) return TbError(-1);
    UCommandListenerPosition command(1);
    float x, y, z;
    front.Get(x, y, z);
    command.SetOrientation(x, y, z);
    top.Get(x, y, z);
    command.SetUpVector(x, y, z);
    status = SendToChannel(channel, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

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

// 0x1002d5b4 (200): the volume (0..127) as a single-precision fraction
TbError CSprocketsModule::SetChannelVolume(uint32_t channel, TbSoundSampleInfo& info) {
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    float volume = static_cast<float>(static_cast<double>(info.mVolume)) / 127.0f;
    UCommandVolume command(volume);
    command.SetImmediate(1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002d6cc (176): sent to channel 0
TbError CSprocketsModule::SetSpeakerConfig(const TbSpeaker::Config& config) {
    CSprocketsChannel* ch = Channels()[0];
    if (ch == nullptr) return TbError(-1);
    UCommandSpeakerType command(GetConfigSpeakerType(config), 0);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

uint32_t CSprocketsSpeaker::Stereo() const { return 0; }       // 0x1002db94
int32_t CSprocketsSpeaker::Headphones() const { return 2; }    // 0x1002dbd0
int32_t CSprocketsSpeaker::Mono() const { return 1; }          // 0x1002dc10

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

// 0x10023afc: CBankContainer::~CBankContainer is defined inline in TbMapStreamer.h (it stores the vtable, then
// destroys mStorage (+0x24), mName (+0xc) and the TbRefRelease base; the hidden delete flag is handled by the compiler).

// TbMapStreamer: ReShuffleSoundForDroppable 0x1002309c (276) and SaveCatgoryFixupTable 0x100233e0 (576), unit v5047.
