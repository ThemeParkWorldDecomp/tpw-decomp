// Advisor.cpp (main app): the advisor's speech and animation (ADV_*). src/Advisor.cpp translates the start-up and
// close, the pause pair, the animation sequences, the accessories and dressing, the per-frame ticks and the part that
// makes him speak (ADV_Call, ADV_Slap, ADV_Kill, ADV_GetTag and ADV_Stat, 0x1000887c..0x10009194); the response table
// they read is data (src/advisor_lines.cpp).
#pragma once
#include <cstdint>

struct tag_sRideAnimSet;

void ADV_Pause();         // 0x10007d54
void ADV_UnPause();       // 0x10007dd4
void ADV_Init();          // 0x10007e5c
void ADV_Close();         // 0x10007fc4
// The animation numbers of a sequence are 1 to 15 (animation file Advisorm<n>, 5 is the group of the ride set that
// holds them); the sequence word also carries the effect bits 0x7f0000 (see SAdvisorState::mFlags).
// 0x1000804c: a random sequence that fills `length` milliseconds (the first animation is always 14, the last 15);
// the answer is the time the sequence takes, which can be more than asked.
int32_t GenerateAnimSequence(int32_t length);               // 0x1000804c
// 0x100081f4: the canned sequence `sequence` (a row of gCannedSequences, 0 ending it; a word with bit 0x400000
// instead sets the time of the "key" event: the time so far plus the next word); if it has no time and `length` is
// not 0, a random one. The answer is the sequence's time.
int32_t SetCannedSequence(int32_t sequence, int32_t length);   // 0x100081f4
// 0x10008330: dresses him for one accessory number: -1 shows the accessories the line brought (and hides those it
// hides), 0 shows both hands, 1 to 14 take the hands off (14 only the left one; 3, 8 and 12 leave them); then, for a
// number other than 0, that accessory is shown (unless mFlags has 0x10000).
void SetAdvisorHatHand(int32_t accessory);                  // 0x10008330
void DressAdvisor();                                        // 0x100084a0
void ADV_Tick2();         // 0x100091d8
void ADV_Tick();          // 0x10009298

// Which line the advisor says (ADV_Call). Passed by value: one word.
struct AdvisorResponse {
    int32_t mId;
};
// 0x1000887c: makes the advisor say the line `response` (the first row of gAdvisorLines with that id): loads its
// lip-sync file, starts the sound and the animation sequence and fills in the state ADV_Tick follows. Answers 0 when
// he cannot (the advisor is off, he is speaking, the id is not in the table, or the sound answers a length of -200 ms,
// which leaves the state half filled); else the time the line takes in milliseconds.
int32_t ADV_Call(AdvisorResponse response);
void ADV_Slap();                           // 0x10008dd4: cuts the line short and sends one of three slap sounds
// 0x10008f00: ends the line (mode 0 stops the sound, any other mode fades it); nothing when he is not speaking.
void ADV_Kill(int32_t mode);
int32_t ADV_GetTag(AdvisorResponse response);   // 0x10009050: the row's tag (word 7), 0x17f when the id is not in the table
int32_t ADV_Stat();                        // 0x100090dc: 0 when silent, else the line's end time less the advisor clock

// A row of the response table (0x10278ce4, 0x20 bytes on the Mac; names invented). The table ends with a row whose id
// is 9999 (all its words are); a search that finds no id before it answers "none".
struct AdvisorLine {
    int32_t mId;                  // 0x00  the response id (610 distinct ids from 0 to 612)
    int32_t mSound;               // 0x04  the sound event
    int32_t mLipFile;             // 0x08  n of the lip-sync file ":Speech:lips:sp_<n>.lip"
    int32_t mSequence;            // 0x0c  -1: a random animation sequence; else the canned sequence mSequence - 0x10
    uint32_t mWhichAndSoundSet;   // 0x10  the advisor model in the low 16 bits, the sound set (0: global bank) above
    int32_t mHat;                 // 0x14  the accessories the line asks for (SAdvisorState::mHat)
    int32_t mHand;                // 0x18
    int32_t mTag;                 // 0x1c  what ADV_GetTag answers
};
constexpr int32_t kAdvisorLastLine = 9999;   // the id of the terminator row
extern AdvisorLine gAdvisorLines[611];       // 0x10278ce4, src/advisor_lines.cpp (not const so that a test can patch a row)
// The lip-sync file ADV_Call opened last (TPFile_Open's answer; the original name is unknown): 0x102b3548.
extern void* gAdvisorFile;

// The advisor's state (0x102b354c, 0x168 bytes on the Mac; the data pointer makes the host's larger). ADV_Init clears
// it. Times are the advisor clock's milliseconds (the game clock less the time spent paused, ADV_Tick).
struct SAdvisorState {
    union {
        uint8_t mF00[0xc];
        struct {
            uint32_t mFlags;       // 0x00  the effect bits (mask 0x7f0000) of the animation playing: 0x10000 no
                                   //       accessories and no mouth, 0x20000 .. 0x200000 the effects DressAdvisor makes
            int32_t mBaseTime;     // 0x04  ADV_Call: the advisor clock 200 ms before the line began; the mouth's times are
                                   //       counted from it
            int32_t mEndTime;      // 0x08  when the line ends
        };
    };
    int32_t mSpeaking;     // 0x0c  nonzero while the advisor talks
    int32_t mMouthOpen;    // 0x10  the mouth is open (it flips at every time of the lip-sync data)
    uint32_t mWhich;       // 0x14  which advisor model (slot of gAdvisorStore) says it
    int32_t mSoundSet;     // 0x18  0: the sound bank gSoundBanks[5], else gAdvisorThemeSoundBank
    int32_t mSound;        // 0x1c  the sound event of the line
    int32_t mSoundHandle;  // 0x20  its handle while it plays
    int32_t mHat;          // 0x24  the accessories the line asks for (SetAdvisorHatHand; 0: none)
    int32_t mHand;         // 0x28
    int32_t mDressAfterAnim;   // 0x2c  DressAdvisor when the animation queued behind the playing one has started
    int32_t mDressNow;     // 0x30  DressAdvisor at the next ADV_Tick2
    int32_t mSoundTime;    // 0x34  when to send the sound event (0: not waiting)
    int32_t mHeadNode[2];  // 0x38  each model's node index of the "bug head"
    int32_t mMouthNodes[2][5];   // 0x40  each model's node indexes of mouth shapes 1 to 5
    int32_t mAnimLengths[20];    // 0x68  the animations' lengths in milliseconds (ADV_Init); [13] is cleared after
    int32_t mSequence[20];       // 0xb8  the animation sequence (word 0 ends it), see ADV_Tick
    int32_t mSequencePos;  // 0x108  the sequence's next word
    int32_t mAnimIndex;    // 0x10c  the animation playing, from 0
    int32_t mMouthCount;   // 0x110  5
    int32_t mMouthTime;    // 0x114  when the lip-sync data next flips the mouth
    int32_t mKeyTime;      // 0x118  when the key is shown (0: not waiting)
    int32_t mClockPaused;  // 0x11c  ADV_Tick has seen the game clock paused
    int32_t mPauseStart;   // 0x120  the game clock when it did
    int32_t mPausedTotal;  // 0x124  the clock time already spent paused
    int32_t mMouthWanted;  // 0x128  the mouth shape asked for (1 to 5; 1 is shut; 0 none)
    int32_t mMouthShown;   // 0x12c  the one shown (-1: none)
    int32_t mMouthChange;  // 0x130  when the shape may next change
    int32_t mSyncSize;     // 0x134  bytes of lip-sync data
    int32_t mSyncPos;      // 0x138  its next word
    int32_t mSyncActive;   // 0x13c
    int32_t* mSyncData;    // 0x140  the data (milliseconds, -1 ends it): gAdvisorStaticBuffer or a malloc block
    int32_t mAccessoriesOn[4];    // 0x144  accessories SetAdvisorHatHand(-1) shows
    int32_t mAccessoriesOff[4];   // 0x154  and hides
    int32_t mPausedSound;  // 0x164  ADV_Pause paused the sound
};
extern SAdvisorState gAdvisorState;

// The lip-sync data of a short line lives in a static buffer (one line at a time); a longer one is malloc'd.
extern int32_t gAdvisorStaticInUse;       // 0x102b36b4
extern int32_t gAdvisorStaticBuffer[250];   // 0x102b36b8 (1,000 bytes)
// The animation sequences SetCannedSequence picks from: 14 rows of 20 words, 0 ending a row (0x1027d944).
extern const uint32_t gCannedSequences[14][20];

// The sound bank of the second advisor voice: the word at 0x102f7cb4, that is gSoundBanks + 0x20 (TPSound.h's array
// stops short of it, as it does of gLobbyThunderBank).
extern int32_t gAdvisorThemeSoundBank;

// ---- what the advisor calls and is not translated yet (declared here until their units land)
// The sound wrappers (0x100ddb18, 0x100ddbb8, 0x100ddf60): pause or resume a sound, or move it (the answer is its
// handle, 0 when it ended).
void SOUND_PauseSound(int32_t handle);                                         // 0x100ddb18
void SOUND_UnpauseSound(int32_t handle);                                       // 0x100ddbb8
int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t a, int32_t b, int32_t c);   // 0x100ddf60
// The effect manager (HLSprite.cpp range): the advisor's sparkle effects.
void EM_Init();                                                     // 0x1007a3d0
int32_t EM_SetEmitersOn(int32_t effect, int32_t count, int32_t emitter);   // 0x1007a454
void EM_SetEffectTranslation(int32_t effect, int32_t index, float x, float y, float z);   // 0x1007a558
void EM_SetEffectOrbit(int32_t effect, int32_t index, float a, float b, float c);          // 0x1007a63c
void EM_Tick();                                                     // 0x1007a6b8
// The interface: redraws the island lobby's key (ui_islandLobbyController.cpp).
void UpdateIslandLobbyKeyDisplay();                                 // 0x101e188c
