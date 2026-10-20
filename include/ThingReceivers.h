// The thing classes whose ReceiveMessage CThing::ReceiveMessage (0x1012e470) hands a message to, by the kind byte at +2 (unit w107).
// PLACEHOLDERS: each class is only a name with its one untranslated member (address and size from the inventory), so the call
// compiles and the launcher gets a stand-in for it. The unit that translates a class moves it into its own header, replaces the
// placeholder here and keeps the member's signature. CAdvisor (kind 11) is in AdvisorQueue.h and CParkAnalyser (kind 13) in
// analyser.h.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "ThingParts.h"
#include "ThingParts2.h"
#include "History.h"
#include "analyser.h"   // CThing: the seven singleton things below hold one as their first member (unit w122a)

class CMsg;
class TbTimeDiff;
struct RideInfo;
class ObjectControl;   // unit w210: CObject::GetControl's answer (include/ObjectControl.h)
struct IDXTYPEUniqueNameThingId;

// The constructors below are declared for CWorld::SetupNewLevel (unit w117), which builds each singleton thing as raw memory of the
// Mac's size plus a placement new of the placeholder; none is translated yet (the launcher gets a stand-in for each).
struct IDXTYPEUniqueNameMapId;
struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;

// Unit w149: the index type of CPeep::DrawMySpriteAt (the values used are 2 and 3). Unit w177 (src/DelayedScript.cpp): the rest of the class, a handle to a
// sprite script and the one waiting to be switched to. ScriptIdx stays opaque: its values are the thirteen indices of the table at 0x102a08fc.
class CDelayedScript {
public:
    enum ScriptIdx : int32_t;
    CDelayedScript();   // 0x100fa898 (20 bytes): zeroes the three words
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100fa8d8 (356 bytes)
    void SetSpriteScript(ScriptIdx script);   // 0x100faa74 (8 bytes): the script to switch to at the next ProcessSpriteScripts
    bool IsRunningThisScript(ScriptIdx script);   // 0x100faad0 (116 bytes)
    void SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index);   // 0x100fab9c (88 bytes)
    void KillSpriteScript();   // 0x100fac4c (60 bytes)
    void ProcessSpriteScripts();   // 0x100facc0 (116 bytes)
    void SetServiceInterval(float rate);   // 0x100fad70 (56 bytes): 62 / rate, clamped to 0..250

    int32_t mSpriteScript;          // 0x00 "mSpriteScript": the script handle (0 for none)
    ScriptIdx mNextAnim;            // 0x04 "mNextAnim": the script to switch to (0 for none)
    int32_t mNextServiceInterval;   // 0x08 "mNextServiceInterval": the service interval to set (0 for none)
};
static_assert(sizeof(CDelayedScript) == 0xc, "CDelayedScript is 0xc bytes");
static_assert(offsetof(CDelayedScript, mSpriteScript) == 0x0, "CDelayedScript::mSpriteScript at 0");
static_assert(offsetof(CDelayedScript, mNextAnim) == 0x4, "CDelayedScript::mNextAnim at 4");
static_assert(offsetof(CDelayedScript, mNextServiceInterval) == 0x8, "CDelayedScript::mNextServiceInterval at 8");

// Unit w184: a position in a cell as the handyman's destination calls take it (the cell's x or y, then the offset in the cell in 13ths of a step; names
// invented). Two bytes, passed in the HIGH halfword of a register on the Mac.
struct MapPos {
    uint8_t mCell;
    uint8_t mOffset;
};
static_assert(sizeof(MapPos) == 2, "MapPos is two bytes");
// Unit w184: the thought bubbles of a staff member (the bytes at +0x30 of the staff classes), by their one member the handyman's UpdateTurn calls.
class CThoughts {
public:
    void ProcessThoughtBubbles();   // 0x1012f058 (72 bytes, untranslated)
    // Unit w189 (untranslated): the staff member's SetState drops the bubble (state 3 and 7).
    void RemoveThoughtBubble();   // 0x1012f35c
    // Unit w191 (untranslated): the staff member's bubbles (types 0x12 to 0x16 are the ones it shows).
    enum ThoughtType : int32_t {};
    void ShowThoughtBubble(ThoughtType type, bool always);   // 0x1012f0d8 (568 bytes)
    // Unit w207 (untranslated): a stand-in called by CWorld::Load (the person and staff blocks it builds in place call it at +0x30).
    CThoughts();   // 0x1012ec88 (152 bytes)
    // Unit v5048 (untranslated stand-in): the n-th most recent event (0 the latest).
    const struct CThoughtEvent* GetRecentEvent(uint32_t index) const;   // 0x1012f4ec (136 bytes)
    // Unit v5052 (untranslated, 168 bytes): notes an event about a thing (the id is the HIGH halfword of the Mac's register).
    enum EventType : int32_t {};
    void RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing);   // 0x1012f3d4
    // Unit v5067: an entry of the action history (SAVEVARSTRUCT<CThoughts::EventHistType> 0x1012f9a0, src/TrackCell_SAVEVARSTRUCT.cpp):
    // two 16-bit fields, each swapped on saving. Field names invented.
    struct EventHistType {
        uint16_t mFirst;    // 0x00
        uint16_t mSecond;   // 0x02
    };
    // Unit v5298 (untranslated stand-in): called by CPeep::FinalDestroy on the bytes at +0x30.
    void FinalDestroy();   // 0x1012ed84 (48 bytes)
    // Unit v5533 (untranslated stand-in): called by CPeep::Serialise on the bytes at +0x30.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x1012ede4 (580 bytes)
};
static_assert(sizeof(CThoughts::EventHistType) == 4, "EventHistType is four bytes");
template<class Value> int32_t SAVEVARSTRUCT(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t SAVEVARSTRUCT<CThoughts::EventHistType>(void*, CThoughts::EventHistType*, const char*, int32_t);   // 0x1012f9a0, 260 B
// Unit v5643: SAVEVAR<CThoughts::ThoughtType> 0x1012f844 (260 B, src/TrackCell_SAVEVARSTRUCT_SAVEVAR_4.cpp): one swapped word.
template<class Value> int32_t SAVEVAR(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t SAVEVAR<CThoughts::ThoughtType>(void*, CThoughts::ThoughtType*, const char*, int32_t);   // 0x1012f844, 260 B
// Unit v5736: LOADVAR<CThoughts::ThoughtType> 0x1012f700 (236 B, src/TrackCell_SAVEVARSTRUCT_SAVEVAR_4_LOADVAR_2.cpp): one word read and swapped.
template<class Value> int32_t LOADVAR(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t LOADVAR<CThoughts::ThoughtType>(void*, CThoughts::ThoughtType*, const char*, int32_t);   // 0x1012f700, 236 B

// The base of CPerson and of the staff classes (unit w121): only the two members the world's loops call. A thing is reached with
// reinterpret_cast, as for the receivers below.
class CPeep {
public:
    void UpdatePositionInEngine(float step);   // 0x101149cc (1,448 bytes, untranslated)
    int32_t IsInsidePark() const;   // 0x10115a80 (212 bytes, untranslated; unit w190 calls it): true on a path, queue, entrance, exit or grass cell
    // Unit w178 (untranslated): the members the entertainer's frame code calls.
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> LastCellVisited();   // 0x10113814 (48 bytes, untranslated; a two-byte struct through a hidden pointer)
    int32_t PeepWalks();   // 0x10114fa8 (636 bytes, untranslated)
    void PlaySound(int32_t category, int32_t sound);   // 0x10115b80 (148 bytes, untranslated)
    int32_t SetRandomDestination();   // 0x10113870 (3,456 bytes, untranslated)
    // Unit w182 (untranslated): the guard's walk to a cell; answers 0 when there is no way.
    int32_t SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x10115400 (160 bytes, untranslated)
    // Unit w184 (untranslated): the handyman's walk to a litter spot (an x and a y position) and his jump to the spot on arrival.
    int32_t SetDest(MapPos x, MapPos y);   // 0x101154f0 (152 bytes)
    void Teleport(MapPos x, MapPos y);   // 0x101159d0 (124 bytes)
    // Unit w149 (untranslated) 0x10114624 (232 bytes, Peep.cpp): draws the peep's sprite at a place; BOUNCE_Tick calls it with 2 (bouncing, frame
    // elapsed / 500) or 3 (still, frame peep % 7).
    void DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script);
    void HideMySprite();   // 0x10114758 (100 bytes, Peep.cpp, untranslated; DrawPeep calls it)
    // Unit w189 (untranslated; Peep.cpp): the base the staff member builds and calls (CStaff::CStaff, FinalDestroy, Serialise, UpdateTurn, SetState).
    CPeep(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model);   // 0x10112e18
    void FinalDestroy();   // 0x101130ac
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10113138
    void UpdateTurn();   // 0x101158c8
    void SetStandScript();   // 0x1011524c
    void SpawnParticleEffect(int32_t effect);   // 0x101152a4 (unit w190, untranslated): SackMe asks for effect 0x13
    // Unit v5048 (src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded.cpp). The peep is reached by offset: the cell x/y bytes at +4/+6, the script handle
    // at +0xc, the CThoughts at +0x30 and the turn stamp it became stranded at (a word) at +0x198.
    bool IsLocalAreaUnchangedSinceBecomingStranded();   // 0x101156fc (388 bytes)
    void ReportAllEvents() const;                       // 0x10115c3c (1,340 bytes)
    // Unit v5281 (src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound.cpp): the stranded stamp at +0x198.
    bool IsStranded();                                  // 0x1011566c (104 bytes)
    void SetStranded(int32_t stranded);                 // 0x101155bc (136 bytes)
    // Unit v5298 (src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_Dtor_CPeep.cpp): the destructor, 0x10116d18 (104 bytes). Runs
    // Vehicle::~Vehicle on the bytes at +0xd4; the Mac's delete-storage flag (r4) is not modelled: the host never frees.
    ~CPeep();
    // Unit v5533 (src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_GetInterpolatedPeepPosition.cpp): the position between the
    // last (+0x190, +0x194) and the current pilot position at share `t` (clamped to -1..2 when `ranged` is 0, to 0..1 otherwise); x and z come out in
    // tenths of 1/65536, y is 0, `stamp` is the word at +0x1c.
    void GetInterpolatedPeepPosition(float t, int32_t ranged, float& x, float& y, float& z, uint32_t& stamp);   // 0x101147e8 (416 bytes)
};
// Unit v5048: one thought event as CThoughts::GetRecentEvent answers it (the type, then the thing the event is about; names invented).
struct CThoughtEvent {
    uint16_t mType;   // 0x00
    uint16_t mThing;  // 0x02
};
// Unit v5048 (names invented): the two halfword constants (both 0 at start; initialised data) and the nine (x, y) steps of the neighbourhood table.
extern uint16_t gStrandedQueueEnd;    // 0x102a0e96 the cell id that ends a walk back along the queue
extern uint16_t gNoThoughtThing;      // 0x102a0e98 the thing id that means "no thing"
struct CNeighbourStep {
    int32_t mDx;   // 0x00
    int32_t mDy;   // 0x04
};
extern CNeighbourStep gNeighbourSteps[9];   // 0x1034c3ac (BSS)

class CObject;   // unit v5238: TooPricey and ExtraExcitementEffects take one
class CPerson {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x101170b4 (3,456 bytes)
    // Unit w176: 0x10116634 (1488 bytes, untranslated): CArrival::CreatePerson builds one at the bus stop cell (the id is the HIGH halfword of the Mac's register).
    explicit CPerson(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    // Unit w132 (untranslated callee of CObject::UpdateTurn; ForceRideAdmittance 0x1011df70 and DismissFromQueue 0x1011f1b8 are declared by their Mac names in the sources, as src/Object.cpp does):
    int32_t InQueueingState() const;   // 0x10120b2c
    // Unit w182 (untranslated): the guard catches a kid, carries him to the entrance and lets him go.
    void GetBagged();   // 0x1011b508 (100 bytes)
    void GetDebagged(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x1011b594 (280 bytes)
    int32_t IsCatchable() const;   // 0x1011b700 (104 bytes)

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x1011f7d0
    void ModelState();   // 0x1011fdb4
    void FinalDestroy();   // 0x10116da4
    void ReceiveMessage(const CMsg& message);   // 0x10116e24, 604 bytes, kind 1

    // Unit v5006 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark.cpp). The person's state word is not declared here (SetState is untranslated).
    enum PersonState : int32_t {};
    void SetState(PersonState state);   // 0x10120354 (1,944 bytes, untranslated)
    void ProcessStateWaitingToCrossRoadOutOfPark();   // 0x1011e42c (480 bytes)
    void ProcessStateLeavingPark();                   // 0x1011e2e8 (268 bytes): +0x1bc, +0x19c (float), +0x188 (int32) are read or written by offset
    void ProcessStateWaitingForBusOuttaHere();        // 0x1011e654 (852 bytes)

    // Unit v5052 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_CPerson.cpp): the constructor and ReceiveMessage above; the
    // fields are reached by Mac offset (+0x19c float, +0x1a0, +0x1bc, +0x1c0 byte, +0x1d4, +0x1d8, +0x1dc and +0x1de halfwords,
    // +0x1e0 and +0x1e8 four halfwords each, +0x204, +0x220 state, +0x228 and +0x22a halfwords; the CPeep base is 0x19c bytes).
    enum Amount : int32_t {};
    void LowerHappiness(Amount amount);   // 0x1011b384 (240 bytes, untranslated)
    void DismissFromQueue();              // 0x1011f1b8 (272 bytes, untranslated)
    // Unit v5078: 0x1011962c (68 bytes, untranslated): the game turns the person has been in the park.
    uint32_t GetTimeInPark() const;
    // Unit v5098 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_ProcessStateIdle.cpp). The person is reached by offset (+0x4 and +0x6 the
    // cell bytes, +0x1c the angle, +0x30 the CThoughts, +0x188, +0x19c / +0x1b0 / +0x1b4 floats, +0x1bc, +0x1c0 byte, +0x1dc halfword, +0x1fc, +0x208,
    // +0x210, +0x220 state). The ids come back through a hidden pointer (one halfword).
    void ProcessStateIdle();   // 0x1011baa8 (2,796 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindNearestReachableLitterBin(uint32_t range);   // 0x1011e9ec (408 bytes, untranslated)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindNearestFireworks();      // 0x1011ed54 (396 bytes, untranslated)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindNearestEntertainer();    // 0x1011ebc4 (344 bytes, untranslated)
    void MakeBalloonPop();     // 0x1011b11c (76 bytes, untranslated)
    void MakeMajorDecision();  // 0x10118430 (408 bytes, untranslated)
    void SetSpotAnim(CDelayedScript::ScriptIdx script);   // 0x10117e64 (212 bytes, untranslated)
    // Unit v5217 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_UpdateTurn.cpp): UpdateTurn and FindNearestFireworks are above.
    void DoSomeThinking();           // 0x10117fd4 (872 bytes, untranslated)
    void UpdateTemporaryHistory();   // 0x10119b68 (108 bytes, untranslated)
    // Unit v5238 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_TooPricey.cpp). Fields by Mac offset: +0x19c, +0x1a4, +0x1a8, +0x1b0 floats, +0x1a0 word.
    uint8_t GetPreferredExcitement() const;                  // 0x1011b4b4 (28 bytes, untranslated; the listing masks the answer with 0xff)
    bool TooPricey(CObject* object) const;                   // 0x10119eec (1,092 bytes): tells the park analyser what it thinks of the price
    void ExtraExcitementEffects(const CObject* object);      // 0x10119c0c (672 bytes): the ride's excitement moves +0x19c and +0x1b0
    // Unit v5245 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin.cpp). Fields by Mac offset: +0x1dc ride, +0x1de minor destination halfwords.
    void ProcessStateBeingUnloaded();   // 0x1011e0e4 (460 bytes)
    void ProcessStateBeingLoaded();     // 0x1011dcc0 (632 bytes)
    int32_t FindQueueDestination();     // 0x1011ef14 (368 bytes, untranslated)
    // Unit v5293 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged.cpp). +0x19c happiness float,
    // +0x210 balloon script handle, +0x218 and +0x21c balloon floats, +0x220 state.
    void RaiseHappiness(Amount amount);                       // 0x1011b258 (236 bytes)
    uint32_t GetHappinessAmount(Amount amount) const;         // 0x1011b198 (124 bytes)
    void DrawBalloon(float x, float y, float z, int32_t mode);   // 0x1011b084 (104 bytes)
    void LosingMods(CObject* object);                         // 0x1011b004 (76 bytes)
    // Unit v5150 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_DismissFromQueue.cpp): the ride is at +0x1dc (halfword), the ride's queue place at +0x1f1 (byte).
    void EvaluateQueueEdit();        // 0x1011f2f8 (636 bytes)
    int32_t ExitRide();              // 0x1011f5a4 (516 bytes)
    void UseObject(class CObject* object); // 0x101196a0 (864 bytes, untranslated)
    // Unit v5529 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_ProcessStateWalksToLitterBin.cpp).
    // Fields by Mac offset: +0x4 and +0x6 cell bytes, +0x1c angle, +0x19c / +0x1ac / +0x1b4 floats, +0x1dc ride, +0x1f1 queue place (byte),
    // +0x1f4 queue patience, +0x1f8 admit flag, +0x1fc and +0x208 turn stamps, +0x228 halfword.
    void ProcessStateWalksToLitterBin();           // 0x1011d400 (432 bytes)
    void ProcessStateStandingInQueue();            // 0x1011d5ec (1,484 bytes)
    void ProcessStateWaitingForScriptToAdmitMe();  // 0x1011dfc8 (216 bytes)
    void LeaveQueue();                             // 0x1011dbf4 (160 bytes, untranslated)
    // Unit v5543 (src/CPerson_..._CPerson_ModelState.cpp): ModelState, GetDebagged and MakeMajorDecision are declared above. Fields by Mac offset:
    // +0x1c angle word, +0x20 and +0x24 script words, +0x188 word, +0x1d8 person index, +0x1dc ride, +0x1f8 admit flag, +0x208 turn stamp, +0x220 state.
    void OnAdmittance();   // 0x1011f114 (120 bytes)
    uint32_t CalculateOptionScore(CObject* object);   // 0x101185f8 (2,612 bytes, untranslated)
    void EndSpotAnim();   // 0x10117f80 (40 bytes, untranslated)
    void ProcessStatePayingAtBooth();   // 0x1011cce4 (508 bytes, untranslated)
    void ProcessStateWaitingForParkToOpen();   // 0x1011c9f8 (684 bytes, untranslated)
    void ProcessStateWalkingToTicketBooths();   // 0x1011c88c (300 bytes, untranslated)
    void ProcessStateWalksToMajor();   // 0x1011cf18 (1,200 bytes, untranslated)
    // Unit v5563: callees of ProcessStateWalksToMajor, untranslated (the ride and the queue place by Mac offset: +0x1dc, +0x1fc, +0x20c, +0x2c byte).
    int32_t ScoreOnCloserInspection(CObject* object);   // 0x10119068 (180 bytes)
    void AddRideToRecentTemporaryHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride);   // 0x10119ac8 (52 bytes)
    void LookForMinorDest();   // 0x1011915c (1,184 bytes)
    // Unit v5671 (src/..._CPerson_WinningMods.cpp): a person has used a ride or shop; fields by Mac offset: +0x20 and +0x24 costume words, +0x30 the CThoughts,
    // +0xc2 and +0xc4 halfwords, +0x19c / +0x1a4 / +0x1a8 / +0x1ac / +0x1b0 / +0x1b4 floats, +0x1a0 and +0x1d0 words, +0x210 balloon script, +0x214 balloon life.
    void WinningMods(CObject* object);   // 0x1011a570 (2,656 bytes)
    // Unit v5954 (src/..._CPerson_LeaveQueue.cpp): the ticket booth walk. Fields by Mac offset: +0x4 and +0x6 cell bytes, +0xc2 sprite halfword,
    // +0x188 word, +0x19c float, +0x1bc, +0x200 wait, +0x204 paid flag.
    int32_t FeelingAboutEntryFee();   // 0x1011c63c (540 bytes): 0 to 3
    // Unit v5962: 0x1011836c (148 bytes, untranslated): the head sprite id (bit 31 set: a custom sprite) EnableRideHead gives a ride's head.
    uint32_t GetHeadSpriteID();
    // Unit v51002 (src/ParkMessages_SAVEVARSTRUCT_3.cpp): the person pays the ride's price per use (+0x1a0 word, the money).
    void PayPricePerUseForObject(CObject* object);   // 0x1011a4ac (132 bytes)
    // Unit v51016 (src/..._CPerson_EveryTimeMods_2.cpp): +0x1c4 / +0x1c8 / +0x1cc ride-kind counters, +0x1b8 float, +0x1d4 turn stamp, +0x1e6 to +0x1ee history halfwords.
    void EveryTimeMods(CObject* object);   // 0x1011a364 (276 bytes)
    void AddRideToRecentHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride);   // 0x10119a30 (52 bytes, untranslated)
    // Unit v51064: 0x1011f0b8 (40 bytes): state 0xb (+0x220) and the queue place (+0x1f1) 0.
    bool ReadyForAdmitting() const;
    void ForceRideAdmittance();   // 0x1011df70 (36 bytes): runs ProcessStateWaitingForScriptToAdmitMe
};
// Unit v51016 (names invented, zero at start; defined in the EveryTimeMods_2 source file): LookForMinorDest's "none" values.
extern uint16_t gMinorNoRide;    // 0x102a0fb0
extern uint16_t gMinorNoQueue;   // 0x102a0fb2
extern uint16_t gMinorListEnd;   // 0x102a0fb4
extern uint16_t gMinorNoDest;    // 0x102a0fb6
extern uint16_t gMinorNoDest2;   // 0x102a0fb8
extern uint16_t gPersonTableBusSprite;   // 0x102a0d58, the halfword after gPersonTable (unit v5954)
// Unit v5671 (names invented): the balloon script's start block (0x1029fd48, initial word 0x10266a60) and the balloon life limits (0x102a0f0c = 25, 0x102a0f10 = 255).
extern uint32_t gWinningBalloonBlock;
extern uint32_t gBalloonLifeMin;
extern uint32_t gBalloonLifeMax;
// Unit v5563 (names invented, zero at start; defined in the ProcessStateWalksToMajor source file): ProcessStateWalksToMajor compares the person's +0x1dc and the back of the queue with them.
extern uint16_t gNoMajorRide;   // 0x102a0ff8
extern uint16_t gNoMajorCell;   // 0x102a0ffa
// Unit v5543 (names invented, zero at start; defined in the ModelState source file).
extern uint16_t gMajorListEnd;   // 0x102a0f18
extern uint16_t gMajorNoQueue;   // 0x102a0f1a
extern uint16_t gMajorNoRide;    // 0x102a0f1c
extern uint16_t gModelListEnd;   // 0x102a100e
extern uint16_t gNoLitterRide;     // 0x102a0ffc (names invented, 0 at start): ProcessStateWalksToLitterBin compares the person's +0x1dc with it
extern uint16_t gNoAdmitState;     // 0x102a0ffe (0 at start): ProcessStateWaitingForScriptToAdmitMe compares the person's +0x228 with it
// Unit v5686 (names invented, defined in the CalculateOptionScore source file, zero at start).
extern uint16_t gNoBackOfQueue;    // 0x102a0fac: CalculateOptionScore asserts the ride's back of the queue differs from it
extern uint16_t gNoThingRide;      // 0x102a0fae: CalculateOptionScore skips the "already been on it" check when the person's +0x1e0 equals it
extern uint8_t gNeedMatchTable[0x79];   // 0x102a0f1e: 11 x 11 bytes, row = need / 10, column = amount / 10
extern uint8_t gAmountMatchTable[0x15]; // 0x102a0f97: indexed by (amount + 4) / 5

// Unit v5315 (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_LOADVAR.cpp).
template<class Value> int32_t LOADVAR(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t LOADVAR<CPerson::PersonState>(void*, CPerson::PersonState*, const char*, int32_t);   // 0x1011b7cc, 236 B
// Unit v51002 (src/ParkMessages_SAVEVARSTRUCT_3.cpp).
template<class Value> int32_t SAVEVAR(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t SAVEVAR<CPerson::PersonState>(void*, CPerson::PersonState*, const char*, int32_t);   // 0x1011b90c, 260 B
extern uint32_t gBalloonPopTarget;   // 0x1029fd84 (names invented): its address is the code ESP_ForceGoto restarts the balloon script at
// Unit v5052: the message CPerson::CPerson posts when a person has arrived (0x101521e4, 40 bytes, untranslated; the type it answers is not recovered).
#include "CMsg.h"
class CMsgPeepArrived : public CMsg {
public:
    CMsgPeepArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> peep);   // 0x101521e4
    int32_t GetType() const override { return 0x1c; }   // 0x1011b794 (unit v51064; was a placeholder 0)
    uint16_t mThingId;   // 0x08 on the Mac (unit v51064)
};
// Unit v51064. The Mac destructors store the vtable and delete when the hidden halfword flag is above 0; the host free functions model the delete.
class CMsgPrankery : public CMsg {
public:
    int32_t GetType() const override { return 0xe; }   // 0x10120b90
};
CMsgPeepArrived* CMsgPeepArrived_Dtor(CMsgPeepArrived* self, int16_t deleteFlag);   // 0x10116c54
CMsgPrankery* CMsgPrankery_Dtor(CMsgPrankery* self, int16_t deleteFlag);            // 0x1011c5c4
void __sinit_Person_cpp();            // 0x1011ba64
void __sinit_PersonQueue_cpp();       // 0x10120bc8
void __sinit_MechanicHQ_cpp();        // 0x10105d2c
void __sinit_VehicleCallbacks_cpp();  // 0x10136060
void __sinit_guard_cpp();             // 0x100fef90
extern int32_t gPersonSinitA;       // 0x1034c4a0 (names invented): set to 0x10000
extern int32_t gPersonSinitB;       // 0x1034c4a4: set to 0
extern int32_t gPersonQueueSinitA;  // 0x1034c4a8
extern int32_t gPersonQueueSinitB;  // 0x1034c4ac
extern int32_t gMechanicHQSinitA;   // 0x1034c2d0
extern int32_t gMechanicHQSinitB;   // 0x1034c2d4
extern int32_t gVehicleCallbacksSinitA;  // 0x1034caa0
extern int32_t gVehicleCallbacksSinitB;  // 0x1034caa4
extern int32_t gGuardSinitA;        // 0x1034c204
extern int32_t gGuardSinitB;        // 0x1034c208
// Unit v5052 (names invented): the halfword at 0x102a0d54 + 2 (initialised data; the person's sprite table), and the two halfwords at
// 0x102a0f14 and 0x102a0f16 (both 0 at start) that ReceiveMessage compares the person's +0x228 and +0x22a with.
extern uint16_t gPersonTable[2];      // 0x102a0d54
extern uint16_t gPersonCellA;         // 0x102a0f14
extern uint16_t gPersonCellB;         // 0x102a0f16
// Unit v5098 (names invented). The message ProcessStateIdle posts after a balloon was popped or litter dropped (vtable 0x102a10dc, 0x10 bytes on the Mac:
// flags +4, the person's id +8, the id modulo 3 at +0xc; the type it answers is not recovered).
class CMsgPersonIdle : public CMsg {
public:
    int32_t GetType() const override;   // placeholder (src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_ProcessStateIdle.cpp); key function
    uint16_t mThingId;   // 0x08 on the Mac
    uint32_t mValue;     // 0x0c on the Mac
};
extern uint32_t gLitterKindCount;   // 0x102a0b20 (initialised data, 6): the litter kinds + 1; a dropped kind is rand % (count - 1) + 1
extern uint16_t gNoLitterBin;       // 0x102a0ff0 (0): the "no thing" id FindNearestFireworks / FindNearestReachableLitterBin answer to say none ... see the .cpp
extern uint16_t gNoFireworks;       // 0x102a0ff2 (0)
extern uint16_t gNoBalloonThing;    // 0x102a0ff4 (0): ends the list of things in a cell
extern uint16_t gNoDecision;        // 0x102a0ff6 (0): MakeMajorDecision leaves this in +0x1dc for "nothing"
int32_t LbMaths_ArcTan(int32_t x, int32_t y);   // 0x1023b1b8 (glue import)
// Unit v5217 (names invented, all halfwords, 0 at start): the id that ends the world's list of things (FindNearestFireworks), and the two
// ids UpdateTurn's assertion compares the person's +0x228 and +0x22a with.
extern uint16_t gThingListEnd;   // 0x102a1008
extern uint16_t gQueueLinkA;     // 0x102a100a
extern uint16_t gQueueLinkB;     // 0x102a100c
// CObject (unit w126, src/Object.cpp): a ride, shop or other object of the park (0x448 bytes on the Mac; the host layout equals it: no
// member holds a pointer). The names are mine. The constructor leaves the halfword at +0x3a, the byte run +0x4d..+0x4f and the halfword
// at +0x6a unwritten (a block of 0xa5 keeps them); the ring buffers of the histories are never written either.
class CObject {
public:
    // The state of the ride (the word at +0x198): SetState stores it after the matching message is posted. 0 and 3 only store it; any other
    // value than 0 to 4 asserts ("Unknown state in CObject::SetState") and then stores it.
    enum ObjectState : int32_t { kStateNormal = 0, kStateBrokenDown = 1, kStateWantsUpgrade = 2, kStateThree = 3, kStateCondemned = 4 };

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x1010d634
    void ModelState();   // 0x1010daa0
    // 0x10105d74, 1,800 bytes (src/Object.cpp, unit w126): the map cell the object stands on, its object type, the rotation and one more long.
    CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t a, int32_t b);
    // 0x1010dc94, 2,000 bytes (src/Object.cpp, unit w126): see there.
    void SetState(ObjectState state);
    // Unit w152 (untranslated callees of the analyser's shop and sideshow queries):
    using ShopType = CObjectShopType;   // the ride record's kRideSlot_ShopType values (0 to 7, src/RideDB_balance.cpp 95); names unknown. The enum itself is
                                        // declared in analyser.h (this header includes it, and CParkAnalyser's queries take it before CObject exists).
    void UpdateEngine(int32_t flag);             // 0x10107d38
    void CreateRideScript();                     // 0x101076b0
    // Unit w132 (untranslated callees of CObject::UpdateTurn, ModelState, CreateRideScript):
    bool IsClosed() const;   // 0x1010cae8 (24 bytes, src/Object_queries.cpp, unit w151): mF64 is 0
    bool IsBrokenDown() const;   // 0x1010c878 (24 bytes, unit w151): mState is 1

    // Unit w197 (src/Object_queue.cpp): the ride's queue. The queue is a list of people (the head in mQueuedPerson, the link to the person behind at +0x228 and
    // to the person ahead at +0x22a of each person) and, for a ride with the queue flag (mFlags 8), a line of map cells from the entry cell (mF32) back to the
    // back of the queue (mF36, cached; mF3C is its length in cells). The tags are one halfword through a hidden pointer or the high half of a register.
    void AddPersonToQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person);   // 0x10108a28 (412 bytes)
    void RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person);   // 0x10108c20 (628 bytes)
    int32_t GetNumPeopleInQueue() const;   // 0x10108ef4 (52 bytes): GetPositionInQueue of nobody
    int32_t GetPositionInQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) const;   // 0x10108f5c (392 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetFrontOfPhysicalQueue() const;   // 0x10109144 (376 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetBackOfPhysicalQueue();   // 0x10109374 (288 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetBackOfVirtualQueue();   // 0x101094cc (44 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetBackOfQueue();   // 0x101092f4 (80 bytes)
    void RecalculateBackOfQueue();   // 0x1010952c (512 bytes)
    int32_t IsBOQConnected();   // 0x10109764 (460 bytes; not const on the Mac)
    int32_t IsExitCellConnected();   // 0x10109960 (348 bytes)
    // 0x10109af0 (608 bytes): the Mac listing never reads `this` (r3 = the hidden result, r4 = the cell id in the high half): a static function.
    static tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetQCellBehind(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    int32_t ConvertQPositionToMapPos(uint32_t position, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* cell, uint8_t* x, uint8_t* y);   // 0x10109da8 (160 bytes)
    int32_t ConvertPhysicalQPositionToMapPos(uint32_t position, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* cell, uint8_t* x, uint8_t* y);   // 0x10109eb4 (920 bytes)
    int32_t ConvertVirtualQPositionToMapPos(uint32_t position, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* cell, uint8_t* x, uint8_t* y);   // 0x1010a2c0 (456 bytes)

    // Unit w198 (src/Object_takings.cpp): the ride's takings, costs, prizes and satisfaction. The money words are mF17C (the takings so far), mF180
    // (the costs so far), mCostOfGoods (the prize value), mF188 (the product quality), mChanceOfNotLoosing, mPricePerUse and mF194 (the amount of the
    // special ingredient); the customers are counted in mF19C and mF1A0 (walked away) and the histories at 0x6c (takings), 0xf4 (costs), 0x1a4
    // (customers), 0x22c (walked away), 0x2b4 (winners) and 0x33c (satisfaction). The names of the fields are the ones above, the meanings are mine.
    int32_t GetObjectAge() const;   // 0x10107f50 (168 bytes): whole days from the build stamp (mBuilt) to the world's funny time
    int32_t GetRemainingLife() const;   // 0x10108024 (20 bytes): mF44 truncated as fctiwz does
    void WalkedAway();   // 0x1010eec4 (28 bytes): one more in mF1A0 and in the walked-away history's temp word
    void HadCustomer();   // 0x1010ef0c (28 bytes): one more in mF19C and in the customer history's temp word
    void RecordNewTakings(uint32_t amount);   // 0x1010ef54 (968 bytes): the bank deposit, the takings words and the challenge counters
    void RecordACost(uint32_t amount);   // 0x1010f34c (320 bytes): the cost words, the bank withdrawal and the challenge counters
    void HadAWinner();   // 0x1010f4b8 (16 bytes): one more in the winners history's temp word
    uint32_t GetPricePerUse() const;   // 0x1010f4f4 (8 bytes)
    uint32_t GetPrizeValue() const;   // 0x1010f52c (8 bytes)
    void SetPrizeValue(int32_t value);   // 0x1010f564 (8 bytes)
    int32_t GetProspectiveCostOfGoods(float quality, float ingredient) const;   // 0x1010f598 (280 bytes): the cost of goods for a quality and an ingredient amount
    int32_t GetCostOfGoods() const;   // 0x1010f6ec (376 bytes): the same for the ride's own quality and amount
    int32_t GetTotalRevenue() const;   // 0x1010f894 (16 bytes): takings minus costs
    int32_t GetProfitability() const;   // 0x1010f8d4 (112 bytes): the last 30 takings minus the last 30 costs
    void ExpressSatisfaction(int32_t value);   // 0x1010f974 (40 bytes)
    int32_t GetCustomerSatisfaction() const;   // 0x1010f9d0 (56 bytes): the average of the last 30 of mHistory33C, plus 50
    uint32_t GetPercentageOfWinners() const;   // 0x1010fa40 (192 bytes)
    uint8_t GetProductQuality() const;   // 0x1010fb38 (12 bytes)
    void SetProductQuality(uint8_t quality);   // 0x1010fb78 (12 bytes)
    uint8_t GetChanceOfWinning() const;   // 0x1010fbb8 (12 bytes)
    void SetChanceOfWinning(uint8_t chance);   // 0x1010fbf8 (12 bytes)
    void SetAmountOfSpecialIngredient(int32_t amount);   // 0x1010fc38 (8 bytes)
    uint8_t GetAmountOfSpecialIngredient() const;   // 0x1010fc7c (12 bytes)
    const unsigned char* GetSpecialIngredientName() const;   // 0x1010fcc4 (156 bytes): the text of the ride record's special ingredient, NULL for none
    int32_t GetResidualValuePercentage() const;   // 0x1010fd98 (500 bytes): the ride record's scrap value of the ride's age
    uint32_t GetCurrentResidualValue() const;   // 0x1010ffc8 (112 bytes): that percentage of the cost of the upgrade
    void PersonUsedToilet(uint32_t amount);   // 0x10110070 (832 bytes): a toilet gets dirtier; the map effect changes when it turns dirty
    bool WillIWin(CPerson* person) const;   // 0x101103e0 (252 bytes): the sideshow's draw
    void SetPricePerUse(int32_t price);   // 0x101105f4 (8 bytes)

    // Unit w199 (src/Object_upkeep.cpp): the ride's wear, breakdown, maintenance, repair, upgrade and cleaning. The staff member assigned to the ride is
    // the halfword mF5A (0 for nobody) with the world turn mF5C it was assigned on; GetAssignedStaffMember and IsBeingWorkedUpon take the staff member
    // off again once 100 turns have passed and the staff member is no longer allocated to this ride.
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetAssignedStaffMember();   // 0x1010c5a0 (256 bytes; a two-byte answer through a hidden pointer)
    void AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff);   // 0x1010c4d4 (36 bytes)
    void UnassignStaffMember();   // 0x1010c554 (24 bytes)
    int32_t IsBeingWorkedUpon();   // 0x1010c6d8 (368 bytes; not const on the Mac): the assigned staff member is working on this ride
    int32_t IsCondemned() const;   // 0x1010c8bc (24 bytes): mState is 4
    int32_t IsDirty() const;   // 0x1010c900 (88 bytes): a relief ride (mFlags 1) whose condition is below 25
    int32_t IsFlaggedForMaintenance() const;   // 0x1010c980 (8 bytes): mF60
    int32_t RequiresUpgrade() const;   // 0x1010c9c0 (24 bytes): mState is 2
    void StartFixing();   // 0x1010ca08 (56 bytes): script variable 4 to 0
    int32_t IsPlayingFixAnim();   // 0x1010ca6c (76 bytes): script variable 7 above 0
    void CleanFully();   // 0x1010bda8 (228 bytes)
    void RepairFully(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff);   // 0x1010b5bc (1,940 bytes)
    int32_t RequestMaintenance(int32_t on);   // 0x1010beb8 (808 bytes): always 1
    void AddWearAndTear();   // 0x1010b244 (840 bytes)
    void RequestBreakdown();   // 0x1010d90c (88 bytes): script variable 4 to 1
    void SetWornFlag();   // 0x1010d994 (56 bytes): script variable 8 to 1
    double GetProspectiveDamagePerTurn(float speed, float capacity, float duration, bool useCapacity) const;   // 0x1010ae5c (816 bytes; the third float is never read)
    int32_t GetProspectiveReliability(float speed, float capacity, float duration) const;   // 0x1010b1cc (60 bytes)
    // Unit w200 (src/Object_opening.cpp): whether the ride opens, the close and reopen steps, whether it runs and may be chosen, the upgrade request and
    // the upgrade parent, the blocked exit, the operating speed, duration and capacity, the ride record of the object type and the messages the ride
    // receives. The ride is closed while mF64 is 0. Callers: the park gates (IsOpenable, CloseObject, Reopen), the handyman (Reopen), the park analyser's rating (IsAvailableToChoose).
    int32_t IsOpenable();   // 0x1010aaac (192 bytes): not broken down, condemned, flagged for maintenance or wanting an upgrade, the queue connected, a coaster's track usable
    int32_t IsCloseable();   // 0x1010ab98 (8 bytes): always 1
    void CloseObject();   // 0x1010abcc (228 bytes): says so, mF64 to 0, forgets the person being loaded, script variable 6 to 1, the hoardings on
    void Reopen();   // 0x1010acdc (344 bytes): five traces when IsOpenable fails (it opens anyway), mF64 to 1, the hoardings off, variable 6 to 0, state normal
    int32_t IsRunning() const;   // 0x1010cb28 (76 bytes): the ride script's variable 9 is above 0
    int32_t IsAvailableToChoose();   // 0x101083e0 (396 bytes; not const on the Mac)
    // 0x1010c214 (656 bytes): `on` asks for an upgrade (answers 0 when the ride is not choosable, mFlags 4); 0 withdraws the request. Answers 1 otherwise.
    int32_t RequestUpgrade(int32_t on);
    void SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x1010d9f8 (16 bytes): the halfword mF2C
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetUpgradeParent() const;   // 0x1010da64 (12 bytes): mF2C, a two-byte answer through a hidden pointer
    void NotifyExitBlocked();   // 0x1010a93c (200 bytes): an open ride posts CMsgBlockedExit and then closes
    RideInfo* GetDBInfo() const;   // 0x10107cd4 (56 bytes): the ride record of the halfword at +0x0e (CRideDatabase::GetRideInfo)
    void SetOperatingSpeed(uint32_t speed);   // 0x10108068 (104 bytes)
    void SetOperatingDuration(uint8_t duration);   // 0x10108104 (272 bytes): clamped to the record's minimum and maximum duration
    void SetOperatingCapacity(uint8_t capacity);   // 0x10108248 (356 bytes): clamped to the record's minimum and maximum capacity
    void ReceiveMessage(const CMsg& message);   // 0x10107ba0 (256 bytes, kind 3): a staff member's death (type 0x1b) and the month end (type 0xb)

    // Unit w202 (src/Object_admit.cpp): the ride takes its guests on and off and judges its queue. The ride script's variable 0 is the person the
    // script has let on (0 for nobody), variable 1 the person it lets off, variable 2 its capacity and variable 7 asks for the ride to break down
    // or to be condemned. A guest's state word (+0x220) is 0xd while it is being loaded, 0xe while it waits to be admitted and 0x10 while it rides.
    int32_t HasFreeQueueSpace() const;   // 0x101085a0 (80 bytes; w200's IsAvailableToChoose calls it): the queue holds fewer people than four to each unit of mF3C. The Mac answers 0 or 1.
    uint32_t GetIdealQueueLength() const;   // 0x10108624 (448 bytes): 100 when the ride has no queue (mFlags 8), else the queue wait constant of the upgrade level
                                            // times the operating speed over the record's initial speed times the capacity, over the duration, at least 4
    bool QueueTooLong() const;   // 0x10108818 (484 bytes): the people in the queue reach the same length as GetIdealQueueLength (inlined, not called)
    int32_t AdmitPerson(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person);   // 0x1010d0e0 (548 bytes): 0 when broken down, closed or condemned or when the script holds someone else
    int32_t FinishedAdmitting();   // 0x1010d35c (96 bytes): the script's variable 0 is no longer the queue's head, mQueuedPerson
    bool ReadyToAdmit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person);   // 0x1010d3ec (32 bytes): the person is the one being loaded
    void PersonLeftDuringAdmittance(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person);   // 0x1010d464 (360 bytes)
    void LetPeopleOn();   // 0x1010e61c (952 bytes): a closed ride forces its admitted guest on or dismisses its head; an open one loads the head of its queue
    void LetPeopleOff();   // 0x1010ea00 (280 bytes): variable 1 names the person to let off; Setvar clears it once the guest has left (CPerson::ExitRide answers non-zero)
    void ProcessStateFunctioning();   // 0x1010eb44 (528 bytes): LetPeopleOn; then LetPeopleOff, or (variable 7 set) broken down while mF44 shows life, else condemned
    void GoFirstPerson(int32_t on) const;   // 0x1010ed8c (264 bytes; unit w140, called by RideRideIfRequired): the first person view of the ride's camera mesh or fly tour

    // Unit w229 (src/Object_serialise.cpp)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10106570 (4,368 bytes)

    // Unit w210 (src/Object_info.cpp): a ride's names, kind, excitement, stand point and final destroy. Each function fetches the ride record again with
    // CRideDatabase::GetRideInfo of mObjectType for every slot it reads (the original calls it each time). The sign text is two wide strings of 0x21
    // characters (mSignA at +0x3c4, mSignB at +0x406); a full 0x21 character mSignA has no terminator and runs on into mSignB.
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> UpgradesWhat();   // 0x1011050c (72 bytes): the record's UpgradesId slot (+0x284), a halfword through a hidden pointer (not const on the Mac)
    ObjectControl* GetControl();   // 0x10110580 (72 bytes): the world's control manager's GetObjectControl of mObjectType (flag 0)
    const char16_t* GetFileName() const;   // 0x1011062c (56 bytes): the ride record's name, a wide string (CRideDatabase::GetFileName; the area note says char*)
    void GetUIName(char16_t* out, uint32_t size);   // 0x10110690 (264 bytes): mSignA and mSignB joined (a trailing '-' of mSignA is dropped and the halves run together, else a space)
    void GetSignNameA(char16_t* out, uint32_t size);   // 0x101107c4 (232 bytes): mSignA (a trailing "--" is cut to one '-')
    void GetSignNameB(char16_t* out, uint32_t size);   // 0x101108dc (60 bytes): mSignB
    void SetSignNameA(const char16_t* text);   // 0x10110948 (412 bytes): copies the text (the Mac takes wchar_t*) into mSignA, tells the master save when this is the gates thing and the game is not online, records action 0x50
    void SetSignNameB(const char16_t* text);   // 0x10110b10 (412 bytes): the same for mSignB, records action 0x55
    int32_t IsCoaster() const;   // 0x10110cd8 (76 bytes, src/gameplay_leaves.cpp, unit v5L07)
    int32_t IsTrackRide() const;   // 0x10110d50 (200 bytes)
    int32_t GetProspectiveExcitementLevel(int32_t a, int32_t b, int32_t c) const;   // 0x1010cba0 (976 bytes): the excitement the ride would have with these settings; for a sideshow a is the prize value, b the price per use and c the chance of winning, for another ride a is the operating speed and b the duration (c is unused; a coaster uses none of them)
    uint8_t GetExcitementLevel(bool flag) const;   // 0x1010cfb0 (252 bytes; the listings mask the answer with 0xff)
    void GetStandPoint(int32_t which, MapPos* x, MapPos* y);   // 0x1010a4fc (1,028 bytes): the stand point of the entry cell (which 0) or of the exit cell (otherwise) as a cell and an offset in each of x and y
    void FinalDestroy();   // 0x10107828 (724 bytes)

    CThing mBase;                // 0x00
    uint16_t mF0C;               // 0x0c  cleared last by the constructor
    uint16_t mObjectType;        // 0x0e  the object type id (the ride database's key)
    int32_t mRotation;           // 0x10  the constructor's third argument
    uint32_t mBuilt[2];          // 0x14  the park time the ride was built (a time stamp as two words, CParkTime::GetFunnyTime)
    uint32_t mF1C;               // 0x1c  the index of the ride's anim set in gRideAnimSets (SetState)
    uint32_t mF20;               // 0x20  the ride's script handle (SetState)
    int32_t mF24;                // 0x24  the constructor's fourth argument (GoFirstPerson asks BMP_GetCameraMesh for its camera mesh while it is not 0; unit w210: GetProspectiveExcitementLevel hands it to BMP_CountTrackSections as the ride)
    int32_t mF28;                // 0x28  1 (the constructor); while it is 0, IsAvailableToChoose refuses a ride whose record's WhichTrackType (slot 36, +0x9c) is 1 or 2
    uint16_t mF2C;               // 0x2c  the upgrade parent (SetUpgradeParent, GetUpgradeParent)
    uint16_t mFlags;             // 0x2e  1 provides relief, 2 chills you out, 4 is choosable, 8 has a queue, 0x10 provides security, 0x20 handles its sprite, 0x40 holds litter, 0x80 fireworks, 0x100 runs continuously
    uint16_t mF30;               // 0x30  the map cell of the ride's offset (1 based id)
    uint16_t mF32;               // 0x32  the map cell of the entry
    uint16_t mF34;               // 0x34  the map cell of the exit
    uint16_t mF36;               // 0x36
    uint16_t mQueuedPerson;      // 0x38  the person the ride is admitting or the thing id of the queue's head (SetState)
    uint16_t mF3A;               // 0x3a  never written by the constructor
    uint32_t mF3C;               // 0x3c  the queue's room: HasFreeQueueSpace lets four people stand to each unit of it
    float mF40;                  // 0x40  100.0f: the condition (AddWearAndTear wears it down, RepairFully and CleanFully set it to 100; a toilet's cleanliness is the same word: PersonUsedToilet, dirty below 25 once truncated to a byte)
    float mF44;                  // 0x44  100.0f: the remaining life (GetRemainingLife; AddWearAndTear wears it down by a fiftieth of the damage)
    float mF48;                  // 0x48  100.0f
    uint8_t mF4C;                // 0x4c  the upgrade level, 0 to 3 (RepairFully of a ride that wants an upgrade adds one; GetResidualValuePercentage reads the ride record's slots of that level, 0x40 bytes each)
    uint8_t mF4D[3];             // 0x4d  never written
    uint32_t mF50;               // 0x50  the script handle of the upgrade effect sprite (RepairFully)
    uint32_t mF54;               // 0x54  the operating speed (RepairFully)
    uint8_t mF58;                // 0x58  the duration (RepairFully)
    uint8_t mF59;                // 0x59  the capacity (RepairFully)
    uint16_t mF5A;               // 0x5a  the staff member assigned to the ride, 0 for nobody
    uint32_t mF5C;               // 0x5c  the world turn the staff member was assigned on
    uint32_t mF60;               // 0x60  1 while the ride is flagged for maintenance (RequestMaintenance)
    uint32_t mF64;               // 0x64  1 while running (SetState clears it)
    uint16_t mPersonLoaded;      // 0x68  the person being loaded (SetState zeroises it)
    uint8_t mF6A[2];             // 0x6a  never written
    CHistory<uint32_t, 30> mHistory6C;     // 0x06c
    CHistory<uint32_t, 30> mHistoryF4;     // 0x0f4
    uint32_t mF17C;              // 0x17c  the takings so far (RecordNewTakings)
    uint32_t mF180;              // 0x180  the costs so far (RecordACost)
    uint32_t mCostOfGoods;       // 0x184  the ride record's InitCostOfGoods; also the prize value (GetPrizeValue, SetPrizeValue)
    uint32_t mF188;              // 0x188  0x32; the product quality (low byte)
    int32_t mChanceOfNotLoosing; // 0x18c  100 minus the record's InitChanceOfLoosing
    uint32_t mPricePerUse;       // 0x190  the record's InitPricePerUse
    uint32_t mF194;              // 0x194  0x32; the amount of the special ingredient (low byte)
    int32_t mState;              // 0x198  SetState's word
    uint32_t mF19C;              // 0x19c  the customers so far (HadCustomer)
    uint32_t mF1A0;              // 0x1a0  the customers who walked away (WalkedAway)
    CHistory<uint32_t, 30> mHistory1A4;    // 0x1a4
    CHistory<uint32_t, 30> mHistory22C;    // 0x22c
    CHistory<uint32_t, 30> mHistory2B4;    // 0x2b4
    CHistory<int32_t, 30> mHistory33C;     // 0x33c
    char16_t mSignA[0x21];       // 0x3c4  the sign text's first part (CRideDatabase::GetSignNameA fills it)
    char16_t mSignB[0x21];       // 0x406  the second part (GetSignNameB)
};
static_assert(sizeof(CObject) == 0x448, "CObject is 0x448 bytes");
static_assert(offsetof(CObject, mObjectType) == 0x0e && offsetof(CObject, mBuilt) == 0x14 && offsetof(CObject, mFlags) == 0x2e, "CObject layout");
static_assert(offsetof(CObject, mQueuedPerson) == 0x38 && offsetof(CObject, mF3C) == 0x3c && offsetof(CObject, mF4C) == 0x4c, "CObject layout");
static_assert(offsetof(CObject, mPersonLoaded) == 0x68 && offsetof(CObject, mHistory6C) == 0x6c && offsetof(CObject, mHistoryF4) == 0xf4, "CObject layout");
static_assert(offsetof(CObject, mState) == 0x198 && offsetof(CObject, mHistory1A4) == 0x1a4 && offsetof(CObject, mHistory33C) == 0x33c, "CObject layout");
static_assert(offsetof(CObject, mSignA) == 0x3c4 && offsetof(CObject, mSignB) == 0x406, "CObject layout");
// Unit w202 (src/Object_admit.cpp): GetIdealQueueLength and QueueTooLong store the speed ratio they work out in this float (the name is mine).
extern float gObjectQueueSpeedRatio;   // 0x1034c35c
// Unit w142: the staff class by its two untranslated members ContextMouse calls (kind 1 of the thing at gIntrface+4).
// Unit w155 reads +0x1e4 (grade, int32), +0x1f4 (happiness, float) and +0x20c (next researcher id, uint16) through casts.
// Unit w189 (src/Staff.cpp) makes it plain data of 0x20c bytes at the Mac's offsets (the CPeep part is built by the CPeep constructor, the subclasses
// add their own members from +0x20c); the names marked "invented" are guesses from the code that uses them.
class CStaff {
public:
    void BeingPointedAt();         // 0x10125f30 (unit w190, 216 bytes)
    const char16_t* GetName() const;   // 0x10126f98 (unit w190; type of the answer is a doubt: used as the text of a TbUniStringVTag)
    int32_t TooTiredToWork() const;   // 0x10126cb4 (56 bytes, unit w190)
    int32_t HasPatrolArea() const;   // 0x10127a78 (84 bytes, unit w191)
    void GetPatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* start, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* end) const;   // 0x1012798c (20 bytes, unit w191)

    // Unit w178: the members the entertainer calls (all untranslated except the destructor, src/Entertainer.cpp). The enums are 32-bit values.
    enum PayGrade : int32_t {};
    enum StaffState : int32_t {};
    CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model);   // 0x1012482c (120 bytes, unit w189)
    CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model, PayGrade grade, uint8_t costume);   // 0x10124908 (136 bytes, unit w189)
    ~CStaff();   // 0x100fb194 (108 bytes, src/Entertainer.cpp, nonvirtual): the Vehicle at +0xd4 is destroyed; the deleting form frees the block
    void FinalDestroy();   // 0x10124b88 (36 bytes, unit w189)
    void ReceiveMessage(const CMsg& message);   // 0x10124bd8 (220 bytes, unit w189)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10124ce8 (1,268 bytes, unit w189)
    void UpdateTurn();   // 0x10125208 (120 bytes, unit w189)
    void SetState(StaffState state);   // 0x101252a8 (528 bytes, unit w189)
    void ModelState();   // 0x101254f4 (124 bytes, unit w189)
    int32_t DoGeneralStaffThing();   // 0x101272b0 (884 bytes, unit w191)
    void GetTiredWorking();   // 0x10126e28 (216 bytes, unit w191)
    void GetTiredPatrolling();   // 0x10126d1c (220 bytes, unit w191)
    void ProcessWalkingToRestArea();   // 0x10126008 (492 bytes, unit w191)
    void ProcessResting();   // 0x101262a8 (368 bytes, unit w191)

    // Unit w189 (src/Staff.cpp).
    void Init();   // 0x10124a08 (348 bytes)
    void UpdateMonthly();   // 0x10125598 (156 bytes): the month's wages are withdrawn and added to the analyser's staff wages history

    // Unit w188 (the researcher's calls):
    int32_t GetGeneralStaffState() const;   // 0x101267cc (108 bytes, unit w190; the mangled name is const; an enum on the Mac, compared with 3, 4 and 5)

    // Unit w189 (for units w190 and w191; the six below are unit w191's): the members the translated functions call.
    uint32_t GetSalary();   // 0x10126fc8 (unit w190)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GoToRestArea();   // 0x10127658 (a two-byte answer through a hidden pointer; SetState's callers ignore it)
    void StopResting();   // 0x10127764
    void CancelPatrolArea();   // 0x10127a24
    void ProcessWalkingToStrike();   // 0x10126444
    void ProcessStriking();   // 0x10126504
    void ProcessBeingStunned();   // 0x10126688

    // Unit w190 (src/Staff_queries.cpp): staff.cpp part 2.
    void SackMe();   // 0x10125660 (464 bytes)
    void TrainMe(int32_t amount);   // 0x10125854 (404 bytes)
    int32_t GetEmploymentDuration() const;   // 0x10125a10 (168 bytes): days since hiring
    int32_t CanBePickedUp();   // 0x10125aec (128 bytes)
    void PickMeUp();   // 0x10125b98 (548 bytes)
    void DropMe(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x10125de4 (252 bytes): the cell is the HIGH halfword of the register
    StaffType GetStaffType();   // 0x10126718 (136 bytes)
    int32_t GetSkill() const;   // 0x1012686c (112 bytes)
    int32_t IsStriking();   // 0x10126904 (172 bytes)
    int32_t IsAllocatedToRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride);   // 0x101269d8 (120 bytes)
    int32_t IsWorkingOnAllocatedObject(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> object);   // 0x10126aac (268 bytes)
    int32_t IsRestingInStaffRoom(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> room) const;   // 0x10126c1c (56 bytes)
    void SetName(const char16_t* name);   // 0x10126f30 (64 bytes)
    static uint32_t GetSalary(StaffType type, uint8_t grade);   // 0x101270b0 (36 bytes): no this; the type in r3, the grade in r4

    CThing mThing;                        // 0x000 (mCellX +4, mCellY +6)
    uint8_t mScript[0xc];                 // 0x00c the CDelayedScript (mSpriteScript at +0xc): reached as Script()
    uint8_t mF18[4];                      // 0x018
    int32_t mF1C;                         // 0x01c cleared by SetState 5 (name invented)
    int32_t mSpriteIndex;                 // 0x020 (the CMMPickUpStaff's second argument)
    int32_t mSpriteSet;                   // 0x024 an ESP_Sprites (the CMMPickUpStaff's first argument)
    uint8_t mF28[8];                      // 0x028
    uint8_t mThoughts[0xc0 - 0x30];       // 0x030 the CThoughts: reached as Thoughts()
    uint16_t mWalkSpeed;                  // 0x0c0 Init: entry 0 (60) of the walking speed table below pay grade 3, entry 2 (100) from it (name invented)
    uint16_t mSpeedBonus;                 // 0x0c2 entry 0 (0), 1 (25) of the bonus table (name invented)
    uint8_t mF0C4[0xd4 - 0xc4];           // 0x0c4
    uint8_t mVehicleA[0x188 - 0xd4];      // 0x0d4 the Vehicle subobject (destroyed by ~CStaff)
    int32_t mF188;                        // 0x188
    uint8_t mVehicleB[0x19c - 0x18c];     // 0x18c
    int32_t mState;                       // 0x19c a StaffState
    char16_t mName[0x21];                 // 0x1a0 (Serialise saves all 33 characters; the subclasses use 0x20)
    uint8_t mF1E2[2];                     // 0x1e2
    int32_t mCurrentPayGrade;             // 0x1e4 a PayGrade (the Serialise name)
    uint8_t mPercentageThroughGrade;      // 0x1e8 the Serialise name
    uint8_t mF1E9[3];                     // 0x1e9
    uint32_t mTimeHired[2];               // 0x1ec the park time at hiring (a time stamp as two words; the Serialise name mTimeHired)
    float mHappiness;                     // 0x1f4 (name invented)
    float mEnergy;                        // 0x1f8 (name invented)
    uint32_t mTimeStartedIdling;          // 0x1fc the Serialise name
    uint32_t mJobsDone;                   // 0x200 the Serialise name
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mRestArea;           // 0x204 the Serialise name
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> mPatrolRegionBL;       // 0x206 the Serialise name
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> mPatrolRegionTR;       // 0x208 the Serialise name
    uint8_t mF20A[2];                     // 0x20a
    // Unit w191 (src/Staff_behaviour.cpp): staff.cpp part 3.
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindNearestRestingPlace();   // 0x10127108 (368 bytes; a two-byte answer through a hidden pointer, as GoToRestArea)
    void SetPatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> bottomLeft, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> topRight);   // 0x101278f0 (28 bytes; the HIGH halfwords of r4 and r5)
    int32_t InsidePatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) const;   // 0x10127af8 (188 bytes)
    int32_t FindDestInPatrolArea();   // 0x10127c10 (440 bytes)
    CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(mScript); }
    CThoughts* Thoughts() { return reinterpret_cast<CThoughts*>(mThoughts); }
};
static_assert(sizeof(CStaff) == 0x20c, "CStaff is 0x20c bytes");
static_assert(offsetof(CStaff, mScript) == 0xc, "mScript at 0xc");
static_assert(offsetof(CStaff, mF1C) == 0x1c, "mF1C at 0x1c");
static_assert(offsetof(CStaff, mSpriteIndex) == 0x20, "mSpriteIndex at 0x20");
static_assert(offsetof(CStaff, mSpriteSet) == 0x24, "mSpriteSet at 0x24");
static_assert(offsetof(CStaff, mThoughts) == 0x30, "mThoughts at 0x30");
static_assert(offsetof(CStaff, mWalkSpeed) == 0xc0, "mWalkSpeed at 0xc0");
static_assert(offsetof(CStaff, mSpeedBonus) == 0xc2, "mSpeedBonus at 0xc2");
static_assert(offsetof(CStaff, mVehicleA) == 0xd4, "mVehicleA at 0xd4");
static_assert(offsetof(CStaff, mF188) == 0x188, "mF188 at 0x188");
static_assert(offsetof(CStaff, mState) == 0x19c, "mState at 0x19c");
static_assert(offsetof(CStaff, mName) == 0x1a0, "mName at 0x1a0");
static_assert(offsetof(CStaff, mCurrentPayGrade) == 0x1e4, "mCurrentPayGrade at 0x1e4");
static_assert(offsetof(CStaff, mPercentageThroughGrade) == 0x1e8, "mPercentageThroughGrade at 0x1e8");
static_assert(offsetof(CStaff, mTimeHired) == 0x1ec, "mTimeHired at 0x1ec");
static_assert(offsetof(CStaff, mHappiness) == 0x1f4, "mHappiness at 0x1f4");
static_assert(offsetof(CStaff, mEnergy) == 0x1f8, "mEnergy at 0x1f8");
static_assert(offsetof(CStaff, mTimeStartedIdling) == 0x1fc, "mTimeStartedIdling at 0x1fc");
static_assert(offsetof(CStaff, mJobsDone) == 0x200, "mJobsDone at 0x200");
static_assert(offsetof(CStaff, mRestArea) == 0x204, "mRestArea at 0x204");
static_assert(offsetof(CStaff, mPatrolRegionBL) == 0x206, "mPatrolRegionBL at 0x206");
static_assert(offsetof(CStaff, mPatrolRegionTR) == 0x208, "mPatrolRegionTR at 0x208");
// Unit w189 (src/Staff.cpp): the two words __sinit_staff_cpp 0x10128350 stores (names invented; nothing reads either).
extern uint32_t gStaffSinitLow;      // 0x1034c4dc stored 0
extern uint32_t gStaffSinitHigh;     // 0x1034c4d8 stored 0x10000

// Unit w178 (src/Entertainer.cpp): the entertainer, 0x214 bytes on the Mac. A standalone class of plain data at the Mac's offsets (the Mac's CStaff base
// and the CDelayedScript are built by the CStaff constructor; here they are reached through casts, as the staff classes are elsewhere). Every member is
// trivial. The Vehicle subobject at +0xd4 is kept as raw bytes up to the state word.
// Unit w143 (untranslated): the constructor CMMPlaceStaff::OnLeftClick calls on a fresh object; the pay grade is a CStaff::PayGrade on the Mac
// (an int32_t here), the cell is the HIGH halfword of the register, `flag` is the costume and `value` the text id of the name.
class CEntertainer {
public:
    CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t payGrade, uint8_t flag, int32_t value);   // 0x100fb224 (176 bytes, unit w178)

    // Unit w123a: direct dispatch callees (unit w178 translates them).
    void UpdateTurn();   // 0x100fb6b0 (196 bytes)
    void ModelState();   // 0x100fba44 (528 bytes)
    void FinalDestroy();   // 0x100fb600 (124 bytes)
    void ReceiveMessage(const CMsg& message);   // 0x100fb448, 36 bytes, kind 6

    // Unit w178 (src/Entertainer.cpp).
    explicit CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x100fb094 (172 bytes): picks the costume 0, 1, 2, 0, ... from a static counter
    void Init(uint8_t costume);   // 0x100fb33c (224 bytes)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100fb4a8 (292 bytes)
    void ChooseNextAction();   // 0x100fb7a4 (344 bytes)
    void SetState(CStaff::StaffState state);   // 0x100fb934 (204 bytes)

    CThing mThing;                        // 0x000 (mCellX +4, mCellY +6)
    uint8_t mScript[0xc];                 // 0x00c the CDelayedScript (mSpriteScript at +0xc): reached as Script()
    uint8_t mF18[8];                      // 0x018
    int32_t mSpriteIndex;                 // 0x020 the costume
    int32_t mSpriteSet;                   // 0x024 an ESP_Sprites (4)
    uint8_t mF28[0xc0 - 0x28];            // 0x028
    uint16_t mSpeed;                      // 0x0c0
    uint8_t mF0C2[0xd4 - 0xc2];           // 0x0c2
    uint8_t mVehicle[0x19c - 0xd4];       // 0x0d4 the Vehicle subobject (destroyed by ~CStaff)
    int32_t mState;                       // 0x19c
    char16_t mName[0x20];                 // 0x1a0
    uint8_t mF1E0[4];                     // 0x1e0
    int32_t mGrade;                       // 0x1e4 the pay grade (0 to 4)
    uint8_t mF1E8[0x1fc - 0x1e8];         // 0x1e8
    uint32_t mStateTurn;                  // 0x1fc
    uint8_t mF200[0x20c - 0x200];         // 0x200
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mNext;   // 0x20c the next entertainer in the world's list
    uint8_t mF20E[2];                     // 0x20e
    uint32_t mTimeStartedEntertaining;    // 0x210
    CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(mScript); }
};
static_assert(sizeof(CEntertainer) == 0x214, "CEntertainer is 0x214 bytes");
static_assert(offsetof(CEntertainer, mScript) == 0xc, "mScript at 0xc");
static_assert(offsetof(CEntertainer, mSpriteIndex) == 0x20, "mSpriteIndex at 0x20");
static_assert(offsetof(CEntertainer, mSpriteSet) == 0x24, "mSpriteSet at 0x24");
static_assert(offsetof(CEntertainer, mSpeed) == 0xc0, "mSpeed at 0xc0");
static_assert(offsetof(CEntertainer, mVehicle) == 0xd4, "mVehicle at 0xd4");
static_assert(offsetof(CEntertainer, mState) == 0x19c, "mState at 0x19c");
static_assert(offsetof(CEntertainer, mName) == 0x1a0, "mName at 0x1a0");
static_assert(offsetof(CEntertainer, mGrade) == 0x1e4, "mGrade at 0x1e4");
static_assert(offsetof(CEntertainer, mStateTurn) == 0x1fc, "mStateTurn at 0x1fc");
static_assert(offsetof(CEntertainer, mNext) == 0x20c, "mNext at 0x20c");
static_assert(offsetof(CEntertainer, mTimeStartedEntertaining) == 0x210, "mTimeStartedEntertaining at 0x210");
// Unit w178 (src/Entertainer.cpp): the constructor's two function statics and the two words __sinit_Entertainer_cpp stores (names invented).
extern uint8_t gEntertainerCostumeGuard;    // 0x1034c1d4 the "counter is set up" flag
extern int32_t gEntertainerCostumeCounter;  // 0x1034c1d8 the next costume (0, 1, 2, 0, ...)
extern uint32_t gEntertainerSinitLow;       // 0x1034c1e0 __sinit_Entertainer_cpp stores 0
extern uint32_t gEntertainerSinitHigh;      // 0x1034c1dc ... and 0x10000 (nothing reads either)
// Unit w182 (src/Guard.cpp): the guard, 0x214 bytes on the Mac. Plain data at the Mac's offsets like CEntertainer above (the CStaff base, the CDelayedScript and
// the Vehicle are built by the CStaff constructor and reached through casts). Every member is trivial. The Vehicle subobject at +0xd4 is kept as raw bytes
// except the word at +0x188 (its +0xb4), which the guard sets to 1 before its second try for the ticket booth and to 0 on the walk to the entrance (name unknown).
class CGuard {
public:
    // Unit w143: the constructor CMMPlaceStaff::OnLeftClick calls on a fresh object; the pay grade is a CStaff::PayGrade on the Mac
    // (an int32_t here), the cell is the HIGH halfword of the register, `value` the text id of the name.
    CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t payGrade, uint8_t flag, int32_t value);   // 0x100fdb08

    // Unit w123a: direct dispatch callees (unit w182 translates them).
    void UpdateTurn();   // 0x100fe344
    void ModelState();   // 0x100fe4e8
    void FinalDestroy();   // 0x100fdd60
    void ReceiveMessage(const CMsg& message);   // 0x100fdec0, 160 bytes, kind 7

    // Unit w182 (src/Guard.cpp).
    explicit CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x100fda68 (84 bytes)
    void Init();   // 0x100fdc10 (300 bytes)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100fdf94 (372 bytes)
    void ProsecutePerp(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> perp);   // 0x100fe134 (292 bytes)
    bool IsAvailable() const;   // 0x100fe2b0 (36 bytes)
    bool IsCarrying() const;   // 0x100fe300 (24 bytes)
    void ChooseNextAction();   // 0x100fe430 (136 bytes)
    void SetState(CStaff::StaffState state);   // 0x100fe5a8 (400 bytes)
    void ProcessIdle();   // 0x100fe774 (232 bytes)
    void ProcessPatrolling();   // 0x100fe888 (228 bytes)
    void ProcessProsecuting();   // 0x100fe99c (912 bytes)
    void ProcessWalkingToEntrance();   // 0x100fed5c (296 bytes)
    void ProcessReturningToPatrol();   // 0x100feebc (156 bytes)

    CThing mThing;                        // 0x000 (mCellX +4, mCellY +6)
    uint8_t mScript[0xc];                 // 0x00c the CDelayedScript (mSpriteScript at +0xc): reached as Script()
    uint8_t mF18[8];                      // 0x018
    int32_t mSpriteIndex;                 // 0x020 the uniform
    int32_t mSpriteSet;                   // 0x024 an ESP_Sprites (7)
    uint8_t mF28[0xc2 - 0x28];            // 0x028
    uint16_t mSpeedBonus;                 // 0x0c2 entry 1 (25) of the speed table while chasing, entry 0 (0) otherwise (name invented)
    uint8_t mF0C4[0xd4 - 0xc4];           // 0x0c4
    uint8_t mVehicleA[0x188 - 0xd4];      // 0x0d4 the Vehicle subobject (destroyed by ~CStaff), up to its word at +0xb4
    int32_t mF188;                        // 0x188 inside the Vehicle: 1 before the second try for the ticket booth, 0 on the walk to the entrance (name unknown)
    uint8_t mVehicleB[0x19c - 0x18c];     // 0x18c
    int32_t mState;                       // 0x19c
    char16_t mName[0x20];                 // 0x1a0
    uint8_t mF1E0[4];                     // 0x1e0
    int32_t mGrade;                       // 0x1e4 the pay grade (0 to 4)
    uint8_t mF1E8[0x1fc - 0x1e8];         // 0x1e8
    uint32_t mStateTurn;                  // 0x1fc
    uint8_t mF200[0x20c - 0x200];         // 0x200
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mNext;   // 0x20c the next guard in the world's list
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mPerp;   // 0x20e the kid being chased (the Serialise name)
    uint32_t mProsecutionTimestamp;       // 0x210 the Serialise name; in fact a countdown of turns
    CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(mScript); }
};
static_assert(sizeof(CGuard) == 0x214, "CGuard is 0x214 bytes");
static_assert(offsetof(CGuard, mScript) == 0xc, "mScript at 0xc");
static_assert(offsetof(CGuard, mSpriteIndex) == 0x20, "mSpriteIndex at 0x20");
static_assert(offsetof(CGuard, mSpriteSet) == 0x24, "mSpriteSet at 0x24");
static_assert(offsetof(CGuard, mSpeedBonus) == 0xc2, "mSpeedBonus at 0xc2");
static_assert(offsetof(CGuard, mVehicleA) == 0xd4, "mVehicleA at 0xd4");
static_assert(offsetof(CGuard, mF188) == 0x188, "mF188 at 0x188");
static_assert(offsetof(CGuard, mState) == 0x19c, "mState at 0x19c");
static_assert(offsetof(CGuard, mName) == 0x1a0, "mName at 0x1a0");
static_assert(offsetof(CGuard, mGrade) == 0x1e4, "mGrade at 0x1e4");
static_assert(offsetof(CGuard, mStateTurn) == 0x1fc, "mStateTurn at 0x1fc");
static_assert(offsetof(CGuard, mNext) == 0x20c, "mNext at 0x20c");
static_assert(offsetof(CGuard, mPerp) == 0x20e, "mPerp at 0x20e");
static_assert(offsetof(CGuard, mProsecutionTimestamp) == 0x210, "mProsecutionTimestamp at 0x210");
// Unit w184 (src/Handyman.cpp): the handyman, 0x218 bytes on the Mac. Plain data at the Mac's offsets like CGuard above (the CStaff base, the CDelayedScript, the
// thoughts and the Vehicle are built by the CStaff constructor and reached through casts). Every member is trivial. Names marked "invented" are guesses.
// Unit w143 (untranslated before w184): the constructor CMMPlaceStaff::OnLeftClick calls on a fresh object; the pay grade is a CStaff::PayGrade on the Mac
// (an int32_t here), the cell is the HIGH halfword of the register, `flag` is the sprite index and `value` the text id of the name.
class CHandyman {
public:
    CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t payGrade, uint8_t flag, int32_t value);   // 0x100ff0a4 (176 bytes, unit w184)

    // Unit w123a: direct dispatch callees (unit w184 translates them).
    void UpdateTurn();   // 0x100ff6b4 (260 bytes)
    void ModelState();   // 0x100ffca8 (164 bytes)
    void FinalDestroy();   // 0x100ff2e0 (180 bytes)
    void ReceiveMessage(const CMsg& message);   // 0x100ff3c4 (196 bytes), kind 5

    // Unit w184 (src/Handyman.cpp).
    explicit CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x100fefd0 (132 bytes): the sprite index comes from a function static
    void Init(uint8_t sprite);   // 0x100ff1b8 (256 bytes)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100ff4c0 (452 bytes)
    void ChooseNextAction();   // 0x100ff7e4 (856 bytes)
    void SetState(CStaff::StaffState state);   // 0x100ffb70 (248 bytes)
    void ProcessPatrolling();   // 0x100ffd78 (140 bytes)
    void ProcessWalkingToLitter();   // 0x100ffe38 (480 bytes)
    void ProcessCleaningLitter();   // 0x10100050 (424 bytes)
    void ProcessWalkingToCleanToilet();   // 0x10100230 (320 bytes)
    void ProcessCleaningToilet();   // 0x101003ac (216 bytes)
    void ProcessIdle();   // 0x101004bc (152 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindNearestDirtyToilet();   // 0x10100580 (660 bytes)

    CThing mThing;                        // 0x000 (mCellX +4, mCellY +6)
    uint8_t mScript[0xc];                 // 0x00c the CDelayedScript (mSpriteScript at +0xc): reached as Script()
    MapPos mPosX;                         // 0x018 the first of the two words Teleport is given on arrival at the litter (name invented)
    MapPos mPosY;                         // 0x01a the second (name invented)
    uint8_t mF1C[4];                      // 0x01c
    int32_t mSpriteIndex;                 // 0x020 the sprite index Init is given
    int32_t mSpriteSet;                   // 0x024 an ESP_Sprites (5)
    uint8_t mF28[8];                      // 0x028
    uint8_t mThoughts[0xc0 - 0x30];       // 0x030 the CThoughts (its end at +0xc0 is a guess: the next member the listings touch is +0xc2): reached as Thoughts()
    uint8_t mF0C0[2];                     // 0x0c0
    uint16_t mSpeedBonus;                 // 0x0c2 entry 1 (25) of the speed table while heading for litter or a toilet (name invented; never reset here)
    uint8_t mF0C4[0xd4 - 0xc4];           // 0x0c4
    uint8_t mVehicle[0x19c - 0xd4];       // 0x0d4 the Vehicle subobject (destroyed by ~CStaff)
    int32_t mState;                       // 0x19c
    char16_t mName[0x20];                 // 0x1a0
    uint8_t mF1E0[4];                     // 0x1e0
    int32_t mGrade;                       // 0x1e4 the pay grade (0 to 4)
    uint8_t mF1E8[0x1fc - 0x1e8];         // 0x1e8
    uint32_t mStateTurn;                  // 0x1fc
    uint8_t mF200[0x20c - 0x200];         // 0x200
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mNext;             // 0x20c the next handyman in the world's list (the Serialise name)
    uint8_t mF20E[2];                     // 0x20e
    uint32_t mTimeStartedCleaning;        // 0x210 the Serialise name
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> mTargetLitterCell;   // 0x214 the Serialise name; the cell with litter being walked to (0 for none)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mToiletToClean;    // 0x216 the Serialise name; the toilet being walked to (0 for none)
    CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(mScript); }
    CThoughts* Thoughts() { return reinterpret_cast<CThoughts*>(mThoughts); }
};
static_assert(sizeof(CHandyman) == 0x218, "CHandyman is 0x218 bytes");
static_assert(offsetof(CHandyman, mScript) == 0xc, "mScript at 0xc");
static_assert(offsetof(CHandyman, mPosX) == 0x18, "mPosX at 0x18");
static_assert(offsetof(CHandyman, mPosY) == 0x1a, "mPosY at 0x1a");
static_assert(offsetof(CHandyman, mSpriteIndex) == 0x20, "mSpriteIndex at 0x20");
static_assert(offsetof(CHandyman, mSpriteSet) == 0x24, "mSpriteSet at 0x24");
static_assert(offsetof(CHandyman, mThoughts) == 0x30, "mThoughts at 0x30");
static_assert(offsetof(CHandyman, mSpeedBonus) == 0xc2, "mSpeedBonus at 0xc2");
static_assert(offsetof(CHandyman, mVehicle) == 0xd4, "mVehicle at 0xd4");
static_assert(offsetof(CHandyman, mState) == 0x19c, "mState at 0x19c");
static_assert(offsetof(CHandyman, mName) == 0x1a0, "mName at 0x1a0");
static_assert(offsetof(CHandyman, mGrade) == 0x1e4, "mGrade at 0x1e4");
static_assert(offsetof(CHandyman, mStateTurn) == 0x1fc, "mStateTurn at 0x1fc");
static_assert(offsetof(CHandyman, mNext) == 0x20c, "mNext at 0x20c");
static_assert(offsetof(CHandyman, mTimeStartedCleaning) == 0x210, "mTimeStartedCleaning at 0x210");
static_assert(offsetof(CHandyman, mTargetLitterCell) == 0x214, "mTargetLitterCell at 0x214");
static_assert(offsetof(CHandyman, mToiletToClean) == 0x216, "mToiletToClean at 0x216");
// Unit w184 (src/Handyman.cpp): the constructor's function static (the sprite index it hands Init; names invented) and the two words __sinit_Handyman_cpp stores.
extern uint8_t gHandymanSpriteGuard;    // 0x1034c20c the "sprite index is set up" flag
extern int32_t gHandymanSprite;         // 0x1034c210 the sprite index (never incremented)
extern uint32_t gHandymanSinitLow;      // 0x1034c218 __sinit_Handyman_cpp stores 0
extern uint32_t gHandymanSinitHigh;     // 0x1034c214 ... and 0x10000 (nothing reads either)
// Unit w187 (src/Mechanic.cpp): the mechanic, 0x218 bytes on the Mac. Plain data at the Mac's offsets like CHandyman above (the CStaff base, the CDelayedScript and
// the Vehicle are built by the CStaff constructor and reached through casts). Every member is trivial. Names marked "invented" are guesses.
// Unit w143 (untranslated before w187): the constructor CMMPlaceStaff::OnLeftClick calls on a fresh object; the pay grade is a CStaff::PayGrade on the Mac
// (an int32_t here), the cell is the HIGH halfword of the register.
class CMechanic {
public:
    CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t payGrade, uint8_t flag, int32_t value);   // 0x10104170 (176 bytes, unit w187)

    // Unit w123a: direct dispatch callees (unit w187 translates them).
    void UpdateTurn();   // 0x101049e0 (36 bytes)
    void ModelState();   // 0x10104928 (140 bytes)
    void FinalDestroy();   // 0x101043b0 (296 bytes)
    void ReceiveMessage(const CMsg& message);   // 0x10104508, 168 bytes, kind 4

    // Unit w187 (src/Mechanic.cpp).
    explicit CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x1010409c (132 bytes): the sprite index comes from a function static
    void Init(uint8_t sprite);   // 0x10104284 (260 bytes)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x101045e8 (372 bytes)
    void SetState(CStaff::StaffState state);   // 0x1010478c (348 bytes)
    void ChooseNextAction();   // 0x10104a30 (576 bytes)
    void ProcessPatrolling();   // 0x10104ca4 (140 bytes)
    void ProcessWalkingToBreakdown();   // 0x10104d64 (564 bytes)
    void ProcessRepairing();   // 0x10104fd4 (268 bytes)
    void ProcessIdle();   // 0x10105114 (152 bytes)

    CThing mThing;                        // 0x000 (mCellX +4, mCellY +6)
    uint8_t mScript[0xc];                 // 0x00c the CDelayedScript (mSpriteScript at +0xc): reached as Script()
    uint8_t mF18[8];                      // 0x018
    int32_t mSpriteIndex;                 // 0x020 the sprite index Init is given
    int32_t mSpriteSet;                   // 0x024 an ESP_Sprites (6)
    uint8_t mF28[0xd4 - 0x28];            // 0x028
    uint8_t mVehicle[0x19c - 0xd4];       // 0x0d4 the Vehicle subobject (destroyed by ~CStaff)
    int32_t mState;                       // 0x19c
    char16_t mName[0x20];                 // 0x1a0
    uint8_t mF1E0[4];                     // 0x1e0
    int32_t mGrade;                       // 0x1e4 the pay grade (0 to 4)
    uint8_t mF1E8[0x1fc - 0x1e8];         // 0x1e8
    uint32_t mStateTurn;                  // 0x1fc
    uint8_t mF200[0x20c - 0x200];         // 0x200
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mNext;             // 0x20c the next mechanic in the world's list (the Serialise name)
    uint8_t mF20E[2];                     // 0x20e
    uint32_t mDurationOfRepair;           // 0x210 the Serialise name; the turns left before the ride is repaired (type invented)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mObjectToRepair;   // 0x214 the Serialise name; the ride being walked to or repaired (0 for none)
    uint8_t mF216[2];                     // 0x216 padding
    CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(mScript); }
};
static_assert(sizeof(CMechanic) == 0x218, "CMechanic is 0x218 bytes");
static_assert(offsetof(CMechanic, mScript) == 0xc, "mScript at 0xc");
static_assert(offsetof(CMechanic, mSpriteIndex) == 0x20, "mSpriteIndex at 0x20");
static_assert(offsetof(CMechanic, mSpriteSet) == 0x24, "mSpriteSet at 0x24");
static_assert(offsetof(CMechanic, mVehicle) == 0xd4, "mVehicle at 0xd4");
static_assert(offsetof(CMechanic, mState) == 0x19c, "mState at 0x19c");
static_assert(offsetof(CMechanic, mName) == 0x1a0, "mName at 0x1a0");
static_assert(offsetof(CMechanic, mGrade) == 0x1e4, "mGrade at 0x1e4");
static_assert(offsetof(CMechanic, mStateTurn) == 0x1fc, "mStateTurn at 0x1fc");
static_assert(offsetof(CMechanic, mNext) == 0x20c, "mNext at 0x20c");
static_assert(offsetof(CMechanic, mDurationOfRepair) == 0x210, "mDurationOfRepair at 0x210");
static_assert(offsetof(CMechanic, mObjectToRepair) == 0x214, "mObjectToRepair at 0x214");
// Unit w187 (src/Mechanic.cpp): the constructor's function static (the sprite index it hands Init) and the two words __sinit_mechanic_cpp stores (names invented).
extern uint8_t gMechanicSpriteGuard;    // 0x1034c2c0 the "sprite index is set up" flag
extern int32_t gMechanicSprite;         // 0x1034c2c4 the sprite index (never incremented)
extern uint32_t gMechanicSinitLow;      // 0x1034c2cc __sinit_mechanic_cpp stores 0
extern uint32_t gMechanicSinitHigh;     // 0x1034c2c8 ... and 0x10000 (nothing reads either)
// Unit w188 (src/Researcher.cpp): the researcher, 0x214 bytes on the Mac. Plain data at the Mac's offsets like CHandyman and CMechanic above (the CStaff base, the
// CDelayedScript and the Vehicle are built by the CStaff constructor and reached through casts). Every member is trivial. Names marked "invented" are guesses.
// There is no CResearcher::SetState on the Mac: the two forms its callers need are inlined in each of them.
// Unit w143 (untranslated before w188): the constructor CMMPlaceStaff::OnLeftClick calls on a fresh object; the pay grade is a CStaff::PayGrade on the Mac
// (an int32_t here), the cell is the HIGH halfword of the register.
class CResearcher {
public:
    CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t payGrade, uint8_t flag, int32_t value);   // 0x10120eec (176 bytes, unit w188)

    // Unit w123a: direct dispatch callees (unit w188 translates them).
    void UpdateTurn();   // 0x10121340 (140 bytes)
    void ModelState();   // 0x101213fc (148 bytes)
    void FinalDestroy();   // 0x1012123c (120 bytes)
    void ReceiveMessage(const CMsg& message);   // 0x101212e4 (36 bytes), kind 8

    // Unit w188 (src/Researcher.cpp).
    explicit CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x10120e34 (100 bytes): the sprite index comes from ESP_GetRandomSpritesFrom
    void Init(uint8_t sprite);   // 0x10121004 (180 bytes)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x101210e4 (292 bytes)
    void ChooseNextAction();   // 0x101214c0 (220 bytes)
    void ProcessPatrolling();   // 0x101215d0 (320 bytes)
    void ProcessResearching();   // 0x10121748 (216 bytes)
    void ProcessIdle();   // 0x10121858 (324 bytes)
    void DoSomeResearch();   // 0x101219cc (136 bytes)

    CThing mThing;                        // 0x000 (mCellX +4, mCellY +6)
    uint8_t mScript[0xc];                 // 0x00c the CDelayedScript (mSpriteScript at +0xc): reached as Script()
    uint8_t mF18[8];                      // 0x018
    int32_t mSpriteIndex;                 // 0x020 the sprite index Init is given
    int32_t mSpriteSet;                   // 0x024 an ESP_Sprites (8)
    uint8_t mF28[0xd4 - 0x28];            // 0x028
    uint8_t mVehicle[0x19c - 0xd4];       // 0x0d4 the Vehicle subobject (destroyed by ~CStaff)
    int32_t mState;                       // 0x19c
    char16_t mName[0x20];                 // 0x1a0
    uint8_t mF1E0[4];                     // 0x1e0
    int32_t mGrade;                       // 0x1e4 the pay grade (0 to 4)
    uint8_t mF1E8[0x1fc - 0x1e8];         // 0x1e8
    uint32_t mStateTurn;                  // 0x1fc
    uint8_t mF200[0x20c - 0x200];         // 0x200
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mNext;             // 0x20c the next researcher in the world's list (the Serialise name)
    uint8_t mF20E[2];                     // 0x20e
    uint32_t mTimeStartedResearching;     // 0x210 the Serialise name; the turn the current research started
    CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(mScript); }
};
static_assert(sizeof(CResearcher) == 0x214, "CResearcher is 0x214 bytes");
static_assert(offsetof(CResearcher, mScript) == 0xc, "mScript at 0xc");
static_assert(offsetof(CResearcher, mSpriteIndex) == 0x20, "mSpriteIndex at 0x20");
static_assert(offsetof(CResearcher, mSpriteSet) == 0x24, "mSpriteSet at 0x24");
static_assert(offsetof(CResearcher, mVehicle) == 0xd4, "mVehicle at 0xd4");
static_assert(offsetof(CResearcher, mState) == 0x19c, "mState at 0x19c");
static_assert(offsetof(CResearcher, mName) == 0x1a0, "mName at 0x1a0");
static_assert(offsetof(CResearcher, mGrade) == 0x1e4, "mGrade at 0x1e4");
static_assert(offsetof(CResearcher, mStateTurn) == 0x1fc, "mStateTurn at 0x1fc");
static_assert(offsetof(CResearcher, mNext) == 0x20c, "mNext at 0x20c");
static_assert(offsetof(CResearcher, mTimeStartedResearching) == 0x210, "mTimeStartedResearching at 0x210");
// Unit w188 (src/Researcher.cpp): the two words __sinit_Researcher_cpp stores (names invented; nothing reads either).
extern uint32_t gResearcherSinitLow;    // 0x1034c4b4 __sinit_Researcher_cpp stores 0
extern uint32_t gResearcherSinitHigh;   // 0x1034c4b0 ... and 0x10000
class CStaffHQ {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x1012ac08 (520 bytes)
    // Unit w143 (untranslated, 0x1012c168): puts a picked up staff member down in the cell (the 1 based map cell id, the HIGH halfword of the register).
    void DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    // Unit w190 (untranslated): SackMe-side calls of the staff member's queries.
    int32_t IsStriking(StaffType type);   // 0x1012bde0
    // Unit w191 (untranslated): CStaff::ProcessStriking asks the headquarters to end the strike once the park is closed and empty.
    void StopStrikeInProgress(StaffType type);   // 0x1012be24 (20 bytes)
    void PickUpStaff(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff);   // 0x1012c100 (the id is the HIGH halfword of the register)

    // Unit v5037: toggles the forced strike flag (mF64) for the staff type (src/ParkMessages_CStaffHQ_DropStaff.cpp).
    void ForceStrike(StaffType type);   // 0x1012c04c (128 bytes)

    // Unit v5681 (src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_ConsiderStriking.cpp): the monthly strike check for one staff type.
    void ConsiderStriking(StaffType type);   // 0x1012b084 (1076 bytes)
    int32_t StaffTypeHasAGrievance(StaffType type);   // 0x1012b56c (2100 bytes, stand-in)

    // Unit w123a: direct dispatch callees, untranslated.
    void FinalDestroy();   // 0x1012a998
    CStaffHQ();   // 0x1012a7fc, 304 bytes (0x68 bytes on the Mac; src/ThingCtors.cpp, unit w122a)
    void ReceiveMessage(const CMsg& message);   // 0x1012a9e8, 492 bytes, kind 9

    // Unit v5918 (src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise.cpp).
    void UpdateMonthly();   // 0x1012ae40 (532 bytes)
    // 0x1012be78 (368 bytes; hidden result pointer): the id of the available guard nearest to this object's own cell (0 when none).
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindNearestAvailableGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);

    struct StrikeType {   // 0xc bytes. CStaffHQ::StrikeType::StrikeType 0x1012a950 (20 bytes): three words cleared
        StrikeType() {
            mF00 = 0;
            mF04 = 0;
            mF08 = 0;
        }
        uint32_t mF00, mF04, mF08;
    };
    CThing mBase;               // 0x00  model 9
    CTrainingCentre mTraining;  // 0x0c  (0x18 bytes)
    StrikeType mStrikes[5];     // 0x24
    uint16_t mF60;              // 0x60  cleared
    uint32_t mF64;              // 0x64  cleared
};
static_assert(sizeof(CStaffHQ) == 0x68, "CStaffHQ is 0x68 bytes");
static_assert(offsetof(CStaffHQ, mTraining) == 0xc, "CStaffHQ::mTraining at 0xc");
static_assert(offsetof(CStaffHQ, mStrikes) == 0x24, "CStaffHQ::mStrikes at 0x24");
static_assert(offsetof(CStaffHQ, mF60) == 0x60, "CStaffHQ::mF60 at 0x60");
static_assert(offsetof(CStaffHQ, mF64) == 0x64, "CStaffHQ::mF64 at 0x64");
// Unit v51002 (src/ParkMessages_SAVEVARSTRUCT_3.cpp).
template<class Value> int32_t LOADVARSTRUCT(void* file, Value* value, const char* name, int32_t noLog);
template<class Value> int32_t SAVEVARSTRUCT(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t SAVEVARSTRUCT<CStaffHQ::StrikeType>(void*, CStaffHQ::StrikeType*, const char*, int32_t);   // 0x1012c3d4, 264 B
template<> int32_t LOADVARSTRUCT<CStaffHQ::StrikeType>(void*, CStaffHQ::StrikeType*, const char*, int32_t);   // 0x1012c27c, 252 B
// Unit v5918: the prank message CStaffHQ::ReceiveMessage reads (type 14): the perp's thing id at +8 on the Mac.
class CMsgStaffHQPrank : public CMsg {
public:
    uint16_t mPerp;   // 0x08
};
// Unit v5918: halfwords of initialised data (zero), defined in src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise.cpp (names invented).
extern uint16_t gStaffHQNoGuard;        // 0x102a11bc
extern uint16_t gStaffHQNoStaff[5];     // 0x102a11be .. 0x102a11c6, indexed by staff type
extern uint16_t gStaffHQGuardListEnd;   // 0x102a11dc
// 0x102a11c8 .. 0x102a11da (names invented; src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise_CStaffHQ_StaffTypeHasAGrievance.cpp): the id that ends
// each staff list StaffTypeHasAGrievance walks, two per staff type (fatigue, then unhappiness); zero at start.
extern uint16_t gStaffHQGrievanceEnd[10];
// Unit v5681: the lazy creation of the message centre, split out so the test library can fake it. The src file gives weak
// definitions for the program (operator new, then the 29 header constructors that __construct_array runs).
namespace staffhq_callee {
void* OperatorNew(uint32_t size);          // 0x102384b8
void ConstructSubscriptions(void* mem);    // 0x10238758 __construct_array(mem, ctor, dtor, 0x10, 0x1d)
}  // namespace staffhq_callee
class CMechanicHQ {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10105348 (212 bytes)

    // Unit w123a: direct dispatch callees, untranslated.
    void FinalDestroy();   // 0x101052f4
    CMechanicHQ();   // 0x1010521c, 176 bytes (0xe bytes; src/ThingCtors.cpp, unit w122a)
    void ReceiveMessage(const CMsg& message);   // 0x10105988, 876 bytes, kind 10
    // Unit w187 (untranslated): the mechanic's next job, the ride to repair (0 for none). The Mac answers the two-byte id through a hidden pointer.
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetNewJob(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mechanic);   // 0x10105450 (876 bytes)
    // Unit w190 (untranslated): DropMe tells it where a mechanic was dropped.
    void ReassignJobsAfterDrop(CMechanic& mechanic, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x10105818

    CThing mBase;    // 0x00  model 10
    uint16_t mF0C;   // 0x0c  cleared
};
static_assert(sizeof(CMechanicHQ) == 0xe, "CMechanicHQ is 0xe bytes");
class CTagSystem {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x1012c64c (56 bytes)

    // Unit w123a: direct dispatch callees, untranslated.
    void FinalDestroy();   // 0x1012c5f8
    CTagSystem();   // 0x1012c57c, 84 bytes (0x80e bytes; src/ThingCtors.cpp, unit w122a)
    void ReceiveMessage(const CMsg& message);   // 0x1012c90c, 760 bytes, kind 12
    void Init();    // 0x1012c6b8 (556 bytes, untranslated): subscribes to the message types 7, 0xc, 0x14, 0x18, 0x19 and 0x13; writes no field

    // Unit v5626: 0x1012cc3c (4308 bytes, src/ThingLoop_CTagSystem_FormatTagText.cpp). The tag's text into mF0C (0x400 wide
    // characters): a NULL element gives the empty text and 0; otherwise by the element's format (0 to 0x24, a jump table) the UI
    // string table formats the element's text id with integer and string tags built from `value`, the balance table or the
    // challenge. Answers 0 when the thing or the ride type is the "none" id or the thing is gone, else 1. The thing id rides in
    // the high half of its register.
    int32_t FormatTagText(const struct TagTableElement* element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                          uint32_t value, const class CMsgChallenge* challenge);

    CThing mBase;                    // 0x00  model 12
    uint8_t mF0C[0x80e - 0xc];       // 0x0c  never written by the constructor
};
static_assert(sizeof(CTagSystem) == 0x80e, "CTagSystem is 0x80e bytes");
// Unit v5626: an entry of the tag table FormatTagText reads (only the two words it reads are known).
struct TagTableElement {
    uint32_t mF00[3];   // 0x00  not known
    uint32_t mTextId;   // 0x0c  the UI string table's id of the text
    uint32_t mF10[3];   // 0x10  not known
    uint32_t mFormat;   // 0x1c  which tags the text takes (0 to 0x24)
};
extern uint16_t gTagNoThingId;    // 0x102a1214, original name unknown: the thing id FormatTagText's format 1 refuses
extern uint16_t gTagNoRideType;   // 0x102a1216, original name unknown: the ride type FormatTagText's format 2 refuses
// Unit v5960 (src/MsgDeath_CThoughts_Serialise_LOADVARSTRUCT_3.cpp). The tag table: 0x17e elements of 0x20 bytes at 0x102a12c0, the first word of
// element i is normally i. Elements 0x17e and 0x17f are never answered. The bytes at +4 (word), +8 (halfword), +0x10, +0x14, +0x18 (words) are copied into the tag.
extern TagTableElement gTagTable[0x17e];   // 0x102a12c0, original name unknown
class CTagTable {
public:
    // 0x1012dd94 (144 bytes): the element whose id is `id` (the slot `id` first, else a search), NULL for 0x17e, 0x17f or no match (CTRACE 3).
    // Ids past 0x17f read beyond the table, as the original does.
    const TagTableElement* GetElement(int32_t id) const;
};
// The messages CTagSystem::ReceiveMessage reads (invented names; Mac offsets in comments, the host's vptr is wider).
class CMsgTagShow : public CMsg {   // type 0x18
public:
    int32_t mTag;       // 0x08
    uint16_t mThing;    // 0x0c
    int32_t mTime;      // 0x10  negative: the element's own time
    uint32_t mValue;    // 0x14
};
class CMsgTagSimple : public CMsg {   // type 0x13
public:
    int32_t mCode;      // 0x08  7, 8 or 9 select a tag
};
// Unit v5960: 0x1012f5a8 (244 bytes). The file read of an action history entry; the two halfwords are byte swapped in place.
template<class Value> int32_t LOADVARSTRUCT(void* file, Value* value, const char* name, int32_t noLog);
template<> int32_t LOADVARSTRUCT<CThoughts::EventHistType>(void*, CThoughts::EventHistType*, const char*, int32_t);   // 0x1012f5a8
extern int32_t gTbErrorZero;   // 0x102a4400, original name unknown: the word the TbError comparisons test against zero (never written)
enum ResearchCat : int32_t {};   // unit w151: the research lab's category argument (0 rides, 1 shops, 2 sideshows, 3 features, 5 all; names mine)
class CResearchLab {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10121ce8 (744 bytes)

    CResearchLab();   // 0x10121ad0, 128 bytes (0x146c bytes; src/ThingCtors.cpp, unit w122a)
    void Init();      // 0x10121bd4 (232 bytes, src/ResearchLab_init.cpp, unit w130): CWorld::SetupNewLevel calls it on the world's research lab
    void SetupProjects();     // 0x10122004 (368 bytes, unit w130)
    void SetupCategories();   // 0x101221a8 (208 bytes, unit w130)
    void ReceiveMessage(const CMsg& message);   // 0x10123144, 716 bytes, kind 14

    // Unit w211 (src/ResearchLab_projects.cpp): the projects, the efforts and the outputs. A category is 0 to 4 (a group: 4 is the statistics
    // upgrades), 5 means all of them where a function accepts it; the group arguments index the five categories unchecked (as the Mac does).
    void FinalDestroy();                                          // 0x10121b7c (36 bytes): the thing's own FinalDestroy
    void ContributeResearch(uint32_t points);                     // 0x101222ac (260 bytes; the Mac's unsigned long): the points shared over the categories by effort
    void NextProject(ResearchCat category);                       // 0x101223e8 (460 bytes): the category's next open project (wraps to the first)
    void PreviousProject(ResearchCat category);                   // 0x101225f0 (460 bytes): the category's previous open project (wraps to the last)
    uint32_t GetEffort(uint32_t group) const;                     // 0x101227fc (16 bytes)
    uint32_t GetTotalEffortFromActiveCategories() const;          // 0x1012283c (148 bytes): the efforts of the categories that have a project not yet done
    uint32_t SetEffort(int32_t group, uint32_t effort);           // 0x10122918 (76 bytes): at most 100; answers the stored effort
    void SetWorkLoad(uint32_t load);                              // 0x10122998 (56 bytes): at most 100 (mF1460)
    uint32_t GetResearchSpeed(ResearchCat category) const;        // 0x10122a04 (172 bytes): the category's output scaled by the researchers' ability
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> GetCurrentProject(ResearchCat category) const;   // 0x10122af4 (68 bytes): the object id, 0 for none
    int32_t GetEstimatedCostForCurrentProject(ResearchCat category) const;   // 0x10122b7c (196 bytes)
    uint32_t GetRemainingPoints(ResearchCat category) const;      // 0x10122c94 (100 bytes): the percentage left of the category's current project
    int32_t GetNumberLeftToResearch(ResearchCat category) const;  // 0x10122d3c (160 bytes; unit w151 calls it): the projects of the category not yet researched
    void UpdateHighestGroup(int32_t group);                       // 0x10122e28 (728 bytes): raises the category's level while enough of it is done
    bool IsAllResearchDone() const;                               // 0x101234d0 (112 bytes)
    void CheatCompleteAll();                                      // 0x10123578 (148 bytes): every project's object becomes available
    void CheatCompleteOnlyPaidFor();                              // 0x10123644 (228 bytes): available exactly for the projects that are done
    bool IsCategoryActive(ResearchCat category) const;            // 0x10123768 (100 bytes)
    void UpdateActiveStatus();                                    // 0x10123810 (336 bytes; unit w188's researcher calls it): mHaveMechanic follows the researcher list
    void CalculateOutputs();                                      // 0x10123998 (140 bytes)
    uint32_t GetResearchOutputFromCategory(ResearchCat category) const;   // 0x10123a5c (16 bytes)

    CThing mBase;                          // 0x00  model 14
    CResearchProject mProjects[0x100];     // 0x0c  (0x14 bytes each; the constructors write nothing)
    CResearchCategory mCategories[5];      // 0x140c
    int32_t mProjectCount;                 // 0x145c  "mProjectCount": set to 0 by Init, counted by SetupProjects
    int32_t mF1460;                        // 0x1460  the work load, 0 to 100 (Init: the balance word at 0x540; SetWorkLoad; the share of the output)
    int32_t mBestProject;                  // 0x1464  index of the project with the highest score, -1 for none
    bool mHaveMechanic;                    // 0x1468  the lab's active flag (Init: NumberOfStaffType(8) > 0; UpdateActiveStatus: the researcher list is not empty)
    uint8_t mPad1469[3];                   // 0x1469
};
static_assert(sizeof(CResearchLab) == 0x146c, "CResearchLab is 0x146c bytes");
static_assert(offsetof(CResearchLab, mCategories) == 0x140c, "CResearchLab::mCategories at 0x140c");
static_assert(offsetof(CResearchLab, mProjectCount) == 0x145c, "CResearchLab::mProjectCount at 0x145c");
static_assert(offsetof(CResearchLab, mBestProject) == 0x1464, "CResearchLab::mBestProject at 0x1464");
static_assert(offsetof(CResearchLab, mHaveMechanic) == 0x1468, "CResearchLab::mHaveMechanic at 0x1468");
// Unit v5024. The message of kind 0x11 that CResearchLab::ReceiveMessage takes (a ride became available; the name is mine). Mac: the object id
// halfword at +8, the category word at +0xc. The host layout puts them after the vptr and flags (+0xc, +0x10).
#include "CMsg.h"
class CMsgRideResearched : public CMsg {
public:
    uint16_t mObjectId;     // Mac +0x08
    int32_t mCategory;      // Mac +0x0c
};
// 0x10151d94 (32 bytes, untranslated stand-in): the category-updated message the lab posts. Mac: category word at +8.
class CMsgResearchCategoryUpdated : public CMsg {
public:
    explicit CMsgResearchCategoryUpdated(ResearchCat category);   // 0x10151d94
    int32_t GetType() const override { return 0; }                // the real message type is not recovered
    ResearchCat mCategory;  // Mac +0x08
};
// Unit w124: types the weather turn uses.
enum PrecipitationType : int32_t {};
// Unit w181: the save name of the Mac's template argument of LOADVARSTRUCT / SAVEVARSTRUCT at 0x10139b90 / 0x10139a04 (twelve bytes: the lightning
// location CWeather::Serialise saves at +0x48; the PC original is presumably a D3DVECTOR with a byte swap). Layout read from the twelve-byte
// read and the three swapped words.
struct D3DVECTOR_SWAPABLE { float x, y, z; };
static_assert(sizeof(D3DVECTOR_SWAPABLE) == 12, "D3DVECTOR_SWAPABLE is 12 bytes");
class TbTimeStamp;
#include "platform/d3d_types.h"
class CWeather {
public:

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x10138670
    void FinalDestroy();   // 0x10137f3c, 52 bytes (src/Weather.cpp, unit w181)
    CWeather();   // 0x10137da0, 376 bytes (0x68 bytes; src/ThingCtors.cpp, unit w122a)
    void ReceiveMessage(const CMsg& message);   // 0x1013855c, 224 bytes, kind 15 (src/Weather.cpp, unit w181)
    // 0x10138e70 (352 bytes, untranslated): the new weather quality (the name is inferred); it draws one to three random numbers.
    uint32_t FindNewQuality() const;
    // Unit w124 (src/Weather_turn.cpp): untranslated callees of UpdateTurn, and the two stamps it compares (held as plain words, see TbTimeDiff.h).
    void SetPrecipitationType(PrecipitationType type);   // 0x10139388, 256 bytes (src/Things2_small.cpp, unit w130)
    void FindLightningCoords(D3DVECTOR& start, D3DVECTOR& end, int32_t strikesLeft);   // 0x10138c10, 528 bytes (src/Weather.cpp, unit w181; strikesLeft unused)
    TbTimeStamp* ForecastTime() { return reinterpret_cast<TbTimeStamp*>(&mForecast); }   // the forecast's time, +0x20
    TbTimeStamp* StampF10() { return reinterpret_cast<TbTimeStamp*>(&mF10); }            // +0x10
    // Unit w181 (src/Weather.cpp)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10137f9c, 1424 bytes
    void SetQuality(int32_t quality);   // 0x10139000, 860 bytes
    void SetWindDirection(float direction);   // 0x101394d0, 168 bytes
    void SetWindForce(float force);   // 0x101395a8, 48 bytes
    void UpdateWindInfoToEngine();   // 0x10139604, 56 bytes
    TbTimeStamp* StampF18() { return reinterpret_cast<TbTimeStamp*>(&mF18); }   // +0x18  mLastForecasted
    D3DVECTOR_SWAPABLE* LastLightningLocation() { return reinterpret_cast<D3DVECTOR_SWAPABLE*>(&mF48); }   // +0x48

    CThing mBase;               // 0x00  model 15
    uint32_t mF0C;              // 0x0c  mQuality (the save name; an int)
    uint32_t mF10, mF14, mF18, mF1C;   // 0x10  mLastChanged (mF10/mF14) and mLastForecasted (mF18/mF1C), TbTimeStamps; cleared first
    CWeatherForecast mForecast; // 0x20  (0x10 bytes)
    uint32_t mF30, mF34;        // 0x30  mCurrentDrops, mTargetDrops (SetQuality stores the target); cleared
    uint8_t mF38;               // 0x38  mLightning (bool); cleared
    uint32_t mF3C, mF40, mF44;  // 0x3c  mLightningCountdown, mLightningNumThisPeriod, mThunderCountdown; cleared
    float mF48, mF4C, mF50;     // 0x48  mLastLightningLocation (D3DVECTOR_SWAPABLE); never written by the constructor
    uint32_t mF54;              // 0x54  mRainSoundHandle (a MacLong in the save); cleared
    uint32_t mF58;              // 0x58  mPType (PrecipitationType); never written by the constructor
    float mWindDirection;       // 0x5c  mWindDir (the save name); 0.0f, then 90.0f (src/DynamicCells.cpp reads it as degrees)
    float mWindForce;           // 0x60  mWindForce; 0.5f
    uint8_t mF64;               // 0x64  mOverridden (bool; ReceiveMessage ignores a quality message when set); cleared
};
static_assert(sizeof(CWeather) == 0x68, "CWeather is 0x68 bytes");
static_assert(offsetof(CWeather, mForecast) == 0x20, "CWeather::mForecast at 0x20");
static_assert(offsetof(CWeather, mF30) == 0x30, "CWeather::mF30 at 0x30");
static_assert(offsetof(CWeather, mF48) == 0x48, "CWeather::mF48 at 0x48");
static_assert(offsetof(CWeather, mWindDirection) == 0x5c, "CWeather::mWindDirection at 0x5c");
static_assert(offsetof(CWeather, mWindForce) == 0x60, "CWeather::mWindForce at 0x60");
static_assert(offsetof(CWeather, mF64) == 0x64, "CWeather::mF64 at 0x64");
class CBankAccount {
public:

    // Unit w123a: direct dispatch callees, untranslated.
    void FinalDestroy();   // 0x100f2cb8 (36 bytes, src/BankAccount_loans.cpp, unit w177)
    CBankAccount();   // 0x100f29f4, 664 bytes, src/BankAccount_ctor.cpp, unit w122b (0x128 bytes)
    void ReceiveMessage(const CMsg& message);   // 0x100f3564, 264 bytes, kind 16 (src/BankAccount_loans.cpp, unit w177)
    void SetAdmissionFee(uint32_t fee);   // 0x100f3a40 (80 bytes, src/BankAccount_ctor.cpp, unit w122b)
    void MakeWithdrawal(uint32_t amount);   // 0x100f33b0 (172 bytes, src/Things2_small.cpp, unit w130; unit w126: CObject::CObject pays the ride's cost of upgrade with it)
    int32_t GetBalance() const;   // 0x100f32b4 (src/ParkMessages.cpp, unit w161; unit w139's CTrackCell::ModifyType asks it): mBalance
    void MakeDeposit(uint32_t amount);   // 0x100f32ec (144 bytes, src/BankAccount_loans.cpp, unit w177)
    // Unit w177 (src/BankAccount_loans.cpp)
    // 0x100f3490 (160 bytes): the funny time since the balance went negative (ticks 0 when the last balance is not negative).
    TbTimeDiff BeenInTheRedFor();
    // 0x100f3ac8 (212 bytes): deposits the admission fee, adds it to the admissions history and tells the analyser a visitor paid it.
    void PayAdmission();
    // 0x100f3bd0 (56 bytes): the number of the eight loans bought.
    int32_t GetNumberOfLoansBought();
    // 0x100f3db8 (244 bytes): buys the loan (false, with a trace, when already bought) and deposits its amount; no bound test on `loan`.
    bool ApplyForLoan(uint32_t loan);
    // 0x100f4278 (52 bytes): the instalments still to pay on a bought loan (0 when not bought); no bound test on `loan`.
    uint32_t OutstandingOnLoan(uint32_t loan) const;
    // 0x100f42e4 (124 bytes): the bought loan with the least outstanding (the first of equals), 0xffffffff for none.
    uint32_t GetCheapestLoanTaken() const;
    // 0x100f439c (8 bytes): mProfitThisYear.
    int32_t GetProfitThisYear() const;
    // 0x100f43dc (84 bytes): the sum of OutstandingOnLoan over the eight loans.
    uint32_t TotalOutstandingOnLoans() const;
    // Unit w173 (src/BankAccount.cpp)
    // 0x100f2d10 (1,392 bytes): CThing::Serialise, then (versions 0 and 1 only) the seven account words and the eight loans' eight words, by the save flag.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
    // 0x100f36a8 (752 bytes): adds the batch to the balance, pays every bought loan's monthly instalment, and posts a CMsgEvent(2) once six months in the red.
    void MonthlyUpdate();
    // 0x100f3c44 (320 bytes): refreshes and returns (by value) the loan's offer: available when its amount fits the park value less the debt of the bought loans.
    CLoan GetLoanInfo(uint32_t loan);
    // 0x100f3ee0 (432 bytes): true when some unbought loan is available (GetLoanInfo of each).
    bool AnyLoanAvailable();
    // 0x100f40c8 (384 bytes): pays off a bought loan at the cost of its remaining instalments; false (with a trace) when not bought or unaffordable.
    bool PayOffLoan(uint32_t loan);
    CThing mBase;                    // 0x00
    int32_t mBalance;                // 0x0c  "mBalance": the cash (GetBalance 0x100f32b4); starts as InitialCash
    int32_t mBatchBalance;           // 0x10  "mBatchBalance": income added at the monthly update; 0 at construction
    CLoan mLoans[8];                 // 0x14  "mLoans[loan].xxx"
    int32_t mWithdrawalsEnabled;     // 0x114 "mWithdrawalsEnabled": 1 (an int in Serialise; MakeWithdrawal acts only when nonzero)
    uint32_t mAdmissionFee;          // 0x118 "mAdmissionFee"
    int32_t mLastBalance;            // 0x11c "mLastBalance": below 0 while in the red
    uint32_t mTurnEnteredRed;        // 0x120 "mTurnEnteredRed": the world turn counter when the balance went negative
    int32_t mProfitThisYear;         // 0x124 "mProfitThisYear"
};
static_assert(sizeof(CBankAccount) == 0x128, "CBankAccount size");
static_assert(offsetof(CBankAccount, mBalance) == 0xc, "CBankAccount::mBalance");
static_assert(offsetof(CBankAccount, mLoans) == 0x14, "CBankAccount::mLoans");
static_assert(offsetof(CBankAccount, mWithdrawalsEnabled) == 0x114, "CBankAccount::mWithdrawalsEnabled");
static_assert(offsetof(CBankAccount, mProfitThisYear) == 0x124, "CBankAccount::mProfitThisYear");

class CUIMsgReceiver {
public:
    // Unit w205 (untranslated): a stand-in called by CWorld::Save.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10130928 (56 bytes)

    // Unit w123a: direct dispatch callees, untranslated.
    void FinalDestroy();   // 0x101308d0
    CUIMsgReceiver();   // 0x101305b0, 756 bytes (0xc bytes; src/ThingCtors.cpp, unit w122a)
    void ReceiveMessage(const CMsg& message);   // 0x10130998, 364 bytes, kind 17

    CThing mBase;   // 0x00  model 17: the whole object
};
static_assert(sizeof(CUIMsgReceiver) == 0xc, "CUIMsgReceiver is 0xc bytes");
class COnlinePerson {
public:

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x10212ff0
    void ModelState();   // 0x10213080
    void FinalDestroy();   // 0x10212f58
    void ReceiveMessage(const CMsg& message);   // 0x10212fb0, 4 bytes, kind 18
    // Unit v5121 (untranslated; the unit's test library fakes them).
    bool IsThisMyName(std::u16string& name) const;   // 0x10213124
    int32_t SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);   // 0x102131e0
    int32_t SetDest(MapPos x, MapPos y);   // 0x102132a0
    void Teleport(MapPos x, MapPos y);   // 0x1021334c
    // Unit v5133 (src/COnlinePerson_SetDest.cpp): fields by Mac offset: +4/+6 MapPos, +0xc CDelayedScript, +0x19c name, +0x39c state, +0x3a0 own-avatar byte.
    void ProcessIdle();             // 0x102133e4 (4 bytes, untranslated)
    void ProcessWalkingToDest();    // 0x1021341c (240 bytes)
};
extern uint16_t gNullThingId;   // 0x102b1d64 (unit v5121; original name unknown): the used list's end marker, defined in src/COnlineControl_..._EarmuffsOff.cpp
class CChallengeHQ {
public:

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x100f6b98
    void FinalDestroy();   // 0x100f6b60 (translated in src/ChallengeHQ_progress.cpp, unit w175)
    CChallengeHQ();   // 0x100f6640, 416 bytes, src/ChallengeHQ_ctor.cpp, unit w122b (0x3e4 bytes)
    void ReceiveMessage(const CMsg& message);   // 0x100f75e4, 1656 bytes, kind 19 (translated in src/ChallengeHQ_progress.cpp, unit w175)
    void Init();                     // 0x100f6904 (412 bytes, src/ChallengeHQ_init.cpp, unit w123b)
    // Unit w130 (src/ChallengeHQ_offer.cpp): AddChallenge, ClearInvalidFollowups, IsSystemOn and OfferNextChallenge are translated.
    bool TellPlayerChallengeOffered(CChallenge* challenge, bool force);   // 0x100f8194 (translated in src/ChallengeHQ_progress.cpp, unit w175)
    // Unit w123b: untranslated (launcher stand-ins). Init builds a CChallenge for each of the level's challenge rows and hands it to AddChallenge, then
    // calls ClearInvalidFollowups once.
    void AddChallenge(CChallenge& challenge);   // 0x100f7498 (164 bytes)
    void ClearInvalidFollowups();               // 0x100f680c (188 bytes)
    // Unit w124 (src/ChallengeHQ_turn.cpp): untranslated callees of UpdateTurn.
    bool IsSystemOn();                          // 0x100f7c98
    CChallenge* GetCurrentChallenge();          // 0x100f757c (translated in src/ChallengeHQ_progress.cpp, unit w175)
    void OfferNextChallenge(bool force);        // 0x100f7dd8
    void EndCurrentChallenge();                 // 0x100f86d8 (translated in src/ChallengeHQ_progress.cpp, unit w175)
    void WonCurrentChallenge();                 // 0x100f87d4 (translated in src/ChallengeHQ_progress.cpp, unit w175)
    // Unit w175 (src/ChallengeHQ_progress.cpp)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100f6fe0 (784 bytes)
    void AddToCounter(ChallengeType type, int32_t amount, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj,
                      tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing);   // 0x100f7324 (216 bytes)
    bool OfferFollowup();                       // 0x100f7fac (436 bytes)
    void ChallengeAccepted();                   // 0x100f83cc (312 bytes)
    void ChallengeDeclined();                   // 0x100f853c (356 bytes)
    void LostCurrentChallenge();                // 0x100f8b20 (544 bytes)
    TbTimeStamp* NextEventTime() { return reinterpret_cast<TbTimeStamp*>(&mNextChallengeEventTime); }   // +0x3d0
    CThing mBase;                    // 0x00
    CChallenge mChallenges[0x14];    // 0x0c  (0x30 each; ends at 0x3cc)
    int32_t mCurrentChallenge;       // 0x3cc "mCurrentChallenge": an index into mChallenges, -1 for none
    uint32_t mNextChallengeEventTime[2];   // 0x3d0 "mNextChallengeEventTime": a TbTimeStamp held as two words
    bool mSystemSwitchedOn;          // 0x3d8 "mSystemSwitchedOn"
    bool mChallengeOn;               // 0x3d9 "mChallengeOn"
    bool mWaitingForReply;           // 0x3da "mWaitingForReply"
    int32_t mValue;                  // 0x3dc "mValue"
    uint16_t mActualThing;           // 0x3e0 "mActualThing" (tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>)
};
static_assert(sizeof(CChallengeHQ) == 0x3e4, "CChallengeHQ size");
static_assert(offsetof(CChallengeHQ, mChallenges) == 0xc, "CChallengeHQ::mChallenges");
static_assert(offsetof(CChallengeHQ, mCurrentChallenge) == 0x3cc, "CChallengeHQ::mCurrentChallenge");
static_assert(offsetof(CChallengeHQ, mNextChallengeEventTime) == 0x3d0, "CChallengeHQ::mNextChallengeEventTime");
static_assert(offsetof(CChallengeHQ, mSystemSwitchedOn) == 0x3d8, "CChallengeHQ::mSystemSwitchedOn");
static_assert(offsetof(CChallengeHQ, mValue) == 0x3dc, "CChallengeHQ::mValue");
static_assert(offsetof(CChallengeHQ, mActualThing) == 0x3e0, "CChallengeHQ::mActualThing");
// Unit v5889 (src/Mechanic_CMechanicHQ_GetNewJob.cpp): the halfwords the mechanic HQ compares thing ids with (all zero; original names unknown).
extern uint16_t gMechHQId102a0b9a;   // 0x102a0b9a  GetNewJob: the cursor means "start from the head"
extern uint16_t gMechHQId102a0b9c;   // 0x102a0b9c  GetNewJob: end of the list walk looking for the cursor
extern uint16_t gMechHQId102a0b9e;   // 0x102a0b9e  GetNewJob: the walk ended without finding the cursor
extern uint16_t gMechHQId102a0ba0;   // 0x102a0ba0  GetNewJob: the cursor is at the end, no job
extern uint16_t gMechHQId102a0ba2;   // 0x102a0ba2  GetNewJob: the scan wraps to the head
extern uint16_t gMechHQId102a0ba4;   // 0x102a0ba4  GetNewJob: the ride has no staff member assigned
extern uint16_t gMechHQId102a0ba6;   // 0x102a0ba6  ReassignJobsAfterDrop: end of the object list
extern uint16_t gMechHQId102a0ba8;   // 0x102a0ba8  ReassignJobsAfterDrop: the ride has no staff member assigned
extern uint16_t gMechHQId102a0baa;   // 0x102a0baa  ReceiveMessage: the mechanic list is empty
extern uint16_t gMechHQId102a0bac;   // 0x102a0bac  ReceiveMessage: end of the mechanic list
extern uint16_t gMechHQId102a0bae;   // 0x102a0bae  ReceiveMessage: a mechanic with no ride to repair

// Unit v51052 (src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise_CStaffHQ_PickUpStaff.cpp). The static initialisers store 0x10000 in the first
// word and 0 in the second (names invented). The destructors are the Mac's deleting forms as free functions (flag in r4, answer this).
namespace v51052_callee { void OperatorDelete(void* block); }   // 0x10238218
void UIMain_DestroyWString(void* object);   // 0x103bcbb8
extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
extern uint32_t gStaffHQSinitHigh;         // 0x1034c6e8
extern uint32_t gStaffHQSinitLow;          // 0x1034c6ec
extern uint32_t gThingSinitHigh;           // 0x1034c6f0
extern uint32_t gThingSinitLow;            // 0x1034c6f4
extern uint32_t gTrainingCentreSinitHigh;  // 0x1034c6f8
extern uint32_t gTrainingCentreSinitLow;   // 0x1034c6fc
extern uint32_t gVector2dSinitHigh;        // 0x1034c700
extern uint32_t gVector2dSinitLow;         // 0x1034c704
extern uint8_t gResearchLabString[];       // 0x1034c4b8 the wide string __sinit_ResearchLab_cpp builds
extern uint8_t gResearchLabStringNode[];   // 0x1034c4cc its __register_global_object record
void __sinit_StaffHQ_cpp();         // 0x1012c538
void __sinit_Thing_cpp();           // 0x1012ec48
void __sinit_TrainingCentre_cpp();  // 0x10130564
void __sinit_vector2d_cpp();        // 0x10130e70
void __sinit_ResearchLab_cpp();     // 0x101247c4
void EventHistType_Ctor(CThoughts::EventHistType* self);   // 0x1012ed48 (blr)
struct Vector2d;
class Follow_Path;
class Avoid_Walls;
class Repel_Obstacle;
class Avoid_Obstacle;
class Separation;
int32_t CMsgResearchCategoryUpdated_GetType(const CMsgResearchCategoryUpdated* self);   // 0x10124510: 0x12
CMsgResearchCategoryUpdated* CMsgResearchCategoryUpdated_Dtor(CMsgResearchCategoryUpdated* self, int16_t flag);   // 0x1012344c
Vector2d* Vector2d_Dtor(Vector2d* self, int16_t flag);              // 0x10130db4
Follow_Path* Follow_Path_Dtor(Follow_Path* self, int16_t flag);           // 0x101351ec
Avoid_Walls* Avoid_Walls_Dtor(Avoid_Walls* self, int16_t flag);           // 0x1013526c
Repel_Obstacle* Repel_Obstacle_Dtor(Repel_Obstacle* self, int16_t flag);  // 0x101352ec
Avoid_Obstacle* Avoid_Obstacle_Dtor(Avoid_Obstacle* self, int16_t flag);  // 0x10135370
Separation* Separation_Dtor(Separation* self, int16_t flag);              // 0x101353f4
