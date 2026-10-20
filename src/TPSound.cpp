// The game's sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the
// range under analyser.cpp). PARTIAL: the three status setters (0x100dd2bc..0x100dd33c), SND_DuckVolume and the
// definitions of the sound system's state and of the registered categories. The registration layer built on them
// (SOUND_Malloc, the category wrappers, the SND_Register*/SND_Free* functions) is in SoundRegister.cpp and the runtime
// (SOUND_Init, SOUND_Tick, SOUND_SendEvent and the rest over the audio object) in SoundRuntime.cpp, so that the test
// libraries that only need the setters and the state do not have to fake their callees.
#include "TPSound.h"

SSoundSettings gSoundSettings;

// The sound system's state and the registered categories (see TPSound.h for the Mac addresses).
int32_t gSoundAllocations;
TbAudioSystem* gAudioSystem;
uint32_t gSoundWanted;
uint32_t gSoundSystemOn;
int32_t gSoundBanks[8];
int32_t gLocalParkMusicBank;
int32_t gLobbyThunderBank;                // 0x102f7cbc  (declared in lobby.h)
int32_t* gLobbyIslandSounds;              // 0x102f7cc0  (declared in lobby.h)
int32_t gLobbyIslandSoundCount;

void SOUND_SetMusicStatus(uint32_t on) {
    gSoundSettings.mMusicOn = on;
    gSoundSettings.mChanged = 1;
}

void SOUND_SetSfxStatus(uint32_t on) {
    gSoundSettings.mSfxOn = on;
    gSoundSettings.mChanged = 1;
}

void SOUND_SetSpeechStatus(uint32_t on) {
    gSoundSettings.mSpeechOn = on;
    gSoundSettings.mChanged = 1;
}

// 0x100deac8: the advisor asks for the other sounds to be ducked (1) while he talks and for them to be let up (0). The
// level goes to the settings' word 0x2c (mDuck, which SOUND_Tick reads) and the settings are marked changed.
void SND_DuckVolume(int32_t duck) {
    gSoundSettings.mDuck = duck;
    gSoundSettings.mChanged = 1;
}
