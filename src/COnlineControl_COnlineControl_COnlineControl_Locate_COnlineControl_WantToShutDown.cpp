// COnlineControl (online.cpp): Shutdown, WantToShutDown, Update, CallbackFn and the first-person moves.
// The Mac object keeps 32-bit pointers at fixed offsets; they are read raw (the arena is 32-bit addressable).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"

namespace {

template <class T>
T* Field(void* object, size_t offset) { return reinterpret_cast<T*>(static_cast<char*>(object) + offset); }

void* PointerAt(void* object, size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<char*>(object) + offset, 4);
    return online_callee::HandleToPointer(word);
}

int32_t Halfword(MapPos p) { return static_cast<int32_t>(static_cast<int16_t>((p.mCell << 8) | p.mOffset)); }

}  // namespace

// 0x10214840
void COnlineControl::Shutdown() {
    bthread_mutex* mutex = Field<bthread_mutex>(this, 0x1c);
    mutex->lock();
    *Field<uint8_t>(this, 0x11) = 1;
    *Field<uint8_t>(this, 0x12) = 1;
    mutex->unlock();
}

// 0x102148bc
bool COnlineControl::WantToShutDown() {
    bthread_mutex* mutex = Field<bthread_mutex>(this, 0x1c);
    mutex->lock();
    bool answer = *Field<bool>(this, 0x12);
    mutex->unlock();
    return answer;
}

// 0x10214944
void COnlineControl::Update() {
    ProcessQueue();
    if (PointerAt(this, 0xe64) != nullptr && *Field<uint8_t>(this, 0xe60) != 0) {
        SamsUtilities::UDebug("We have a vote connection, but we are done voting, release and kill it\n");
        online_callee::VoteRelease(PointerAt(this, 0xe64));
        online_callee::VoteDtor(PointerAt(this, 0xe64), 1);
        *Field<uint32_t>(this, 0xe64) = 0;
        *Field<uint8_t>(this, 0xe60) = 0;
        SamsUtilities::UDebug("Vote connection released, ready for new vote\n");
        online_callee::DisableVoteButton(false);
    }
}

// 0x10214b70
void COnlineControl::CallbackFn(int32_t id, void* arguments) {
    bthread_mutex* mutex = Field<bthread_mutex>(this, 0x18);
    mutex->lock();
    online_callee::ArgumentDequePushBack(Field<void>(this, 0xe68), arguments);
    mutex->unlock();
}

// 0x102177bc
void COnlineControl::EnteringFirstPerson(MapPos x, MapPos y) {
    CPeep* peep = static_cast<CPeep*>(PointerAt(this, 4));
    peep->HideMySprite();
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 1, this);
}

// 0x10217884
void COnlineControl::LeavingFirstPerson(MapPos x, MapPos y) {
    CPeep* peep = static_cast<CPeep*>(PointerAt(this, 4));
    peep->DrawMySpriteAt(0.0f, 0.0f, 0.0f, 0, static_cast<CDelayedScript::ScriptIdx>(3));
    uint32_t handle;
    std::memcpy(&handle, reinterpret_cast<char*>(peep) + 0xc, 4);
    SCR_SetFloatVariable(static_cast<int32_t>(handle), 8, 1.5f);
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 1, this);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
}

// 0x1021797c
void COnlineControl::UpdateFirstPersonPos(MapPos x, MapPos y) {
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 0, this);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
}

// 0x10217aa8
void COnlineControl::UnHideAvatar(MapPos x, MapPos y) {
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 3, this);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
}
