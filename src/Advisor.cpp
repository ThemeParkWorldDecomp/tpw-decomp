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
#include "Advisor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Clock.h"
#include "Debug.h"
#include "FS_System.h"
#include "LbFile.h"
#include "Main.h"
#include "TPAdvisor.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "rsse.h"

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
