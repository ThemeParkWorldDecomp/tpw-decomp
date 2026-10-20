// The sound registration layer of the game's sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the
// range under analyser.cpp): SOUND_Malloc and SOUND_Free (0x100dc5e4, 0x100dc64c), the category wrappers
// SOUND_RegisterCatagory (two forms) and SOUND_FreeCatagory (0x100dc6ac..0x100dc89c) and the SND_Register*/SND_Free*
// functions that register and free the game's sound categories (0x100def68..0x100df778). The state they work on is
// defined in TPSound.cpp; the audio object is the abstract TbAudioSystem (TbAudioSystem.h).
#include <cstring>

#include "Advisor.h"
#include "Debug.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "lobby.h"

// 0x100dc5e4: a block of the sound system's heap, counted. Nothing for a size of 0.
void* SOUND_Malloc(int32_t size) {
    if (size == 0) return nullptr;
    ++gSoundAllocations;
    return LbMem_Alloc(static_cast<uint32_t>(size));
}

// 0x100dc64c
void SOUND_Free(void* block) {
    if (block == nullptr) return;
    --gSoundAllocations;
    LbMem_Free(block);
}

// 0x100dc6ac: registers the category `name` of the bank file `path` with the audio object; 0 while the system is off.
// (The original leaves its handle word unset if the object never writes it; the host starts it at 0.)
int32_t SOUND_RegisterCatagory(const char* path, const char* name) {
    uint32_t handle = 0;
    if (gSoundWanted == 0 || gSoundSystemOn == 0) {
        handle = 0;
    } else {
        gAudioSystem->RegisterCatagory(path, name, handle);
    }
    return static_cast<int32_t>(handle);
}

// 0x100dc74c: finds the bank file of the category `name` in the resource's storages, then registers it. 0 when there is
// none (the original tests the answer's low byte against 1) or while the system is off.
int32_t SOUND_RegisterCatagory(const char* name, const CTPSoundResource& resource) {
    TbDynamicStringTemplate<char> path;
    bool found;
    {
        TbDynamicStringTemplate<char> nameString(name);
        found = static_cast<uint8_t>(resource.FindSoundFile(nameString, path)) == 1;
    }
    if (!found) return 0;
    if (gSoundWanted == 0 || gSoundSystemOn == 0) return 0;
    uint32_t handle = 0;
    gAudioSystem->RegisterCatagory(path.mData, name, handle);
    return static_cast<int32_t>(handle);
}

// 0x100dc89c: frees the banks of a category (the callers zero their copies of the handle).
void SOUND_FreeCatagory(int32_t category) {
    if (gSoundWanted != 0 && gSoundSystemOn != 0) {
        TbSysCommand::FlushCatagory command(static_cast<uint32_t>(category));
        gAudioSystem->SystemCommand(command);
    }
}

namespace {
// The resource names' strings sit at fixed places in gTPGlobals; the resource found is a sound resource.
const CTPSoundResource& AsSound(CTPBaseResource* resource) {
    return *static_cast<CTPSoundResource*>(resource);
}
}  // namespace

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
