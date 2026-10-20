// The park-level sound wrappers (main app of SimTheme Park 2.04b3; the inventory files the range under analyser.cpp): the
// park music, the chatter and the rain over the runtime's SOUND_SendEvent, SOUND_SetParamValue and SOUND_FadeSoundEffect
// (SoundRuntime.cpp), SND_DipMusic (the twin of SND_DuckVolume in TPSound.cpp), SND_SendIndirectedEvent, and the two functions
// that put the sound settings block in a game save and read it back (SOUND_SaveStatus, SOUND_LoadStatus). The handles are the
// first words of the settings block (SSoundSettings in TPSound.h); every sound the wrappers start is kept there.
#include <cstdint>

#include "LbFile.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "rsse.h"

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

// The save's size word: the settings block is written whole.
constexpr uint32_t kSavedSettingsSize = 0x54;

// A file's little-endian word in the host's order (the original's inline byte reversal; nothing on a little-endian host).
inline uint32_t FileWord(uint32_t word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

// SamsUtilities::USwapInplace32 on a word of unsigned type (the original's template is on long).
inline void SwapWord(uint32_t& word) { SamsUtilities::USwapInplace32(reinterpret_cast<int32_t&>(word)); }
inline void SwapWord(int32_t& word) { SamsUtilities::USwapInplace32(word); }
inline void SwapWord(int16_t& half) { SamsUtilities::USwapInplace16(half); }

// The fields of the settings block that are kept in the save, in the order the original swaps them: the block's words but for
// 0x0c (the rain handle) and 0x44 to 0x53 (raw), with the two shorts at 0x30 and 0x32. SaveStatus swaps its copy with them
// and LoadStatus the block itself, so a sound handle (0x0c) and the tail never go through a swap.
void SwapSavedFields(SSoundSettings& settings) {
    SwapWord(settings.mParkMusicHandle);
    SwapWord(settings.mChatterHandle);
    SwapWord(settings.mActive);
    SwapWord(settings.mChatterLevel);
    SwapWord(settings.mF14);
    SwapWord(settings.mGlobalVolume);
    SwapWord(settings.mSfxVolume);
    SwapWord(settings.mMusicVolume);
    SwapWord(settings.mSpeechVolume);
    SwapWord(settings.mChanged);
    SwapWord(settings.mDuck);
    SwapWord(settings.mMusicMute);
    SwapWord(settings.mPauseOn3DOnly);
    SwapWord(settings.mF34);
    SwapWord(settings.mSfxOn);
    SwapWord(settings.mMusicOn);
    SwapWord(settings.mSpeechOn);
}

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

// 0x100df858: the event code RSSE_GetSoundEventCode finds for (a, b); 0 (no such event) answers 0 and starts nothing, else the
// event goes to SOUND_SendEvent. The answer is the sound's handle.
int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    const int32_t code = RSSE_GetSoundEventCode(a, b);
    if (code == 0) return 0;
    return SOUND_SendEvent(entity, category, code, p5, p6, p7);
}

// 0x100de1a0: writes, little-endian and each with its own LbFile_Write (the transferred count is not asked for, the results
// dropped): the system-on word, the wanted word, the size 0x54 and a 0x54-byte copy of the settings block with the saved fields
// swapped. Then, when the sound was asked for and the system is on, the audio object adds its own data (TbSysCommand::Save).
void SOUND_SaveStatus(void* file) {
    uint32_t word = FileWord(gSoundSystemOn);
    LbFile_Write(file, &word, sizeof word, nullptr);
    word = FileWord(gSoundWanted);
    LbFile_Write(file, &word, sizeof word, nullptr);
    word = FileWord(kSavedSettingsSize);
    LbFile_Write(file, &word, sizeof word, nullptr);
    SSoundSettings copy = gSoundSettings;
    SwapSavedFields(copy);
    LbFile_Write(file, &copy, sizeof copy, nullptr);
    if (gSoundWanted != 0 && gSoundSystemOn != 0) {
        const TbSysCommand::Save save(file);
        gAudioSystem->SystemCommand(save);
    }
}

// 0x100de48c: reads what SOUND_SaveStatus wrote: the saved system-on word (kept in a local), the wanted word (straight into
// gSoundWanted) and the size, then `size` bytes into the settings block, whatever the size says (a size above 0x54 runs on into
// the words after the block, as on the Mac), and swaps the saved fields in place. Afterwards: a save made without sound
// (system-on word 0) keeps the wanted word the game had before when that was nonzero. A save made with sound passes the file
// to the audio object (Load, then the preset reverb the block now holds), or, with no audio object, skips the object's block:
// the first three words of it are read and dropped, the fourth is its size and the file moves on by that size minus 4 (the size
// is read raw, in the host's own byte order, as the original does).
void SOUND_LoadStatus(void* file) {
    const uint32_t oldWanted = gSoundWanted;
    uint32_t savedOn;
    int32_t size;
    LbFile_Read(file, &savedOn, sizeof savedOn, nullptr);
    LbFile_Read(file, &gSoundWanted, sizeof gSoundWanted, nullptr);
    LbFile_Read(file, &size, sizeof size, nullptr);
    savedOn = FileWord(savedOn);
    gSoundWanted = FileWord(gSoundWanted);
    const uint32_t bytes = FileWord(static_cast<uint32_t>(size));
    LbFile_Read(file, &gSoundSettings, bytes, nullptr);
    SwapSavedFields(gSoundSettings);
    if (savedOn == 0) {
        if (oldWanted != 0) gSoundWanted = oldWanted;
    } else if (gAudioSystem == nullptr) {
        uint32_t scratch = 0;
        int32_t skip = 0;
        LbFile_Read(file, &scratch, sizeof scratch, nullptr);
        LbFile_Read(file, &scratch, sizeof scratch, nullptr);
        LbFile_Read(file, &scratch, sizeof scratch, nullptr);
        LbFile_Read(file, &skip, sizeof skip, nullptr);
        LbFile_Seek(file, skip - 4, 1);
    } else {
        const TbSysCommand::Load load(file, 1);
        gAudioSystem->SystemCommand(load);
        const TbSysCommand::SetPresetReverb reverb(gSoundSettings.mF14);
        gAudioSystem->SystemCommand(reverb);
    }
}
