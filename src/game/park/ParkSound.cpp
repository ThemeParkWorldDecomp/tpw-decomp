// ParkSound.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Advisor.h"
#include "Debug.h"
#include "Game.h"
#include "LbFile.h"
#include "lobby.h"
#include "rsse.h"
#include "TbAudioSystem.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPSound.h"
#include <cstdint>
#include <cstring>

// The game's sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the
// range under analyser.cpp). PARTIAL: the three status setters (0x100dd2bc..0x100dd33c), SND_DuckVolume and the
// definitions of the sound system's state and of the registered categories. The registration layer built on them
// (SOUND_Malloc, the category wrappers, the SND_Register*/SND_Free* functions) is in SoundRegister.cpp and the runtime
// (SOUND_Init, SOUND_Tick, SOUND_SendEvent and the rest over the audio object) in SoundRuntime.cpp, so that the test
// libraries that only need the setters and the state do not have to fake their callees.

extern SSoundSettings gSoundSettings;

// 0x100deac8: the advisor asks for the other sounds to be ducked (1) while he talks and for them to be let up (0). The
// level goes to the settings' word 0x2c (mDuck, which SOUND_Tick reads) and the settings are marked changed.
void SND_DuckVolume(int32_t duck) {
    gSoundSettings.mDuck = duck;
    gSoundSettings.mChanged = 1;
}

// The park-level sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the range under analyser.cpp): the
// park music, the chatter and the rain over the runtime's SOUND_SendEvent, SOUND_SetParamValue and SOUND_FadeSoundEffect
// (SoundRuntime.cpp), SND_DipMusic (the twin of SND_DuckVolume in TPSound.cpp), SND_SendIndirectedEvent, and the two functions
// that put the sound settings block in a game save and read it back (SOUND_SaveStatus, SOUND_LoadStatus). The handles are the
// first words of the settings block (SSoundSettings in TPSound.h); every sound the wrappers start is kept there.

namespace {

// The sound parameters the wrappers set.
constexpr uint32_t kParamParkMusicLevel = 4;
constexpr uint32_t kParamChatterLevel = 7;
constexpr uint32_t kParamRainLevel = 8;
// The sound events the wrappers start (the banks are the categories: gLocalParkMusicBank, gSoundBanks[1] kids, [0] ambient).
constexpr int32_t kEventParkMusic = 2;
constexpr int32_t kEventChatter = 0x5b;
constexpr int32_t kEventRain = 0x21;
constexpr int32_t kChatterLevelMax = 100;
constexpr int32_t kRainLevelMax = 100;

}  // namespace

// 0x100deb04: the twin of SND_DuckVolume (TPSound.cpp): the level is stored as a short in the music mute word and the settings
// are marked changed.
void SND_DipMusic(int32_t level) {
    gSoundSettings.mMusicMute = static_cast<int16_t>(level);
    gSoundSettings.mChanged = 1;
}

// 0x100deb40: starts the park theme's music (an event of gLocalParkMusicBank) with the handle the word already holds as the
// entity, then sets its level parameter to 0. Both answers are stored back as the handle.
void SND_StartParkMusic() {
    gSoundSettings.mParkMusicHandle = SOUND_SendEvent(gSoundSettings.mParkMusicHandle, gLocalParkMusicBank, kEventParkMusic, 0, 0, 0);
    gSoundSettings.mParkMusicHandle = SOUND_SetParamValue(gSoundSettings.mParkMusicHandle, kParamParkMusicLevel, 0);
}

// 0x100debd4: fades the park music when there is a handle. The handle is not cleared.
void SND_StopParkMusic() {
    if (gSoundSettings.mParkMusicHandle != 0) SOUND_FadeSoundEffect(gSoundSettings.mParkMusicHandle);
}

// 0x100dec30
void SND_SetParkMusicLevel(int32_t level) {
    gSoundSettings.mParkMusicHandle = SOUND_SetParamValue(gSoundSettings.mParkMusicHandle, kParamParkMusicLevel, level);
}

// 0x100dec9c: the stored level is clamped to 100 from above only (no clamp below), but the parameter call below is given the
// argument as it came (a quirk of the original: the stored word and the parameter differ when the argument is above 100).
// While the chatter is on: a level of 0 fades the sound and clears its handle, any other level starts the sound when there is no
// handle and sets its level parameter.
void SND_SetChatterLevel(int32_t level) {
    gSoundSettings.mChatterLevel = level;
    if (gSoundSettings.mChatterLevel > kChatterLevelMax) gSoundSettings.mChatterLevel = kChatterLevelMax;
    if (gSoundSettings.mActive != 0) {
        if (gSoundSettings.mChatterLevel == 0) {
            if (gSoundSettings.mChatterHandle != 0) {
                SOUND_FadeSoundEffect(gSoundSettings.mChatterHandle);
                gSoundSettings.mChatterHandle = 0;
            }
        } else {
            if (gSoundSettings.mChatterHandle == 0) {
                gSoundSettings.mChatterHandle = SOUND_SendEvent(0, gSoundBanks[1], kEventChatter, 0, 0, 0);
            }
            gSoundSettings.mChatterHandle = SOUND_SetParamValue(gSoundSettings.mChatterHandle, kParamChatterLevel, level);
        }
    }
}

// 0x100deda4: the chatter-on word (the settings' 0x08, mActive) takes the argument. Off: the sound fades and its handle is
// cleared. On without a sound: it starts at the stored level (read as unsigned, not clamped here). On with a sound: nothing.
void SND_SetChatterOnOff(int32_t on) {
    gSoundSettings.mActive = static_cast<uint32_t>(on);
    if (gSoundSettings.mActive == 0) {
        if (gSoundSettings.mChatterHandle != 0) {
            SOUND_FadeSoundEffect(gSoundSettings.mChatterHandle);
            gSoundSettings.mChatterHandle = 0;
        }
    } else if (gSoundSettings.mChatterHandle == 0) {
        gSoundSettings.mChatterHandle = SOUND_SendEvent(0, gSoundBanks[1], kEventChatter, 0, 0, 0);
        gSoundSettings.mChatterHandle =
            SOUND_SetParamValue(gSoundSettings.mChatterHandle, kParamChatterLevel, static_cast<uint32_t>(gSoundSettings.mChatterLevel));
    }
}

// 0x100dee84: the level is clamped to 0 to 100 (above first, then below). 0 fades the rain when it has a handle (the handle is
// not cleared); any other level starts the rain (the ambient category) when there is no handle and sets its level parameter.
void SND_SetRainLevel(int32_t level) {
    if (level > kRainLevelMax) level = kRainLevelMax;
    if (level < 0) level = 0;
    if (level == 0) {
        if (gSoundSettings.mRainHandle != 0) SOUND_FadeSoundEffect(gSoundSettings.mRainHandle);
    } else {
        if (gSoundSettings.mRainHandle == 0) {
            gSoundSettings.mRainHandle = SOUND_SendEvent(0, gSoundBanks[0], kEventRain, 0, 0, 0);
        }
        gSoundSettings.mRainHandle = SOUND_SetParamValue(gSoundSettings.mRainHandle, kParamRainLevel, level);
    }
}

// The sound registration layer of the game's sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the
// range under analyser.cpp): SOUND_Malloc and SOUND_Free (0x100dc5e4, 0x100dc64c), the category wrappers
// SOUND_RegisterCatagory (two forms) and SOUND_FreeCatagory (0x100dc6ac..0x100dc89c) and the SND_Register*/SND_Free*
// functions that register and free the game's sound categories (0x100def68..0x100df778). The state they work on is
// defined in TPSound.cpp; the audio object is the abstract TbAudioSystem (TbAudioSystem.h).

void* SOUND_Malloc(int32_t size);

void SOUND_Free(void* block);

int32_t SOUND_RegisterCatagory(const char* path, const char* name);

int32_t SOUND_RegisterCatagory(const char* name, const CTPSoundResource& resource);

void SOUND_FreeCatagory(int32_t category);

namespace {
// The resource names' strings sit at fixed places in gTPGlobals; the resource found is a sound resource.
const CTPSoundResource& AsSound(CTPBaseResource* resource) {
    return *static_cast<CTPSoundResource*>(resource);
}
}  // namespace

// 0x100def68: the lobby's global category (the thunder), from "GlobalSounds".
void SND_RegisterGlobalLobbySounds() {
    CTPBaseResource* resource = nullptr;
    GAME_ASSERTMSG(gLobbyThunderBank == 0, "SND: Someone is trying to register the Global Lobby category twice - Grrr !");
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4768, &resource) == 1) {
        gLobbyThunderBank = SOUND_RegisterCatagory("cat_globallobbysfx", AsSound(resource));
    }
}

// 0x100df02c: the sound set of one island, from the resource "LocalSounds_<name>": its music and sfx categories go into a
// new row at the end of gLobbyIslandSounds (a block regrown by one row each time, the old rows copied). Answers the row
// (word 0 the sfx, word 1 the music), or -1 when there is no such resource.
int32_t SND_RegisterLocalLobbySounds(char* name) {
    CTPBaseResource* resource = nullptr;
    TbDynamicStringTemplate<char> resourceName(gTPGlobals.mF4790);
    resourceName += name;
    if (gGame.mResourceManager->FindResource(resourceName, &resource) == 1) {
        int32_t music = SOUND_RegisterCatagory("cat_locallobbymusic", AsSound(resource));
        int32_t sfx = SOUND_RegisterCatagory("cat_locallobbysfx", AsSound(resource));
        ++gLobbyIslandSoundCount;
        int32_t* table = static_cast<int32_t*>(SOUND_Malloc(gLobbyIslandSoundCount << 3));
        if (gLobbyIslandSounds != nullptr) {
            std::memcpy(table, gLobbyIslandSounds, (gLobbyIslandSoundCount - 1) * 8);
            SOUND_Free(gLobbyIslandSounds);
        }
        gLobbyIslandSounds = table;
        gLobbyIslandSounds[gLobbyIslandSoundCount * 2 - 1] = music;
        gLobbyIslandSounds[gLobbyIslandSoundCount * 2 - 2] = sfx;
        return gLobbyIslandSoundCount - 1;
    }
    return -1;
}

// 0x100df1bc
void SND_FreeGlobalLobbySounds() {
    if (gLobbyThunderBank != 0) {
        SOUND_FreeCatagory(gLobbyThunderBank);
        gLobbyThunderBank = 0;
    }
}

// 0x100df234: frees every island's two categories (sfx first) and the table.
void SND_FreeLocalLobbySounds() {
    if (gLobbyIslandSounds != nullptr) {
        for (int32_t row = 0; row < gLobbyIslandSoundCount; ++row) {
            SOUND_FreeCatagory(gLobbyIslandSounds[row * 2]);
            SOUND_FreeCatagory(gLobbyIslandSounds[row * 2 + 1]);
        }
        SOUND_Free(gLobbyIslandSounds);
        gLobbyIslandSounds = nullptr;
        gLobbyIslandSoundCount = 0;
    }
}

// 0x100df310: the six global categories, from the resources "GlobalSounds" (all but the speech) and "GlobalSpeech". The
// registration order is ambient, rides, ui, kids, staff, then speech; each is asserted not to be registered yet.
void SND_RegisterGlobalSounds() {
    GAME_ASSERTMSG(gSoundBanks[0] == 0, "SND: Someone is trying to register the Global Ambient category twice - Grrr !");
    GAME_ASSERTMSG(gSoundBanks[2] == 0, "SND: Someone is trying to register the Global Ride category twice - Grrr !");
    GAME_ASSERTMSG(gSoundBanks[3] == 0, "SND: Someone is trying to register the Global Ui category twice - Grrr !");
    GAME_ASSERTMSG(gSoundBanks[1] == 0, "SND: Someone is trying to register the Global kids category twice - Grrr !");
    GAME_ASSERTMSG(gSoundBanks[4] == 0, "SND: Someone is trying to register the Global staff category twice - Grrr !");
    GAME_ASSERTMSG(gSoundBanks[5] == 0, "SND: Someone is trying to register the Global speech category twice - Grrr !");
    CTPBaseResource* resource = nullptr;
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4768, &resource) == 1) {
        gSoundBanks[0] = SOUND_RegisterCatagory("cat_ambient", AsSound(resource));
        gSoundBanks[2] = SOUND_RegisterCatagory("cat_rides", AsSound(resource));
        gSoundBanks[3] = SOUND_RegisterCatagory("cat_ui", AsSound(resource));
        gSoundBanks[1] = SOUND_RegisterCatagory("cat_kids", AsSound(resource));
        gSoundBanks[4] = SOUND_RegisterCatagory("cat_staff", AsSound(resource));
    }
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF477C, &resource) == 1) {
        gSoundBanks[5] = SOUND_RegisterCatagory("cat_speech", AsSound(resource));
    }
}

// 0x100df4d4: the park theme's categories: ambient and rides from "LocalSounds_<theme>", the speech from
// "LocalSpeech_<theme>" and the music from "LocalMusic_<theme>". Each resource is optional.
void SND_RegisterLocalParkSounds(const TbDynamicStringTemplate<char>& theme) {
    CTPBaseResource* resource = nullptr;
    TbDynamicStringTemplate<char> resourceName(gTPGlobals.mF4790);
    resourceName += theme;
    if (gGame.mResourceManager->FindResource(resourceName, &resource) == 1) {
        gSoundBanks[6] = SOUND_RegisterCatagory("cat_ambient", AsSound(resource));
        gSoundBanks[7] = SOUND_RegisterCatagory("cat_rides", AsSound(resource));
    }
    resourceName = gTPGlobals.mF47A4;
    resourceName += theme;
    if (gGame.mResourceManager->FindResource(resourceName, &resource) == 1) {
        gAdvisorThemeSoundBank = SOUND_RegisterCatagory("cat_speech", AsSound(resource));
    }
    resourceName = gTPGlobals.mF47B8;
    resourceName += theme;
    if (gGame.mResourceManager->FindResource(resourceName, &resource) == 1) {
        gLocalParkMusicBank = SOUND_RegisterCatagory("cat_music", AsSound(resource));
    }
}

// 0x100df67c: frees the global categories (ambient, rides, ui, kids, staff, speech) and then zeroes them all.
void SND_FreeGlobalSounds() {
    static const int kOrder[6] = {0, 2, 3, 1, 4, 5};
    for (int index : kOrder) {
        if (gSoundBanks[index] != 0) SOUND_FreeCatagory(gSoundBanks[index]);
    }
    for (int index : kOrder) gSoundBanks[index] = 0;
}

// 0x100df778: frees the park theme's four categories, then zeroes them.
void SND_FreeLocalParkSounds() {
    if (gSoundBanks[6] != 0) SOUND_FreeCatagory(gSoundBanks[6]);
    if (gSoundBanks[7] != 0) SOUND_FreeCatagory(gSoundBanks[7]);
    if (gAdvisorThemeSoundBank != 0) SOUND_FreeCatagory(gAdvisorThemeSoundBank);
    if (gLocalParkMusicBank != 0) SOUND_FreeCatagory(gLocalParkMusicBank);
    gSoundBanks[6] = 0;
    gSoundBanks[7] = 0;
    gAdvisorThemeSoundBank = 0;
    gLocalParkMusicBank = 0;
}

// 0x100df858: the event code RSSE_GetSoundEventCode finds for (a, b); 0 (no such event) answers 0 and starts nothing, else the
// event goes to SOUND_SendEvent. The answer is the sound's handle.
int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    const int32_t code = RSSE_GetSoundEventCode(a, b);
    if (code == 0) return 0;
    return SOUND_SendEvent(entity, category, code, p5, p6, p7);
}
