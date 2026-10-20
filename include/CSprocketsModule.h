// CSprocketsModule.h: the host sound device module, unit p-sound ("sound symbols shared", file CSprocketsModule.cpp, Code at
// 0x10000000, TOC r2 = 0x1003abf0). On the Mac it kept one Sound Sprockets channel (CSprocketsChannel, a SamsUtilities::
// USoundChannel) per buffer; here the channels are SDL3 audio streams (platform/sound_module.cpp and platform/sound_device.cpp,
// the platform layer). The class keeps the Mac's name and its base's slot order so that CModuleFactory::GenerateModule
// (unit w79b) is untouched, as the macdoze shims keep their Win32 names.
//
// Mac layout: the 0x14-byte TbSoundSystemModule, then CSprocketsChannel** mChannels at +0x14 (0x18 bytes). The HOST object has
// no member of its own and stays at the base's 24 bytes, because tests/test_SoundCore.py requires sizeof(CSprocketsModule) ==
// 0x18 (the allocation event of GenerateModule); the channel array, the SDL device and the listener the Mac kept in the Sound
// Sprockets commands it sent to channel 0 live in a record that platform/sound_module.cpp keeps per module (Channels(),
// Device() and Listener() read it).
// The Mac answers through a hidden r3 TbError buffer; the host returns the TbError.
#pragma once
#include "TbSoundSystemModule.h"

namespace tpw {
class SoundDevice;
}
class CSprocketsChannel;   // platform/sound_module.h

// What the Mac sent to channel 0 as UCommandListenerPosition: kept for a later panning pass (the first version plays
// every 3D sound centred, at its volume).
struct CSprocketsListener {
    float position[3] = {0.0f, 0.0f, 0.0f};   // SetListenerPosition
    float front[3] = {0.0f, 0.0f, 0.0f};      // SetListenerDirection, first argument (orientation)
    float top[3] = {0.0f, 0.0f, 0.0f};        // SetListenerDirection, second argument (up vector)
    float velocity[3] = {0.0f, 0.0f, 0.0f};   // SetListenerVelocity
    float scale[3] = {0.01f, 0.0f, 0.0f};     // Init: UCommandListenerScale(0.01, 0.0, 0.0)
    double soundDistance = 400.0;             // Init: UCommandSoundDistance(400.0) on every channel
    uint32_t calls = 0;                       // the listener commands taken since Init, for the tests
};

class CSprocketsModule : public TbSoundSystemModule {
public:
    CSprocketsModule();                       // 0x1002b234 (108)
    ~CSprocketsModule() override;             // +0x08, 0x1002b374 (100)

    TbError SetOptions(uint32_t options) override;                                   // +0x1c, 0x1002bd90 (36): 0
    TbError Init(const TbSoundSystemInfo& info) override;                            // +0x20, 0x1002b408 (416)
    void Close() override;                                                           // +0x24, 0x1002bde8 (160)
    TbError StreamData(TbSoundPipeChannel* channel) override;                        // +0x28, 0x1002bf20 (188)
    TbError StopChannel(uint32_t channel) override;                                  // +0x2c, 0x1002c068 (164)
    TbError StopAll() override;                                                      // +0x30, 0x1002c238 (172)
    TbError PauseChannel(uint32_t channel) override;                                 // +0x34, 0x1002c314 (168)
    TbError RestartChannel(uint32_t channel) override;                               // +0x38, 0x1002c51c (168)
    TbError PauseAll() override;                                                     // +0x3c, 0x1002c670 (164)
    TbError RestartAll() override;                                                   // +0x40, 0x1002c748 (164)
    TbError SetSpeakerConfig(const TbSpeaker::Config& config) override;              // +0x54, 0x1002d6cc (176)
    TbError Set3DDSPFunctionality(uint32_t channel, uint32_t options) override;      // +0x58, 0x1002beb8 (36): 0
    TbError SetSpeedOfSound(float speed) override;                                   // +0x5c, 0x1002d034 (36): 0
    TbError GetPosition(uint32_t channel, TbSoundPosition& position) override;       // +0x68, 0x1002cfa8 (68)
    TbError SetListenerVelocity(const TbSoundPosition& velocity) override;           // +0x74, 0x1002d4a4 (192)
    TbError SetListenerPosition(const TbSoundPosition& position) override;           // +0x78, 0x1002d090 (192)
    TbError SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top) override;   // +0x7c, 0x1002d220 (280)
    TbError SetChannelInfo(uint32_t channel, TbSoundPipeChannel* pipe, TbSoundSampleInfo& info) override;   // +0x88, 0x1002c820 (760)
    TbError SetChannelVolume(uint32_t channel, TbSoundSampleInfo& info) override;    // +0x8c, 0x1002d5b4 (200)

    // Non-virtual: the pool maintenance after Init (0x1002b8c4, 112). On the host it keeps the MPEG file player pool at its level
    // (UPooled<CMpegMacFilePlayerUnpooled, 10>::MaintainPoolLevel); the other pools allocate on demand, see sound_module.cpp.
    void Tick();

    // Host only. Channels() is the Mac's mChannels (+0x14): mBuffers channels, NULL where one could not be made, or NULL
    // before Init and after Close. ChannelAt is the Mac's array index with a bounds test (it indexed without one).
    CSprocketsChannel** Channels() const;
    CSprocketsChannel* ChannelAt(uint32_t index) const;
    tpw::SoundDevice* Device() const;   // the SDL device and its streams, one per channel
    const CSprocketsListener& Listener() const;
};

// ---- unit v5250 (src/TbSoundPipeChannel_CSprocketsModule_SetChannelVolume.cpp): the Sound Sprockets commands the channel functions send.
// The Mac's commands are stack objects of about 0x40 to 0x6c bytes built by the constructors below (imports of main; the host
// has no real ones in the test library, they are recording fakes).
struct TbLegacyError {
    int32_t mValue;
};

namespace SamsUtilities {
enum SpeakerType : int32_t {};

// The status a channel's send answers (4 bytes on the Mac); IfOk<TbLegacyError> 0x1002c144 (28) picks the first value when it is ok.
struct UStatus {
    int32_t mValue;
    int32_t IfOk(TbLegacyError ok, TbLegacyError failed) const;   // 0x1002c144
    bool IsSet() const;                                           // 0x1002cb7c (20), unit v5997
    UStatus();                                                    // 0x1002cf68 (12): mValue = 0, unit v51053
    UStatus(int32_t value) : mValue(value) {}                     // host only: keeps the aggregate-style uses compiling
    UStatus& operator=(const UStatus& other);                     // 0x1002ce54 (12), unit v51053
};

class UCommand {
public:
    void SetImmediate(uint8_t immediate);   // 0x1002a5c4 (56)
    uint8_t mStorage[128];
};
class UCommandStop : public UCommand {
public:
    UCommandStop();                          // 0x1002c1c4 (56)
    uint32_t GetTag();                       // 0x1002da58 (12): 'stop', unit v51053
};
class UCommandPause : public UCommand {
public:
    explicit UCommandPause(uint32_t on);     // 0x1002c3f4 (56)
    uint32_t GetTag();                       // 0x1002da0c (12): 'paus', unit v51053
};
class UCommandVolume : public UCommand {
public:
    explicit UCommandVolume(double volume);  // 0x1002cdcc (76)
    uint32_t GetTag();                       // 0x1002d974 (12): 'volu', unit v51053
};
class UCommandSpeakerType : public UCommand {
public:
    UCommandSpeakerType(SpeakerType type, int32_t flags);   // 0x1002d7cc (88)
    uint32_t GetTag();                                      // 0x1002d884 (12): 'spkr', unit v51053
};
class UCommand3dPosition : public UCommand {
public:
    // Unit v51053: Mac layout: +4 vtable word (0x1003dd88), +8 the UHasFlags<unsigned long>, doubles at +0xc (location), +0x24
    // (velocity), +0x3c (orientation), +0x54 (up vector). The argument is passed through by the derived constructors, unused.
    explicit UCommand3dPosition(uint32_t on);   // 0x1002cd48 (64)
    UCommand3dPosition() {}   // host only: other units' test fakes build derived constructors without the base one
    void SetLocation(double x, double y, double z);   // 0x1002cbc8 (56)
    void SetVelocity(double x, double y, double z);   // 0x1002cc4c (56), unit v5997
    void SetOrientation(double x, double y, double z);   // 0x1002d420 (56), unit v5997
    void SetUpVector(double x, double y, double z);   // 0x1002d39c (56), unit v5997
};
class UCommandListenerPosition : public UCommand3dPosition {
public:
    explicit UCommandListenerPosition(uint32_t on);   // 0x1002d1a0 (56)
    uint32_t GetTag();                                // 0x1002d8d4 (12): 'lisp', unit v51053
};

// The glue 0x1002f154 the Mac calls a channel's virtual send through (vtable at channel +0x10, slot +0x2c): (status*, channel, command).
UStatus SendToChannel(CSprocketsChannel* channel, UCommand& command);
// The same glue through slot +0x08 of the speaker configuration: its speaker type.
SpeakerType GetConfigSpeakerType(const TbSpeaker::Config& config);
}  // namespace SamsUtilities

// ---- unit v5263 (src/TbAudioBody_create_CSprocketsChannel_Callback.cpp): the Sound Sprockets channel class.
// Mac layout (0x10c bytes): the USoundChannel base (0x00..0x13, vtable words at +4, +8 and +0x10), the Sound Manager channel at +0x14,
// the UReferenceCounter<USoundDescription> at +0x18, the pipe at +0x3c, the buffer at +0x40, its size at +0x44, the 3D flag at +0x48
// and a flag at +0x49. The host class keeps the fields in that order with native pointers (the offsets differ, the test exports them).
struct SndCommand {
    uint16_t cmd;       // +0
    int16_t param1;     // +2
    int32_t param2;     // +4
};

namespace SamsUtilities {
enum SoundType : int32_t {};
struct USoundDescription;
template <class T>
struct UReferenceCounter {
    operator T*() const;   // 0x1002a6e0 (8)
    T* mObject;
    uint8_t mRest[28];
};
class UCommandPlay : public UCommand {
public:
    UCommandPlay();   // 0x1002a774 (56)
    explicit UCommandPlay(uint32_t on);   // 0x1002c5fc (56), unit v51113: UCommand's constructor with `on`, then the vtable word 0x1003e010 at +4
    uint32_t GetTag();   // 0x1002dce8 (12): 'play', unit v51053
};
class UCommandSoundType : public UCommand {
public:
    explicit UCommandSoundType(SoundType type);   // 0x1002b158 (72)
    uint32_t GetTag();                            // 0x1002dc48 (12): 'srcm', unit v51053
};
}  // namespace SamsUtilities

struct TbSampleHeader;   // TbSoundBank.h (unit v51032)

// Named ...Mac because platform/sound_module.h owns the name CSprocketsChannel (the host SDL channel).
class CSprocketsChannelMac;
namespace SamsUtilities {
UStatus SendToChannel(CSprocketsChannelMac* channel, UCommand& command);   // the same glue 0x1002f154
}
class CSprocketsChannelMac {
public:
    CSprocketsChannelMac();                  // 0x1002a29c (112)
    ~CSprocketsChannelMac();                 // 0x1002a33c (132), the deleting form frees through operator delete (0x1002ea94)
    void Cleanup();                       // 0x1002a3f0 (168)
    void Callback(SndCommand* command);   // 0x1002a4cc (184)
    void Play();                          // 0x1002a8a0 (124)
    void Set3D(uint8_t on);               // 0x1002b0b8 (112)
    void SetPipe(void* pipe);             // 0x1002a94c (8), unit v51107: mPipe = pipe
    SamsUtilities::USoundDescription* GetDescription();   // 0x1002a99c (36), unit v51107: the counter's object
    uint8_t Is3D();                       // 0x1002b1fc (8), unit v51107: m3D
    void SetDescription(TbSampleHeader& header);   // 0x1002a9f8 (332), unit v51032
    static void operator delete(void* object);

    uint32_t mWord0;                      // +0x00
    uint32_t mVtable4;                    // +0x04 0x1003e0e4
    uint32_t mVtable8;                    // +0x08 0x1003e0f4
    uint32_t mWordC;                      // +0x0c
    uint32_t mVtable10;                   // +0x10 0x1003e0dc
    void* mSndChannel;                    // +0x14
    SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription> mDescription;   // +0x18
    void* mPipe;                          // +0x3c
    void* mBuffer;                        // +0x40
    uint32_t mBufferSize;                 // +0x44
    uint8_t m3D;                          // +0x48
    uint8_t mFlag49;                      // +0x49
};

// Imports of main (recording fakes in the test library): the USoundChannel base and the Sound Manager.
void USoundChannel_Construct(CSprocketsChannelMac* channel, int32_t size);   // 0x1002ec74
void USoundChannel_Destruct(CSprocketsChannelMac* channel, int16_t flag);    // 0x1002ec44
void USoundChannel_Callback(CSprocketsChannelMac* channel, SndCommand* command);   // 0x1002ec5c
void USoundChannel_Free(void* channel);                                   // 0x1002ea94
void SndDoImmediate_(void* sndChannel, const SndCommand* command);        // 0x1002ec2c
void DisposeBuffer_(void* buffer);                                        // 0x1002ebcc

// Unit v5758 (src/SamsUtilities_UCommandLoadSound_UCommandLoadSoun.cpp): the load-sound command.
// Mac layout: the UCommand base (its own constructor, 0x1002a7e8) at +0, the vtable word at +4 (0x1003dfdc), the
// UReferenceCounter<USoundDescription> at +8. Imports of main (recording fakes in the test library):
namespace SamsUtilities {
void UCommand_Construct(UCommand* command);   // 0x1002a7e8
void UCommand_ConstructOn(UCommand* command, uint32_t on);   // 0x1002c468 (56), unit v51113: the base constructor of UCommandPlay(on)
void USoundDescriptionCounter_Copy(UReferenceCounter<USoundDescription>* self, const UReferenceCounter<USoundDescription>* source);   // 0x1002ad08
void USoundDescriptionCounter_Destruct(UReferenceCounter<USoundDescription>* self, int16_t flag);   // 0x1002ae58
class UCommandLoadSound {
public:
    explicit UCommandLoadSound(const UReferenceCounter<USoundDescription>& description);   // 0x1002ac30 (80)
    uint32_t GetTag();   // 0x1002dc98 (12): 'lsnd', unit v51053
    UCommandLoadSound* Destruct(int16_t deleteFlag);   // 0x1002ab8c (100): ~UCommandLoadSound with the hidden delete flag; this may be NULL

    uint32_t mWord0;       // +0x00 (the UCommand base)
    uint32_t mVtable4;     // +0x04 0x1003dfdc
    UReferenceCounter<USoundDescription> mDescription;   // +0x08
};
}  // namespace SamsUtilities

// Unit v51014
namespace SamsUtilities {
class UCommandListenerScale : public UCommand {
public:
    UCommandListenerScale(double x, double y, double z);   // 0x1002b788 (104): vtable word +4 (0x1003de58), doubles +8, +0x10, +0x18
    uint32_t GetTag();                                     // 0x1002daf4 (12): 'liss', unit v51053
};
class UCommandCPULoad : public UCommand {
public:
    explicit UCommandCPULoad(double load);   // 0x1002b700 (72): vtable word +4 (0x1003de80), double +8
    uint32_t GetTag();                       // 0x1002db48 (12): 'cpul', unit v51053
};
// Mac: +0 vptr, +4 the count; virtual +8 the deleting destructor, +0xc Dispose. The host's vptr is wider, the count is a member.
class UReferenceCounted {
public:
    virtual ~UReferenceCounted();    // slot +8 on the Mac
    virtual void Dispose() = 0;      // slot +0xc on the Mac
    uint32_t RemoveReference();      // 0x1002af28 (120)
    uint32_t AddReference();         // 0x1002adfc (20), unit v51107: ++mCount, answers the new count
    uint32_t mCount;                 // Mac +4
};
}  // namespace SamsUtilities

// Unit v51032: imports of main and in-binary stand-ins that CSprocketsChannelMac::SetDescription and the counter call (recording
// fakes in the test library).
namespace SamsUtilities {
void* UAllocatorMake(int32_t size);   // 0x1002eac4 (CMpegBase.h declares the same)
void USoundDescriptionCounter_Make(UReferenceCounter<USoundDescription>* self, USoundDescription* description);   // 0x1002afec (64)
void UReferenceCounted_AddReference(UReferenceCounted* object);   // 0x1002adfc (20)
// 0x1002ecd4: the USoundBuffer constructor (buffer, size, bits, rate, compression 'NONE' = 0x4e4f4e45, channels).
void USoundBuffer_Construct(void* object, void* buffer, uint32_t size, uint8_t bits, double rate, uint32_t compression, int32_t channels);
}  // namespace SamsUtilities
void* AllocBuffer_(uint32_t bytes);   // 0x1002ebfc: the buffer allocation (NULL when it fails); DisposeBuffer_ frees it

// Unit v5997 (src/TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_CSprocketsModule_SetListenerDirection_2.cpp)
namespace SamsUtilities {
class UCommandPreroll : public UCommand {
public:
    explicit UCommandPreroll(uint32_t on);    // 0x1002ceac (56)
    uint32_t GetTag();                        // 0x1002d928 (12): 'prer', unit v51053
};
class UCommandPosition : public UCommand3dPosition {
public:
    explicit UCommandPosition(uint32_t on);   // 0x1002ccd0 (56)
    uint32_t GetTag();                        // 0x1002d9c0 (12): 'posn', unit v51053
};
}  // namespace SamsUtilities

namespace TbSpeaker {
class Module;
// TbSpeaker::Config::SetModuleSpeaker(Module*), a static member on the Mac (0x100149e8, 12); a free function here
// because TbSpeaker::Config is only declared.
void ConfigSetModuleSpeaker(Module* speaker);
}  // namespace TbSpeaker

// The speaker object the module creates (4 bytes on the Mac: just the vtable word).
class CSprocketsSpeaker {
public:
    CSprocketsSpeaker();   // 0x1002b2d0 (56)
    uint32_t Stereo() const;       // 0x1002db94 (8): 0, unit v51053
    int32_t Headphones() const;    // 0x1002dbd0 (8): 2
    int32_t Mono() const;          // 0x1002dc10 (8): 1
};

// Unit v51053: the rest of the listener commands. Imports and in-binary stand-ins the constructors call (recording fakes in the test
// library). The UHasFlags<unsigned long> is at command +8; SetFlag ORs a bit in.
void UHasFlags_SetFlag(void* flags, uint32_t flag);            // 0x1002a690 (16)
void UHasFlags_Construct(void* flags);                         // 0x1002a858 (12)
namespace SamsUtilities {
void UCommand_ConstructBase(UCommand* command);                // 0x1002c468 (56)
class UCommandSoundDistance : public UCommand {
public:
    explicit UCommandSoundDistance(double distance);   // 0x1002b838 (72), unit v51107: vtable word +4, the double at +8
    uint32_t GetTag();   // 0x1002daa0 (12): 'srcd'
};
}  // namespace SamsUtilities
// The static initialiser of CSprocketsModule.cpp 0x1002dd30 (120): the same two guarded objects as __sinit_TbMMFileBank_cpp.
void __sinit_CSprocketsModule_cpp();
extern uint32_t gSinit_1003f3b8[3];   // the __register_global_object record at 0x1003f3b8

// 0x1002ecbc: an import of main called with no arguments at the end of CSprocketsModule::Tick (what it does is not known).
void SoundTickImport_();

// Unit v51075: the speaker module the global _mpSpeaker (0x1003f348, AutoDelete::_mpSpeaker) points to. The Mac vtable starts at +8
// (the RTTI pair before it); the three conversions below each call one slot. They are stateless classes on the Mac.
namespace TbSpeaker {
class Module {
public:
    virtual int32_t StereoValue() = 0;       // +0x08
    virtual int32_t HeadphonesValue() = 0;   // +0x0c
    virtual int32_t MonoValue() = 0;         // +0x10
};
class Stereo {
public:
    operator int() const;   // 0x10014a44 (52): the speaker's slot +8
};
class Headphones {
public:
    operator int() const;   // 0x10014aa8 (52): the speaker's slot +0xc
};
class Mono {
public:
    operator int() const;   // 0x10014b14 (52): the speaker's slot +0x10
};
}  // namespace TbSpeaker

// Unit v51107 (src/TbAudioBody_create_SamsUtilities_UCommand_SetImmediate.cpp). In-binary stand-ins as free functions on the raw
// object (the Mac classes are not representable on the host); each writes the Mac's words, a vtable word as the TOC (0x1003abf0)
// plus an offset, native-endian.
struct TbRect;
void UHasFlags_ClearFlag(void* flags, uint32_t flag);          // 0x1002a63c (16): flags &= ~flag
void UHasFlags_ConstructWith(void* flags, uint32_t value);     // 0x1002c4d8 (8): flags = value
namespace SamsUtilities {
void UCommand_ConstructFlags(UCommand* command, uint32_t flags);   // 0x1002c468 (56): UHasFlags(flags), vtable word +4 = TOC+0x33f8
}
void TbRect_Construct(TbRect* rect, int32_t left, int32_t top, int32_t right, int32_t bottom);   // 0x1002879c (20)
namespace TbSpeaker {
void ModuleConstruct(void* module);   // 0x1002b338 (12): vtable word +0
void ConfigConstruct(void* config);   // 0x1002b6c4 (12): vtable word +0 = TOC+0x32a4
void StereoConstruct(void* stereo);   // 0x1002b65c (56): ConfigConstruct, then vtable word +0 = TOC+0x1cdc
}  // namespace TbSpeaker
