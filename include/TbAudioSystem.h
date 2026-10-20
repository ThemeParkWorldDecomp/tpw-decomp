// TbAudioSystem.h: the sound library's audio object, as the game sees it. On the Mac gAudioSystem (0x102f7c34, set by
// SOUND_Init) points at a TbAudioBody of the "sound symbols shared" library (CreateAudioObject makes it); the game
// only calls its virtuals. On the host this is an abstract interface: nothing implements it in the launcher (the launcher's
// interim CreateAudioObject fails, so the "system on" flag stays 0 and the wrappers never touch the object), and the tests
// fake it.
//
// The slots, with the vtable word offsets the game's calls use (TbAudioBody's vtable is at 0x1003cb70 in the sound
// library, checked with devtools/vt.py; the two words before the slots are the class's type info and a zero). The
// declaration order is the Mac's slot order; the signatures are the library's own names (functions.txt), with the result
// conventions the game's calls show (unit w66): Release, SetListenerOrientation, RegisterCatagory and SystemCommand answer a
// TbError through a hidden result buffer (r3, the object in r4); SendEvent, UpdateHandle and UpdateParameter answer one
// word in r3 with the object in r3 and the first argument in r4 (the sound's handle). The result types of the slots after
// UpdateParameter are not checked: nothing calls those slots.
#pragma once
#include <cstdint>

#include "TbError.h"
#include "TbSoundPosition.h"

class TbAudioBody;
namespace TbSpeaker {
struct Config;   // include/TbSoundInfo.h
}

// The sound library's position (three floats, 12 bytes; the game builds them on the stack with the inline
// (float, float, float) constructor): include/TbSoundPosition.h (unit w77).

// An event the game hands to SendEvent: seven words on the Mac. The game's copy lives in main (vtable at 0x102a0158 in
// main's data, typeinfo "TbAudioEvent"; the sound library has its own class of the same name with its own copies of the
// getters). SOUND_SendEvent builds it with the last word 0.0f.
class TbAudioEvent {
public:
    TbAudioEvent(int32_t category, int32_t event, const TbSoundPosition& position, float w)
        : mCategory(category), mEvent(event), mPosition(position), mW(w) {}
    // +0x08 0x100de9e0: answers 0
    virtual int32_t GetType() const { return 0; }
    // +0x0c 0x100dea18: the bank (the sound category's handle)
    virtual int32_t GetCategory() const { return mCategory; }
    // +0x10 0x100dea54: the event number
    virtual int32_t GetEvent() const { return mEvent; }
    // +0x14 0x100dea8c: the address of the position at +0x0c
    virtual const TbSoundPosition& GetPosition() const { return mPosition; }

    int32_t mCategory;        // 0x04
    int32_t mEvent;           // 0x08
    TbSoundPosition mPosition;   // 0x0c
    float mW;                 // 0x18  (the sound library's TbAudioEvent constructor leaves it 0.0f as well)
};

// What SOUND_Init hands to CreateAudioObject: a double and a word on the stack of the original (0x48 and 0x50 of its
// frame; the word at 0x54 is never written).
class CAudioConfig {
public:
    CAudioConfig(double quality, int32_t channels) : mQuality(quality), mChannels(channels) {}
    double mQuality;      // 0x00  the sound quality divided by 100.0
    int32_t mChannels;    // 0x08  the channel count SOUND_Init works out, clamped to 0x10
};

// The commands SystemCommand takes: a vtable word (the vtable is an import of the sound library) and 0, 1, 2 or 3 payload
// words on the Mac. Execute is slot +0x08 of every vtable: the Execute bodies are the sound library's (unit w79h,
// src/TbSysCommand.cpp, 0x10027fd4 to 0x1002e348) and the audio object's SystemCommand calls them. The Mac's inlined constructors
// store the base class vtables (TbSysCommand::Command 0x102a01b0, TbSysCommand::SetValue 0x102a01a4) before the derived one; the host
// needs no such stores. The library's classes the game never constructs are declared here too (unit w79h), in the same style.
namespace TbSysCommand {
struct Command {
    virtual TbError Execute(const TbAudioBody* body) const = 0;
};
#define TB_SYSCOMMAND_EXECUTE TbError Execute(const TbAudioBody*) const override;

// The library's base of every command with an integer payload at +0x04 (the accessors, 8 bytes each, answer that word: the
// host's payload is the union below, so each class keeps the member name its game unit uses). FlushCatagory, the handle commands,
// SetQSoundInfo and the value commands derive from it.
struct SetValue : Command {
    explicit SetValue(int32_t value) : mValue(value) {}
    uint32_t GetSysValue() const;             // 0x1002899c (8)
    float GetFloatSysValue() const;           // 0x10028658 (8): the payload read as a float (SetDistanceMapping's)
    void* GetTbHandleSysValue() const;        // 0x100287d8 (8): the payload as a handle (zero-extended; a handle is a Mac word)
    union {
        int32_t mValue;       // 0x04
        int32_t mHandle;      // 0x04  the handle commands
        uint32_t mCategory;   // 0x04  FlushCatagory
        int32_t mQSound;      // 0x04  SetQSoundInfo
        float mFloat;         // 0x04  SetDistanceMapping
    };
};
using ValueCommand = SetValue;
// The library's base of the commands that answer through a pointer to the caller's word, at +0x04 on the Mac.
struct GetValue : Command {
    explicit GetValue(int32_t* out) : mOut(out) {}
    int32_t* GetSysValue() const;   // 0x10028aa8 (8)
    int32_t* mOut;   // 0x04 on the Mac (a 32-bit pointer to the caller's word)
};
// The base of Save and Load: the open save file at +0x04.
struct Serialise : Command {
    explicit Serialise(void* file) : mFile(file) {}
    void* GetHandle() const;   // 0x1002df40 (8)
    void* mFile;   // 0x04 on the Mac (a 32-bit pointer to the save file)
};

// 0x10028c4c Execute: flushes (frees) the banks of a registered category.
struct FlushCatagory : SetValue {
    explicit FlushCatagory(uint32_t category) : SetValue(static_cast<int32_t>(category)) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10028c4c (88)
};
// 0x1002859c Execute: SOUND_Init sets the distance mapping with one of these (a float).
struct SetDistanceMapping : SetValue {
    // The host keeps the float under its own name as well (w66's test reads mValue); the payload word is the same.
    explicit SetDistanceMapping(float value) : SetValue(0), mValue(value) { mFloat = value; }
    TB_SYSCOMMAND_EXECUTE   // 0x1002859c (104)
    float mValue;   // 0x04
};
// 0x10028cf0 Execute: a speaker configuration for the device (the pointer at +0x04 goes to the system info's constructor).
struct SetSpeakerConfig : Command {
    explicit SetSpeakerConfig(const TbSpeaker::Config* config) : mConfig(config) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10028cf0 (128)
    const TbSpeaker::Config* mConfig;   // 0x04 on the Mac (a 32-bit pointer)
};
// Commands without a payload. Import vtables: Tick 0x103bd304, Process 0x103bd2a4, PauseAll 0x103bd2f8, RestartAll
// 0x103bd2d4, StopAll 0x103bd2e8.
struct Tick : Command { TB_SYSCOMMAND_EXECUTE };      // 0x1002de28 (64)
struct Process : Command { TB_SYSCOMMAND_EXECUTE };   // 0x10027fd4 (136)
struct PauseAll : Command { TB_SYSCOMMAND_EXECUTE };  // 0x10028350 (108)
struct RestartAll : Command { TB_SYSCOMMAND_EXECUTE };   // 0x10028474 (108)
struct StopAll : Command { TB_SYSCOMMAND_EXECUTE };   // 0x100281a8 (128)
struct StopAllWithFade : Command { TB_SYSCOMMAND_EXECUTE };   // 0x100282dc (36): answers -1
// One integer payload at +0x04. Import vtables: SetMasterVolume 0x103bd2fc, SetMusicVolume 0x103bd308, SetUser2Volume
// 0x103bd2b0 (the sfx volume), SetSpeechVolume 0x103bd30c, ToggleSFX 0x103bd2b4, ToggleMusic 0x103bd2e4, ToggleSpeech
// 0x103bd2cc, SetEAX 0x103bd310, SetHalfRate 0x103bd2d0, SetFades 0x103bd2ac, SetNumberOfChannels 0x103bd2ec. The volumes are
// the game's 0..100 integers (the control index in the comment: 0 master, 1 music, 2 speech and user 1, 3 user 2 and sfx).
struct SetMasterVolume : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x100294f4 (132), control 0
struct SetMusicVolume : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };    // 0x10029c94 (132), control 1
struct SetUser2Volume : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x1002998c (132), control 3
struct SetSpeechVolume : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };  // 0x1002967c (132), control 2
struct SetUser1Volume : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x10029804 (132), control 2
struct SetSFXVolume : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };     // 0x10029b14 (132), control 3
// The toggles: the payload is the on/off value for the control (ToggleVolumeControl's second argument).
struct ToggleSFX : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };     // 0x10029be4 (104), control 3
struct ToggleMusic : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x10029d68 (104), control 1
struct ToggleSpeech : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };  // 0x10029750 (104), control 2
struct ToggleMaster : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };  // 0x100295c8 (104), control 0
struct ToggleUser1 : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x100298d8 (104), control 2
struct ToggleUser2 : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x10029a60 (104), control 3
struct SetEAX : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };        // 0x10028f60 (36): answers -1
struct SetHalfRate : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x10028e2c (36): answers -1
struct SetFades : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };      // 0x10028dc0 (36): answers -1
struct SetNumberOfChannels : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x10028824 (216)
struct SetDroppable : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };  // 0x10028fc8 (120)
struct SetGeometryInfo : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x1002908c (36): answers -1
// 0x103bd318 (unit w74, SOUND_LoadStatus): one integer payload at +0x04, the reverb preset the save held. The Execute answers 0
// and does nothing (the library has no reverb here).
struct SetPresetReverb : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };   // 0x1002e1f0 (36)
// Library classes the game never constructs, with a payload of the same shape; each answers 0 and does nothing.
struct RegisterObject : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };    // 0x1002e264 (36)
struct DeleteObject : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };      // 0x1002e2d8 (36)
struct DeleteAllObjects : ValueCommand { using ValueCommand::ValueCommand; TB_SYSCOMMAND_EXECUTE };  // 0x1002e348 (36)
// Import vtables: PauseHandle 0x103bd2d8, RestartHandle 0x103bd2c4, StopHandle 0x103bd2b8 (SOUND_StopSoundEffect and
// SOUND_FadeSoundEffect send the same one).
struct HandleCommand : SetValue {
    explicit HandleCommand(int32_t handle) : SetValue(handle) {}
};
struct PauseHandle : HandleCommand { using HandleCommand::HandleCommand; TB_SYSCOMMAND_EXECUTE };      // 0x1002e028 (152)
struct RestartHandle : HandleCommand { using HandleCommand::HandleCommand; TB_SYSCOMMAND_EXECUTE };    // 0x1002e10c (152)
struct StopHandle : HandleCommand { using HandleCommand::HandleCommand; TB_SYSCOMMAND_EXECUTE };      // 0x10029100 (108)
struct StopHandleWithFade : HandleCommand { using HandleCommand::HandleCommand; TB_SYSCOMMAND_EXECUTE };   // 0x1002947c (36): answers -1
// 0x103bd2f4
struct SetHandleVolume : HandleCommand {
    SetHandleVolume(int32_t handle, int32_t volume) : HandleCommand(handle), mVolume(volume) {}
    TB_SYSCOMMAND_EXECUTE   // 0x100291b8 (260)
    int32_t mVolume;   // 0x08
};
// The same with a pitch word after the volume.
struct SetHandleVolumeAndPitch : HandleCommand {
    SetHandleVolumeAndPitch(int32_t handle, int32_t volume, int32_t pitch) : HandleCommand(handle), mVolume(volume), mPitch(pitch) {}
    TB_SYSCOMMAND_EXECUTE   // 0x1002930c (280)
    int32_t mVolume;   // 0x08
    int32_t mPitch;    // 0x0c
};
// The handle's bounding rectangle (left, top, right, bottom words at +0x08..+0x14).
struct SetBoundingRectangle : HandleCommand {
    SetBoundingRectangle(int32_t handle, int32_t left, int32_t top, int32_t right, int32_t bottom)
        : HandleCommand(handle), mLeft(left), mTop(top), mRight(right), mBottom(bottom) {}
    TB_SYSCOMMAND_EXECUTE   // 0x100286a4 (164)
    int32_t mLeft;     // 0x08
    int32_t mTop;      // 0x0c
    int32_t mRight;    // 0x10
    int32_t mBottom;   // 0x14
};
// 0x103bd2bc: three 0/1 words, in the order of the balance rows QSOUND, REAR, CROSSOVER.
struct SetQSoundInfo : SetValue {
    SetQSoundInfo(int32_t qsound, int32_t rear, int32_t crossover) : SetValue(qsound), mRear(rear), mCrossover(crossover) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10028e9c (120)
    int32_t mRear;        // 0x08
    int32_t mCrossover;   // 0x0c
};
// 0x103bd2a8: the library writes the highest channel count it can mix into *out.
struct GetMaxNumberOfChannels : GetValue {
    explicit GetMaxNumberOfChannels(int32_t* out) : GetValue(out) {}
    TB_SYSCOMMAND_EXECUTE   // 0x100289e0 (112)
};
// The number of samples the device is playing, into *out (library class, never constructed by the game).
struct GetNumberOfPlayingChannels : GetValue {
    explicit GetNumberOfPlayingChannels(int32_t* out) : GetValue(out) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10028aec (192)
};
// The device's IDirectSound slot into *out (library class, never constructed by the game).
struct GetDirectSoundObject : GetValue {
    explicit GetDirectSoundObject(int32_t* out) : GetValue(out) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10029e1c (100)
};
// The volumes as the game's 0..100 integers into *out (library classes, never constructed by the game).
struct GetSFXVolume : GetValue {
    explicit GetSFXVolume(int32_t* out) : GetValue(out) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10029fbc (120)
};
struct GetMusicVolume : GetValue {
    explicit GetMusicVolume(int32_t* out) : GetValue(out) {}
    TB_SYSCOMMAND_EXECUTE   // 0x1002a0c4 (120)
};
// 0x103bd2c8: the library writes the length in milliseconds of the sound `handle` into *out.
struct GetSampleLength : GetValue {
    GetSampleLength(int32_t* out, int32_t handle) : GetValue(out), mHandle(handle) {}
    TB_SYSCOMMAND_EXECUTE   // 0x10029ed4 (152)
    int32_t mHandle;   // 0x08 on the Mac
};
// 0x103bd300 (unit w74, SOUND_SaveStatus): the audio object writes its own data to the open save file.
struct Save : Serialise {
    explicit Save(void* file) : Serialise(file) {}
    TB_SYSCOMMAND_EXECUTE   // 0x1002deac (80)
};
// 0x103bd314 (unit w74, SOUND_LoadStatus): the audio object reads its own data back from the save file. The Mac's object is
// 0x0c bytes: the file at +0x04 and a byte at +0x08 (always 1 here; the three bytes after it are never written).
struct Load : Serialise {
    Load(void* file, uint8_t flag) : Serialise(file), mFlag(flag) {}
    TB_SYSCOMMAND_EXECUTE   // 0x1002df84 (96)
    uint8_t mFlag;   // 0x08
};
#undef TB_SYSCOMMAND_EXECUTE
}  // namespace TbSysCommand

class TbAudioSystem {
public:
    // +0x08 (hidden TbError result): deletes the object itself
    virtual TbError Release() = 0;
    // +0x0c (hidden TbError result)
    virtual TbError SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& forward,
                                           const TbSoundPosition& up, const TbSoundPosition& velocity) = 0;
    // +0x10: registers the category `name` of the sound bank file at `path`; `handle` is the category's handle. The game
    // drops the TbError and keeps the handle.
    virtual TbError RegisterCatagory(const char* path, const char* name, uint32_t& handle) = 0;
    // +0x14: starts the sound of `event` for the entity `entity` (a handle, 0 for a new sound); answers the sound's handle
    virtual int32_t SendEvent(int32_t entity, const TbAudioEvent& event) = 0;
    // +0x18: moves a sound; answers a word the game keeps as the handle
    virtual int32_t UpdateHandle(int32_t handle, const TbSoundPosition& position, const TbSoundPosition& velocity) = 0;
    // +0x1c: answers a word
    virtual int32_t UpdateParameter(int32_t handle, uint8_t parameter, uint32_t value) = 0;
    // +0x20: runs a command; the game drops the TbError.
    virtual TbError SystemCommand(const TbSysCommand::Command& command) const = 0;
    // +0x24
    virtual TbError Init(const char* name, const CAudioConfig& config) = 0;
    // +0x28
    virtual TbError UpdateDistortion(void* handle, float value) = 0;
    // +0x2c
    virtual TbError UpdateLowPass(void* handle, float value) = 0;
    // +0x30: the destructor of TbAudioBody (0x100163bc)
    virtual ~TbAudioSystem() {}
    // +0x34
    virtual TbError StopHandle(void* handle) const = 0;
    // +0x38
    virtual TbError DeregisterCatagoryBanks(uint32_t category) const = 0;
    // +0x3c
    virtual TbError CreateSystemChain(const CAudioConfig& config) = 0;
};

// 0x1002a18c in the sound library (an import of main): makes the audio object and stores it in *audio. The result is
// negative on failure (the launcher's interim answers -1, see launcher/pending_manual.cpp).
TbError CreateAudioObject(const char* name, TbAudioSystem** audio, const CAudioConfig& config);

// The sams utilities the sound start-up asks about the keyboard (imports of main). The listing's vtable slots of the real
// UKeyboard: +0x08 destructor, +0x0c IsShiftDown, +0x10 IsOptionDown, +0x14 IsControlDown, +0x18 IsCommandDown, +0x1c
// GetModifiers, +0x20 IsKeyDown, +0x24 and +0x28 Translate; SOUND_Init asks the two named here and nothing else.
namespace SamsUtilities {
class UKeyboard {
public:
    UKeyboard();   // 0x10001808 (an import; the object is 0xc bytes on the Mac)
    virtual ~UKeyboard() {}   // +0x08 0x1000188c
    virtual uint8_t IsOptionDown();    // +0x10 0x10001a08
    virtual uint8_t IsCommandDown();   // +0x18 0x10001910
    virtual uint32_t GetModifiers() { return 0; }   // +0x1c (unit w142; the host stand-in answers 0)
};
extern UKeyboard* gKeyboard;   // the import variable (TOC word 0x103bcef4): the keyboard object, or NULL until the first use

class USoundChannel {
public:
    static void SetSafeMode(uint8_t on);   // 0x1001e9f0 (an import): the sound channels play in a degraded safe mode
};
}  // namespace SamsUtilities
