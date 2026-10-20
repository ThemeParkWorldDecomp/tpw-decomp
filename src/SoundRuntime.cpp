// The sound runtime of the game's sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the range under
// analyser.cpp): SOUND_Init, SOUND_Close, the quality and channel set-up, the volume setters, the per-frame SOUND_Tick, the
// event senders, the listener and the handle commands (0x100dc940 to 0x100de758), plus the five empty wrappers (0x100de8e4 to
// 0x100de9ac) and the TbAudioEvent getters inlined in TbAudioSystem.h (0x100de9e0 to 0x100dea8c). Everything goes through the
// abstract audio object gAudioSystem (TbAudioSystem.h); every wrapper that talks to it first tests the two flags
// gSoundWanted and gSoundSystemOn (defined in TPSound.cpp). The status setters (SOUND_SetMusicStatus and the other two) are in
// TPSound.cpp.
//
// The numbers read from the sound balance table (gSoundBalance, sound.sam) are named by their value slot: mValues[n] is the
// Mac word at +0x08 + 4 n. The descriptions are in SoundBalance.cpp: 1 QSOUND (+0x0c), 2 REAR (+0x10), 3 CROSSOVER (+0x14), 4 EAX
// (+0x18), 6 MPEGRATE (+0x20), 7 DUCKINGLEVEL (+0x24), 8 DROPPABLELEVEL (+0x28), 10 FADETOGGLE (+0x30); the "ChannelInfo" group
// repeats {CHANNELS, SWITCH} three times: 16 and 17 (+0x48, +0x4c), 18 and 19 (+0x50, +0x54), 20 and 21 (+0x58, +0x5c).
#include <cstdint>
#include <cstring>

#include "Advisor.h"
#include "Debug.h"
#include "SoundBalance.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "lobby.h"
#include "ppc_math.h"

namespace {
enum BalanceSlot {
    kQSound = 1,         // +0x0c
    kRear = 2,           // +0x10
    kCrossover = 3,      // +0x14
    kEAX = 4,            // +0x18
    kMpegRate = 6,       // +0x20
    kDuckingLevel = 7,   // +0x24
    kDroppableLevel = 8,   // +0x28
    kFadeToggle = 10,    // +0x30
    kChannels0 = 16,     // +0x48
    kSwitch0 = 17,       // +0x4c
    kChannels1 = 18,     // +0x50
    kSwitch1 = 19,       // +0x54
    kChannels2 = 20,     // +0x58
    kSwitch2 = 21,       // +0x5c
};

int32_t Balance(BalanceSlot slot) { return gSoundBalance.mValues[slot].mInteger; }

// The PowerPC integer subtraction and multiplication wrap.
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

// divw and divwu leave the result undefined for a zero divisor (and divw for INT32_MIN / -1). The emulator the tests use answers
// the dividend (it divides by 1 instead), and so do the other units' translations; the host must not trap.
int32_t Divw(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

uint32_t Divwu(uint32_t a, uint32_t b) { return b == 0 ? a : a / b; }

// True while the sound system exists and may be talked to (the test every wrapper starts with).
bool SoundIsOn() { return gSoundWanted != 0 && gSoundSystemOn != 0; }

// The signed division by 100 the compiler makes of `volume * ducking / 100` (multiply-high by 0x51eb851f); the product wraps.
int32_t Ducked(int32_t volume, int32_t level) { return Mul(volume, level) / 100; }

// The coordinates arrive as longs and go to the position as floats (the 0x43300000 idiom: exact in double, then rounded to single).
float CoordinateFloat(int32_t value) { return static_cast<float>(static_cast<double>(value)); }

// The keyboard object is made on first use (the original inlines this twice).
SamsUtilities::UKeyboard* Keyboard() {
    if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new SamsUtilities::UKeyboard;
    return SamsUtilities::gKeyboard;
}
}  // namespace

// 0x100dc940: starts the sound system when `wanted` is nonzero (the flags word of the game's options). Always answers 0.
// The channel count is interpolated from the balance table by the quality, over the three ranges the CHANNELS/SWITCH pairs
// give; the audio object is made by the sound library; on success the distance mapping is set, the settings say the
// system is on, and the keyboard decides on the sound channels' safe mode (Option and Command held at start-up).
int32_t SOUND_Init(char* name, uint32_t wanted, int32_t quality) {
    gSoundWanted = wanted;
    if (wanted == 0) return 0;
    if (gSoundSystemOn != 0) {
        GAME_ASSERTMSG(0, "SOUND: Tried to initialise sound system twice !");
        return 0;
    }
    // the bank block (0x102f7c94, 0x34 bytes) and the settings (0x54 bytes) are cleared
    std::memset(gSoundBanks, 0, sizeof gSoundBanks);
    gAdvisorThemeSoundBank = 0;
    gLocalParkMusicBank = 0;
    gLobbyThunderBank = 0;
    gLobbyIslandSounds = nullptr;
    gLobbyIslandSoundCount = 0;
    std::memset(&gSoundSettings, 0, sizeof gSoundSettings);
    gSoundAllocations = 0;

    int32_t from;
    int32_t to;
    double fraction;
    if (Balance(kSwitch2) < quality) {
        from = Balance(kChannels2);
        to = 0x10;
        fraction = static_cast<double>(Sub(quality, Balance(kSwitch2))) / static_cast<double>(Sub(100, Balance(kSwitch2)));
    } else if (Balance(kSwitch1) < quality) {
        from = Balance(kChannels1);
        to = Balance(kChannels2);
        fraction = static_cast<double>(Sub(quality, Balance(kSwitch1))) /
                   static_cast<double>(Sub(Balance(kSwitch2), Balance(kSwitch1)));
    } else {
        from = Balance(kChannels0);
        to = Balance(kChannels1);
        fraction = static_cast<double>(Sub(quality, Balance(kSwitch0))) /
                   static_cast<double>(Sub(Balance(kSwitch1), Balance(kSwitch0)));
    }
    // fmadd: one rounding
    int32_t channels = ppc_fctiwz(ppc_fmadd(fraction, static_cast<double>(Sub(to, from)), static_cast<double>(from)));
    // not below the first CHANNELS word (kept even when it is above 0x10), otherwise at most 0x10
    if (channels < Balance(kChannels0)) {
        channels = Balance(kChannels0);
    } else if (channels > 0x10) {
        channels = 0x10;
    }

    const CAudioConfig config(static_cast<double>(quality) / 100.0, channels);
    const TbError created = CreateAudioObject(name, &gAudioSystem, config);
    if (created >= 0) {
        const TbSysCommand::SetDistanceMapping distance(1.0f);
        gAudioSystem->SystemCommand(distance);
        gSoundSystemOn = 1;
        gSoundSettings.mActive = 1;
        gSoundSettings.mF14 = -1;
        SOUND_SetReverbLevel(0);
        gSoundSettings.mMusicOn = 1;
        gSoundSettings.mSfxOn = 1;
        gSoundSettings.mSpeechOn = 1;
        // safe mode: Option held, then Command held (the keyboard object is looked up again between the two questions)
        if (Keyboard()->IsOptionDown() != 0) {
            if (Keyboard()->IsCommandDown() != 0) SamsUtilities::USoundChannel::SetSafeMode(1);
        }
    }
    return 0;
}

// 0x100dccd4: releases the audio object (which deletes itself; gAudioSystem is left dangling) and checks the allocation count.
void SOUND_Close() {
    if (SoundIsOn()) gAudioSystem->Release();
    gSoundSystemOn = 0;
    if (gSoundAllocations != 0) GAME_ASSERTMSG(0, "SOUND: More malloc than free I would say.");
}

// 0x100dcd80: the channel count for a sound quality. Above the last SWITCH: from the second CHANNELS word up to the library's
// maximum (unsigned arithmetic, the divisor is 100 minus the SECOND switch, as in the listing); above the second SWITCH: the
// second CHANNELS word; otherwise a signed interpolation between the first two. No flag test.
void SOUND_SetChannels(int32_t quality) {
    if (Balance(kSwitch2) < quality) {
        int32_t maximum = 0;   // (a stack word on the Mac; the library always writes it)
        const TbSysCommand::GetMaxNumberOfChannels ask(&maximum);
        gAudioSystem->SystemCommand(ask);
        const uint32_t part = Divwu(static_cast<uint32_t>(Mul(Sub(maximum, Balance(kChannels1)), Sub(quality, Balance(kSwitch1)))),
                                    static_cast<uint32_t>(Sub(100, Balance(kSwitch1))));
        const TbSysCommand::SetNumberOfChannels set(Add(Balance(kChannels1), static_cast<int32_t>(part)));
        gAudioSystem->SystemCommand(set);
    } else if (Balance(kSwitch1) < quality) {
        const TbSysCommand::SetNumberOfChannels set(Balance(kChannels1));
        gAudioSystem->SystemCommand(set);
    } else {
        const int32_t part = Divw(Mul(Sub(Balance(kChannels1), Balance(kChannels0)), Sub(quality, Balance(kSwitch0))),
                                  Sub(Balance(kSwitch1), Balance(kSwitch0)));
        const TbSysCommand::SetNumberOfChannels set(Add(Balance(kChannels0), part));
        gAudioSystem->SystemCommand(set);
    }
}

// 0x100dcf0c: the sound quality's effects, each on or off against its balance threshold.
void SOUND_SetSoundQuality(int32_t quality) {
    if (!SoundIsOn()) return;
    const TbSysCommand::SetQSoundInfo qsound(Balance(kQSound) < quality ? 1 : 0, Balance(kRear) < quality ? 1 : 0,
                                             Balance(kCrossover) < quality ? 1 : 0);
    gAudioSystem->SystemCommand(qsound);
    if (Balance(kEAX) < quality) {
        const TbSysCommand::SetEAX eax(1);
        gAudioSystem->SystemCommand(eax);
    } else {
        SOUND_SetReverbLevel(0);
        const TbSysCommand::SetEAX eax(0);
        gAudioSystem->SystemCommand(eax);
    }
    if (Balance(kMpegRate) < quality) {
        const TbSysCommand::SetHalfRate rate(0);
        gAudioSystem->SystemCommand(rate);
    } else {
        const TbSysCommand::SetHalfRate rate(1);
        gAudioSystem->SystemCommand(rate);
    }
    SOUND_SetOcclusionProcessing(quality);
    SOUND_SetChannels(quality);
    if (Balance(kFadeToggle) < quality) {
        const TbSysCommand::SetFades fades(1);
        gAudioSystem->SystemCommand(fades);
    } else {
        const TbSysCommand::SetFades fades(0);
        gAudioSystem->SystemCommand(fades);
    }
    SOUND_SetDroppableLevel(Balance(kDroppableLevel) < quality ? 0 : 1);
}

// 0x100dd1b4, 0x100dd1f8, 0x100dd238, 0x100dd278: a store and the changed flag, like the status setters in TPSound.cpp.
void SOUND_SetGlobalVolume(uint32_t volume) {
    gSoundSettings.mGlobalVolume = static_cast<int32_t>(volume);
    gSoundSettings.mChanged = 1;
}

void SOUND_SetMusicVolume(uint32_t volume) {
    gSoundSettings.mMusicVolume = static_cast<int32_t>(volume);
    gSoundSettings.mChanged = 1;
}

void SOUND_SetSfxVolume(uint32_t volume) {
    gSoundSettings.mSfxVolume = static_cast<int32_t>(volume);
    gSoundSettings.mChanged = 1;
}

void SOUND_SetSpeechVolume(uint32_t volume) {
    gSoundSettings.mSpeechVolume = static_cast<int32_t>(volume);
    gSoundSettings.mChanged = 1;
}

// 0x100dd380: sets one parameter of a sound (the low byte of `param`); answers the library's word, 0 while the system is off.
int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    if (!SoundIsOn()) return 0;
    return gAudioSystem->UpdateParameter(handle, static_cast<uint8_t>(param), value);
}

// 0x100dd41c and 0x100dd4b0: both send StopAll (the fade is not a different command in this build).
void SOUND_StopAllSfx() {
    if (!SoundIsOn()) return;
    const TbSysCommand::StopAll command;
    gAudioSystem->SystemCommand(command);
}

void SOUND_FadeOutAllSfx() {
    if (!SoundIsOn()) return;
    const TbSysCommand::StopAll command;
    gAudioSystem->SystemCommand(command);
}

// 0x100dd548 and 0x100dd5d8
void SOUND_Pause() {
    if (!SoundIsOn()) return;
    const TbSysCommand::PauseAll command;
    gAudioSystem->SystemCommand(command);
}

void SOUND_UnPause() {
    if (!SoundIsOn()) return;
    const TbSysCommand::RestartAll command;
    gAudioSystem->SystemCommand(command);
}

// 0x100dd668: once a frame. Sends Tick; when a setting changed, sends the music and sfx volumes (ducked by the balance
// table's DUCKINGLEVEL percent while the advisor talks: the duck flag and the speech switch are on and the speech volume is
// above 0; the music is silenced when mMusicMute is nonzero), then the master and speech volumes and the three switches;
// always ends with Process.
void SOUND_Tick() {
    if (!SoundIsOn()) return;
    {
        const TbSysCommand::Tick tick;
        gAudioSystem->SystemCommand(tick);
    }
    if (gSoundSettings.mChanged != 0) {
        if (gSoundSettings.mDuck != 0 && gSoundSettings.mSpeechOn != 0 && gSoundSettings.mSpeechVolume > 0) {
            const int32_t level = Balance(kDuckingLevel);
            const TbSysCommand::SetMusicVolume music(gSoundSettings.mMusicMute != 0 ? 0 : Ducked(gSoundSettings.mMusicVolume, level));
            gAudioSystem->SystemCommand(music);
            const TbSysCommand::SetUser2Volume sfx(Ducked(gSoundSettings.mSfxVolume, level));
            gAudioSystem->SystemCommand(sfx);
        } else {
            const TbSysCommand::SetMusicVolume music(gSoundSettings.mMusicMute != 0 ? 0 : gSoundSettings.mMusicVolume);
            gAudioSystem->SystemCommand(music);
            const TbSysCommand::SetUser2Volume sfx(gSoundSettings.mSfxVolume);
            gAudioSystem->SystemCommand(sfx);
        }
        const TbSysCommand::SetMasterVolume master(gSoundSettings.mGlobalVolume);
        gAudioSystem->SystemCommand(master);
        const TbSysCommand::SetSpeechVolume speech(gSoundSettings.mSpeechVolume);
        gAudioSystem->SystemCommand(speech);
        const TbSysCommand::ToggleSpeech speechOn(static_cast<int32_t>(gSoundSettings.mSpeechOn));
        gAudioSystem->SystemCommand(speechOn);
        const TbSysCommand::ToggleMusic musicOn(static_cast<int32_t>(gSoundSettings.mMusicOn));
        gAudioSystem->SystemCommand(musicOn);
        const TbSysCommand::ToggleSFX sfxOn(static_cast<int32_t>(gSoundSettings.mSfxOn));
        gAudioSystem->SystemCommand(sfxOn);
        gSoundSettings.mChanged = 0;
    }
    const TbSysCommand::Process process;
    gAudioSystem->SystemCommand(process);
}

// 0x100dd9fc: starts the sound `event` of `bank` (event 0 starts nothing); `entity` is the handle of a sound to replace (0 for
// a new one). Answers the handle the audio object gives, 0 while the system is off.
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t x, int32_t y, int32_t z) {
    if (!SoundIsOn() || event == 0) return 0;
    const TbAudioEvent audioEvent(bank, event, TbSoundPosition(CoordinateFloat(x), CoordinateFloat(y), CoordinateFloat(z)), 0.0f);
    return gAudioSystem->SendEvent(entity, audioEvent);
}

// 0x100ddb18, 0x100ddbb8
void SOUND_PauseSound(int32_t handle) {
    if (!SoundIsOn()) return;
    const TbSysCommand::PauseHandle command(handle);
    gAudioSystem->SystemCommand(command);
}

void SOUND_UnpauseSound(int32_t handle) {
    if (!SoundIsOn()) return;
    const TbSysCommand::RestartHandle command(handle);
    gAudioSystem->SystemCommand(command);
}

// 0x100ddc5c
void SOUND_SetVolumeOnSound(int32_t handle, int32_t volume) {
    if (!SoundIsOn()) return;
    const TbSysCommand::SetHandleVolume command(handle, volume);
    gAudioSystem->SystemCommand(command);
}

// 0x100ddd08: SOUND_SendEvent with a last float (w) after the position.
int32_t SOUND_SendEventExt(int32_t entity, int32_t bank, int32_t event, int32_t x, int32_t y, int32_t z, int32_t w) {
    if (!SoundIsOn() || event == 0) return 0;
    const TbAudioEvent audioEvent(bank, event, TbSoundPosition(CoordinateFloat(x), CoordinateFloat(y), CoordinateFloat(z)),
                                  CoordinateFloat(w));
    return gAudioSystem->SendEvent(entity, audioEvent);
}

// 0x100dde38: no flag test; the word is stored as a short.
void SOUND_SetPauseOn3DSoundsOnly(int32_t on) { gSoundSettings.mPauseOn3DOnly = static_cast<int16_t>(on); }

// 0x100dde7c: the listener's position, forward and up vectors. The word set by SOUND_SetPauseOn3DSoundsOnly puts the listener
// at y = 10000.0 (the argument is replaced), velocity is zero.
void SOUND_SetEarPosition(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz) {
    if (!SoundIsOn()) return;
    if (gSoundSettings.mPauseOn3DOnly != 0) y = 10000.0f;
    gAudioSystem->SetListenerOrientation(TbSoundPosition(x, y, z), TbSoundPosition(fx, fy, fz), TbSoundPosition(ux, uy, uz),
                                         TbSoundPosition(0.0f, 0.0f, 0.0f));
}

// 0x100ddf60: moves a sound; answers the library's word (the new handle), 0 while the system is off.
int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t x, int32_t y, int32_t z) {
    if (!SoundIsOn()) return 0;
    return gAudioSystem->UpdateHandle(handle, TbSoundPosition(CoordinateFloat(x), CoordinateFloat(y), CoordinateFloat(z)),
                                      TbSoundPosition(0.0f, 0.0f, 0.0f));
}

// 0x100de058 and 0x100de0fc: both stop the sound (SOUND_FadeSoundEffect sends the same StopHandle command).
void SOUND_StopSoundEffect(int32_t handle) {
    if (!SoundIsOn()) return;
    const TbSysCommand::StopHandle command(handle);
    gAudioSystem->SystemCommand(command);
}

void SOUND_FadeSoundEffect(int32_t handle) {
    if (!SoundIsOn()) return;
    const TbSysCommand::StopHandle command(handle);
    gAudioSystem->SystemCommand(command);
}

// 0x100de758: the length of a sound in milliseconds; 1 while the system is off (the library overwrites the word).
int32_t SOUND_GetCurrentSampleLengthInMilliseconds(int32_t handle) {
    int32_t length = 1;
    if (SoundIsOn()) {
        const TbSysCommand::GetSampleLength command(&length, handle);
        gAudioSystem->SystemCommand(command);
    }
    return length;
}

// The empty wrappers (the sound library's occlusion, reverb and droppable controls and the 3D object list are unused).
void SOUND_SetOcclusionProcessing(int32_t) {}   // 0x100de944
void SOUND_SetReverbLevel(int32_t) {}           // 0x100de97c
void SOUND_SetDroppableLevel(uint32_t) {}       // 0x100de9ac
int32_t SOUND_AddObject(float, float, float, float, float) { return 0; }   // 0x100de8e4
void SOUND_DelObject(uint32_t) {}               // 0x100de918
