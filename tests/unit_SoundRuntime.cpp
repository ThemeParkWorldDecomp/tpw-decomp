// Fakes and test entry points for unit w66, the sound runtime (libtpw_unit_SoundRuntime.dylib, driven by
// tests/test_SoundRuntime.py; see calllog.h):
//   src/SoundRuntime.cpp  SOUND_Init 0x100dc940, SOUND_Close, SOUND_SetChannels, SOUND_SetSoundQuality, the four volume setters,
//                         SOUND_SetParamValue, StopAllSfx, FadeOutAllSfx, Pause, UnPause, SOUND_Tick, SendEvent, PauseSound,
//                         UnpauseSound, SetVolumeOnSound, SendEventExt, SetPauseOn3DSoundsOnly, SetEarPosition, MoveSoundEffect,
//                         StopSoundEffect, FadeSoundEffect, GetCurrentSampleLengthInMilliseconds and the five empty wrappers
//   include/TbAudioSystem.h  the TbAudioEvent getters (0x100de9e0 to 0x100dea8c)
//   src/TPSound.cpp       the state (and the status setters and SND_DuckVolume, which the scripts also call)
// Fakes, each recording what the original's callee would be given (the PowerPC side records the same from its stubs): the audio
// object (a TbAudioSystem whose SystemCommand records the command's kind and payload words and writes the out words of the two
// query commands, whose SendEvent/UpdateHandle/UpdateParameter answer the queues "Handle" and "Word"), CreateAudioObject (the
// queue "Create" is its answer; a nonnegative answer installs the audio object), GAME_ASSERTMSG, the sams keyboard (the queues
// "Option" and "Command" are the answers of its two virtuals) and USoundChannel::SetSafeMode. gSoundBalance is raw storage here.
//
// The case drives the functions with tpw_sx_op and reads the state back with tpw_sx_get after every operation. The state is one
// numbered list of words (the PowerPC side reads the same words from the Mac's addresses): see kStateWords below.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Advisor.h"
#include "Debug.h"
#include "SoundBalance.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "calllog.h"
#include "lobby.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

// gSoundBalance (0x102b4e2c): raw storage, the unit reads its value slots only.
#ifdef __APPLE__
extern unsigned char gSoundBalanceStorage[sizeof(CSoundBalance)] __asm__("_gSoundBalance");
#else
extern unsigned char gSoundBalanceStorage[sizeof(CSoundBalance)] __asm__("gSoundBalance");
#endif
alignas(16) unsigned char gSoundBalanceStorage[sizeof(CSoundBalance)];

// src/Advisor.cpp's (the unit does not link it).
int32_t gAdvisorThemeSoundBank;

namespace {
constexpr int kLabelAudio = 2;
constexpr int kLabelKeyboard = 3;
constexpr int kSettingsEntries = 22;   // see kStateWords: the settings block as 12 words, 2 shorts and 8 words
constexpr int kStateWords = 17 + kSettingsEntries;

// The kind numbers of the commands, as in tests/test_SoundRuntime.py.
enum Kind {
    kTick = 1, kProcess, kSetMasterVolume, kSetMusicVolume, kSetUser2Volume, kSetSpeechVolume, kToggleSFX, kToggleMusic,
    kToggleSpeech, kSetDistanceMapping, kSetQSoundInfo, kSetEAX, kSetHalfRate, kSetFades, kSetNumberOfChannels,
    kGetMaxNumberOfChannels, kPauseAll, kRestartAll, kStopAll, kPauseHandle, kRestartHandle, kStopHandle, kSetHandleVolume,
    kGetSampleLength, kUnknown = 99
};

template <class T>
const T* as(const TbSysCommand::Command& command) {
    return dynamic_cast<const T*>(&command);
}

class FakeAudio : public TbAudioSystem {
public:
    TbError Release() override {
        ev("Audio.Release", {lab(this)});
        return TbError(0);
    }
    TbError SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& forward, const TbSoundPosition& up,
                                   const TbSoundPosition& velocity) override {
        ev("Audio.SetListener", {lab(this), fbits(position.mX), fbits(position.mY), fbits(position.mZ), fbits(forward.mX),
                                 fbits(forward.mY), fbits(forward.mZ)});
        ev("Audio.SetListener2", {fbits(up.mX), fbits(up.mY), fbits(up.mZ), fbits(velocity.mX), fbits(velocity.mY),
                                  fbits(velocity.mZ)});
        return TbError(0);
    }
    TbError RegisterCatagory(const char*, const char*, uint32_t&) override {
        ev("Audio.RegisterCatagory", {lab(this)});
        return TbError(0);
    }
    // The event: the object, the entity, whether the type answers 0 (the Mac compares the vtable word), the category, the event
    // number, the position and the last float.
    int32_t SendEvent(int32_t entity, const TbAudioEvent& event) override {
        const TbSoundPosition& position = event.GetPosition();
        ev("Audio.SendEvent", {lab(this), entity, event.GetType() == 0 ? 1 : 0, event.GetCategory(), event.GetEvent(),
                               fbits(position.mX), fbits(position.mY), fbits(position.mZ), fbits(event.mW)});
        return static_cast<int32_t>(ret("Handle", 0));
    }
    int32_t UpdateHandle(int32_t handle, const TbSoundPosition& position, const TbSoundPosition& velocity) override {
        ev("Audio.UpdateHandle", {lab(this), handle, fbits(position.mX), fbits(position.mY), fbits(position.mZ),
                                  fbits(velocity.mX), fbits(velocity.mY), fbits(velocity.mZ)});
        return static_cast<int32_t>(ret("Word", 0));
    }
    int32_t UpdateParameter(int32_t handle, uint8_t parameter, uint32_t value) override {
        ev("Audio.UpdateParameter", {lab(this), handle, parameter, value});
        return static_cast<int32_t>(ret("Word", 0));
    }
    // The command: its kind (see test_SoundRuntime.py) and up to three payload words. The out pointers record whether they are
    // set; the two query commands write the queue "Max" or "Length" into the word unless the queue "Skip" says otherwise.
    TbError SystemCommand(const TbSysCommand::Command& command) const override {
        int64_t kind = kUnknown;
        int64_t p[3] = {0, 0, 0};
        if (as<TbSysCommand::Tick>(command)) kind = kTick;
        else if (as<TbSysCommand::Process>(command)) kind = kProcess;
        else if (auto* c = as<TbSysCommand::SetMasterVolume>(command)) { kind = kSetMasterVolume; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetMusicVolume>(command)) { kind = kSetMusicVolume; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetUser2Volume>(command)) { kind = kSetUser2Volume; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetSpeechVolume>(command)) { kind = kSetSpeechVolume; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::ToggleSFX>(command)) { kind = kToggleSFX; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::ToggleMusic>(command)) { kind = kToggleMusic; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::ToggleSpeech>(command)) { kind = kToggleSpeech; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetDistanceMapping>(command)) { kind = kSetDistanceMapping; p[0] = fbits(c->mValue); }
        else if (auto* c = as<TbSysCommand::SetQSoundInfo>(command)) {
            kind = kSetQSoundInfo;
            p[0] = c->mQSound;
            p[1] = c->mRear;
            p[2] = c->mCrossover;
        }
        else if (auto* c = as<TbSysCommand::SetEAX>(command)) { kind = kSetEAX; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetHalfRate>(command)) { kind = kSetHalfRate; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetFades>(command)) { kind = kSetFades; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::SetNumberOfChannels>(command)) { kind = kSetNumberOfChannels; p[0] = c->mValue; }
        else if (auto* c = as<TbSysCommand::GetMaxNumberOfChannels>(command)) {
            kind = kGetMaxNumberOfChannels;
            p[0] = c->mOut != nullptr ? 1 : 0;
            if (c->mOut != nullptr && ret("Skip", 0) == 0) *c->mOut = static_cast<int32_t>(ret("Max", 0x20));
        }
        else if (as<TbSysCommand::PauseAll>(command)) kind = kPauseAll;
        else if (as<TbSysCommand::RestartAll>(command)) kind = kRestartAll;
        else if (as<TbSysCommand::StopAll>(command)) kind = kStopAll;
        else if (auto* c = as<TbSysCommand::PauseHandle>(command)) { kind = kPauseHandle; p[0] = c->mHandle; }
        else if (auto* c = as<TbSysCommand::RestartHandle>(command)) { kind = kRestartHandle; p[0] = c->mHandle; }
        else if (auto* c = as<TbSysCommand::StopHandle>(command)) { kind = kStopHandle; p[0] = c->mHandle; }
        else if (auto* c = as<TbSysCommand::SetHandleVolume>(command)) {
            kind = kSetHandleVolume;
            p[0] = c->mHandle;
            p[1] = c->mVolume;
        }
        else if (auto* c = as<TbSysCommand::GetSampleLength>(command)) {
            kind = kGetSampleLength;
            p[0] = c->mOut != nullptr ? 1 : 0;
            p[1] = c->mHandle;
            if (c->mOut != nullptr && ret("Skip", 0) == 0) *c->mOut = static_cast<int32_t>(ret("Length", 1234));
        }
        ev("Audio.SystemCommand", {lab(this), kind, p[0], p[1], p[2]});
        return TbError(0);
    }
    TbError Init(const char*, const CAudioConfig&) override { return TbError(0); }
    TbError UpdateDistortion(void*, float) override { return TbError(0); }
    TbError UpdateLowPass(void*, float) override { return TbError(0); }
    TbError StopHandle(void*) const override { return TbError(0); }
    TbError DeregisterCatagoryBanks(uint32_t) const override { return TbError(0); }
    TbError CreateSystemChain(const CAudioConfig&) override { return TbError(0); }
};
FakeAudio gAudio;

// The keyboard object that exists before the case (the label 3), and the ones the code makes.
alignas(16) unsigned char gKeyboardStorage[sizeof(SamsUtilities::UKeyboard)];
SamsUtilities::UKeyboard* gPresetKeyboard;
int32_t gNewKeyboards;

int32_t& bank(int32_t index) { return gSoundBanks[index]; }

BalanceValue& balanceValue(int32_t slot) { return reinterpret_cast<CSoundBalance*>(gSoundBalanceStorage)->mValues[slot]; }
}  // namespace

// ---- the callees outside the unit
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The sound library's: the config is recorded as its channel count and the bits of its double; a nonnegative answer installs
// the audio object, a negative one leaves the pointer alone.
TbError CreateAudioObject(const char* name, TbAudioSystem** audio, const CAudioConfig& config) {
    int64_t quality;
    std::memcpy(&quality, &config.mQuality, sizeof quality);
    ev("CreateAudioObject", {config.mChannels, quality}, {name});
    const int32_t answer = static_cast<int32_t>(ret("Create", 0));
    if (answer >= 0) *audio = &gAudio;
    return TbError(answer);
}

namespace SamsUtilities {
UKeyboard* gKeyboard;

UKeyboard::UKeyboard() {
    if (!on()) return;
    ev("UKeyboard.ctor", {newlab(this)});
    ++gNewKeyboards;
}

uint8_t UKeyboard::IsOptionDown() {
    ev("Keyboard.IsOptionDown", {lab(this)});
    return static_cast<uint8_t>(ret("Option", 0));
}

uint8_t UKeyboard::IsCommandDown() {
    ev("Keyboard.IsCommandDown", {lab(this)});
    return static_cast<uint8_t>(ret("Command", 0));
}

void USoundChannel::SetSafeMode(uint8_t on) { ev("SetSafeMode", {on}); }
}  // namespace SamsUtilities

extern "C" {
// The state, by number: 0 allocations, 1 audio object (0 or 1), 2 wanted, 3 on, 4 to 11 the banks, 12 gAdvisorThemeSoundBank,
// 13 the music bank, 14 the thunder bank, 15 the island table (0 or 1), 16 the island count, then the settings block: entries 0
// to 11 are the words at 0x00 to 0x2c, 12 and 13 the shorts at 0x30 and 0x32, 14 to 21 the words at 0x34 to 0x50, then the
// keyboard (its label) as the last word. kStateWords counts the settings entries, so the keyboard is 17 + 22 = 39.
int32_t tpw_sx_get(int32_t index) {
    if (index < 17) {
        switch (index) {
        case 0: return gSoundAllocations;
        case 1: return gAudioSystem != nullptr ? 1 : 0;
        case 2: return static_cast<int32_t>(gSoundWanted);
        case 3: return static_cast<int32_t>(gSoundSystemOn);
        case 12: return gAdvisorThemeSoundBank;
        case 13: return gLocalParkMusicBank;
        case 14: return gLobbyThunderBank;
        case 15: return gLobbyIslandSounds != nullptr ? 1 : 0;
        case 16: return gLobbyIslandSoundCount;
        default: return bank(index - 4);
        }
    }
    if (index == kStateWords) return lab(SamsUtilities::gKeyboard);
    const int32_t entry = index - 17;
    const unsigned char* base = reinterpret_cast<const unsigned char*>(&gSoundSettings);
    if (entry == 12) return gSoundSettings.mMusicMute;
    if (entry == 13) return gSoundSettings.mPauseOn3DOnly;
    int32_t word;
    std::memcpy(&word, base + (entry < 12 ? 4 * entry : 4 * (entry - 14) + 0x34), sizeof word);
    return word;
}

void tpw_sx_put(int32_t index, int32_t value) {
    if (index < 17) {
        switch (index) {
        case 0: gSoundAllocations = value; break;
        case 1: gAudioSystem = value != 0 ? &gAudio : nullptr; break;
        case 2: gSoundWanted = static_cast<uint32_t>(value); break;
        case 3: gSoundSystemOn = static_cast<uint32_t>(value); break;
        case 12: gAdvisorThemeSoundBank = value; break;
        case 13: gLocalParkMusicBank = value; break;
        case 14: gLobbyThunderBank = value; break;
        case 15: gLobbyIslandSounds = value != 0 ? reinterpret_cast<int32_t*>(gSoundBalanceStorage) : nullptr; break;
        case 16: gLobbyIslandSoundCount = value; break;
        default: bank(index - 4) = value; break;
        }
        return;
    }
    const int32_t entry = index - 17;
    unsigned char* base = reinterpret_cast<unsigned char*>(&gSoundSettings);
    if (entry == 12) {
        gSoundSettings.mMusicMute = static_cast<int16_t>(value);
    } else if (entry == 13) {
        gSoundSettings.mPauseOn3DOnly = static_cast<int16_t>(value);
    } else {
        std::memcpy(base + (entry < 12 ? 4 * entry : 4 * (entry - 14) + 0x34), &value, sizeof value);
    }
}

// Value slot `slot` of gSoundBalance (the Mac word at +0x08 + 4 slot).
void tpw_sx_put_balance(int32_t slot, int32_t value) { balanceValue(slot).mInteger = value; }

// Starts a case: no keyboard (or the preset one), nothing recorded, the state cleared.
void tpw_sx_begin(int32_t keyboard) {
    std::memset(&gSoundSettings, 0, sizeof gSoundSettings);
    std::memset(gSoundBanks, 0, sizeof gSoundBanks);
    std::memset(gSoundBalanceStorage, 0, sizeof gSoundBalanceStorage);
    gAdvisorThemeSoundBank = 0;
    gLocalParkMusicBank = 0;
    gLobbyThunderBank = 0;
    gLobbyIslandSounds = nullptr;
    gLobbyIslandSoundCount = 0;
    gSoundAllocations = 0;
    gAudioSystem = nullptr;
    gSoundWanted = 0;
    gSoundSystemOn = 0;
    gNewKeyboards = 0;
    if (gPresetKeyboard == nullptr) gPresetKeyboard = new (gKeyboardStorage) SamsUtilities::UKeyboard;
    SamsUtilities::gKeyboard = keyboard != 0 ? gPresetKeyboard : nullptr;
}

// The objects the labels stand for: 2 the audio object, 3 the preset keyboard.
void* tpw_sx_object(int32_t label) {
    if (label == kLabelAudio) return &gAudio;
    if (label == kLabelKeyboard) return gPresetKeyboard;
    return nullptr;
}

// How many keyboard objects the case made (each asks for 0xc bytes on the Mac).
int32_t tpw_sx_new_keyboards() { return gNewKeyboards; }

// Ends a case: frees the keyboards the case made.
void tpw_sx_end() {
    if (SamsUtilities::gKeyboard != nullptr && SamsUtilities::gKeyboard != gPresetKeyboard) delete SamsUtilities::gKeyboard;
    SamsUtilities::gKeyboard = nullptr;
}

static float fl(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

// One operation; `a` holds its arguments (float arguments as bits); the answer is its result (for the event getters, the value).
// Operation numbers are in test_SoundRuntime.py.
int64_t tpw_sx_op(int32_t op, const int64_t* a, const char* text) {
    const int32_t i0 = static_cast<int32_t>(a[0]);
    const int32_t i1 = static_cast<int32_t>(a[1]);
    const int32_t i2 = static_cast<int32_t>(a[2]);
    const int32_t i3 = static_cast<int32_t>(a[3]);
    switch (op) {
    case 0: return SOUND_Init(const_cast<char*>(text), static_cast<uint32_t>(a[0]), i1);
    case 1: SOUND_Close(); return 0;
    case 2: SOUND_SetChannels(i0); return 0;
    case 3: SOUND_SetSoundQuality(i0); return 0;
    case 4: SOUND_SetGlobalVolume(static_cast<uint32_t>(a[0])); return 0;
    case 5: SOUND_SetMusicVolume(static_cast<uint32_t>(a[0])); return 0;
    case 6: SOUND_SetSfxVolume(static_cast<uint32_t>(a[0])); return 0;
    case 7: SOUND_SetSpeechVolume(static_cast<uint32_t>(a[0])); return 0;
    case 8: return SOUND_SetParamValue(i0, static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]));
    case 9: SOUND_StopAllSfx(); return 0;
    case 10: SOUND_FadeOutAllSfx(); return 0;
    case 11: SOUND_Pause(); return 0;
    case 12: SOUND_UnPause(); return 0;
    case 13: SOUND_Tick(); return 0;
    case 14: return SOUND_SendEvent(i0, i1, i2, i3, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]));
    case 15: SOUND_PauseSound(i0); return 0;
    case 16: SOUND_UnpauseSound(i0); return 0;
    case 17: SOUND_SetVolumeOnSound(i0, i1); return 0;
    case 18:
        return SOUND_SendEventExt(i0, i1, i2, i3, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]),
                                  static_cast<int32_t>(a[6]));
    case 19: SOUND_SetPauseOn3DSoundsOnly(i0); return 0;
    case 20:
        SOUND_SetEarPosition(fl(a[0]), fl(a[1]), fl(a[2]), fl(a[3]), fl(a[4]), fl(a[5]), fl(a[6]), fl(a[7]), fl(a[8]));
        return 0;
    case 21: return SOUND_MoveSoundEffect(i0, i1, i2, i3);
    case 22: SOUND_StopSoundEffect(i0); return 0;
    case 23: SOUND_FadeSoundEffect(i0); return 0;
    case 24: return SOUND_GetCurrentSampleLengthInMilliseconds(i0);
    case 25: SOUND_SetOcclusionProcessing(i0); return 0;
    case 26: SOUND_SetReverbLevel(i0); return 0;
    case 27: SOUND_SetDroppableLevel(static_cast<uint32_t>(a[0])); return 0;
    case 28: return SOUND_AddObject(fl(a[0]), fl(a[1]), fl(a[2]), fl(a[3]), fl(a[4]));
    case 29: SOUND_DelObject(static_cast<uint32_t>(a[0])); return 0;
    case 30: SOUND_SetMusicStatus(static_cast<uint32_t>(a[0])); return 0;
    case 31: SOUND_SetSfxStatus(static_cast<uint32_t>(a[0])); return 0;
    case 32: SOUND_SetSpeechStatus(static_cast<uint32_t>(a[0])); return 0;
    case 33: SND_DuckVolume(i0); return 0;
    case 60:
    case 61:
    case 62:
    case 63: {
        // the event getters on an event of category a0, event a1 and position a2..a4, w a5 (floats as bits)
        const TbAudioEvent event(i0, i1, TbSoundPosition(fl(a[2]), fl(a[3]), fl(a[4])), fl(a[5]));
        if (op == 60) return event.GetType();
        if (op == 61) return event.GetCategory();
        if (op == 62) return event.GetEvent();
        const TbSoundPosition& position = event.GetPosition();
        ev("Position", {fbits(position.mX), fbits(position.mY), fbits(position.mZ)});
        return 0;
    }
    }
    return -12345;
}
}
