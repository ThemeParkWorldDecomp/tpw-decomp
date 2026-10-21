// The game's sound wrappers (SOUND_*, SND_*). The original file name is unknown: the
// inventory files this range under analyser.cpp. The runtime wrappers (SOUND_Init to SOUND_GetCurrentSampleLengthInMilliseconds)
// are in SoundRuntime.cpp, the registration layer in SoundRegister.cpp, the status setters and the state in TPSound.cpp.
// The park music, chatter and rain wrappers and the status save/load are in SoundPark.cpp.
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"

class CTPSoundResource;
class TbAudioSystem;

// The wrappers' own state (0x102f7c40, BSS, 0x54 bytes; names invented). SOUND_Init clears all of it. The volumes are
// what SOUND_Tick hands the audio object; every setter raises mChanged.
// The handles and levels below 0x14 are SoundPark.cpp's (SOUND_SaveStatus writes the block but for 0x0c and 0x44 to 0x53, which
// it copies raw, and SOUND_LoadStatus reads it back).
struct SSoundSettings {
    int32_t mParkMusicHandle;       // 0x00  SND_StartParkMusic, SND_SetParkMusicLevel: the park music's sound handle
    int32_t mChatterHandle;         // 0x04  the chatter sound's handle (0 while it does not play)
    uint32_t mActive;               // 0x08  SOUND_Init sets it to 1 once the audio object exists; SND_SetChatterOnOff stores its
                                    //       argument here (the chatter-on word of the park wrappers)
    int32_t mRainHandle;            // 0x0c  SND_SetRainLevel: the rain sound's handle (not saved: copied raw)
    int32_t mChatterLevel;          // 0x10  SND_SetChatterLevel: the level, at most 100
    int32_t mF14;                   // 0x14  SOUND_Init sets it to -1; SOUND_LoadStatus hands it to the audio object as the preset
                                    //       reverb (TbSysCommand::SetPresetReverb), so it is the reverb preset
    int32_t mGlobalVolume;          // 0x18  SOUND_SetGlobalVolume
    int32_t mSfxVolume;             // 0x1c  SOUND_SetSfxVolume
    int32_t mMusicVolume;           // 0x20  SOUND_SetMusicVolume
    int32_t mSpeechVolume;          // 0x24  SOUND_SetSpeechVolume
    int32_t mChanged;               // 0x28
    int32_t mDuck;                  // 0x2c  SND_DuckVolume: 1 while the advisor talks (SOUND_Tick ducks the music and sfx)
    int16_t mMusicMute;             // 0x30  nonzero: SOUND_Tick sends music volume 0 (SND_DipMusic sets it)
    int16_t mPauseOn3DOnly;         // 0x32  SOUND_SetPauseOn3DSoundsOnly; SOUND_SetEarPosition puts the listener's y at 10000
    uint32_t mF34;                  // 0x34  (a word in the save; retyped from uint8_t[4] by SoundPark.cpp, the layout is the same)
    uint32_t mSfxOn;                // 0x38
    uint32_t mMusicOn;              // 0x3c
    uint32_t mSpeechOn;             // 0x40
    uint8_t mF44[0x54 - 0x44];      // 0x44
};
static_assert(sizeof(SSoundSettings) == 0x54, "the sound settings block");
extern SSoundSettings gSoundSettings;

// Starts the sound system when `flags` is nonzero; answers 0 always (the system is on when gSoundSystemOn is set afterwards).
int32_t SOUND_Init(char* name, uint32_t flags, int32_t quality);   // 0x100dc940
void SOUND_Close();                         // 0x100dccd4
void SOUND_SetChannels(int32_t quality);       // 0x100dcd80
void SOUND_SetSoundQuality(int32_t quality);   // 0x100dcf0c
void SOUND_SetGlobalVolume(uint32_t volume);   // 0x100dd1b4
void SOUND_SetMusicVolume(uint32_t volume);    // 0x100dd1f8
void SOUND_SetSfxVolume(uint32_t volume);      // 0x100dd238
void SOUND_SetSpeechVolume(uint32_t volume);   // 0x100dd278
void SOUND_SetMusicStatus(uint32_t on);     // 0x100dd2bc
void SOUND_SetSfxStatus(uint32_t on);       // 0x100dd2fc
void SOUND_SetSpeechStatus(uint32_t on);    // 0x100dd33c
void SOUND_StopAllSfx();                    // 0x100dd41c
void SOUND_FadeOutAllSfx();                 // 0x100dd4b0
void SOUND_Pause();                         // 0x100dd548
void SOUND_UnPause();                       // 0x100dd5d8
void SOUND_SetPauseOn3DSoundsOnly(int32_t on); // 0x100dde38
int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value);   // 0x100dd380
void SOUND_Tick();                          // 0x100dd668
// (Answers the handle of the sound it started, which the lobby's islands keep; other callers ignore it.)
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c);   // 0x100dd9fc
// SendEvent with a last float (the seventh argument).
int32_t SOUND_SendEventExt(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c, int32_t d);   // 0x100ddd08
// The listener: position, forward and up vectors.
void SOUND_SetEarPosition(float x, float y, float z, float fx, float fy, float fz, float ux, float uy,
                          float uz);        // 0x100dde7c
void SOUND_SetOcclusionProcessing(int32_t on);   // 0x100de944 (empty)
void SOUND_SetReverbLevel(int32_t level);        // 0x100de97c (empty)
void SOUND_SetDroppableLevel(uint32_t level);    // 0x100de9ac (empty)
int32_t SOUND_AddObject(float a, float b, float c, float d, float e);   // 0x100de8e4: answers 0
void SOUND_DelObject(uint32_t object);           // 0x100de918 (empty)
// The sound handles the advisor keeps (the answer of SOUND_SendEvent is the handle).
void SOUND_PauseSound(int32_t handle);                            // 0x100ddb18
void SOUND_UnpauseSound(int32_t handle);                          // 0x100ddbb8
int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t x, int32_t y, int32_t z);   // 0x100ddf60
void SOUND_SetVolumeOnSound(int32_t handle, int32_t volume);      // 0x100ddc5c
void SOUND_StopSoundEffect(int32_t handle);                       // 0x100de058
void SOUND_FadeSoundEffect(int32_t handle);                       // 0x100de0fc
int32_t SOUND_GetCurrentSampleLengthInMilliseconds(int32_t handle);   // 0x100de758: 1 when the sound system is off
void SND_DuckVolume(int32_t duck);          // 0x100deac8
void SND_DipMusic(int32_t level);           // 0x100deb04 (SoundPark.cpp): the twin of SND_DuckVolume: sets mMusicMute (a short)
void SND_StartParkMusic();                  // 0x100deb40
void SND_StopParkMusic();                   // 0x100debd4
void SND_SetParkMusicLevel(int32_t level);  // 0x100dec30
void SND_SetChatterLevel(int32_t level);    // 0x100dec9c
void SND_SetChatterOnOff(int32_t on);       // 0x100deda4
void SND_SetRainLevel(int32_t level);       // 0x100dee84
// The sound event code of (a, b) from RSSE_GetSoundEventCode (0x100d6d04, src/game/rides/rsse.cpp); 0 answers 0, else
// SOUND_SendEvent(entity, category, code, p5, p6, p7).
int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6,
                                int32_t p7);   // 0x100df858
int32_t RSSE_GetSoundEventCode(int32_t a, int32_t b);   // 0x100d6d04 (src/game/rides/rsse.cpp, 328 bytes, unit w171)
// The game save's sound block (SoundPark.cpp): the system-on word, the wanted word, the size 0x54 and the settings block
// (little-endian in the file), then the audio object's own data (TbSysCommand::Save); the load reads the same.
void SOUND_SaveStatus(void* file);          // 0x100de1a0
void SOUND_LoadStatus(void* file);          // 0x100de48c
// The registration layer (0x100dc5e4 to 0x100dc89c, 0x100def68 to 0x100df778). The category wrappers answer the category's
// handle (0 when the sound system is off or the sound file is not found).
void* SOUND_Malloc(int32_t size);           // 0x100dc5e4: counted LbMem_Alloc; NULL for 0
void SOUND_Free(void* block);               // 0x100dc64c
int32_t SOUND_RegisterCatagory(const char* path, const char* name);                  // 0x100dc6ac
int32_t SOUND_RegisterCatagory(const char* name, const CTPSoundResource& resource);  // 0x100dc74c
void SOUND_FreeCatagory(int32_t category);  // 0x100dc89c
void SND_RegisterGlobalSounds();            // 0x100df310
void SND_RegisterGlobalLobbySounds();       // 0x100def68
// Registers the lobby sound set "LocalSounds_<name>" and answers its row in gLobbyIslandSounds, or -1.
int32_t SND_RegisterLocalLobbySounds(char* name);   // 0x100df02c
void SND_FreeGlobalLobbySounds();           // 0x100df1bc
void SND_FreeLocalLobbySounds();            // 0x100df234
void SND_RegisterLocalParkSounds(const TbDynamicStringTemplate<char>& theme);   // 0x100df4d4
void SND_FreeGlobalSounds();                // 0x100df67c
void SND_FreeLocalParkSounds();             // 0x100df778

// The sound system's state (BSS, 0x102f7c30 to 0x102f7c3c; names invented). SOUND_Init (untranslated, stays a stand-in
// in the launcher) sets them; while gSoundSystemOn is 0 every registration answers 0 without touching the object.
extern int32_t gSoundAllocations;           // 0x102f7c30  blocks SOUND_Malloc handed out and SOUND_Free has not got back
extern TbAudioSystem* gAudioSystem;         // 0x102f7c34  the audio object
extern uint32_t gSoundWanted;               // 0x102f7c38  the flags word SOUND_Init is given first
extern uint32_t gSoundSystemOn;             // 0x102f7c3c  1 once SOUND_Init made the object

// The registered sound categories: one block, 0x102f7c94, 0x34 bytes, cleared by SOUND_Init (the host has it in pieces;
// names invented). The first six are the global categories: [0] ambient, [1] kids, [2] rides, [3] ui, [4] staff, [5] the
// global speech; [6] and [7] are the park theme's ambient and rides. GAMESYSTEM_Init sends its start-up event to [3]; the
// advisor uses [5]. The rest of the block, by offset: +0x20 gAdvisorThemeSoundBank (Advisor.h; the park theme's speech),
// +0x24 gLocalParkMusicBank, +0x28 gLobbyThunderBank (lobby.h; the lobby's global sfx), +0x2c gLobbyIslandSounds (lobby.h; the
// island table) and +0x30 gLobbyIslandSoundCount.
extern int32_t gSoundBanks[8];
extern int32_t gLocalParkMusicBank;         // 0x102f7cb8  the park theme's music category (original name unknown)
extern int32_t gLobbyIslandSoundCount;      // 0x102f7cc4  rows of gLobbyIslandSounds (original name unknown)
