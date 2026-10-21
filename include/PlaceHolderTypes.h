// PlaceHolderTypes.h: the kinds of playing sound built on CAudioPlaceHolder (unit w79g3, "sound symbols shared", Code at 0x10000000,
// TOC r2 = 0x1003abf0; the library's file is CSprocketsModule.cpp; the code is in src/sound/PlaceHolderTypes.cpp, src/sound/PlaceHolderSentence.cpp, src/sound/ListSentence.cpp with the addresses).
// This header REPLACES the merged placeholder of units w79g1 (the CreateObject statics) and w79g4 (CPlaceHolderOneShot); every
// signature they compiled against is kept.
//
// Class tree (Mac vtables, each 0x58 or 0x5c bytes; r2 + the constructor's `addi r0, r2, X`; every object is 0x60 bytes and NONE
// adds a data member: a subclass is constructed into the list's slot of sizeof(CAudioPlaceHolder) bytes and its constructor leaves
// mListHandle, mNext and mPrevious alone, which the list set before CreateObject):
//   CAudioPlaceHolder 0x1003cc04 (w79g2)
//    +- CPlaceHolderOneShot 0x1003d384            one sample (PlaySample through the manager); class id 3
//    |    +- CPlaceHolderOneShotSentence 0x1003d438   children (w79g4's Elements) played one after another; class id 4; new slot +0x58 GetElement
//    |         +- CPlaceHolderOneShotBranchingSentence 0x1003cd00   GetElement makes branching Elements; class id 6
//    |    +- CPlaceHolderOneShotThrowaway 0x1003d4f0 (w79g4) <- ...SentenceElement 0x1003d0d8 <- ...BranchingSentenceElement 0x1003cff8
//    +- CPlaceHolderSentence 0x1003d580           chains samples through SoundCallback; class id 5
//         +- CPlaceHolderLinearSentence 0x1003d2f4    the sounds of an event in order; class id 2
//         +- CPlaceHolderBranchingSentence 0x1003d240 the sound follows a parameter byte; class id 1
//         +- CPlaceHolderSentenceShuffle 0x1003d18c   the sounds of an event in shuffled order; class id 10
//         +- CListSentence 0x1003ce00                 the events of an event list in order; class id 5 (the Sentence's)
// Overrides against the base's 20 slots (+8 .. +0x54):
//   OneShot   +0x10 destructor 0x1001df38, +0x1c PlaySample 0x10025a9c, +0x54 GetClassID 0x10025bf8
//   OneShotSentence  +0x10 0x10025d48, +0x1c PlaySample 0x10026168, +0x20 Stop 0x10025e28, +0x30 Update 0x10025f18, +0x34 UpdateParameter
//                    0x10026028, +0x38 UpdateParameterDepandants 0x100260c4, +0x54 GetClassID 0x100267a0, +0x58 GetElement 0x10026514
//   OneShotBranchingSentence  +0x10 0x1001e5e8, +0x54 GetClassID 0x1001e598, +0x58 GetElement 0x1001e4f8 (the rest is the Sentence's)
//   Sentence  +8 SoundCallback 0x100268e0, +0x10 0x1001e2e8, +0x1c PlaySample 0x10026aac, +0x4c FillBasicSampleInfo 0x100269f8, +0x54 GetClassID
//             0x1001f09c
//   Linear    +0x10 0x10025904, +0x3c ChooseRandomSound 0x100257f8, +0x54 GetClassID 0x100258bc
//   Branching +0x10 0x10025654, +0x34 UpdateParameter 0x10025380, +0x3c ChooseRandomSound 0x10025468, +0x54 GetClassID 0x1002560c
//   Shuffle   +0x10 0x10024db0, +0x3c ChooseRandomSound 0x100250b8, +0x54 GetClassID 0x1002515c
//   ListSentence  +0x10 0x1001ef68, +0x3c ChooseRandomSound 0x1001ed20 (GetClassID is the Sentence's)
// The Mac constructors set their vtable and call the base's; the destructors set their vtable, call the base's with flag 0 and delete
// when the flag is positive (here: the compiler's chain; `delete` is the deleting form).
//
// The extra data's +0x1c word (CAudioPlaceHolderExtraData::mParent, w79g2) holds a CListData* in a CListSentence and a CListShuffle* in
// a CPlaceHolderSentenceShuffle (the Elements of w79g4 keep their parent there); the child chain of a one-shot sentence is the extra
// data's mChild (+0x18): each child's own mChild is its successor.
//
// The CreateObject statics. The four-argument form: r3 = the category container, r4 = the slot NewPlaceHolderMemory made (the object
// is constructed in it), r5 = the sound data the mapper chose (a CEventData*), r6 = the event. It constructs through the one-argument
// form, calls the object's Init (virtual +0x18, hidden TbError buffer) with the container, the sound data, the event's position
// (+0xc) and the event's float (+0x18) and answers the object, or 0 when the construction or Init failed (a negative result). The
// list (w79g1) ignores the answer, so the host declares them void. The one-argument form (Load's) only constructs.
#pragma once
#include <cstdint>

#include "AudioPlaceHolder.h"
#include "TbMapStreamer.h"
#include "snd_containers.h"

class CCatagoryContainer;
class CPlaceHolderOneShotSentenceElement;   // w79g4 (include/PlaceHolderHelpers.h)

// The game's list of events (main program; unit w79g4 first declared its ListItem). It derives from the library's TbAudioEvent (the
// event type 1 the list tests), keeps the array of ListItem at +0x1c and its count at +0x20. Only these two members are read here.
class TbAudioEventList : public sound::TbAudioEvent {
public:
    // The pair of words CListSentence keeps per entry (the category and the event number, passed on to the sound library's
    // TbAudioEvent); 8 bytes on the Mac. Moved here from include/PlaceHolderHelpers.h (unit w79g4) so that CListData can hold the array.
    struct ListItem {
        ListItem() {}   // 0x1001ece4 (4): empty, the words stay as they were
        uint32_t mCategory;   // +0
        uint32_t mEvent;      // +4
    };
    ListItem* mItems;    // +0x1c
    uint32_t mCount;     // +0x20
};
static_assert(sizeof(TbAudioEventList::ListItem) == 8, "the Mac record is two words");

// ---- CPlaceHolderOneShot ---------------------------------------------------------------------------------------------------
class CPlaceHolderOneShot : public CAudioPlaceHolder {
public:
    CPlaceHolderOneShot();                       // 0x1001dfd0 (60): the base constructor, then the vtable 0x1003d384
    ~CPlaceHolderOneShot() override;             // 0x1001df38 (104), slot +0x10
    // 0x10025a9c (292), slot +0x1c: -1 without a manager or a sound; 0 when the placeholder is playing (flag 1) or finished (0x10);
    // else InitFade, flag 1, FillSampleInfo (virtual +0x48); a failure sets flag 0x10 and answers -1; else the manager's PlaySample
    // (mHandle receives the control); a refusal clears flag 1 and the handle; answers 0.
    TbError PlaySample(uint32_t now) override;
    uint32_t GetClassID() override;              // 0x10025bf8 (8), slot +0x54: 3
    // 0x100259a0 (156) and 0x1001e194 (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CPlaceHolderOneShotSentence ---------------------------------------------------------------------------------------------
// A sentence of children: each child (w79g4's CPlaceHolderOneShotSentenceElement, made by GetElement) plays one sample and, once its
// end time has passed, a new child is chained after it. The parent has no sample of its own.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
class CPlaceHolderOneShotSentence : public CPlaceHolderOneShot {
public:
    CPlaceHolderOneShotSentence();               // 0x1001deb8 (72): OneShot's constructor, vtable 0x1003d438, AllocateExtraData
    ~CPlaceHolderOneShotSentence() override;     // 0x10025d48 (168): deletes the children (their deleting destructor), then OneShot's
    TbError Stop() override;                     // 0x10025e28 (listing 28, 240 with its tail), slot +0x20: stops and deletes every child, clears the chain, the base's Stop
    TbError Update(const TbSoundPosition& position, const TbSoundPosition& velocity) override;   // 0x10025f18 (176), slot +0x30
    void UpdateParameter(uint8_t id, uint32_t value) override;   // 0x10026028 (84), slot +0x34
    void UpdateParameterDepandants() override;   // 0x100260c4 (84), slot +0x38: each child's, not the base's
    TbError PlaySample(uint32_t now) override;   // 0x10026168 (128), slot +0x1c
    uint32_t GetClassID() override;              // 0x100267a0 (8), slot +0x54: 4
    // 0x10026514 (88), slot +0x58: a new Element (0x60 bytes) of this sentence as its parent
    virtual CPlaceHolderOneShotSentenceElement* GetElement();
    CAudioPlaceHolder* GetFirstChild(uint32_t now);                   // 0x10026228 (124)
    void ProcessChildPlaceHolders(uint32_t now);                      // 0x100262e8 (196)
    bool AddNewElementTo(CAudioPlaceHolder* parent, uint32_t now);    // 0x100263fc (192)
    void RemoveCompleteChildPlaceHolders(uint32_t now);               // 0x100265ac (156)
    // 0x1002669c (156) and 0x1001e11c (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CPlaceHolderOneShotBranchingSentence ------------------------------------------------------------------------------------
class CPlaceHolderOneShotBranchingSentence : public CPlaceHolderOneShotSentence {
public:
    CPlaceHolderOneShotBranchingSentence();      // 0x1001de3c (56): the sentence's constructor, vtable 0x1003cd00
    ~CPlaceHolderOneShotBranchingSentence() override;   // 0x1001e5e8 (104)
    uint32_t GetClassID() override;              // 0x1001e598 (8), slot +0x54: 6
    // 0x1001e4f8 (88), slot +0x58: a new CPlaceHolderOneShotBranchingSentenceElement (it is an Element)
    CPlaceHolderOneShotSentenceElement* GetElement() override;
    // 0x10025c38 (156) and 0x1001ddbc (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CPlaceHolderSentence ------------------------------------------------------------------------------------------------------
// A sentence of samples: when a sample ends the manager calls SoundCallback, which picks the next sound and sample and starts it.
class CPlaceHolderSentence : public CAudioPlaceHolder {
public:
    CPlaceHolderSentence();                      // 0x1001e0ac (60): the base constructor, vtable 0x1003d580
    ~CPlaceHolderSentence() override;            // 0x1001e2e8 (104)
    // 0x100268e0 (216), slot +8: picks the next sound (virtual +0x3c, rotate true) and sample (+0x40), fills the basic info (+0x4c) and
    // hands the finished sample `sample` to the manager's ConstructSample with the info (the control is a dead local).
    void SoundCallback(void* sample, void* userData) override;
    // 0x10026aac (320), slot +0x1c: like CPlaceHolderOneShot's, and a loop-forever sentence asks for a loop count of 1
    TbError PlaySample(uint32_t now) override;
    bool FillBasicSampleInfo(TbSoundSampleInfo& info) override;   // 0x100269f8 (96), slot +0x4c: the base's, then the info's callback is this
    uint32_t GetClassID() override;              // 0x1001f09c (8), slot +0x54: 5
    // 0x10026c24 (156) and 0x1001e03c (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CPlaceHolderLinearSentence ----------------------------------------------------------------------------------------------
class CPlaceHolderLinearSentence : public CPlaceHolderSentence {
public:
    CPlaceHolderLinearSentence();                // 0x1001e278 (56): the Sentence's constructor, vtable 0x1003d2f4
    ~CPlaceHolderLinearSentence() override;      // 0x10025904 (100)
    // 0x100257f8 (128), slot +0x3c: the first sound of the event, then each next one; at the end it stops (false) unless the loop-forever
    // flag (4) wraps to the first. No event: true, nothing changed.
    bool ChooseRandomSound(bool rotate) override;
    uint32_t GetClassID() override;              // 0x100258bc (8), slot +0x54: 2
    // 0x100256f4 (156) and 0x1001e204 (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CPlaceHolderBranchingSentence -------------------------------------------------------------------------------------------
class CPlaceHolderBranchingSentence : public CPlaceHolderSentence {
public:
    CPlaceHolderBranchingSentence();             // 0x1001e3fc (56): the Sentence's constructor, vtable 0x1003d240
    ~CPlaceHolderBranchingSentence() override;   // 0x10025654 (100)
    // 0x10025380 (160), slot +0x34: the base's, then (flag 0x40 and the parameter byte not 0x7f) a new sound (virtual +0x3c, rotate true) and,
    // when that worked and a sound is set, a new sample (the base's ChooseRandomSample, rotate true; stored before it is tested); a
    // sample clears flag 0x40
    void UpdateParameter(uint8_t id, uint32_t value) override;
    bool ChooseRandomSound(bool rotate) override;   // 0x10025468 (84), slot +0x3c: AssignSoundToNextBranch(the parameter byte); InitParamatisedControls on success
    uint32_t GetClassID() override;              // 0x1002560c (8), slot +0x54: 1
    // 0x100251a4 (396): the same branch walk as w79g4's branching Element, on this placeholder's own sound; `parameter` is the byte
    bool AssignSoundToNextBranch(uint32_t parameter);
    // 0x10025504 (156) and 0x1001e384 (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CPlaceHolderSentenceShuffle ---------------------------------------------------------------------------------------------
class CPlaceHolderSentenceShuffle : public CPlaceHolderSentence {
public:
    // The order of an event's sounds: the sound records of the event are SHUFFLED IN PLACE (42 bytes each) and handed out one by one.
    // 0x10 bytes on the Mac; here the event pointer is native.
    struct CListShuffle {
        explicit CListShuffle(CEventData* event);   // 0x10024910 (112): index 0, steps 1 and 2, toggle 0, seed = the clock, Init
        void Init(CEventData* event);               // 0x100249d4 (40): the event, Shuffle
        void Shuffle();                             // 0x10024a50 (448)
        // 0x10024c5c (116): the next sound record; at the end (index >= the sound count) a shuffle when `reshuffle`, else NULL
        CSoundData* GetNextSound(uint32_t reshuffle);
        CEventData* mEvent;     // +0
        uint32_t mSeed;         // +4 the clock at construction (a copy of it feeds every shuffle's random draw)
        uint32_t mIndex;        // +8
        uint8_t mStep[2];       // +0xc, +0xd: 1 and 2, the strides of the second cursor
        uint8_t mToggle;        // +0xe: which stride is next
    };
    CPlaceHolderSentenceShuffle();               // 0x10024d24 (84): the Sentence's constructor, vtable 0x1003d18c, AllocateExtraData, +0x1c cleared
    ~CPlaceHolderSentenceShuffle() override;     // 0x10024db0 (124): frees the shuffle (plain delete)
    void SetListInfo(CEventData* event);         // 0x10024fe0 (140): makes the shuffle on first use, else re-initialises it when the event changed
    bool ChooseRandomSound(bool rotate) override;   // 0x100250b8 (92), slot +0x3c: SetListInfo(mEvent), the next sound (reshuffle), true when there is one
    uint32_t GetClassID() override;              // 0x1002515c (8), slot +0x54: 10
    // 0x10024e64 (156) and 0x10024f68 (52)
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    static void CreateObject(CAudioPlaceHolder* slot);
};

// ---- CListSentence -----------------------------------------------------------------------------------------------------------
class CListSentence : public CPlaceHolderSentence {
public:
    // The list's copy of the event list's entries and the cursor: 0xc bytes on the Mac (count, index, the array holder).
    struct CListData {
        explicit CListData(const TbAudioEventList& list);   // 0x1001ea2c (116): count, index 0, the array, a copy of the entries
        ~CListData();                                       // 0x1001f008 (92): frees the array
        uint32_t mCount;     // +0
        uint32_t mIndex;     // +4
        CAutoArrayPtr<TbAudioEventList::ListItem> mItems;   // +8
    };
    explicit CListSentence(const TbAudioEventList* list);   // 0x1001e974 (120): the Sentence's constructor, vtable 0x1003ce00, extra data, the list data
    ~CListSentence() override;                   // 0x1001ef68 (116): deletes the list data, then the Sentence's
    // 0x1001ed20 (268), slot +0x3c: loop flag 2; the next entry's event through the category container's mapper; the base's choice
    bool ChooseRandomSound(bool rotate) override;
    // 0x1001e7e8 (144): the one-argument form below with `event` as the event list, then Init as the other classes do
    static void CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event);
    // 0x1001e8e4 (72): constructs a CListSentence in the slot from the event list (NOT Load's form: it takes the list)
    static void CreateObject(CAudioPlaceHolder* slot, const TbAudioEventList* list);
};
#pragma clang diagnostic pop
