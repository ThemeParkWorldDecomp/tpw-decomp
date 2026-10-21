// AudioPlaceHolder.h: CAudioPlaceHolder, the base of every sound the game is playing or waiting to play, with its priority record
// CDistPriority, its extra data CAudioPlaceHolderExtraData and the callback base TbSndCallback (unit w79g2, "sound symbols
// shared", Code at 0x10000000, TOC r2 = 0x1003abf0; the library's file is AudioPlaceHolder.cpp). The functions are in
// src/sound/AudioPlaceHolder.cpp, src/sound/TbAudioBody.cpp, src/sound/AudioPlaceHolderList.cpp, src/sound/TbSoundSampleCallback.cpp with their addresses.
//
// Mac layout of a placeholder, 0x60 bytes (the pool, unit w79g1, keeps them in blocks of 100 slots; the subclasses of unit w79g3
// are placement-constructed into the slots). The host members are native, so the host object is larger; the order is the Mac's:
//   +0 vptr  +4 mEvent  +8 mSound  +0xc mSample  +0x10 mPosition  +0x1c mVelocity  +0x28 mEndTime  +0x2c mPriority  +0x30 mHandle
//   +0x34 mCatagory  +0x38 mListHandle  +0x3c mFlags  +0x40 mNext  +0x44 mPrevious  +0x48 mPauseTime  +0x4c mMaxDistance
//   +0x50 mLastSound  +0x51 mLastSample  +0x52 mVolumeControlIndex  +0x54 mFade  +0x58 mVolume  +0x5a mPitch  +0x5c mExtra
// The constructor stores +4 +8 +0xc +0x28 +0x2c(as 0) +0x30 +0x34 +0x3c +0x48 +0x4c +0x50 +0x51 +0x5c and the two positions; it leaves
// +0x38, +0x40, +0x44, +0x52, +0x54, +0x58 and +0x5a as they were (the host constructor leaves them too).
//
// Vtable 0x1003cc04 (the constructor's `addi r0, r2, 0x2014`; the two words before the slots, 0x1003cbfc and 0, are CodeWarrior's
// offset and RTTI words): +8 SoundCallback 0x1000d48c (TbSndCallback's, empty), +0xc CompletedCallback 0x100185bc, +0x10 the
// destructor 0x100179e0 (r4 = a short flag: a positive flag also deletes), +0x14 Release 0x1001864c, +0x18 Init 0x10018874,
// +0x1c PlaySample 0x1001ba48, +0x20 Stop 0x1001a480, +0x24 Pause 0x1001a544, +0x28 Restart 0x1001a678, +0x2c ChangeDistPriority
// 0x10017aa4, +0x30 Update 0x100182a4, +0x34 UpdateParameter 0x10017f30, +0x38 UpdateParameterDepandants 0x10018194, +0x3c
// ChooseRandomSound 0x10019ca0, +0x40 ChooseRandomSample 0x100198e8, +0x44 GetParameterValue 0x10018a44, +0x48 FillSampleInfo
// 0x100191cc, +0x4c FillBasicSampleInfo 0x10018f34, +0x50 GetPsuedoTime 0x10019060, +0x54 GetClassID 0x1001afe4 (the subclasses
// override more). The host's virtuals are declared in this order; the host vtable layout is the compiler's own.
//
// Calling convention: every function returning TbError returns it through a hidden r3 buffer on the Mac (this moves to r4); the
// host returns TbError by value. A virtual call on the Mac is `lwz r12, 0(this); lwz r12, SLOT(r12); bl 0x1002f154`.
#pragma once
#include <cstdint>

#include "TbError.h"
#include "TbGraphicFile.h"
#include "TbSoundInfo.h"
#include "TbSoundPosition.h"

struct CEventData;
struct CSoundData;
struct CSampleData;
class CCatagoryContainer;
class TbMapStreamer;
class TbSoundManager;
class CAudioPlaceHolder;

// ---- callees outside this unit that are not translated yet (stand-ins in the launcher) -----------------------------------
namespace bfl::maths {
// 0x10019ad0 (32): a wrapper of LbMaths_Random 0x10018cf4 (80): seed = seed * 0x19660d + 0x3c6ef35f, answers the new seed. The
// placeholder always passes a COPY of CAudioPlaceHolder::mRandomSeed, so the static itself is never advanced.
uint32_t Random(int32_t& seed);
// 0x10018c30 (32): a wrapper of LbMaths_RangedRandomSlow 0x10018c88 (56): Random(seed) % range (unsigned; no test for a zero range).
uint32_t RangedRandomSlow(int32_t& seed, uint32_t range);
}  // namespace bfl::maths

// Unit v5657: LbMaths_ArcTan__Fll 0x100295f0 (260): angle of (x, y) in 2048 units per turn.
extern "C" double gLbMathsAtanScale;  // 0x10074810: angle units per radian, set at run time
namespace bfl::maths {
int32_t LbMaths_ArcTan(int32_t x, int32_t y);
}  // namespace bfl::maths

// Unit v5738: LbMaths_Random__FRl 0x10018cf4 (80): seed = seed * 0x19660d + 0x3c6ef35f; calls RotateRight(seed, 13) and drops it.
int32_t LbMaths_Random(int32_t& seed);
namespace SamsUtilities::UBinary {
// RotateRight<long> 0x10018d6c (20): a stand-in callee; the unit library fakes it.
template <class T> T RotateRight(T value, T amount);
}  // namespace SamsUtilities::UBinary

// TbMapStreamer::FixUpCatagoryIndex(uint32_t) 0x10023378 is a member declared in TbMapStreamer.h (a stand-in until translated).

// ---- TbSndCallback ------------------------------------------------------------------------------------------------------
// The callback base a sample is played with (a TbSoundSampleInfo holds a TbSndCallback* for the sample and for its completion).
// Mac vtable 0x1003cc5c: +8 SoundCallback, +0xc CompletedCallback. No data.
class TbSndCallback {
public:
    TbSndCallback();   // 0x100179a8 (12): the vptr
    virtual void SoundCallback(void* sample, void* userData);                       // +8, 0x1000d48c (4): empty
    virtual void CompletedCallback(void* sample, void* userData, int32_t reason);   // +0xc, 0x1001baa4 (4): empty
};

// ---- CAudioPlaceHolderExtraData -------------------------------------------------------------------------------------------
// 0x20 bytes on the Mac (made by operator new(0x20) in AllocateExtraData). The constructor 0x100180cc (84) sets the rectangle
// (TbRect::TbRect 0x10018158 zeroes its four ints), mChild = 0, mParamId[0] = 0 and zeroes the four values; mParamId[1..3] and
// +0x1c (mParent) are left as the allocator gave them.
struct CAudioPlaceHolderExtraData {
    CAudioPlaceHolderExtraData();   // 0x100180cc
    uint8_t mParamId[4];            // +0 the event's parameter id (Init), +1 and +2 the sound's two (InitParamatisedControls), +3 zero
    uint8_t mParamValue[4];         // +4 the values UpdateParameter writes under the ids; +5 and +6 are read by GetParameterValue
    TbRect mBounds;                 // +8 left, +0xc top (the highest z), +0x10 right, +0x14 bottom (the lowest z)
    CAudioPlaceHolder* mChild;      // +0x18 the child placeholder of a one-shot sentence (Load parks the saved class id here first)
    CAudioPlaceHolder* mParent;     // +0x1c the parent a child is made with (set by unit w79g3's code; never by this unit)
};

// ---- CAudioPlaceHolder --------------------------------------------------------------------------------------------------
class CAudioPlaceHolder : public TbSndCallback {
public:
    // Distance and base priority packed in one big-endian word on the Mac: the u16 at +0 is the distance, the u16 at +2 the base
    // priority, and the word (operator unsigned long, operator=(unsigned long)) is distance << 16 | base priority. The sound
    // library hands that word to the device as the sample's priority.
    struct CDistPriority {
        CDistPriority() : mDistance(0), mBasePriority(0) {}   // 0x1001795c (12): the whole word zero
        uint32_t GetBasePriority() const;                     // 0x10016dc4 (8): lhz r3, 2(r3), zero-extended in r3 (w79f declares it so)
        void SetDistance(uint16_t distance);                  // 0x10017ca4 (8)
        void SetBasePriority(uint16_t priority);              // 0x10017d38 (8)
        operator uint32_t() const;                            // 0x10017e9c (8)
        CDistPriority& operator=(uint32_t word);              // 0x10017ee8 (8)
        uint16_t mDistance;       // Mac +0
        uint16_t mBasePriority;   // Mac +2
    };

    // The bits of mFlags (names are this unit's reading; 0x20 and 0x40 are set by the subclasses of unit w79g3).
    enum Flags : uint32_t {
        kPlaying = 0x1,             // the sample is playing (mHandle is nonzero)
        kLooping = 0x2,             // never finishes by itself (FinishedPlaying answers false)
        kLoopForever = 0x4,         // FillSampleInfo asks for a loop count of 9999
        kNonPositional = 0x8,       // plays at (0, 1, 0) with no distance
        kFinished = 0x10,           // the list removes it
        kSubclass20 = 0x20,         // set and tested by the subclasses
        kSubclass40 = 0x40,         // set by the sentence subclasses; CompletedCallback does not mark a placeholder finished then
        kMaxDistanceFromEvent = 0x80,    // mMaxDistance was given to Init (times the manager's distance multiplier)
        kProcessedByVirtual = 0x100,     // the list processes it through a virtual (unit w79g1)
        kUserId = 0x200,            // FillBasicSampleInfo passes the event pointer as the sample's user id
        kFixedVolume = 0x400,       // UpdateParameterDepandants keeps the volume
        kOutOfRange = 0x800,        // stopped for being out of range
        kBoundingRectangle = 0x1000,    // the position follows the listener inside mExtra's rectangle
        kPaused = 0x2000,
        kVolumeControlOff = 0x4000, // the volume control was off when it was last tested
        kVolumeSet = 0x8000,        // SetVolume, SetVolumeAndFrequency or SetVolumeAndPitch gave mVolume
        kPitchSet = 0x10000,        // ... and mPitch
        kEventMode = 0x40000,       // the event's flag 0x4000
        kFading = 0x80000,          // the volume fades in by 0.1 per ProcessPositionAndPriority
        kFresh = 0x100000,          // set by SetFlagsFromEvent, cleared by InitFade
    };

    CAudioPlaceHolder();   // 0x10017898 (148)
    // 0x100179e0 (148): the handle keeps its low 16 bits, the extra data is deleted (its child is NOT released: the extra data has
    // no destructor), the category container is released through its virtual +8. The deleting form (flag > 0) also deletes the
    // object. The host's `delete` is that; Release() is the flag -1 form (an explicit destructor call).
    virtual ~CAudioPlaceHolder();

    // ---- TbSndCallback ------------------------------------------------------------------------------------------------
    // 0x100185bc (80), slot +0xc: only for the sample whose user data is this placeholder's handle: clears the playing flag and
    // the sound handle; with reason 0 and none of the flags 0x40, 0x4000 and 0x800 it marks the placeholder finished.
    void CompletedCallback(void* sample, void* userData, int32_t reason) override;

    // ---- the virtuals --------------------------------------------------------------------------------------------------
    virtual void Release();   // 0x1001864c (48), slot +0x14: the destructor with flag -1 (the object is destroyed, not freed)
    // 0x10018874 (376), slot +0x18. Takes a reference on `container`, stores the event, copies a valid position, takes the volume
    // control index from the container's data, makes the extra data when the event carries a parameter id, SetFlagsFromEvent,
    // ChooseRandomSound(false), InitParamatisedControls, ChooseRandomSample(false) when there is no sample yet, and sets
    // mEndTime = sample length + now + 250 ms. A failure (no sound and none chosen, or no sample) releases the container and
    // answers -1.
    virtual TbError Init(CCatagoryContainer* container, void* event, const TbSoundPosition& position, float maxDistance);
    virtual TbError PlaySample(uint32_t now);   // 0x1001ba48 (36), slot +0x1c: -1
    virtual TbError Stop();                     // 0x1001a480 (148), slot +0x20: stops the sample, marks it finished
    virtual TbError Pause();                    // 0x1001a544 (152), slot +0x24
    virtual TbError Restart();                  // 0x1001a678 (156), slot +0x28
    // 0x10017aa4 (280), slot +0x2c: the base priority from the event, the distance field from the squared distance to the listener
    // (0xffff - low16(0.01 * (dx*dx + dz*dz))), 0xffff for a non-positional one, 0 without a listener. Answers the squared distance
    // (0.0 for the last two cases), a float.
    virtual float ChangeDistPriority();
    virtual TbError Update(const TbSoundPosition& position, const TbSoundPosition& velocity);   // 0x100182a4 (384), slot +0x30
    virtual void UpdateParameter(uint8_t id, uint32_t value);   // 0x10017f30 (200), slot +0x34
    virtual void UpdateParameterDepandants();                   // 0x10018194 (204), slot +0x38
    virtual bool ChooseRandomSound(bool rotate);                // 0x10019ca0 (264), slot +0x3c
    virtual CSampleData* ChooseRandomSample(bool rotate);       // 0x100198e8 (236), slot +0x40
    virtual bool GetParameterValue(uint32_t mask, uint32_t& value);   // 0x10018a44 (76), slot +0x44
    virtual bool FillSampleInfo(uint32_t now, TbSoundSampleInfo& info);   // 0x100191cc (816), slot +0x48
    virtual bool FillBasicSampleInfo(TbSoundSampleInfo& info);  // 0x10018f34 (220), slot +0x4c
    virtual int32_t GetPsuedoTime();                            // 0x10019060 (308), slot +0x50
    virtual uint32_t GetClassID();                              // 0x1001afe4 (8), slot +0x54: 0

    // ---- the rest ------------------------------------------------------------------------------------------------------
    static void SetSoundManager(TbSoundManager* manager);   // 0x100166b4 (12): stores r3 in mpSoundManager
    void SetPriorityAndVolume(uint32_t priority);           // 0x10017d8c (208)
    void AllocateExtraData();                               // 0x10018034 (92): operator new(0x20) and the constructor, once
    void SetFlagsFromEvent(float maxDistance);              // 0x100186b0 (164)
    void InitParamatisedControls();                         // 0x10018790 (160)
    int32_t GetRandomVolume();                              // 0x10018ad0 (292)
    int32_t GetRandomPitch();                               // 0x10018dc0 (316)
    uint32_t GetVolume();                                   // 0x10019798 (128)
    float GetMaxDistance();                                 // 0x1001984c (100)
    void GetNextSample(bool rotate, int8_t index, CSampleData*& sample);   // 0x10019a10 (120)
    void GetNextSound(bool rotate, int8_t index, CSoundData*& sound);      // 0x10019de4 (116)
    bool VolumeControlIsOn();                               // 0x10019b1c (220)
    void SetVolume(uint32_t volume);                        // 0x10019ea0 (20)
    void SetVolumeAndFrequency(uint32_t volume, float frequency);   // 0x10019ee8 (240)
    int32_t ConvertFrequencyToPitchIndex(float frequency);  // 0x1001a01c (208)
    void SetVolumeAndPitch(uint32_t volume, int32_t pitch); // 0x1001a1bc (32)
    bool ProcessPositionAndPriority();                      // 0x1001a21c (268)
    void InitFade();                                        // 0x1001a36c (44)
    void SetOutOfRange();                                   // 0x1001a3cc (124)
    bool FinishedPlaying(uint32_t now);                     // 0x1001a7b8 (56)
    // 0x1001a82c (1220): the class id (0 answers -1 before anything is written), the position, the four map ids (CMapReference;
    // a zero first id answers -1), the extra data, the bounding-rectangle flag byte, the list handle and the child. Every word is
    // written big-endian by LbFile_Write; the result of a write is not tested. `reserved` is the second argument (passed on to the
    // child's Save, never used).
    TbError Save(void* file, uint32_t reserved);
    void WriteMagicHeader(void* file);                      // 0x1001aecc (84): 0x12d591
    TbError Load(void* file, uint32_t reserved);            // 0x1001b020 (1512)
    bool ReadMagicHeader(void* file);                       // 0x1001b7ac (84)
    void ModifyPositionForBoundingRectangle();              // 0x1001b83c (448)
    void* GetHandle() const;                                // 0x1001c9b0 (8): mListHandle
    void* SetHandle(void* handle);                          // 0x1001ccd0 (12)

    static TbSoundManager* mpSoundManager;   // sound 0x1003f360 (exported as mpSoundManager__17CAudioPlaceHolder)
    static int32_t mRandomSeed;              // sound 0x1003f35c (mRandomSeed__17CAudioPlaceHolder): 0; only ever read

    CEventData* mEvent;                  // +4 the event (CEventData of the category: sounds, flags at +0x10, parameter id at +0x12)
    CSoundData* mSound;                  // +8 the chosen sound of the event
    CSampleData* mSample;                // +0xc the chosen sample of the sound
    TbSoundPosition mPosition;           // +0x10
    TbSoundPosition mVelocity;           // +0x1c
    uint32_t mEndTime;                   // +0x28 milliseconds: sample length + the clock at Init + 250
    CDistPriority mPriority;             // +0x2c
    void* mHandle;                       // +0x30 the sample handle from the manager (0 when not playing)
    CCatagoryContainer* mCatagory;       // +0x34 held (AddRef) by Init, released by the destructor
    void* mListHandle;                   // +0x38 the list's handle: low 16 bits the slot, high 16 bits the generation
    uint32_t mFlags;                     // +0x3c
    CAudioPlaceHolder* mNext;            // +0x40 the list's links
    CAudioPlaceHolder* mPrevious;        // +0x44
    uint32_t mPauseTime;                 // +0x48 the clock when paused, or FillSampleInfo's pseudo time (one word with two uses)
    float mMaxDistance;                  // +0x4c 0.0 = unset
    int8_t mLastSound;                   // +0x50 the sound picked last (-1)
    int8_t mLastSample;                  // +0x51 the sample picked last (-1)
    uint8_t mVolumeControlIndex;         // +0x52
    float mFade;                         // +0x54
    uint16_t mVolume;                    // +0x58
    int16_t mPitch;                      // +0x5a
    CAudioPlaceHolderExtraData* mExtra;  // +0x5c
};

// ---- Unit v51112 (src/sound/AudioPlaceHolder.cpp; sound program) -------------------------------
class CFormat;
class CAudioPlaceHolder;
namespace bfl::FileSystem {
class CFile;
}
// LbMaths_RangedRandomSlow__FRlUl 0x10018c88 (56): LbMaths_Random(seed) % range (unsigned; a zero range answers the random value).
uint32_t LbMaths_RangedRandomSlow(int32_t& seed, uint32_t range);
// The LB_MIN / LB_MAX instances (invented spellings; the comparison keeps the original's choice of reference on a tie).
const uint32_t& LbMaxULong(const uint32_t& a, const uint32_t& b);   // 0x100196b0 (24) LB_MAX<unsigned long,unsigned long>: a > b ? a : b
const float& LbMinFloat(const float& a, const float& b);            // 0x1001a17c (24) LB_MIN<float,float>: a < b ? a : b (fcmpo)
const uint32_t& LbMinUInt(const uint32_t& a, const uint32_t& b);    // 0x1001bdec (24) LB_MIN<unsigned int,unsigned int>
const int32_t& LbMaxLong(const int32_t& a, const int32_t& b);       // 0x1001e470 (24) LB_MAX<long,long>
const int32_t& LbMinInt(const int32_t& a, const int32_t& b);        // 0x100112f8 and 0x1000e7d8 (24 each) LB_MIN<int,int>
// TbRect::operator= 0x1001c31c (36): four word copies; r3 is left as dest.
TbRect* TbRectAssign(TbRect* dest, const TbRect* source);
// CFormat::operator const tWAVEFORMATEX&() 0x1000214c (48): FillWaveFormatEx, then this + 4.
void* CFormatToWaveFormatEx(CFormat* self);
// std::logf 0x1001a134 (40): frsp of the runtime's log (glue 0x1002f094).
namespace tbstd {
float logf(float x);
}
// CAutoArrayPtr<CAudioPlaceHolder*> out-of-line members: the constructor 0x1001bce8 (new[] of count * 4 bytes on the Mac), the
// destructor 0x1001bc44 (r4 = the hidden delete flag short) and operator CAudioPlaceHolder**() 0x1001c110.
template <class T> class CAutoArrayPtr;
CAutoArrayPtr<CAudioPlaceHolder*>* CAutoArrayPtrPlaceHolderCtor(CAutoArrayPtr<CAudioPlaceHolder*>* self, uint32_t count);
CAutoArrayPtr<CAudioPlaceHolder*>* CAutoArrayPtrPlaceHolderDtor(CAutoArrayPtr<CAudioPlaceHolder*>* self, int16_t deleteFlag);
CAudioPlaceHolder** CAutoArrayPtrPlaceHolderGet(const CAutoArrayPtr<CAudioPlaceHolder*>* self);
// bfl::FileSystem::CFile::Write 0x1001d9bc (64): -1 without an access object, else its Write (slot 0x10).
int32_t CFileWriteMain(bfl::FileSystem::CFile* self, const void* source, uint32_t size);
// The static cleanup objects of four sound files: the destructor words (stand-ins that answer self) and the registration records.
void* DestructStandInMPEG2(void* self, int16_t deleteFlag);      // 0x1000ed30
void* DestructStandInCallback(void* self, int16_t deleteFlag);   // 0x1000d608
void* DestructStandInCodecLoop(void* self, int16_t deleteFlag);  // 0x1000b590
// operator_new__FUlPv 0x1001d248 (8): placement new, answers where. The existing operator_new (in a file that cannot link into a test
// library) is the same code; this is the sound program's own spelling.
void* OperatorNewPlacementSound(uint32_t size, void* where);
// TbSoundSampleStore::CChannelIterator::operator++ 0x1000fed8 (40): the same code as the one in src/sound/TbSoundSampleStore.cpp (that file
// does not link into a test library); the next channel (the element size is the Mac's 0x60) or NULL at the end.
class TbSoundPipeChannel;
namespace TbSoundSampleStoreSound {
struct ChannelIterator {
    TbSoundPipeChannel* mCurrent;   // +0
    TbSoundPipeChannel* mEnd;       // +4
};
TbSoundPipeChannel* IteratorIncrement(ChannelIterator* self, uint32_t elementSize);
}  // namespace TbSoundSampleStoreSound
extern uint32_t gCS_TbSoundSampleWAVObject;        // 0x1003f2cc
extern uint32_t gCS_TbSoundSampleWAVNode;          // 0x1003f2d0
extern uint32_t gCS_TbSoundSampleMPEG2Object;      // 0x1003f260
extern uint32_t gCS_TbSoundSampleMPEG2Node;        // 0x1003f264
extern uint32_t gCS_TbSoundSampleCallbackObject;   // 0x1003ec3c
extern uint32_t gCS_TbSoundSampleCallbackNode;     // 0x1003ec40
extern uint32_t gCS_TbSoundCodecLoopObject;        // 0x1003ec18
extern uint32_t gCS_TbSoundCodecLoopNode;          // 0x1003ec1c
void __sinit_TbSoundSampleWAV_cpp();        // 0x1001144c (48)
void __sinit_TbSoundSampleMPEG2_cpp();      // 0x1000eccc (48)
void __sinit_TbSoundSampleCallback_cpp();   // 0x1000d5a4 (48)
void __sinit_TbSoundCodecLoop_cpp();        // 0x1000b530 (48)
void __sinit_AudioPlaceHolder_cpp();        // 0x1001bae4 (44): mRandomSeed = glue 0x1002eaac()
extern "C" int32_t MslSeedGlue1002eaac();   // runtime glue 0x1002eaac, no arguments read
