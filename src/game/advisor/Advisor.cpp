// Advisor.cpp: name proven by __sinit_Advisor_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "effect_manager.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "helpers/AdvisorHelpers.h"
#include "helpers/ThingHelpers.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Light.h"
#include "Main.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ParkMessages.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "std_basic_filebuf.h"
#include "tb_hfield.h"
#include "tb_rain.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <vector>

// Declared before the includes below, whose inline code would otherwise instantiate the general template first: the definitions are further down.
template<> int32_t LOADVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog);
template<> int32_t SAVEVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog);

#include "AdvisorQueue.h"
#include "MsgAdvisor.h"

// Four small leaves of the main app (SimTheme Park 2.04b3) that the effect manager (src/effect_manager.cpp) calls
// every pass: the real-time clock that stands still while the game is paused (RealTimePauseAdjusted,
// 0x10007c68, the inventory's Advisor.cpp range) and the table of sines (HOST_Sin and HOST_Cos, 0x100218f4 and
// 0x10021928, and BMP_GenerateSinTab, 0x100259d0, the inventory's TPAdvisor.cpp range). See
// include/effect_manager.h.

// The game clock in milliseconds less the time it has spent paused, and while it is paused the moment the pause
// began: the same count as the advisor's clock (SAdvisorState keeps the pause: it is told of a pause here, on the
// first read after it, and of its end on the first read after that). The reads of the clock are in the
// original's order: the time first, then the pause flag.
uint32_t RealTimePauseAdjusted() {
    SAdvisorState& state = gAdvisorState;
    const int32_t now = gClock.ReadFixed();
    if (gClock.GetPauseFlag() != 0) {
        if (state.mClockPaused == 0) {
            state.mClockPaused = 1;
            state.mPauseStart = now;
        }
        return static_cast<uint32_t>(state.mPauseStart - state.mPausedTotal);
    }
    if (state.mClockPaused != 0) {
        state.mClockPaused = 0;
        state.mPausedTotal = state.mPausedTotal + (now - state.mPauseStart);
    }
    return static_cast<uint32_t>(now - state.mPausedTotal);
}

// Advisor.cpp (main app of SimTheme Park 2.04b3): the advisor's per-frame state machine. Translated: ADV_Pause and
// ADV_UnPause (0x10007d54, 0x10007dd4), ADV_Init and ADV_Close (0x10007e5c, 0x10007fc4), GenerateAnimSequence,
// SetCannedSequence, SetAdvisorHatHand and DressAdvisor (0x1000804c..0x100087d8), ADV_Tick2 and ADV_Tick (0x100091d8,
// 0x10009298), and the part that makes him speak: ADV_Call, ADV_Slap, ADV_Kill, ADV_GetTag and ADV_Stat (0x1000887c,
// 0x10008dd4, 0x10008f00, 0x10009050, 0x100090dc), over the response table of src/advisor_lines.cpp. Not translated
// here: the light class that shares the range (src/Light.cpp).
//
// The advisor is one animated mesh (slot gAdvisorStore[mWhich], src/TPAdvisor.cpp) in ride animation set group 5. A line
// he says is a sequence of animations (mSequence, 1 to 15, the effect bits 0x7f0000 beside the number) that ADV_Tick
// starts one after another as the previous one ends, a sound and the mouth shapes of the line's lip-sync data. ADV_Tick2
// dresses him (hats, hands, sparkles) once the animation has started. ADV_Call fills the state in: it looks the line up in
// the table, opens its lip-sync file, starts the sound and queues the sequence.
//
// The time here is the advisor clock: the game clock less the time spent paused.

SAdvisorState gAdvisorState;                  // 0x102b354c
int32_t gAdvisorStaticInUse;                  // 0x102b36b4
int32_t gAdvisorStaticBuffer[250];            // 0x102b36b8
int32_t gAdvisorThemeSoundBank;               // 0x102f7cb4 (gSoundBanks + 0x20)
void* gAdvisorFile;                           // 0x102b3548

// 0x1027d944. A row is the animations of one canned line; 0 ends it. 0x400000 is not an animation: the word after it is
// the time of the "key" event, counted from the start of the sequence.
const uint32_t gCannedSequences[14][20] = {
    {0x10},
    {0x20011, 0x20011, 0x20011, 0x20011, 0x40012, 0x90013, 0x110014},
    {},
    {},
    {},
    {0xe, 5, 0xb, 0xf},
    {0xe, 0xb, 1, 0xc, 1, 0xf},
    {0xe, 0xb, 0xb, 5, 0xf},
    {},
    {0xe, 0xb, 4, 0xf},
    {0xe, 5, 1, 0xb, 6, 0xf},
    {0xe, 0xb, 9, 0xf},
    {0x400000, 0x4268},
    {0x20011, 0x20011, 0x20011, 0x20011, 0x40012, 0x90013, 0x400000, 0x3e8, 0x110014},
};

namespace {
// A sequence word is the animation number in its low 16 bits and in bits 24 to 31; the bits between are effects.
constexpr uint32_t kAnimBits = 0xff80ffff;
constexpr uint32_t kEffectBits = 0x7f0000;
constexpr uint32_t kNoAccessories = 0x10000;   // mFlags: no hat, no hands, no mouth
constexpr uint32_t kKeyMark = 0x400000;

// A node of the advisor's mesh by its place in the header's full nodes (no mini nodes: the mouth, head and
// accessory numbers ADV_Init and DressAdvisor keep are all full nodes).
tag_sNode* AdvisorNode(tag_sRideAnimSet* set, int32_t index) { return &set->mHeads->mHeader->mNodes[index]; }

// The advisor clock: ReadFixed less the paused time. ADV_Tick keeps the game clock's pause in mClockPaused,
// mPauseStart and mPausedTotal, and while paused the time stands at the moment the pause began.
int32_t AdvisorClock() {
    SAdvisorState& state = gAdvisorState;
    const int32_t now = gClock.ReadFixed();
    if (gClock.GetPauseFlag() != 0) {
        if (state.mClockPaused == 0) {
            state.mClockPaused = 1;
            state.mPauseStart = now;
        }
        return state.mPauseStart - state.mPausedTotal;
    }
    if (state.mClockPaused != 0) {
        state.mClockPaused = 0;
        state.mPausedTotal = state.mPausedTotal + (now - state.mPauseStart);
    }
    return now - state.mPausedTotal;
}

// The lip-sync data is either in the static buffer (its use flag is then cleared) or a malloc block.
void FreeSyncData() {
    SAdvisorState& state = gAdvisorState;
    if (state.mSyncData == gAdvisorStaticBuffer) {
        gAdvisorStaticInUse = 0;
    } else {
        std::free(state.mSyncData);
    }
}

// A time word less the advisor clock, taken as a signed 32-bit number (the original's `subf.` then `bgt`).
bool Due(int32_t when, int32_t now) { return when - now <= 0; }

// A sum of two time words, wrapping as the original's add does.
int32_t Plus(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }

// The first row of the response table with that id, or none: the search stops at the terminator row (id 9999), which
// is checked before a row's id is compared, so the id 9999 itself is never found.
const AdvisorLine* FindLine(int32_t id) {
    for (const AdvisorLine* line = gAdvisorLines; line->mId != kAdvisorLastLine; ++line) {
        if (line->mId == id) return line;
    }
    return nullptr;
}
}  // namespace

void ADV_Pause() {
    SAdvisorState& state = gAdvisorState;
    if (state.mSpeaking != 0 && gPlayerSettings.mCurrent.mAdvisorOn && state.mSoundHandle != 0) {
        SOUND_PauseSound(state.mSoundHandle);
        state.mPausedSound = 1;
    }
}

void ADV_UnPause() {
    SAdvisorState& state = gAdvisorState;
    if (state.mPausedSound != 0 && gPlayerSettings.mCurrent.mAdvisorOn && state.mSoundHandle != 0) {
        SOUND_UnpauseSound(state.mSoundHandle);
        state.mPausedSound = 0;
    }
}

// Quirk: the last word cleared is mAnimLengths[13] (0x9c), after the loop that filled it; the sequences of
// GenerateAnimSequence start with animation 14, which therefore takes no time.
void ADV_Init() {
    SAdvisorState& state = gAdvisorState;
    std::memset(&state, 0, sizeof state);
    state.mMouthCount = 5;
    state.mMouthShown = -1;
    for (uint32_t which = 0; static_cast<int32_t>(which) < 2; ++which) {
        tag_sRideAnimSet* set = GetAdvisorRideAnim(which);
        if (set == nullptr) continue;
        for (int32_t shape = 0; shape < state.mMouthCount; ++shape)
            state.mMouthNodes[which][shape] = FindDataNodeIndexByIDAdvisorTempHack(set, 0, shape + 1);
        state.mHeadNode[which] = FindDataNodeIndexByIDAdvisorTempHack(set, 0, 0xffffffff);
    }
    tag_sRideAnimSet* set = GetAdvisorRideAnim(1);
    if (set == nullptr) set = GetAdvisorRideAnim(0);
    if (set != nullptr) {
        for (uint32_t i = 0; static_cast<int32_t>(i) < 20; ++i) state.mAnimLengths[i] = GetAnimLength(set, 5, i);
        state.mAnimLengths[13] = 0;
        EM_Init();
    }
}

void ADV_Close() {
    SAdvisorState& state = gAdvisorState;
    state.mSpeaking = 0;
    if (state.mSyncData != nullptr) {
        FreeSyncData();
        state.mSyncData = nullptr;
    }
}

// A random sequence for a line of `length` milliseconds: animation 14, then (until the time is used up or 19 are
// chosen) a random animation, but the first whose length covers what is left when the random one is longer than that,
// then animation 15. The count the trace prints is the number of words including the 15.
int32_t GenerateAnimSequence(int32_t length) {
    SAdvisorState& state = gAdvisorState;
    int32_t total = 0;
    int32_t count = 0;
    while (length > 0 && count <= 0x12) {
        const int32_t pick = msl_rand() % 10;
        if (count == 0) {
            state.mSequence[0] = 0xe;
        } else if (length < state.mAnimLengths[pick]) {
            int32_t first = 10;
            for (int32_t i = 0; i < 10; ++i) {
                if (length <= state.mAnimLengths[i]) {
                    first = i + 1;
                    break;
                }
            }
            state.mSequence[count] = first;
        } else {
            state.mSequence[count] = pick + 1;
        }
        const int32_t taken = state.mAnimLengths[state.mSequence[count] - 1];
        ++count;
        total += taken;
        length -= taken;
    }
    state.mSequence[count] = 0xf;
    // The terminator's zero lands on mSequencePos when the sequence has 20 words; it is cleared next anyway.
    if (count + 1 < 20) state.mSequence[count + 1] = 0;
    total += state.mAnimLengths[state.mSequence[count] - 1];
    state.mSequencePos = 0;
    CTRACE(8, "Advisor anim sequence of %d", count + 1);
    return total;
}

// Quirk: a key mark takes the place of the next word in the sequence (the sequence index does not move), and the
// first real word, unless it is animation 14, cancels a waiting sound (mSoundTime).
int32_t SetCannedSequence(int32_t sequence, int32_t length) {
    SAdvisorState& state = gAdvisorState;
    const uint32_t* row = gCannedSequences[sequence];
    int32_t total = 0;
    int32_t count = 0;
    int32_t at = 0;
    state.mKeyTime = 0;
    for (;;) {
        const uint32_t word = row[at];
        state.mSequence[count] = static_cast<int32_t>(word);
        if (word == 0) break;
        if ((word & kKeyMark) == kKeyMark) {
            state.mKeyTime = total + static_cast<int32_t>(row[at + 1]);
            at += 2;
        } else {
            if (count == 0 && (word & kAnimBits) != 0xe) state.mSoundTime = 0;
            ++count;
            ++at;
            total += state.mAnimLengths[(static_cast<uint32_t>(state.mSequence[count - 1]) & kAnimBits) - 1];
        }
    }
    if (length != 0 && total == 0) return GenerateAnimSequence(length);
    state.mSequencePos = 0;
    return total;
}

// The numbers -1 to 14. -1 puts on the accessories the line brought (mAccessoriesOn, hiding mAccessoriesOff), 0 puts
// on both hands (19 and 20), most of the others take the hands off and 14 takes off the left one only (21); 3, 8 and
// 12 leave them as they are. Then the hat itself.
void SetAdvisorHatHand(int32_t accessory) {
    SAdvisorState& state = gAdvisorState;
    switch (accessory) {
    case -1:
        for (int32_t i = 0; i < 4; ++i) {
            if (state.mAccessoriesOn[i] != 0 && (state.mFlags & kNoAccessories) == 0)
                EnableAdvisorAccessory(state.mAccessoriesOn[i], state.mWhich);
            if (state.mAccessoriesOff[i] != 0) DisableAdvisorAccessory(state.mAccessoriesOff[i], state.mWhich);
        }
        break;
    case 0:
        EnableAdvisorAccessory(0x13, state.mWhich);
        EnableAdvisorAccessory(0x14, state.mWhich);
        break;
    case 1: case 2: case 4: case 5: case 6: case 7: case 9: case 10: case 11: case 13:
        DisableAdvisorAccessory(0x13, state.mWhich);
        DisableAdvisorAccessory(0x14, state.mWhich);
        break;
    case 14:
        DisableAdvisorAccessory(0x15, state.mWhich);
        break;
    default:
        break;
    }
    if (accessory != 0 && (state.mFlags & kNoAccessories) == 0) EnableAdvisorAccessory(accessory, state.mWhich);
}

namespace {
// The sparkle effects use the animation's time (in whole milliseconds, as a float) as their depth.
float AnimTime() { return static_cast<float>(gAdvisorState.mAnimLengths[gAdvisorState.mAnimIndex]); }
}  // namespace

// Hides every mouth shape, puts on the line's hat and hands, hides the head ("bug head") for the no-accessory animations
// and starts the effect the animation's bits name (only while he speaks, except that effect 0x200000 turns the
// emitters off in both cases). The bits are tested as a whole: a word with two of them does nothing.
void DressAdvisor() {
    SAdvisorState& state = gAdvisorState;
    tag_sRideAnimSet* set = GetAdvisorRideAnim(state.mWhich);
    if (set == nullptr) return;
    for (int32_t shape = 0; shape < state.mMouthCount; ++shape)
        AdvisorNode(set, state.mMouthNodes[state.mWhich][shape])->mFlags |= 0x10;
    state.mMouthShown = -1;
    SetAdvisorHatHand(state.mHat);
    if (state.mHand != 0) SetAdvisorHatHand(state.mHand);
    if ((state.mFlags & kNoAccessories) != 0) AdvisorNode(set, state.mHeadNode[state.mWhich])->mFlags |= 0x10;
    const uint32_t effect = state.mFlags & 0x3e0000;
    if (effect == 0) return;
    if (state.mSpeaking == 0) {
        if ((state.mFlags & 0x200000) != 0) EM_SetEmitersOn(0, 0, 0);
        return;
    }
    switch (effect) {
    case 0x20000:
        if (EM_SetEmitersOn(0, 1, 0x2c) != 0) {
            EM_SetEffectTranslation(0, 1, 0.34f, 0.62f, AnimTime());
            EM_SetEffectOrbit(0, 1, 0.01f, 0.2f, 1.0f);
        }
        break;
    case 0x40000:
        SOUND_SendEvent(0, gSoundBanks[3], 0xc5, 0, 0, 0);
        EM_SetEmitersOn(0, 2, 0x31);
        EM_SetEffectTranslation(0, 0, 0.0f, 0.0f, AnimTime());
        EM_SetEffectOrbit(0, 0, 0.6f, 0.4f, 2.0f);
        break;
    case 0x80000:
        EM_SetEmitersOn(0, 3, 0x31);
        EM_SetEffectTranslation(0, 0, 0.0f, 0.0f, AnimTime());
        EM_SetEffectOrbit(0, 0, 0.6f, 0.4f, 3.0f);
        break;
    case 0x100000:
        EM_SetEmitersOn(0, 0, 0x2c);
        EM_SetEmitersOn(0, 2, 0x2c);
        EM_SetEffectTranslation(0, 0, 0.8f, -0.8f, AnimTime());
        EM_SetEffectOrbit(0, 0, 0.0f, 0.0f, 2.0f);
        break;
    case 0x200000:
        EM_SetEmitersOn(0, 0, 0);
        break;
    default:
        break;
    }
}

// The lip-sync file of a line is ":Speech:lips:sp_<n>.lip" in the global data or in the theme's folder (the sound set).
// Quirks: the Mac's _USwapBlock32 swaps `count` WORDS and the original passes it the file's size in BYTES, so it swaps
// four times the data it read (past the end of the 1,000-byte static buffer when the file is longer than 250 bytes;
// the host's function does nothing on a little-endian host, where it does no harm). FindFilePath is called twice for
// the name. A file that is not found or not opened leaves the previous line's lip-sync data in place. The sound is sent
// and stopped again at once when mSoundTime is set (it nearly always is): ADV_Tick sends it a second later. A sound
// that answers a length of -200 ms ends the call with the state half filled and mSpeaking still 0.
int32_t ADV_Call(AdvisorResponse response) {
    SAdvisorState& state = gAdvisorState;
    if (!gPlayerSettings.mCurrent.mAdvisorOn || state.mSpeaking != 0) return 0;
    CTRACE(8, "Advisor says ResponseID %d", response.mId);
    const AdvisorLine* line = FindLine(response.mId);
    if (line == nullptr) return 0;
    state.mSound = line->mSound;
    state.mWhich = line->mWhichAndSoundSet & 0xffff;
    state.mSoundSet = static_cast<int32_t>(line->mWhichAndSoundSet) >> 16;
    state.mHat = line->mHat;
    state.mHand = line->mHand;
    state.mBaseTime = Plus(AdvisorClock(), -200);
    char name[100];
    char path[260];
    std::sprintf(name, ":Speech:lips:sp_%03d.lip", line->mLipFile);
    if (state.mSoundSet == 0) {
        std::strcpy(path, "Data:Global");
    } else {
        std::strcpy(path, get_theme_path());
    }
    std::strcat(path, name);
    state.mMouthOpen = 0;
    state.mSyncActive = 0;
    state.mSoundTime = Plus(state.mBaseTime, 1000);
    if (FindFilePath(path, kFileReturnName, nullptr) != nullptr) {
        gAdvisorFile = TPFile_Open(FindFilePath(path, kFileReturnName, nullptr), "rb");
        if (gAdvisorFile != nullptr) {
            state.mSyncSize = LbFile_GetSize(gAdvisorFile);
            if (state.mSyncData != nullptr) FreeSyncData();
            if (state.mSyncSize < 1000 && gAdvisorStaticInUse == 0) {
                gAdvisorStaticInUse = 1;
                state.mSyncData = gAdvisorStaticBuffer;
            } else {
                state.mSyncData = static_cast<int32_t*>(std::malloc(static_cast<uint32_t>(state.mSyncSize)));
            }
            LbFile_Read(gAdvisorFile, state.mSyncData, static_cast<uint32_t>(state.mSyncSize), nullptr);
            SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(state.mSyncData), state.mSyncSize);
            LbFile_Close(gAdvisorFile);
            state.mSyncPos = 0;
            state.mSyncActive = 1;
            state.mMouthOpen = 1;
        }
    }
    SND_DuckVolume(1);
    const int32_t bank = state.mSoundSet == 0 ? gSoundBanks[5] : gAdvisorThemeSoundBank;
    state.mSoundHandle = SOUND_SendEvent(0, bank, state.mSound, 0, 0, 0);
    const int32_t length = Plus(SOUND_GetCurrentSampleLengthInMilliseconds(state.mSoundHandle), 200);
    if (length == 0) return 0;
    CTRACE(8, "Advisor sample is %d ms long ", length);
    CTRACE(8, "Advisor sample ID is %d", state.mSound);
    // The sequence is asked for 300 ms more than the sound.
    int32_t sequenceTime;
    if (line->mSequence == -1) {
        sequenceTime = GenerateAnimSequence(Plus(length, 300));
    } else {
        sequenceTime = SetCannedSequence(Plus(line->mSequence, -0x10), Plus(length, 300));
    }
    EM_SetEmitersOn(0, 0, 0);
    const uint32_t word = static_cast<uint32_t>(state.mSequence[state.mSequencePos]);
    StartAdvisorAnimation(word & kAnimBits, state.mWhich);
    state.mAnimIndex = static_cast<int32_t>((word & kAnimBits) - 1);
    state.mFlags = word & kEffectBits;
    ++state.mSequencePos;
    state.mDressNow = 1;
    if (state.mSoundTime != 0) {
        SOUND_StopSoundEffect(state.mSoundHandle);
        state.mSoundHandle = 0;
        state.mMouthOpen = 0;
        state.mBaseTime = Plus(state.mBaseTime, 1000);
    }
    // The first time of the lip-sync data (milliseconds; -1 ends it) is where the mouth first flips.
    int32_t mouth = -1;
    if (state.mSyncData != nullptr) {
        const int32_t value = state.mSyncData[state.mSyncPos++];
        if (value != -1) mouth = value / 1000;
    }
    state.mMouthTime = Plus(state.mBaseTime, mouth);
    state.mSpeaking = 1;
    state.mEndTime = Plus(state.mBaseTime, sequenceTime);
    if (state.mKeyTime != 0) state.mKeyTime = Plus(state.mKeyTime, state.mBaseTime);
    return Plus(Plus(sequenceTime, state.mAnimLengths[14]), 1000);
}

// Quirk: the slap sound goes to the entity mSoundHandle, which ADV_Kill has just zeroed, so it is always 0.
void ADV_Slap() {
    SAdvisorState& state = gAdvisorState;
    if (state.mSpeaking == 0) return;
    if (state.mSoundHandle == 0) {
        ADV_Kill(0);
        return;
    }
    ADV_Kill(0);
    const int32_t pick = msl_rand() % 3;
    if (pick == 0) SOUND_SendEvent(state.mSoundHandle, gSoundBanks[5], 0x27f, 0, 0, 0);
    if (pick == 1) SOUND_SendEvent(state.mSoundHandle, gSoundBanks[5], 0x280, 0, 0, 0);
    if (pick == 2) SOUND_SendEvent(state.mSoundHandle, gSoundBanks[5], 0x281, 0, 0, 0);
}

void ADV_Kill(int32_t mode) {
    SAdvisorState& state = gAdvisorState;
    if (state.mSpeaking == 0) return;
    SOUND_SetVolumeOnSound(state.mSoundHandle, 0);
    if (state.mPausedSound != 0 && gPlayerSettings.mCurrent.mAdvisorOn && state.mSoundHandle != 0) {
        SOUND_UnpauseSound(state.mSoundHandle);
        state.mPausedSound = 0;
    }
    state.mSpeaking = 0;
    SND_DuckVolume(0);
    EM_SetEmitersOn(0, 0, 0);
    DisableAdvisorDisplay(state.mWhich);
    if (state.mSyncData != nullptr) {
        FreeSyncData();
        state.mSyncData = nullptr;
        state.mSyncSize = 0;
        state.mSyncActive = 0;
    }
    if (state.mSoundHandle != 0) {
        if (mode != 0) {
            SOUND_FadeSoundEffect(state.mSoundHandle);
        } else {
            SOUND_StopSoundEffect(state.mSoundHandle);
        }
        state.mSoundHandle = 0;
    }
}

int32_t ADV_GetTag(AdvisorResponse response) {
    const AdvisorLine* line = FindLine(response.mId);
    return line != nullptr ? line->mTag : 0x17f;
}

int32_t ADV_Stat() {
    SAdvisorState& state = gAdvisorState;
    if (state.mSpeaking == 0) return 0;
    return static_cast<int32_t>(static_cast<uint32_t>(state.mEndTime) - static_cast<uint32_t>(AdvisorClock()));
}
void ADV_Tick2() {
    SAdvisorState& state = gAdvisorState;
    tag_sRideAnimSet* set = GetAdvisorRideAnim(state.mWhich);
    if (state.mDressAfterAnim != 0) {
        uint32_t kind;
        uint32_t index;
        GetDeferredAnim(set, &kind, &index, 0);
        if (kind == 0xc) {
            DressAdvisor();
            state.mDressAfterAnim = 0;
            state.mDressNow = 0;
        }
    }
    if (state.mDressNow != 0) {
        DressAdvisor();
        state.mDressNow = 0;
    }
    EM_Tick();
}

void ADV_Tick() {
    SAdvisorState& state = gAdvisorState;
    const int32_t now = AdvisorClock();
    if (state.mSpeaking == 0) {
        // The line is over: let go of its lip-sync data.
        if (state.mSyncData != nullptr) {
            FreeSyncData();
            state.mSyncData = nullptr;
            state.mSyncSize = 0;
            state.mSyncActive = 0;
        }
        return;
    }
    // The sound: followed while it plays; the line ends when it has stopped and the line's end time has come.
    if (state.mSoundHandle != 0) state.mSoundHandle = SOUND_MoveSoundEffect(state.mSoundHandle, 0, 0, 0);
    if (state.mSoundHandle == 0 && Due(state.mEndTime, now)) {
        state.mSpeaking = 0;
        SND_DuckVolume(0);
        EM_SetEmitersOn(0, 0, 0);
    }
    // The sound starts a second after the line began (ADV_Call sets the time).
    if (state.mSoundTime != 0 && Due(state.mSoundTime, now)) {
        const int32_t bank = state.mSoundSet == 0 ? gSoundBanks[5] : gAdvisorThemeSoundBank;
        state.mSoundHandle = SOUND_SendEvent(0, bank, state.mSound, 0, 0, 0);
        state.mSoundTime = 0;
        state.mMouthOpen = 1;
    }
    // The key (the lobby's island key display) at its time in the canned line.
    if (state.mKeyTime != 0 && Due(state.mKeyTime, now)) {
        PAR_InitEmitter(0x57, 90000, 0, 7000);
        SOUND_SendEvent(0, gSoundBanks[3], 0xc6, 0, 0, 0);
        UpdateIslandLobbyKeyDisplay();
        state.mKeyTime = 0;
    }
    // The next animation of the sequence, once the playing one has ended (nothing queued behind it).
    if (state.mDressNow == 0 && (state.mSequence[state.mSequencePos] & kAnimBits) != 0) {
        tag_sRideAnimSet* set = GetAdvisorRideAnim(state.mWhich);
        uint32_t kind;
        uint32_t index;
        GetDeferredAnim(set, &kind, &index, 0);
        if (kind == 0xc) {
            const uint32_t word = static_cast<uint32_t>(state.mSequence[state.mSequencePos]);
            SetCurRideAnim(set, 5, (word & kAnimBits) - 1, 0, 1.0f, 0);
            state.mFlags = word & kEffectBits;
            state.mAnimIndex = static_cast<int32_t>((word & kAnimBits) - 1);
            ++state.mSequencePos;
            state.mDressAfterAnim = 1;
        }
    }
    // The mouth opens and shuts at the times of the lip-sync data (each a millisecond count; -1 ends it).
    if (state.mSyncActive != 0 && now > state.mMouthTime) {
        state.mMouthOpen = state.mMouthOpen == 0 ? 1 : 0;
        int32_t next = -1;
        if (state.mSyncData != nullptr) {
            const int32_t value = state.mSyncData[state.mSyncPos++];
            if (value != -1) next = value / 1000;
        }
        state.mMouthTime = next;
        if (next == -1) {
            state.mSyncActive = 0;
            state.mMouthOpen = 0;
        } else {
            state.mMouthTime = next + state.mBaseTime;
        }
    }
    // While it is open, a random mouth shape every 100 ms; shut, shape 1.
    if (state.mMouthOpen == 0 || state.mSyncActive == 0) {
        state.mMouthWanted = 1;
    } else if (now > state.mMouthChange) {
        state.mMouthWanted = msl_rand() % state.mMouthCount + 1;
        state.mMouthChange = now + 100;
    }
    if (state.mMouthShown != state.mMouthWanted) {
        tag_sRideAnimSet* set = GetAdvisorRideAnim(state.mWhich);
        if (state.mMouthShown > 0)
            AdvisorNode(set, state.mMouthNodes[state.mWhich][state.mMouthShown - 1])->mFlags |= 0x10;
        if (state.mMouthWanted != 0 && (state.mFlags & kNoAccessories) == 0)
            AdvisorNode(set, state.mMouthNodes[state.mWhich][state.mMouthWanted - 1])->mFlags &= ~0x10u;
        state.mMouthShown = state.mMouthWanted;
    }
}

// Advisor.cpp (main app): the advisor's part of the save file. ADV_Save 0x10009788 (808 bytes) and ADV_Load 0x10009cb8
// (708 bytes). The file image of the state is 0x168 bytes with the Mac's layout (a 32-bit pointer at +0x140); the host's
// SAdvisorState is larger (a native pointer), so the image is packed from and unpacked into the fields. The file's words are
// little-endian: the Mac swaps every field one by one (USwapInplace32 and friends); on this little-endian host those swaps are
// no-ops, so only the calls that go through the platform (_USwapBlock32, _USwapCopy32, _USwapDelete) are kept.

namespace {

constexpr uint32_t kAdvisorFileTag = 0x5f564441;   // "ADV_" (little-endian bytes 41 44 56 5f)
constexpr uint32_t kStateImageSize = 0x168;
constexpr uint32_t kPointerOffset = 0x140;         // the data pointer in the image
constexpr uint32_t kTailSize = 0x24;               // mAccessoriesOn, mAccessoriesOff and mPausedSound

void PackState(uint8_t* image) {
    SAdvisorState& state = gAdvisorState;
    std::memcpy(image, &state, kPointerOffset);
    const uint32_t pointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(state.mSyncData));
    std::memcpy(image + kPointerOffset, &pointer, 4);
    std::memcpy(image + kPointerOffset + 4, &state.mAccessoriesOn, kTailSize);
}

void UnpackState(const uint8_t* image) {
    SAdvisorState& state = gAdvisorState;
    std::memcpy(&state, image, kPointerOffset);
    uint32_t pointer;
    std::memcpy(&pointer, image + kPointerOffset, 4);
    state.mSyncData = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(pointer));
    std::memcpy(&state.mAccessoriesOn, image + kPointerOffset + 4, kTailSize);
}

// A word written through _USwapCopy32 (the original's four-byte writes).
void WriteWord(void* file, uint32_t value) {
    uint32_t transferred;
    uint32_t* copy = SamsUtilities::_USwapCopy32(&value, 1);
    LbFile_Write(file, copy, 4, &transferred);
    SamsUtilities::_USwapDelete(copy);
}

}  // namespace

// 0x10009788
void ADV_Save(void* file) {
    SAdvisorState& state = gAdvisorState;
    WriteWord(file, kAdvisorFileTag);
    WriteWord(file, kStateImageSize);
    uint32_t image[kStateImageSize / 4];
    PackState(reinterpret_cast<uint8_t*>(image));
    LbFile_Write(file, image, kStateImageSize, nullptr);
    if (state.mSyncData == nullptr) {
        WriteWord(file, 0);
    } else {
        uint32_t size;
        std::memcpy(&size, &state.mSyncSize, 4);
        WriteWord(file, size);
        uint32_t* copy = SamsUtilities::_USwapCopy32(reinterpret_cast<const uint32_t*>(state.mSyncData), static_cast<int32_t>(size >> 2));
        LbFile_Write(file, copy, size, nullptr);
        SamsUtilities::_USwapDelete(copy);
    }
}

// 0x10009cb8
void ADV_Load(void* file) {
    SAdvisorState& state = gAdvisorState;
    uint32_t transferred;
    uint32_t tag = 0;
    LbFile_Read(file, &tag, 4, &transferred);
    SamsUtilities::_USwapBlock32(&tag, 1);
    if (tag != kAdvisorFileTag) return;

    if (state.mSyncData != nullptr) {
        if (state.mSyncData == gAdvisorStaticBuffer)
            gAdvisorStaticInUse = 0;
        else
            std::free(state.mSyncData);
        state.mSyncData = nullptr;
    }
    uint32_t length = 0;
    LbFile_Read(file, &length, 4, &transferred);
    SamsUtilities::_USwapBlock32(&length, 1);
    // The Mac reads `length` bytes straight over the state (0x168 in every file the game writes) and swaps its fields.
    std::vector<uint8_t> image(length > kStateImageSize ? length : kStateImageSize, 0);
    PackState(image.data());
    LbFile_Read(file, image.data(), length, nullptr);
    UnpackState(image.data());

    LbFile_Read(file, &state.mSyncSize, 4, &transferred);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&state.mSyncSize), 1);
    if (state.mSyncSize == 0) return;
    if (state.mSyncSize < 1000 && gAdvisorStaticInUse == 0) {
        gAdvisorStaticInUse = 1;
        state.mSyncData = gAdvisorStaticBuffer;
    } else {
        state.mSyncData = static_cast<int32_t*>(std::malloc(static_cast<uint32_t>(state.mSyncSize)));
    }
    LbFile_Read(file, state.mSyncData, static_cast<uint32_t>(state.mSyncSize), nullptr);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(state.mSyncData), static_cast<int32_t>(static_cast<uint32_t>(state.mSyncSize) >> 2));
}

// The constructors of the things a new level makes (unit w122a, main app): CThing::CThing (Thing.cpp) and the constructors of CStaffHQ,
// CMechanicHQ, CAdvisor, CTagSystem, CResearchLab, CWeather and CUIMsgReceiver. The six leaf constructors they build their parts with
// (CTrainingCentre, CWeatherForecast, CStaffHQ::StrikeType, AdvisorHistory (host: AdvisorMessageState, written in CAdvisor's loop),
// CResearchProject, CResearchCategory) are inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every derived constructor builds its CThing first (the host holds it as the first member, which is constructed before the others) with
// map cell id 1 and the model of its class, then its own fields, then subscribes to the message centre. The Mac's constructors leave most
// bytes of the object alone (a new object keeps whatever the allocator gave); so do these, and no member has an initialiser.

// 0x10009f9c (172 bytes, Advisor.cpp), model 0xb. The eight requests of the queue (0x1000a0b4, the valid byte) and mLastTutorialRequest
// (the inlined store of +0x16e4) are built by CAdvisorRequest's constructor. mPlayStartTime (+0xd8) is written by Init only.
CAdvisor::CAdvisor() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xb)) {
    std::memset(mF0C, 0, sizeof mF0C);
    std::memset(mFD4, 0, sizeof mFD4);
    mPlayDuration = 0;
    // AdvisorHistory::AdvisorHistory (0x1000a06c) for each of the 0x15f records: +0, +4 (-1), the byte +8 and +0xc; +9 to +0xb stay.
    for (AdvisorMessageState& state : mMessageState) {
        state.mLastSaid.mTimeSig = 0;
        state.mResponseIndex = -1;
        state.mSaidOnce = 0;
        state.mKillCount = 0;
    }
    Init();
}

// gameplay_leaves.cpp (main, unit v5L07): small leaves a park run reaches, from different Mac files: CellIsRide 0x1006d718
// (the inventory's Debug.cpp range), LocatorByeByeKid 0x101bf98c (ui_staffPanel.cpp), CObject::IsCoaster 0x10110cd8 (Object.cpp) and CAdvisor::FinalDestroy 0x1000a0ec (Advisor.cpp).

// 0x1000a0ec (36 bytes): the advisor has nothing of its own to free; the thing's part is.
void CAdvisor::FinalDestroy() { reinterpret_cast<CThing*>(this)->FinalDestroy(); }

// The seven thing-loop entry points (unit w123a, main app). External dispatch,
// death-message support, random/season providers and tree insertion remain callees.

// 0x1000a13c, 664 bytes. Each subscription repeats the lazy-centre test.
int32_t CAdvisor::Init() {
    for (int32_t type : {5, 17, 19, 23, 14, 8, 25}) SubscribeThing(mBase, type);
    const uint32_t zero = 0;
    std::memcpy(mF0C + 4, &zero, 4);
    std::memcpy(mFD4, &zero, 4);
    mPlayDuration = 0;
    mPlayStartTime = 0;
    return 1;
}

// The message centre's post and the message and subscription objects it uses (main app, unit w107): CMessageCentre::PostMessage
// 0x1015256c, the CMsgEvent constructor 0x10151df8 and destructor 0x100f39cc (and GetType 0x100f4470, which its vtable needs), the
// CSubscriptionHeader constructor 0x1000a3f8 and destructor 0x101522e0. The inventory files them under RideDB.cpp and BlockSave.cpp.

// An empty tree. The Mac constructs the tree at +4 of the header and touches nothing else.
CSubscriptionHeader::CSubscriptionHeader() { mTree.Construct(); }

// CAdvisor::UpdateTurn, main app 0x1000a8c8 (1212 bytes), unit w124: queues the best message the scorers like, and says the best queued one when
// it scores above the minimum and he is not already speaking. Reads *gWorld for the random response. The scoring object is a function-local static
// on the Mac (the guard byte at 0x102b4100 is tested before each use), so every use below calls TheScoring() again.

void CAdvisor::UpdateTurn() {
    // 1: queue the best message of the next record when it scores above the minimum.
    AdvisorMessage best = TheScoring().GetHighestScoringFromNext(1);
    int32_t score = TheScoring().GetScoreFor(best);
    int32_t threshold = TheScoring().mBalance.mValues[kAdvisorBalanceMinScore].mInteger;
    if (score > threshold) {
        QueueRequest(CAdvisorRequest(best, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1));
    }
    // 2: nothing while he speaks; one turn of silence after the byte at +0x16e8 is 1.
    if (IsPlaying()) return;
    if (mF16E8[0] == 1) {
        mF16E8[0] = 0;
        return;
    }
    // 3: the best queued request.
    int32_t slot = FindHighestScoringSlotInQueue();
    if (slot >= 0) {
        int32_t queuedScore = mQueue[slot].mValid != 0 ? mQueue[slot].mScore : -1;
        int32_t minimum = TheScoring().mBalance.mValues[kAdvisorBalanceMinScore].mInteger;
        if (queuedScore > minimum) {
            CAdvisorRequest* request = &mQueue[slot];
            int32_t index = request->mResponseNo;
            if (index < 0) {
                // The response is the scorer's choice. The original leaves an unassigned index in a caller-saved register (r25); 0 is used here.
                int32_t count = TheScoring().GetNumResponsesFor(request->mMessage);
                int32_t cycling = TheScoring().GetCyclingFor(request->mMessage);
                int32_t current = mMessageState[request->mMessage].mResponseIndex;
                index = 0;
                switch (cycling) {
                case 3:
                    index = static_cast<int32_t>(gWorld->GetRand() % static_cast<uint32_t>(count));
                    break;
                case 1:
                    index = current < count - 1 ? current + 1 : current;
                    break;
                case 2:
                    index = current + 1;
                    break;
                case 0:
                    if (current != count - 1) index = current + 1;
                    break;
                default:
                    break;
                }
                if (index >= 0) {
                    int32_t count2 = TheScoring().GetNumResponsesFor(request->mMessage);
                    if (!(index < count2)) index = 0;
                } else {
                    index = 0;
                }
            }
            // 5: say it.
            uint32_t length;
            int32_t played;
            if (request->mScoreOverridden == 0) {
                played = request->Play(&length, index);
            } else {
                played = request->PlayWithoutScore(&length, index);
            }
            if (played == 0) return;
            SetPlayingFor(length + 1000u);
            mMessageState[request->mMessage].mLastSaid.SetTimeSig();
            mMessageState[request->mMessage].mResponseIndex = index;
            mMessageState[request->mMessage].mSaidOnce = 1;
            mTypePlaying = request->mMessage;
            return;
        }
    }
    // 6: nothing to say; the clock is read and the answer dropped.
    TheScoring();
    Clock().SecondsSinceLastUpdate();
}

// AdvisorScoring.cpp (main app of SimTheme Park 2.04b3): the advisor's message scoring, 0x10010054..0x10010f38 and the destructor at
// 0x1000adb0 (the inventory files it under Advisor.cpp). The constructor loads the advisor's balance table; the ten queries look a
// message up in the static record table (src/AdvisorScoring_table.cpp) and answer one of its fields, or a number of the message's category in
// the balance table, or run the record's scorer. The scorers themselves (the _ADVISOR_* members) are not here.
//
// Every query repeats the original's inlined lookup of a message's record: the two numbers kAdvisorMessageCount and kAdvisorNoMessage
// have none; otherwise the record at the message's own index is taken if its number matches, else all 351 records are searched, and a
// hit there STILL answers the record at the original index (the code adds the index it started with, not the one it found, to the
// table). With the table in order the search cannot hit; a patched table (the tests) shows it. A miss traces "Not found: %d\n" at level 3.

// 0x1000adb0: puts the derived vptr back and releases the table (the deleting form, with a positive flag, frees the object: the caller).
CAdvisorScoring::~CAdvisorScoring() {
    mBalance.UnInit();
}

// Advisor.cpp (main app): the request queue, 0x1000ae48..0x1000b8c4.

CAdvisorScoring& TheScoring() {
    static CAdvisorScoring scoring;
    return scoring;
}

int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) mLastTutorialRequest = request;
    if (!CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce != 0)) return 0;
    int32_t slot = FindFirstInvalidSlotInQueue();
    if (slot >= 0) {
        mQueue[slot] = request;
        return 1;
    }
    slot = FindLowestScoringSlotInQueue();
    if (slot < 0) return 0;
    const int32_t newScore = request.mValid ? request.mScore : -1;
    const int32_t oldScore = mQueue[slot].mValid ? mQueue[slot].mScore : -1;
    if (oldScore < newScore) mQueue[slot] = request;
    return 1; // A full queue reports success even when it drops the request.
}

// Advisor.cpp (main app): the advisor request's constructors and SetScore (0x1000deb4, 0x1000de30, 0x1000e8cc; the default constructor
// 0x1000a0b4 and Valid 0x1000e894 are inline in AdvisorQueue.h) and CAdvisor::CancelMessage, PlayLastTutorialMessage and GetTypePlaying
// (0x1000b048, 0x1000b1c4, 0x1000c760).

namespace {
// The slot of the advisor balance table (src/AdvisorBalance_table.cpp, the walk of CBalanceTable::Init) the original reads as the word at
// CAdvisorScoring + 0x32c: 0x14 + 4 * 198, the row "ContextHelpScore" of the heading "HelpMessage".
constexpr int32_t kContextHelpScoreSlot = 198;
}  // namespace

// 0x1000b048: the tutorial request is dropped only when the message is a tutorial one (category 1) and the same message (the category
// is looked up only while the request is valid); every valid queued request of the message is invalidated; and if he is saying the
// message he is slapped (ADV_Slap), the message may be said again, its kill count goes up and the line ends at once.
// Quirk: the message state is indexed without a range check, so kAdvisorNoMessage (what GetTypePlaying answers while he is silent) would
// reach state[0x160], that is mF16E8[0] and mTypePlaying.
int32_t CAdvisor::CancelMessage(AdvisorMessage message) {
    int32_t answer = 0;
    if (mLastTutorialRequest.Valid()) {
        if (TheScoring().GetMessageCategory(message) == 1 && message == mLastTutorialRequest.mMessage) {
            mLastTutorialRequest.mValid = 0;
        }
    }
    for (int32_t i = 0; i < 8; ++i) {
        if (mQueue[i].Valid() && message == mQueue[i].mMessage) {
            answer = 1;
            mQueue[i].mValid = 0;
        }
    }
    if (message == GetTypePlaying()) {
        answer = 1;
        ADV_Slap();
        mMessageState[message].mSaidOnce = 0;
        mMessageState[message].mKillCount = mMessageState[message].mKillCount + 1;
        mPlayDuration = 0;
    }
    return answer;
}

// 0x1000b1c4: F1. 0 when there is no tutorial request. Else the request is scored with the balance table's ContextHelpScore, forced past
// the "say only once" and kill-count gates, and put in the queue as QueueRequest would (a free slot, else over the lowest scoring slot
// when that scores strictly lower; invalid scores count as -1). The answer is 1 whatever happened to the request. The category lookup's
// answer is dropped (the original compares it with 1 and does nothing with the result).
int32_t CAdvisor::PlayLastTutorialMessage() {
    if (!mLastTutorialRequest.Valid()) return 0;
    mLastTutorialRequest.SetScore(TheScoring().mBalance.mValues[kContextHelpScoreSlot].mInteger);
    mLastTutorialRequest.mOverrideOnlyOnce = 1;
    TheScoring().GetMessageCategory(mLastTutorialRequest.mMessage);
    if (CanIQueueThis(mLastTutorialRequest.mMessage, mLastTutorialRequest.mOverrideOnlyOnce != 0)) {
        int32_t slot = FindFirstInvalidSlotInQueue();
        if (slot >= 0) {
            mQueue[slot] = mLastTutorialRequest;
        } else {
            slot = FindLowestScoringSlotInQueue();
            if (slot >= 0) {
                const int32_t newScore = mLastTutorialRequest.mValid ? mLastTutorialRequest.mScore : -1;
                const int32_t oldScore = mQueue[slot].mValid ? mQueue[slot].mScore : -1;
                if (oldScore < newScore) mQueue[slot] = mLastTutorialRequest;
            }
        }
    }
    return 1;
}

bool CAdvisor::CanIQueueThis(AdvisorMessage message, bool force) const {
    if (!gPlayerSettings.mCurrent.mTutorialOn && TheScoring().IsThisATutorialMessage(message)) return false;
    const AdvisorMessageState& state = mMessageState[message]; // Original has no range check.
    if (state.mLastSaid.GetTimeSecond() != 0) {
        const uint32_t minTime = static_cast<uint32_t>(TheScoring().GetMinTimeFor(message));
        const uint32_t elapsed = static_cast<uint32_t>(state.mLastSaid.SecondsSinceLastUpdate());
        if (elapsed < minTime) return false;
    }
    if (!force && TheScoring().SayOnlyOnce(message) && state.mSaidOnce != 0) return false;
    if (!force && TheScoring().HasThisBeenKilledTooManyTimes(message, state.mKillCount)) return false;
    int32_t count = 0;
    for (int32_t i = 0; i < 8; ++i) {
        if (mQueue[i].Valid() && mQueue[i].mMessage == message) ++count;
    }
    const int32_t maximum = static_cast<int32_t>(TheScoring().GetMaxDuplicatesInQueueFor(message) & 0xff);
    if (count < maximum) {
        if (TheScoring().GetCyclingFor(message) == 0) {
            const int32_t responses = TheScoring().GetNumResponsesFor(message);
            if (state.mResponseIndex == static_cast<int32_t>(static_cast<uint32_t>(responses) - 1u)) return false;
        }
        return true;
    }
    return false;
}

int32_t CAdvisor::FindFirstInvalidSlotInQueue() {
    for (int32_t i = 0; i < 8; ++i) {
        if (!mQueue[i].Valid()) return i;
    }
    return -1;
}

int32_t CAdvisor::FindLowestScoringSlotInQueue() {
    int32_t slot = -1;
    int32_t lowest = 0;
    for (int32_t i = 0; i < 8; ++i) {
        if (!mQueue[i].Valid()) continue;
        const int32_t score = mQueue[i].mValid ? mQueue[i].mScore : -1;
        if (slot < 0 || score < lowest) {
            slot = i;
            lowest = score;
        }
    }
    return slot;
}

// The things' second batch, the small functions (unit w130): CParkAnalyser::BuiltSomething 0x100e46a4 (120 bytes) and NumberOfStaffType 0x100e8144
// (404 bytes), CBankAccount::MakeWithdrawal 0x100f33b0 (172 bytes), CAdvisor::IsPlaying 0x1000c6e0 (84 bytes) and FindHighestScoringSlotInQueue
// 0x1000b900 (216 bytes), CWeather::SetPrecipitationType 0x10139388 (256 bytes).

namespace {
constexpr int32_t kAdvisorQueueSize = 8;
}  // namespace

// The valid request with the highest score (a signed compare; the first valid one seeds it, later ones must beat it); -1 for none.
int32_t CAdvisor::FindHighestScoringSlotInQueue() {
    int32_t best = -1;
    bool seeded = false;
    int32_t bestScore = 0;   // the Mac's register is uninitialised until the first valid request seeds it
    for (int32_t i = 0; i < kAdvisorQueueSize; ++i) {
        if (!mQueue[i].Valid()) continue;
        const int32_t score = mQueue[i].mValid != 0 ? mQueue[i].mScore : -1;
        if (!seeded) {
            best = i;
            bestScore = score;
            seeded = true;
        }
        const int32_t again = mQueue[i].mValid != 0 ? mQueue[i].mScore : -1;
        if (again > bestScore) {
            best = i;
            bestScore = again;
        }
    }
    return best;
}

// CAdvisor::HandleEvent 0x1000ba18 (2,496 bytes): an event message (jump table at 0x1027dde4, events 0 to 10; anything above does
// nothing) queues a canned advisor message, or on the reset event clears the history of every message. The original repeats the same
// queueing code inline for each message; QueueEventMessage is that code once.

namespace {

// Builds the request, remembers it when it is a tutorial message, and puts it in the first free slot or over the lowest scoring one
// when that scores less.
void QueueEventMessage(CAdvisor* self, AdvisorMessage message) {
    CAdvisorRequest request(message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1);
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) self->mLastTutorialRequest = request;
    if (!self->CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce)) return;
    int32_t slot = 0;
    while (slot < 8) {
        if (!self->mQueue[slot].Valid()) break;
        slot++;
    }
    if (slot >= 8) slot = -1;
    if (slot >= 0) {
        self->mQueue[slot] = request;
        return;
    }
    slot = self->FindLowestScoringSlotInQueue();
    if (slot < 0) return;
    int32_t newScore = request.mValid != 0 ? request.mScore : -1;
    int32_t oldScore = self->mQueue[slot].mValid != 0 ? self->mQueue[slot].mScore : -1;
    if (oldScore < newScore) self->mQueue[slot] = request;
}

}  // namespace

void CAdvisor::HandleEvent(EventType event) {
    switch (static_cast<uint32_t>(event)) {
    case 0:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0));
        if (TheGameType().mType == 2) QueueEventMessage(this, static_cast<AdvisorMessage>(0x143));
        break;
    case 2:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0x6a));
        break;
    case 3:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0x80));
        break;
    case 4:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0x81));
        break;
    case 10:
        SamsUtilities::UDebug("Clearing advisor history afer receiving RESET_FOR_EASY_MODE event\n");
        for (int32_t i = 0; i < kAdvisorMessageCount; i++) {
            mMessageState[i].mKillCount = 0;
            mMessageState[i].mResponseIndex = -1;
            mMessageState[i].mSaidOnce = 0;
        }
        break;
    default:
        break;
    }
}

// Advisor.cpp (main app): CAdvisor::HandleStaffInfo 0x1000c40c and CAdvisor::Serialise 0x1000a468. Offsets are the Mac's.

// 0x1000c40c, 660 bytes. For a staff type outside 0..4 the original traces and carries on with whatever r26 held (the
// message below stays 0 here).
void CAdvisor::HandleStaffInfo(const CMsgStaffInfo& info) {
    const int32_t staff = static_cast<int32_t>(info.mStaff);
    const int32_t situation = static_cast<int32_t>(info.mSituation);
    int32_t message = 0;
    switch (staff) {
        case 0: message = situation * 5 + 0x22; break;
        case 1: message = situation * 5 + 0x21; break;
        case 2: message = situation * 5 + 0x24; break;
        case 3: message = situation * 5 + 0x23; break;
        case 4: message = situation * 5 + 0x25; break;
        default: CTRACE(3, "CAdvisor::HandleStaffInfo() doesn't know about the staff type %d.\n", staff); break;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId;
    thingId.mIndex = 0;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), thingId, 0, -1, false, -1);
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) mLastTutorialRequest = request;
    if (!CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce != 0)) return;
    int32_t slot;
    for (slot = 0; slot < 8; slot++) {
        if (mQueue[slot].Valid() == 0) break;
    }
    if (slot >= 8) slot = -1;
    if (slot >= 0) {
        mQueue[slot] = request;
        return;
    }
    const int32_t lowest = FindLowestScoringSlotInQueue();
    if (lowest < 0) return;
    const int32_t newScore = request.mValid != 0 ? request.mScore : -1;
    const int32_t oldScore = mQueue[lowest].mValid != 0 ? mQueue[lowest].mScore : -1;
    if (oldScore < newScore) mQueue[lowest] = request;
}

namespace {
template<class V> int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}
template<class V> int32_t Struct(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVARSTRUCT<V>(file, value, name, 0) : LOADVARSTRUCT<V>(file, value, name, 0);
}
}  // namespace

// 0x1000a468, 1072 bytes. The save and load halves are the same walk.
int32_t CAdvisor::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (version < 0 || version >= 2) return 0;
    if (Clock().Serialise(file, save, version, unused) != 0) return 1;
    if (Var<MacLong>(save, file, reinterpret_cast<MacLong*>(&mF0C[4]), "mSoundId") != 0) return 1;
    for (int32_t i = 0; i < 8; i++) {
        if (mQueue[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    if (Var<int32_t>(save, file, reinterpret_cast<int32_t*>(&mFD4[0]), "mNextAvailableQueuePos") != 0) return 1;
    if (Var<MacULong>(save, file, reinterpret_cast<MacULong*>(&mPlayStartTime), "mLastActionStarted") != 0) return 1;
    if (Var<MacULong>(save, file, reinterpret_cast<MacULong*>(&mPlayDuration), "mLastActionDuration") != 0) return 1;
    for (int32_t i = 0; i < 0x15f; i++) {
        if (Struct<AdvisorMessageState>(save, file, &mMessageState[i], "mHistory[i]") != 0) return 1;
    }
    if (mLastTutorialRequest.Serialise(file, save, version, unused) != 0) return 1;
    if (Var<bool>(save, file, reinterpret_cast<bool*>(&mF16E8[0]), "mRequestedLastTutorial") != 0) return 1;
    if (Var<AdvisorMessage>(save, file, &mTypePlaying, "mMessagePlaying") != 0) return 1;
    return 0;
}

// He is saying a line while the clock (unsigned) is before the line's start plus its length.
bool CAdvisor::IsPlaying() {
    return static_cast<uint32_t>(gClock.ReadFixed()) < mPlayStartTime + mPlayDuration;
}

// 0x1000c760: the compare is unsigned, so a start plus duration that wraps past 2^32 ends the line at once (and a clock that wraps
// starts it again).
AdvisorMessage CAdvisor::GetTypePlaying() {
    const uint32_t now = static_cast<uint32_t>(gClock.ReadFixed());
    if (now < mPlayStartTime + mPlayDuration) return mTypePlaying;
    return kAdvisorNoMessage;
}

// Unit v51049: std::locale::~locale 0x10070d44, std::basic_filebuf<char>::imbue 0x10070bf4 and pbackfail 0x100715b4,
// DisableRideHighlight 0x100672e4, ESP_GetRandomSpritesFrom 0x1007c900, ToggleProceduralTexturing 0x100634f8,
// DestroyDynamicMeshSetInstance 0x10063264, CAdvisor::SetPlayingFor 0x1000c7fc.

// ---- CAdvisor::SetPlayingFor (0x1000c7fc)
void CAdvisor::SetPlayingFor(uint32_t length) {
    mPlayStartTime = static_cast<uint32_t>(gClock.ReadFixed());
    mPlayDuration = length;
}

// CAdvisor::ReceiveMessage (main 0x1000c874, 5,440 bytes; unit v5631). Advisor.cpp on the Mac.

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The tail the original inlines after every request it builds (QueueRequest's body): a tutorial line is remembered for F1, then the request
// goes to the first free slot of the queue or, with the queue full, over the lowest scoring slot when it scores higher (an invalid request
// scores -1).
void Offer(CAdvisor& advisor, const CAdvisorRequest& request) {
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) advisor.mLastTutorialRequest = request;
    if (!advisor.CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce != 0)) return;
    int32_t slot;
    for (slot = 0; slot < 8; slot++) {
        if (advisor.mQueue[slot].Valid() == 0) break;
    }
    if (slot >= 8) slot = -1;
    if (slot >= 0) {
        advisor.mQueue[slot] = request;
        return;
    }
    const int32_t lowest = advisor.FindLowestScoringSlotInQueue();
    if (lowest < 0) return;
    const int32_t newScore = request.mValid != 0 ? request.mScore : -1;
    const int32_t oldScore = advisor.mQueue[lowest].mValid != 0 ? advisor.mQueue[lowest].mScore : -1;
    if (oldScore < newScore) advisor.mQueue[lowest] = request;
}

// A request for `message` with the original's defaults: response -1 (the scorer picks), no override, score -1 (looked up).
void Build(CAdvisor& advisor, int32_t message, uint16_t thing, uint32_t secondary) {
    ThingId thingId;
    thingId.mIndex = thing;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), thingId, secondary, -1, false, -1);
    Offer(advisor, request);
}

}  // namespace

// Types 5 (an advisor request), 8 (a ride condemned), 0xe (a thing's notice), 0x11 (research completed), 0x13 (an event), 0x17 (staff info)
// and 0x19 (a challenge's line) are handled; every other type is traced. The original's jump table (0x1027de10) covers types 5 to 0x19.
void CAdvisor::ReceiveMessage(const CMsg& message) {
    switch (message.GetType()) {
    case 5: {
        const CAdvisorRequest& request = static_cast<const CMsgAdvisor&>(message).mRequest;
        if (request.Valid() == 0) return;
        Offer(*this, request);
        return;
    }
    case 0x11: {
        const CMsgResearchCompleted& research = static_cast<const CMsgResearchCompleted&>(message);
        int32_t line;
        switch (static_cast<int32_t>(research.mCategory)) {
        case 0: line = 0x5d; break;
        case 1: line = 0x5f; break;
        case 2: line = 0x60; break;
        case 3: line = 0x61; break;
        case 4: line = 0x5e; break;
        default:
            CTRACE(3, "unknown research category in CAdvisor::ReceiveMessage.\n");
            return;
        }
        Build(*this, line, 0, research.mObj);
        return;
    }
    case 0x13:
        HandleEvent(static_cast<EventType>(static_cast<const CMsgEvent&>(message).mEvent));
        return;
    case 0x17:
        HandleStaffInfo(static_cast<const CMsgStaffInfo&>(message));
        return;
    case 0xe: {
        const CMsgThingAdvice& advice = static_cast<const CMsgThingAdvice&>(message);
        const uint16_t row = advice.mThingId;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        // The thing's map cell id: 1 + x + (y << 7), kept to 16 bits.
        const CThing* thing = static_cast<const CThing*>(gThingArray[row].mThing);
        const uint16_t cell = static_cast<uint16_t>(thing->mCellX + ((static_cast<uint32_t>(thing->mCellY) << 7) + 1));
        int32_t line;
        switch (advice.mKind) {
        case 0: line = 0x7d; break;
        case 1: line = 0x7e; break;
        case 2: line = 0x7f; break;
        default: return;
        }
        Build(*this, line, row, cell);
        return;
    }
    case 8:
        Build(*this, 0x102, static_cast<const CMsgRideCondemned&>(message).mThingId, 0);
        return;
    case 0x19: {
        const AdvisorMessage line = static_cast<const CMsgChallenge&>(message).mMessage;
        if (line == kAdvisorNoMessage) return;
        Build(*this, line, 0, 0);
        return;
    }
    default:
        TRACE("CAdvisor::ReceiveMessage has encountered an unknown (to it) message type (%d).\n", message.GetType());
        return;
    }
}

// GAME_ASSERTMSG (main app of SimTheme Park 2.04b3, 0x1000dde8; the inventory files it under
// Advisor.cpp). The release build reports nothing: the original only spills the registers of
// a variable argument list into the caller's frame and returns. Every balance table error
// goes through it.

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {}

// 0x1000de30: field by field in the original's order; the two padding regions keep what they held.
CAdvisorRequest::CAdvisorRequest(const CAdvisorRequest& other) {
    mThingId = other.mThingId;
    mMessage = other.mMessage;
    mScore = other.mScore;
    mValid = other.mValid;
    mSecondary = other.mSecondary;
    mResponseNo = other.mResponseNo;
    mOverrideOnlyOnce = other.mOverrideOnlyOnce;
    mScoreOverridden = other.mScoreOverridden;
}

// 0x1000deb4: the message, the thing id (the high half of the tag word; the low half is never read), the secondary; then the score: a
// negative one is looked up (and not marked as overridden), else it is taken and marked; the response number, the override flag, and
// the request becomes valid last.
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    mMessage = message;
    mThingId = thingId.mIndex;
    mSecondary = secondary;
    if (score < 0) {
        mScore = TheScoring().GetScoreFor(message);
        mScoreOverridden = 0;
    } else {
        mScore = score;
        mScoreOverridden = 1;
    }
    mResponseNo = responseNo;
    mOverrideOnlyOnce = overrideOnlyOnce;
    mValid = 1;
}

uint16_t gAdvisorNullThingId = 0;   // 0x1027de64

namespace {

// The request's first word as the Mac reads it with lwz (big-endian: thing id in the high half, then the two padding bytes).
uint32_t FirstWord(const CAdvisorRequest& r) {
    return (static_cast<uint32_t>(r.mThingId) << 16) | (static_cast<uint32_t>(r.mPad02[0]) << 8) | r.mPad02[1];
}

// The tail both functions share: the line was chosen; say 0 and fail for 0x266, else call, tag and post.
int32_t Speak(CAdvisorRequest& r, uint32_t* length, int32_t response) {
    if (response == 0x266) {
        *length = 0;
        return 0;
    }
    *length = static_cast<uint32_t>(ADV_Call(AdvisorResponse{response}));
    int32_t tag = ADV_GetTag(AdvisorResponse{response});
    CMsgTagObj msg(tag, FirstWord(r), r.mSecondary, -1);
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre();
    gMessageCentre->PostMessage(msg);
    return 1;
}

int32_t PickResponse(const CAdvisorRequest& r, int32_t responseNo) {
    CAdvisorScoring& scoring = TheScoring();
    if (responseNo < scoring.GetNumResponsesFor(r.mMessage)) return responseNo + TheScoring().GetFirstResponseFor(r.mMessage);
    return TheScoring().GetFirstResponseFor(r.mMessage);
}

}  // namespace

// 0x1000dff4 (684 bytes).
int32_t CAdvisorRequest::Play(uint32_t* length, int32_t responseNo) {
    if (mValid == 0) return 0;
    mValid = 0;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId;
    thingId.mIndex = 0;
    uint32_t value = 0;
    int32_t score = TheScoring().LastMinuteCheck(mMessage, thingId, value);
    if (score < kAdvisorBalanceMinScore) return 0;
    if (thingId.mIndex != gAdvisorNullThingId) mThingId = thingId.mIndex;
    if (value != 0) mSecondary = value;
    return Speak(*this, length, PickResponse(*this, responseNo));
}

// 0x1000e340 (492 bytes).
int32_t CAdvisorRequest::PlayWithoutScore(uint32_t* length, int32_t responseNo) {
    if (mValid == 0) return 0;
    mValid = 0;
    return Speak(*this, length, PickResponse(*this, responseNo));
}

// Unit v5937: SAVEVARSTRUCT<AdvisorHistory> 0x1000f96c, CAdvisorScoring::LastMinuteCheck 0x100102d8, CAdvisorRequest::Serialise 0x1000e568.

// 0x1000f96c (296 bytes). The Mac builds a 16-byte copy on the stack (+0, +4, the byte at +8, +0xc; the three padding bytes are stack
// garbage there and zero here), puts the three words in file order (SwapWord, as LOADVARSTRUCT<AdvisorMessageState>'s UnswapWord: the host
// does not reverse them, v5L15) and writes it. The result test is (result >= 0) == (zero word == 0): the zero
// word at 0x1027dec8 never changes, so it is Succeeded().
template<> int32_t SAVEVARSTRUCT<AdvisorMessageState>(void* file, AdvisorMessageState* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(0x10, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    uint32_t copy[4] = {0, 0, 0, 0};
    const uint8_t* source = reinterpret_cast<const uint8_t*>(value);
    std::memcpy(&copy[0], source + 0, 4);
    std::memcpy(&copy[1], source + 4, 4);
    reinterpret_cast<uint8_t*>(copy)[8] = source[8];
    std::memcpy(&copy[3], source + 0xc, 4);
    GAME_ASSERTMSG(1, "should be a struct");
    copy[0] = serialise::SwapWord(copy[0]);
    copy[1] = serialise::SwapWord(copy[1]);
    copy[3] = serialise::SwapWord(copy[3]);
    TbError result = LbFile_Write(file, copy, 0x10, &written);
    if (result.Succeeded() && written == 0x10) return 0;
    GAME_ASSERTMSG(0, "File write failed");
    return 1;
}

// Declared before its first use: the definition is further down.
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
template<> int32_t LOADVAR<bool>(void* file, bool* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
template<> int32_t SAVEVAR<bool>(void* file, bool* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
namespace {
using ThingIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
}
template<> int32_t LOADVARSTRUCT<ThingIdStruct>(void* file, ThingIdStruct* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
namespace {
using ThingIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
}
template<> int32_t SAVEVARSTRUCT<ThingIdStruct>(void* file, ThingIdStruct* value, const char* name, int32_t noLog);

// 0x1000e568 (756 bytes).
int32_t CAdvisorRequest::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version >= 2 || version < 0) return 0;
    typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;
    ThingId* thingId = reinterpret_cast<ThingId*>(&mThingId);
    MacULong* secondary = reinterpret_cast<MacULong*>(&mSecondary);
    bool* valid = reinterpret_cast<bool*>(&mValid);
    bool* overrideOnlyOnce = reinterpret_cast<bool*>(&mOverrideOnlyOnce);
    bool* scoreOverridden = reinterpret_cast<bool*>(&mScoreOverridden);
    if (save != 0) {
        if (SAVEVARSTRUCT<ThingId>(file, thingId, "mThingId", 0) != 0) return 1;
        if (SAVEVAR<MacULong>(file, secondary, "mSecondary", 0) != 0) return 1;
        if (SAVEVAR<AdvisorMessage>(file, &mMessage, "mMessage", 0) != 0) return 1;
        if (SAVEVAR<int32_t>(file, &mScore, "mScore", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, valid, "mValid", 0) != 0) return 1;
        if (SAVEVAR<int32_t>(file, &mResponseNo, "mResponseNo", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, overrideOnlyOnce, "mOverrideOnlyOnce", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, scoreOverridden, "mScoreOverridden", 0) != 0) return 1;
    } else {
        if (LOADVARSTRUCT<ThingId>(file, thingId, "mThingId", 0) != 0) return 1;
        if (LOADVAR<MacULong>(file, secondary, "mSecondary", 0) != 0) return 1;
        if (LOADVAR<AdvisorMessage>(file, &mMessage, "mMessage", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mScore, "mScore", 0) != 0) return 1;
        if (LOADVAR<bool>(file, valid, "mValid", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mResponseNo, "mResponseNo", 0) != 0) return 1;
        if (LOADVAR<bool>(file, overrideOnlyOnce, "mOverrideOnlyOnce", 0) != 0) return 1;
        if (LOADVAR<bool>(file, scoreOverridden, "mScoreOverridden", 0) != 0) return 1;
    }
    return 0;
}

// 0x1000e8cc
void CAdvisorRequest::SetScore(int32_t score) {
    mScore = score;
    mScoreOverridden = 1;
}

// Unit v51116: the 8 byte scorers (a balance value read, or 0), two small destructors and the sound library's inline byte swaps.

// 0x1000e2d0
CMsgTag* CMsgTagDestruct(CMsgTag* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtable = 0x1027de84;
        if (deleteFlag > 0) v51116_callee::OperatorDelete(self);
    }
    return self;
}

int32_t CMsgTag::GetType() const { return 0x18; }   // 0x1000e90c

namespace v51116_swap {
namespace {
inline void Inplace(uint32_t& word) { word = __builtin_bswap32(word); }
}  // namespace

int32_t* USwap32_long_ptr(int32_t* p) { return SwapPtr(p); }   // 0x10009ad0

void USwapInplace32_long_10021db8(int32_t& v) { Inplace(reinterpret_cast<uint32_t&>(v)); }
void USwapInplace32_ulong_10021dfc(uint32_t& v) { Inplace(v); }
void USwapInplace32_long_10009c74(int32_t& v) { Inplace(reinterpret_cast<uint32_t&>(v)); }
void USwapInplace32_int_10009c30(int32_t& v) { Inplace(reinterpret_cast<uint32_t&>(v)); }
void USwapInplace32_ulong_10009b2c(uint32_t& v) { Inplace(v); }

void USwapBlock32_ulong(uint32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(words, count); }   // 0x10009bd0
void USwapBlock32_long(int32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }   // 0x10009b74
}  // namespace v51116_swap

// AdvisorBalance.cpp: CAdvisorBalance, the balance table of data:advisor:advisor.sam (main app, Advisor.cpp 0x1000e93c and 0x1000e98c).
// The class is only its two virtuals: the description rows (src/AdvisorBalance_table.cpp, generated) and the value slots.

// 0x1000e93c: the row's description (0x3c bytes a row from 0x1028318c); no bounds test.
const BalanceDescription* CAdvisorBalance::GetBalanceDescription(int32_t row) {
    return &gAdvisorBalanceDescriptions[row];
}

// 0x1000e98c: this + 8, the table's first slot (the reserved slot 0).
BalanceValue* CAdvisorBalance::GetBalanceValuesStart() {
    return mValues;
}

// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).

// ---- Advisor.cpp
const char16_t gAdvisorMsgText[] = u"gm";   // 0x1023c316 (only the first two characters are known)
unsigned char gAdvisorMsgStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x102b3aa8
unsigned char gAdvisorMsgStringNode[8];                                              // 0x102b3abc

// 0x1000fff0: the global wide string is made and registered with the wide string destructor glue.
void __sinit_Advisor_cpp() {
    auto* text = new (gAdvisorMsgStringStorage) TbDynamicStringTemplate<char16_t>(gAdvisorMsgText);
    __register_global_object(text, reinterpret_cast<void*>(&UIMain_DestroyWString), gAdvisorMsgStringNode);
}

// LOADVARSTRUCT<AdvisorHistory> 0x1000ecb4 (260 B): the file's 16 bytes, then the words at +0, +4 and +0xc byte-reversed.
// The 0x10 byte record is AdvisorMessageState here.
static void UnswapForFile(AdvisorMessageState& state) {
    static_assert(sizeof(AdvisorMessageState) == 0x10, "the record is 16 bytes");
    state.mLastSaid.mTimeSig = serialise::UnswapWord(state.mLastSaid.mTimeSig);
    state.mResponseIndex = serialise::UnswapWord(state.mResponseIndex);
    state.mKillCount = serialise::UnswapWord(state.mKillCount);
}

template<> int32_t LOADVARSTRUCT<AdvisorMessageState>(void* file, AdvisorMessageState* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);
}

// Unit v5L05: the save-file instances for AdvisorMessage, LOADVAR 0x1000f250 and SAVEVAR 0x1000f820 (Advisor.cpp on the Mac), which
// CAdvisorRequest::Serialise calls. A message is one unvalidated signed word, the shape of LOADVAR<int> and SAVEVAR<int> beside them.

// 0x1000f250 (236 bytes).
template<> int32_t LOADVAR<AdvisorMessage>(void* file, AdvisorMessage* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}

// 0x1000f820 (260 bytes).
template<> int32_t SAVEVAR<AdvisorMessage>(void* file, AdvisorMessage* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}

unsigned char gCLightParallelVt1[0x40];   // 0x10278c60
unsigned char gCLightParallelVt2[0x40];   // 0x10278c9c
unsigned char gCLightParallelVt3[0x40];   // 0x10278cb4

namespace {
// A Mac address is an offset from the arena base (include/Ptr32.h).
inline uint32_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(p) - tpw::gArenaBase); }
inline uint32_t Rd(uint32_t a) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + a); }
inline void Wr(uint32_t a, uint32_t v) { *reinterpret_cast<uint32_t*>(tpw::gArenaBase + a) = v; }
}  // namespace

// The import operator delete (0x10238218); weak so the unit's test library can replace it with a recording fake.
__attribute__((weak)) void CLightParallel_Delete(void* block) { ::operator delete(block); }

// 0x10007750: restores the vtable words the Mac destructor writes, then deletes when the flag is positive.
CLightParallel* CLightParallel_DtorMac(CLightParallel* self, int16_t flag) {
    if (self == nullptr) return self;
    uint32_t t = Off(self);
    uint32_t t2 = Off(gCLightParallelVt2);   // r10
    uint32_t t3 = Off(gCLightParallelVt3);   // r9
    uint32_t t1 = Off(gCLightParallelVt1);   // r3
    Wr(t + 4, t1);
    Wr(Rd(t) + 0x5c, t1 + 0x24);
    Wr(Rd(Rd(t)), t1 + 0xc);
    uint32_t b = Rd(Rd(t));
    Wr(b + 4, (t + 8) - b);
    uint32_t a = Rd(t);
    Wr(a + 0x60, (t + 0x10) - a);
    if (flag != 0) {
        if (t + 0x10 != 0) {
            Wr(t + 0x6c, t3);
            uint32_t r = Rd(t + 0x10);
            Wr(r, t3 + 0x18);
            Wr(r + 4, (t + 0x70) - r);
        }
        if (t + 8 != 0) Wr(t + 8, t2);
    }
    if (flag > 0) CLightParallel_Delete(self);
    return self;
}

// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.

// Loaders: 0x1000ef18 (int), 0x1000f144 (bool), 0x100f44a4 (unsigned int), 0x100f5074
// (unsigned short), 0x1016d2c8 (char), 0x10111844 (wchar_t).
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000ef18, 236 bytes
}
template<> int32_t LOADVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000f144, 228 bytes
}
// Savers: 0x1000f4b0 (int), 0x1000f70c (bool), 0x100f45bc (unsigned int), 0x100f5530
// (unsigned short), 0x1016d3d4 (char), 0x10111a98 (wchar_t).
template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f4b0, 260 bytes
}
template<> int32_t SAVEVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f70c, 236 bytes
}
// The thing id (a halfword in a struct): the file's order is little-endian, so the two bytes are exchanged on the way in and out (the listings'
// sthbrx). The Mac's swap functions are inlined into the instances.
namespace {
using ThingIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
}
static void SwapForFile(ThingIdStruct& id) { id.mIndex = serialise::SwapWord(id.mIndex); }
static void UnswapForFile(ThingIdStruct& id) { id.mIndex = serialise::UnswapWord(id.mIndex); }

template<> int32_t LOADVARSTRUCT<ThingIdStruct>(void* file, ThingIdStruct* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x1000eb4c, 224 bytes
}
template<> int32_t SAVEVARSTRUCT<ThingIdStruct>(void* file, ThingIdStruct* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x1000e9d4, 240 bytes
}

// Unit w160. LOADVAR and SAVEVAR of the Mac's long (0x1000ee04, 0x1000f384) and unsigned long (0x1000f02c, 0x1000f5dc), four bytes each (MacLong and
// MacULong in Serialise.h); the listings are the int instances' (the log call, the file call, the failure assert, the two success asserts in
// the order "should not be a struct" then "dummy assert", the reversal of the four bytes).
template<> int32_t LOADVAR<MacLong>(void* file, MacLong* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000ee04, 236 bytes
}
template<> int32_t LOADVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000f02c, 236 bytes
}
template<> int32_t SAVEVAR<MacLong>(void* file, MacLong* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f384, 260 bytes
}
template<> int32_t SAVEVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f5dc, 260 bytes
}
