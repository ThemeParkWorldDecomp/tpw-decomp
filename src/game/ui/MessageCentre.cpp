// MessageCentre.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of MessageCentre code, named after it.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CBalanceTable.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "DateMessages.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "InterfaceFontWrapper.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Main.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "MsgAdvisor.h"
#include "MsgCheatMode.h"
#include "MslSubscriptionTree.h"
#include "OptionsBalance.h"
#include "ParkMessages.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "SkyObjects.h"
#include "tb_sky.h"
#include "TbDynamicStringTemplate.h"
#include "TbResource.h"
#include "Text.h"
#include "ThingMessages.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPOptions.h"
#include "ui_ime.h"
#include "v51029_decls.h"
#include "VehicleSteer.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <ios>
#include <new>

// The weather and challenge messages (main app, unit w124): the three constructors 0x101518c4, 0x10151870 and 0x10152008, the GetType functions
// 0x101399d0, 0x10139994 and 0x100f93b0, and the destructors 0x10138b9c, 0x10138b20 and 0x100f65c8. They live together because the native vtable
// needs a key function (GetType); the pattern is src/MsgDeath.cpp's.

CMsgWeatherWarning::CMsgWeatherWarning(int32_t days, int32_t quality) {
    mFlags = 0;
    mQuality = quality;
    mDays = days;
}

// The Mac writes the base vtable, a zero flags word, the message's own vtable and then the payload, in that order.
CMsgWeather::CMsgWeather(int32_t quality) {
    mFlags = 0;
    mQuality = quality;
}

// CMsgAdvisor (main app; unit w180): the constructor 0x1015190c, GetType 0x100fd9cc and the destructor 0x100fcea8.

// The base constructor stores the CMsg vtable, the flags word (+4) is cleared, the class's own vtable is stored and the request is copied
// (CAdvisorRequest's copy constructor, 0x1000de30: the eight fields, not the padding) into +8.
CMsgAdvisor::CMsgAdvisor(const CAdvisorRequest& request) : mRequest(request) { mFlags = 0; }

// The ride messages (main app, unit w200): the constructors 0x10151998, 0x10151a14, 0x10151a98, 0x10151b1c and 0x10151e48, the GetType functions
// 0x10110f6c, 0x10110e84, 0x10110ec0, 0x10110e44, 0x10110f34 and 0x10110efc, and the deleting destructors 0x101064fc, 0x1010e524, 0x1010e5a0, 0x1010e4a4,
// 0x10107b28 and 0x1010aa34. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

// The Mac writes the base vtable, a zero flags word, the message's own vtable and the thing id (the HIGH halfword of its register argument), in that order.
CMsgNewRide::CMsgNewRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgRideBrokenDown::CMsgRideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgRideCondemned::CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgRideWantsUpgrade::CMsgRideWantsUpgrade(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

// Unit w210: CObject::FinalDestroy builds this message on its stack and posts it; the id (the HIGH halfword of the register argument) goes to +8 as the others do.
CMsgRideDeleted::CMsgRideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

// The park's date messages (main app, unit w129): the three constructors 0x10151c24, 0x10151c6c and 0x10151cb4, the GetType functions 0x10112de0, 0x10112da8
// and 0x10112d70, and the destructors 0x1011261c, 0x101125a4 and 0x1011252c. They live together because the native vtable needs a key function (GetType);
// the pattern is src/ThingMessages.cpp's.

// The Mac writes the base vtable, a zero flags word and the message's own vtable, in that order; there is no payload.
CMsgEndOfDay::CMsgEndOfDay() { mFlags = 0; }
CMsgEndOfMonth::CMsgEndOfMonth() { mFlags = 0; }
CMsgEndOfYear::CMsgEndOfYear() { mFlags = 0; }

// The park analyser's three messages (main app, unit w161): the constructors 0x1015210c (CMsgAddToChallenge), 0x10151cfc (CMsgResearchCompleted) and
// 0x10151f14 (CMsgStaffInfo), the GetType functions 0x100ef1f4, 0x101244d0 and 0x1012c244, the destructors 0x100e4584, 0x1012412c and 0x1012b4f4, and
// CBankAccount::GetBalance 0x100f32b4. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

CMsgResearchCompleted::CMsgResearchCompleted(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj, ResearchCat category) {
    mFlags = 0;
    mObj = obj.mIndex;
    mCategory = category;
}

// Unit v51118: small stand-ins of the main app (CTimeSig's three members are inline in include/TimeSig.h and are tested through it;
// CSkyObjects, CSaveLoad, InterfaceFontWrapper, the ride database and balance destructors, two messages, and the std containers'
// destructors, constructors and allocator functions of the master save).

namespace v51118 {

// Runtime glue the destructors call; weak defaults here, recording fakes in the test library.
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }   // 0x10238218
// 0x10154660
CRideDatabase* RideDatabaseDtor(CRideDatabase* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->ClearDatabase();
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x101544ec: the Mac first stores the base class's vtable (0x102a5d2c) at +4; the host's vptr is native, so that store has no host form.
CRideBalance* RideBalanceDtor(CRideBalance* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->UnInit();
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

}  // namespace v51118

CMsgResearchCategoryUpdated::CMsgResearchCategoryUpdated(ResearchCat category) {
    mFlags = 0;
    mCategory = category;
}

// The message centre's post and the message and subscription objects it uses (main app, unit w107): CMessageCentre::PostMessage
// 0x1015256c, the CMsgEvent constructor 0x10151df8 and destructor 0x100f39cc (and GetType 0x100f4470, which its vtable needs), the
// CSubscriptionHeader constructor 0x1000a3f8 and destructor 0x101522e0. The inventory files them under RideDB.cpp and BlockSave.cpp.

// The Mac sets the vtable (twice, through the base class's), a zero flags word and the event type, in this order.
CMsgEvent::CMsgEvent(EventType type) {
    mFlags = 0;
    mEvent = type;
}

CMsgBlockedExit::CMsgBlockedExit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

// 0x10151ec8, 0x10151d94: the base's flags word cleared, then the payload.
CMsgCheatMode::CMsgCheatMode(bool cheating) {
    mFlags = 0;
    mCheating = cheating;
}

CMsgStaffInfo::CMsgStaffInfo(StaffType staff, StaffSituation situation) {
    mFlags = 0;
    mStaff = staff;
    mSituation = situation;
}

CMsgChallenge::CMsgChallenge(int32_t tagId, AdvisorMessage message, bool flag, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e,
                             tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj2,
                             StaffType staff) {
    mFlags = 0;
    mTagId = tagId;
    mMessage = message;
    mFlag = flag;
    mPrize = a;
    mTargetTime = b;
    mTargetVal = c;
    mDays = d;
    mValue = e;
    mObj = obj.mIndex;
    mObj2 = obj2.mIndex;
    mStaff = staff;
}

// The Mac writes the base vtable, a zero flags word, the message's own vtable and then the payload, in that order.
CMsgAddToChallenge::CMsgAddToChallenge(ChallengeType type, int32_t value, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj,
                                       tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    mFlags = 0;
    mType = type;
    mValue = value;
    mObj = obj.mIndex;
    mThing = thing.mIndex;
}

// Unit v51064: twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (main app).

// 0x1011b794 is CMsgPeepArrived::GetType (inline in the header, answers 0x1c); 0x10120b90 is CMsgPrankery::GetType (inline, 0xe).

// 0x101521e4: the flags are cleared and the person's id (the high halfword of the register on the Mac) is stored. The vtable stores are not modelled.
CMsgPeepArrived::CMsgPeepArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> peep) {
    mFlags = 0;
    mThingId = peep.mIndex;
}

// CMsgDeath (main app, unit w123c): the constructor 0x10152264, GetType 0x1012ec14 and destructor 0x1012e984. The inventory files them under
// RideDB.cpp and Thing.cpp (unreliable). The three live together because the native vtable needs a key function (GetType) and the
// message is made by CThing::KillMe.

// The Mac writes the base class's vtable, a zero flags word, the death vtable and the id at +8, in that order (the vtables are the
// compiler's here). Nothing after the id is written.
CMsgDeath::CMsgDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

// Frees every node when there is a root; the tree's own words are left as they were. The Mac's `this != 0` and `this + 4 != 0` checks
// are the compiler's (a delete of null); the delete flag is the C++ delete.
CSubscriptionHeader::~CSubscriptionHeader() {
    if (mTree.mRoot != nullptr) mTree.destroy(static_cast<SubscriptionTree::node*>(mTree.mRoot));
}

// The seven thing-loop entry points (unit w123a, main app). External dispatch,
// death-message support, random/season providers and tree insertion remain callees.

// 0x10152388, 128 bytes. The signed bound has no lower limit. Original insert_one
// produces a 0/1 byte, independently of its iterator pointer.
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    if (type >= 29) return false;
    const CSubscription key{thing.GetThingId().mIndex};
    return mSubscriptions[type].mTree.insert_one(key).mInserted;
}

// CMessageCentre::Save 0x10152758, Load 0x10152840, UnsubscribeAll 0x10152448 (unit v5760).

// 0x10152448
int32_t CMessageCentre::UnsubscribeAll(const CThing& thing) {
    int32_t total = 0;
    for (int32_t i = 0; i < 0x1d; i++) {
        CSubscription key;
        key.mThingId = thing.GetThingId().mIndex;
        SubscriptionTree& tree = mSubscriptions[i].mTree;
        SubscriptionTree::node_base* it = tree.find(key);
        if (it != tree.end()) {
            tree.erase(it);
            total += 1;
        }
    }
    return total;
}

// Type 0x1b is the "thing destroyed" message (CMsgDeath, the dying thing's id as a uint16_t at +8). Before it is delivered the dying
// thing's subscription is removed from every header: the key searched for is the id the THING reports (CThing::GetThingId on the
// thing table's entry for the message's id), one search and at most one erase per header. Then every subscriber of the message's own
// type, in id order, gets it: the thing table's entry of the node's id (five words per entry on the Mac). Quirks kept: a type above
// 0x1c indexes past the 29 headers; a dying thing missing from the table is dereferenced; the table's entry 0 must be free and the
// check only reports (and the report's condition is read again before every delivery).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    static const char kZeroEntryText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
    const int32_t type = message.GetType();
    if (type == 0x1b) {
        const uint16_t dyingId = static_cast<const CMsgDeath&>(message).mThingId;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kZeroEntryText);
        const CThing* dying = static_cast<const CThing*>(gThingArray[dyingId].mThing);
        for (int32_t i = 0; i < 0x1d; i++) {
            CSubscription key;
            key.mThingId = dying->GetThingId().mIndex;
            SubscriptionTree& tree = mSubscriptions[i].mTree;
            SubscriptionTree::node_base* found = tree.find(key);
            if (found != tree.end()) tree.erase(found);
        }
    }
    SubscriptionTree& tree = mSubscriptions[type].mTree;
    SubscriptionTree::node_base* position = tree.begin();
    while (position != tree.end()) {
        const uint16_t id = static_cast<SubscriptionTree::node*>(position)->mValue.mThingId;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kZeroEntryText);
        static_cast<CThing*>(gThingArray[id].mThing)->ReceiveMessage(message);
        MslRedBlackTree::increment(position);
    }
    return 1;
}

// 0x10152758: writes the count 0x1d (the Mac's big-endian swap of the word; the host's bytes 1d 00 00 00 are the same),
// then each header; stops at the first nonzero answer and returns it.
int32_t CMessageCentre::Save(void*& file) const {
    uint32_t count = 0x1d;
    uint32_t written;
    LbFile_Write(file, &count, 4, &written);
    for (int32_t i = 0; i < 0x1d; i++) {
        int32_t r = mSubscriptions[i].Save(file);
        if (r != 0) return r;
    }
    return 0;
}

// 0x10152840: clears all 29 trees, reads one word (ignored), then loads each header.
int32_t CMessageCentre::Load(void*& file) {
    for (int32_t i = 0; i < 0x1d; i++) mSubscriptions[i].mTree.clear();
    uint32_t value;
    uint32_t read;
    LbFile_Read(file, &value, 4, &read);
    for (int32_t i = 0; i < 0x1d; i++) {
        int32_t r = mSubscriptions[i].Load(file);
        if (r != 0) return r;
    }
    return 0;
}


// CSubscriptionHeader::Save (0x10152928) and Load (0x10152a10): the subscriptions of one message type as a file image: the count, then
// one CSubscription each in id order. The file is little-endian: the Mac swaps its big-endian count on the way out and back in, and on
// the host the count is written and read as it is (swapping it here read a park's 1 as 0x01000000; v5L05, the park's load).

// Writes the node count, then every subscription in order. The results of the writes are not looked at. Answers 0.
int32_t CSubscriptionHeader::Save(void*& file) const {
    uint32_t swapped = mTree.mCount;
    uint32_t transferred;
    LbFile_Write(file, &swapped, 4, &transferred);
    SubscriptionTree& tree = const_cast<SubscriptionTree&>(mTree);
    SubscriptionTree::node_base* position = tree.begin();
    while (position != tree.end()) {
        static_cast<SubscriptionTree::node*>(position)->mValue.Save(file);
        MslRedBlackTree::increment(position);
    }
    return 0;
}

// Reads the count (the read's result is not looked at), then loads and inserts that many subscriptions (the count compared signed).
// Answers -1 when an insert reports not inserted (an id already there), else 0.
int32_t CSubscriptionHeader::Load(void*& file) {
    uint32_t count;
    uint32_t transferred;
    LbFile_Read(file, &count, 4, &transferred);
    const int32_t total = static_cast<int32_t>(count);
    for (int32_t i = 0; i < total; i++) {
        CSubscription subscription;
        subscription.Load(file);
        SubscriptionInsertResult result = mTree.insert_one(subscription);
        if (!result.mInserted) return -1;
    }
    return 0;
}

// Unit v51029: seven small functions of the main app.
//   CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010,
//   CSubscription::Save 0x10152b08, __sinit_keyboard_cpp 0x101512fc,
//   InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38.

// The game calls the void form (CSubscriptionHeader::Save ignores the answer), as CSubscription::Load is defined over LoadResult.
// Without it the launcher's stand-in wrote nothing, so a save kept each header's count but none of its ids and the load read
// the following modules' bytes as subscriptions (v5L15).
void CSubscription::Save(void*& file) const { SaveResult(file); }

// Unit v5942: the red-black tree's balance_insert (0x10153b24), the subscription tree's insert_node_at (0x101538d8) and
// CSubscription::Load (0x10152bb4).

namespace {

using node_base = MslRedBlackTree::node_base;

inline node_base* ParentOf(node_base* node) { return reinterpret_cast<node_base*>(node->mParent & ~uintptr_t(1)); }
inline bool IsRed(node_base* node) { return (node->mParent & 1) != 0; }
inline void SetRed(node_base* node) { node->mParent |= 1; }
inline void SetBlack(node_base* node) { node->mParent &= ~uintptr_t(1); }

}  // namespace

// 0x10153b24. `root` lives in a local the rotations update, and the loop and the last line read that local.
void MslRedBlackTree::balance_insert(node_base* node, node_base* root) {
    SetRed(node);
    while (node != root && IsRed(ParentOf(node))) {
        node_base* parent = ParentOf(node);
        node_base* grand = ParentOf(parent);
        if (parent == grand->mLeft) {
            node_base* uncle = grand->mRight;
            if (uncle != nullptr && IsRed(uncle)) {
                SetBlack(parent);
                SetBlack(uncle);
                node = ParentOf(ParentOf(node));
                SetRed(node);
            } else {
                if (node == parent->mRight) {
                    node = parent;
                    rotate_left(parent, root);
                }
                SetBlack(ParentOf(node));
                SetRed(ParentOf(ParentOf(node)));
                rotate_right(ParentOf(ParentOf(node)), root);
            }
        } else {
            node_base* uncle = grand->mLeft;
            if (uncle != nullptr && IsRed(uncle)) {
                SetBlack(parent);
                SetBlack(uncle);
                node = ParentOf(ParentOf(node));
                SetRed(node);
            } else {
                if (node == parent->mLeft) {
                    node = parent;
                    rotate_right(parent, root);
                }
                SetBlack(ParentOf(node));
                SetRed(ParentOf(ParentOf(node)));
                rotate_left(ParentOf(ParentOf(node)), root);
            }
        }
    }
    SetBlack(root);
}

// 0x101538d8. A full tree (count 0xffffffff) throws a std::length_error("tree::insert length error"); the fakes of __throw return,
// and the insert goes on. The Mac's first-node word (+8) is not kept on the host (begin() is derived), so `leftmost` has no store.
SubscriptionTree::node_base* SubscriptionTree::insert_node_at(node_base* parent, bool left, bool leftmost, const CSubscription& key) {
    (void)leftmost;
    if (mCount > 0xfffffffeu) {
        static const char kMessage[] = "tree::insert length error";   // 0x10248db0
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    node* created = static_cast<node*>(IME::MslNew(sizeof(node)));   // the Mac's node is 0xe bytes
    created->mValue = key;
    created->mRight = nullptr;
    created->mLeft = nullptr;
    created->mParent = reinterpret_cast<uintptr_t>(parent) | (created->mParent & 1);
    if (left)
        parent->mLeft = created;
    else
        parent->mRight = created;
    mCount = mCount + 1;
    MslRedBlackTree::balance_insert(created, mRoot);
    return created;
}

// 0x10152bb4. The file holds the id little-endian: the Mac reads it as a big-endian halfword and swaps it in place; on the host the
// two bytes are the id as they are (the swap here made a park's id 7 into 0x700; v5L05, the park's load).
void CSubscription::Load(void*& file) { LoadResult(file); }

int32_t CSubscription::LoadResult(void*& file) {
    uint32_t transferred = 0;
    LbFile_Read(file, this, 2, &transferred);
    if (transferred == 2) return 0;
    DEBUG_MESSAGE("Warning! Something went hideously wrong while loading the message subscriptions!", 0);
    return -1;
}

// The advisor's tag message (CMsgTag on the Mac, "MsgTag.cpp?" in the inventory): its constructor 0x10151f7c (52 bytes). GetType
// (0x1000e90c, 0x18) is inline in include/AdvisorQueue.h.

// CTagSystem::ReceiveMessage reads the message as a CMsgTagShow: the two classes must lay their fields out alike.
static_assert(offsetof(CMsgTagObj, mTag) == offsetof(CMsgTagShow, mTag), "tag");
static_assert(offsetof(CMsgTagObj, mThing) == offsetof(CMsgTagShow, mThing), "thing");
static_assert(offsetof(CMsgTagObj, mTime) == offsetof(CMsgTagShow, mTime), "time");
static_assert(offsetof(CMsgTagObj, mValue) == offsetof(CMsgTagShow, mValue), "value");

// 0x10151f7c: a zero flags word, the tag, the thing id (the high half of its word: the Mac stores the halfword it reads back from
// the stack), then `extra` at +0x10 and `secondary` at +0x14. The halfword after the thing id is never written.
CMsgTagObj::CMsgTagObj(int32_t tag, uint32_t thingWord, uint32_t secondary, int32_t extra) {
    mFlags = 0;
    mTag = tag;
    mThing = static_cast<uint16_t>(thingWord >> 16);
    mTime = extra;
    mValue = secondary;
}
