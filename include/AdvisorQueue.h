// Advisor.cpp (main app): CAdvisor's request queue. Offsets are the Mac's.
#pragma once
#include <cstddef>
#include <cstdint>
#include "AdvisorScoring.h"
#include "TimeSig.h"
#include "Serialise.h"
#include "analyser.h"   // tag_IDX_TYPE (unit w101)

// 0x18 bytes. Names from Serialise; queue copies include both padding regions.
// Play, PlayWithoutScore and Serialise remain untranslated.
struct IDXTYPEUniqueNameThingId;   // (unit w101)
struct CAdvisorRequest {
    // Unit w103 (src/AdvisorRequest.cpp): 0x1000deb4, 208 bytes. The thing id rides in the high half of one word; `score` below 0
    // is looked up (CAdvisorScoring::GetScoreFor) and above or equal is taken as it is.
    CAdvisorRequest() { mValid = 0; }   // 0x1000a0b4 (12 bytes; inline here so every unit that holds a request builds without the source)
    CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                    int32_t responseNo, bool overrideOnlyOnce, int32_t score);   // 0x1000deb4
    // 0x1000de30 (68 bytes): copies the eight fields and leaves both padding regions (+2..+3 and +0x17) alone.
    CAdvisorRequest(const CAdvisorRequest& other);
    CAdvisorRequest& operator=(const CAdvisorRequest&) = default;   // the queue's plain word copies (all 0x18 bytes, padding too)
    uint8_t Valid() const { return mValid; } // 0x1000e894
    // Unit w124: 0x1000dff4 and 0x1000e340, untranslated. Both answer whether the line was played and set its length.
    int32_t Play(uint32_t* length, int32_t responseNo);               // 0x1000dff4
    int32_t PlayWithoutScore(uint32_t* length, int32_t responseNo);   // 0x1000e340
    void SetScore(int32_t score);            // 0x1000e8cc: the score is now an explicit one
    // Unit v5223: 0x1000e568 (756 bytes, untranslated), called by CAdvisor::Serialise.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);

    uint16_t mThingId;           // 0x00, IDXTYPEUniqueNameThingId
    uint8_t mPad02[2];           // 0x02
    uint32_t mSecondary;         // 0x04
    int32_t mResponseNo;         // 0x08
    AdvisorMessage mMessage;    // 0x0c
    int32_t mScore;              // 0x10
    uint8_t mValid;              // 0x14, nonzero also means the score is usable
    uint8_t mOverrideOnlyOnce;   // 0x15, the force argument of CanIQueueThis
    uint8_t mScoreOverridden;    // 0x16
    uint8_t mPad17;              // 0x17
};
static_assert(sizeof(CAdvisorRequest) == 0x18, "advisor request stride");

struct AdvisorMessageState {
    CTimeSig mLastSaid;          // 0x00
    int32_t mResponseIndex;      // 0x04
    uint8_t mSaidOnce;           // 0x08
    uint8_t mPad09[3];           // 0x09
    int32_t mKillCount;          // 0x0c
};
static_assert(sizeof(AdvisorMessageState) == 0x10, "advisor message state stride");

enum EventType : int32_t;   // include/CMsgEvent.h (unit v5631: CAdvisor::HandleEvent)

// Host object; the known fields have the Mac's layout, with no native pointers.
class CAdvisor {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x1000a468 (1,072 bytes)

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x1000a8c8
    void FinalDestroy();   // 0x1000a0ec (src/gameplay_leaves.cpp, unit v5L07)
    CAdvisor();   // 0x10009f9c (172 bytes, src/ThingCtors.cpp, unit w122a; 0x16f0 bytes on the Mac; CWorld::SetupNewLevel makes the advisor)
    int32_t QueueRequest(const CAdvisorRequest& request); // 0x1000ae48
    void ReceiveMessage(const CMsg& message);              // 0x1000c874 (5,440 bytes, untranslated; CThing::ReceiveMessage kind 11)
    // 0x1000b048 (unit w103): drops the message from the last tutorial request and the queue and, if he is saying it, cuts him short.
    // The answer is 1 when a queued request or the line being said was cancelled, else 0.
    int32_t CancelMessage(AdvisorMessage message);
    // 0x1000b1c4 (unit w103): queues the last tutorial request again (F1), scored by the balance table's HelpMessage ContextHelpScore;
    // 0 when there is none, else 1 (also when nothing was queued).
    int32_t PlayLastTutorialMessage();
    // 0x1000c760 (unit w103): the message being said, or kAdvisorNoMessage once the clock has passed mPlayStartTime + mPlayDuration.
    AdvisorMessage GetTypePlaying();
    bool CanIQueueThis(AdvisorMessage message, bool force) const; // 0x1000b3d4
    int32_t FindFirstInvalidSlotInQueue();                // 0x1000b740
    int32_t FindLowestScoringSlotInQueue();               // 0x1000b7e8
    // Unit w124 (src/Advisor_turn.cpp): untranslated callees of UpdateTurn.
    bool IsPlaying();                                     // 0x1000c6e0
    void SetPlayingFor(uint32_t length);                  // 0x1000c7fc
    int32_t FindHighestScoringSlotInQueue();              // 0x1000b900
    CTimeSig& Clock() { return *reinterpret_cast<CTimeSig*>(&mF0C[0]); }   // the clock word at +0xc

    // Unit v5223 (src/AdvisorQueue_CAdvisor_HandleStaffInfo.cpp): 0x1000c40c, 660 bytes. Builds the request for the staff type and
    // situation and queues it like QueueRequest does.
    void HandleStaffInfo(const class CMsgStaffInfo& info);

    int32_t Init();   // 0x1000a13c (664 bytes, untranslated): subscribes to seven message types, clears +0x10, +0xd4, +0xdc and +0xd8; answers 1

    // Unit v5631 (src/AdvisorQueue_CAdvisor_ReceiveMessage.cpp calls it): 0x1000ba18 (2,496 bytes, untranslated), the event message's word.
    void HandleEvent(EventType event);

    CThing mBase;                                        // 0x00  model 0xb (unit w122a: was mF00[0x14])
    uint8_t mF0C[8];                                     // 0x0c  two words cleared by the constructor (+0xc, +0x10); +0x10 is also cleared by Init
    CAdvisorRequest mQueue[8];                           // 0x14
    uint8_t mFD4[0xd8 - 0xd4];                          // 0xd4
    uint32_t mPlayStartTime;                             // 0xd8, unsigned long in Serialise; CAdvisor::SetPlayingFor 0x1000c7fc sets both (names guessed)
    uint32_t mPlayDuration;                              // 0xdc, CancelMessage zeroes it; IsPlaying 0x1000c6e0 and GetTypePlaying read both
    AdvisorMessageState mMessageState[kAdvisorMessageCount]; // 0xe0
    CAdvisorRequest mLastTutorialRequest;                // 0x16d0
    uint8_t mF16E8[0x16ec - 0x16e8];                    // 0x16e8, untouched
    AdvisorMessage mTypePlaying;                         // 0x16ec, the message being said (valid while the clock is before the end)
};
static_assert(sizeof(CAdvisor) == 0x16f0, "advisor object size");
static_assert(offsetof(CAdvisor, mQueue) == 0x14, "the advisor's queue at 0x14");

// Unit v5223: the save templates CAdvisor::Serialise uses (AdvisorHistory on the Mac is AdvisorMessageState here).
template<> int32_t SAVEVAR<AdvisorMessage>(void*, AdvisorMessage*, const char*, int32_t);   // 0x1000f820
template<> int32_t LOADVAR<AdvisorMessage>(void*, AdvisorMessage*, const char*, int32_t);   // 0x1000f250
// Unit v51069 (src/AdvisorScoring_sinit_Advisor_cpp.cpp): Advisor.cpp's static initialiser 0x1000fff0 (a global wide string).
extern unsigned char gAdvisorMsgStringStorage[];   // 0x102b3aa8 the string (TbDynamicStringTemplate<char16_t>)
extern unsigned char gAdvisorMsgStringNode[8];     // 0x102b3abc its registration node
extern const char16_t gAdvisorMsgText[];           // 0x1023c316 its text (only "gm" of it is known)
void __sinit_Advisor_cpp();                        // 0x1000fff0
template<> int32_t SAVEVARSTRUCT<AdvisorMessageState>(void*, AdvisorMessageState*, const char*, int32_t);   // 0x1000f96c
template<> int32_t LOADVARSTRUCT<AdvisorMessageState>(void*, AdvisorMessageState*, const char*, int32_t);   // 0x1000ecb4

// Shared scoring singleton: Mac object 0x102b3ac8, guard byte 0x102b4100.
CAdvisorScoring& TheScoring();

// Unit v51003: the message CAdvisorRequest::Play builds on the stack (CMsgTag on the Mac, constructor 0x10151f7c, src/MsgTag.cpp).
// Its fields are in the Mac's order, which is CMsgTagShow's (include/ThingReceivers.h), the view CTagSystem::ReceiveMessage reads
// a type 0x18 message through: the constructor's fourth argument goes to +0x14 and its fifth to +0x10.
#include "CMsg.h"
class CMsgTagObj : public CMsg {
public:
    CMsgTagObj(int32_t tag, uint32_t thingWord, uint32_t secondary, int32_t extra);   // 0x10151f7c
    int32_t GetType() const override { return 0x18; }   // 0x1000e90c
    int32_t mTag;           // 0x08
    uint16_t mThing;        // 0x0c  the high half of the request's first word (the Mac stores only that halfword)
    int32_t mTime;          // 0x10  the constructor's `extra` (Play passes -1: the tag's own time)
    uint32_t mValue;        // 0x14  the constructor's `secondary`
};
extern uint16_t gAdvisorNullThingId;   // 0x1027de64: the "no thing" id Play compares the checked one with
