// TbAudioBody.cpp: the audio object the game holds, decompiled from the sound library of SimTheme Park 2.04b3 ("sound symbols
// shared", Code at 0x10000000, TOC r2 = 0x1003abf0; file AudioPlaceHolder.cpp, with GetSoundManager, GetPlaceHolderList and
// CreateAudioObject in CSprocketsModule.cpp and the factory in LbMpeg.cpp). See include/TbAudioBody.h for the layout and the
// slots.
#include "TbAudioBody.h"

#include <cstring>

#include "AudioPlaceHolder.h"
#include "TbMapStreamer.h"
#include "TbSoundInfo.h"

// 0x10016a84 (_IsSlash, 64): the signed char is a path separator or the volume colon.
bool IsSlash(char c) {
    return c == '\\' || c == '/' || c == ':';
}

namespace {

// Both TbAudioEvent classes declare the same first slots (GetType +8, GetCategory +0xc, GetEvent +0x10): the game's object goes
// to the library's functions unchanged.
const sound::TbAudioEvent& LibraryEvent(const TbAudioEvent& event) {
    return reinterpret_cast<const sound::TbAudioEvent&>(event);
}

// The sound handles are words on the Mac (CUniqueHandle values, passed as void*).
void* HandleOf(int32_t handle) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(handle)));
}

int32_t WordOf(void* handle) {
    return static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle)));
}

}  // namespace

// ---- CAudioComponentFactory -----------------------------------------------------------------------------------------------

// 0x10001b54 (72)
TbSoundCore* CAudioComponentFactory::CreateCore() {
    return new TbSoundCore();
}

// 0x10001bd8 (72)
CAudioComponent* CAudioComponentFactory::CreateComponent() {
    return new CAudioComponent();
}

// ---- TbAudioBody ----------------------------------------------------------------------------------------------------------

// 0x100161a0 (168): the base stores its vptr (TbAudioSystem 0x10016384, 0x1003cbb0), then the body's (0x1003cb70); the three
// positions are three zeros; the list is a new CAudioPlaceHolderList(0xc) (0x2c bytes; the Mac skips the constructor when the
// allocation answers NULL and keeps the NULL); the manager and the component are NULL; the listener position is zeros.
TbAudioBody::TbAudioBody()
    : mPlaceHolders(new CAudioPlaceHolderList(0xc)), mManager(nullptr), mComponent(nullptr) {}

// 0x100163bc (156): drops one reference on the manager and one on the component (TbRefRelease::Release through the sub-object
// at +4, vtable slot +8), then the list is deleted (the CAutoPtr member, 0x10016270, deleting destructor slot +8 with flag 1).
// The chain holds its own reference on the manager, so the manager goes when the component lets go of its next.
TbAudioBody::~TbAudioBody() {
    if (mManager != nullptr) mManager->Release();
    if (mComponent != nullptr) mComponent->Release();
}

// 0x10016804 (84): `delete this` (the deleting destructor, slot +0x30 with flag 1) when this is not NULL; the answer is built after
// the object is gone and never touches it.
TbError TbAudioBody::Release() {
    delete this;
    return TbError(0);
}

// 0x10016480 (120): the head of the chain is a plain component, the tail a new manager (0x68 bytes; the Mac skips the constructor
// on a NULL allocation but stores the NULL); mComponent->Chain(mManager) (the TbError answer is dropped). The config is unused.
// A void function on the Mac (the host's declaration answers a TbError, nothing reads it).
TbError TbAudioBody::CreateSystemChain(const CAudioConfig&) {
    mComponent = CAudioComponentFactory::CreateComponent();
    mManager = new TbSoundManager();
    mComponent->Chain(mManager);
    return TbError(0);
}

// 0x1001653c (316). The config is two words on the Mac: the double at +0 (the sound quality / 100.0) and the channel count at +8.
// The double is copied by lfd / stfd into the stack slot that is the info's words +0xc and +0x10 (the info is 0x4c bytes at
// 0x40(r1), the copy goes to 0x4c(r1)), so the info carries the quality there as the two words of a big-endian double (high word
// first); the info's setters never touch those words and TbSoundCore::Init copies them along.
// Sequence: CreateSystemChain (virtual, slot +0x3c); a fresh info with SetHalfRate(0) (empty) and SetBuffersToUse(channels);
// mComponent->Init(info) (virtual slot +0x38; the head forwards it to the manager's TbSoundCore::Init and answers 0 even when
// that fails); then, when that answered >= 0, mManager->Init(name) (the manager's own virtual, slot +0x40: the sound path); then,
// when that answered >= 0, the manager gets its audio system (the component), the streamer its sound manager, the manager its
// listener position (the address of mPosition) and the placeholders their static manager; answers 0. Any negative answer: -1.
TbError TbAudioBody::Init(const char* name, const CAudioConfig& config) {
    CreateSystemChain(config);
    TbSoundSystemInfo info;
    info.SetHalfRate(0);
    info.SetBuffersToUse(static_cast<uint32_t>(config.mChannels));
    uint64_t quality;
    std::memcpy(&quality, &config.mQuality, sizeof quality);
    info.mFieldC = static_cast<uint32_t>(quality >> 32);
    info.mField10 = static_cast<uint32_t>(quality);
    const TbError started = mComponent->Init(info);
    if (started >= 0) {
        const TbError named = mManager->Init(name);
        if (named >= 0) {
            mManager->SetAudioSystem(mComponent);
            TbMapStreamer::Instance()->SetSoundManager(mManager);
            mManager->SetListenerPosition(mPosition);
            CAudioPlaceHolder::SetSoundManager(mManager);
            return TbError(0);
        }
    }
    return TbError(-1);
}

// 0x10016884 (100): category 0 is not a category (-1); anything else is the streamer's RemoveCatagory, whose answer goes out
// through the same buffer.
TbError TbAudioBody::DeregisterCatagoryBanks(uint32_t category) const {
    if (category == 0) return TbError(-1);
    return TbMapStreamer::Instance()->RemoveCatagory(category);
}

// 0x10016928 (208): a path with a drive ("X:" and a separator, where a separator is '\\', '/' or ':') or two separators (a network
// path) is taken as it is (the string's virtual operator=(const char*), slot +0x20); anything else is the manager's sound path
// (operator=(const TbDynamicStringTemplate&), slot +0x1c) with the path appended (operator+=(const char*), slot +0x30). The
// tests read path[1] and path[2] of a short string as the original does.
void TbAudioBody::QualifyPath(const char* path, TbDynamicStringTemplate<char>& out) {
    if (path[1] == ':' && IsSlash(path[2])) {
        out = path;
        return;
    }
    if (IsSlash(path[0]) && IsSlash(path[1])) {
        out = path;
        return;
    }
    out = mManager->GetSoundPath();
    out += path;
}

// 0x10016ae4 (184). The handle is zeroed first, even when there is no manager (-1). The qualified path is a temporary string
// (destroyed with flag -1) whose operator const char* (0x10016bd8) answers the data pointer; the streamer registers it under
// `name` and its answer goes out through the same buffer. (The listing's argument order is garbled in the decompiled text; the
// registers are: path = r5, name = r6, handle = r7, the qualified text goes to the streamer as its first string.)
TbError TbAudioBody::RegisterCatagory(const char* path, const char* name, uint32_t& handle) {
    handle = 0;
    if (mManager == nullptr) return TbError(-1);
    TbDynamicStringTemplate<char> full;
    QualifyPath(path, full);
    return TbMapStreamer::Instance()->RegisterCatagory(full.mData, name, handle);
}

// 0x10016f68 (32)
bool TbAudioBody::CanBeDroppedDueToVolControl(uint32_t flags) {
    return (flags & 4) == 0 && (flags & 8) == 0;
}

// 0x10016c1c (360). Needs the list and the manager. The event's category (virtual slot +0xc of the event) indexes the streamer's
// catagory deque (operator[] grows it for a category past the end; the new slot is whatever the allocation held). With a
// container that has data, the mapper chooses the event's sound (TbEventMapper::MapEvent, a CEventData whose flag half-word at
// +0x10 is tested here and whose word at +0xc is the sound's priority); a sound that can be dropped (neither flag 4 nor 8) plays
// only when the volume control of the catagory's channel group (the low byte of the catagory data's half-word at +8) is on. A
// placeholder that already exists for the entity and is not finished (flag 0x10) keeps playing when the sound's priority does not
// exceed the placeholder's base priority (the entity comes back unchanged); otherwise it is flagged 0x100 and a new placeholder
// is made. Every other path answers the entity.
int32_t TbAudioBody::SendEvent(int32_t entity, const TbAudioEvent& event) {
    if (static_cast<CAudioPlaceHolderList*>(mPlaceHolders) == nullptr || mManager == nullptr) return entity;
    const uint32_t category = static_cast<uint32_t>(event.GetCategory());
    CCatagoryContainer* container = TbMapStreamer::Instance()->GetCatagoryDeque()[category];
    if (container == nullptr || container->mData == nullptr) return entity;
    CEventData* sound = TbEventMapper::MapEvent(container->mData, LibraryEvent(event));
    if (sound == nullptr) return entity;
    const bool droppable = CanBeDroppedDueToVolControl(sound->mFlags);
    if (droppable && !mManager->VolumeControlIsOn(static_cast<uint8_t>(container->mData->mF08))) return entity;
    CAudioPlaceHolder* holder = mPlaceHolders->FindPlaceHolder(HandleOf(entity));
    if (holder != nullptr && (holder->mFlags & 0x10) == 0) {
        if (sound->mF0C <= (holder->mPriority.GetBasePriority() & 0xffff)) return entity;
        holder->mFlags |= 0x100;
    }
    return WordOf(mPlaceHolders->CreatePlaceHolder(container, sound, LibraryEvent(event)));
}

// 0x10017314 (104)
int32_t TbAudioBody::UpdateHandle(int32_t handle, const TbSoundPosition& position, const TbSoundPosition& velocity) {
    if (handle == 0) return 0;
    return static_cast<int32_t>(mPlaceHolders->UpdatePlaceHolder(HandleOf(handle), position, velocity));
}

// 0x100173d4 (132)
int32_t TbAudioBody::UpdateParameter(int32_t handle, uint8_t parameter, uint32_t value) {
    if (handle == 0 || static_cast<CAudioPlaceHolderList*>(mPlaceHolders) == nullptr) return 0;
    return static_cast<int32_t>(mPlaceHolders->UpdatePlaceHolderParameter(HandleOf(handle), parameter, value));
}

// 0x10017490 (124): the placeholder of the handle stops itself (virtual slot +0x20 of the placeholder, the answer goes out through
// the same buffer); -1 for a NULL handle or an unknown one. There is no test of the list pointer.
TbError TbAudioBody::StopHandle(void* handle) const {
    if (handle != nullptr) {
        CAudioPlaceHolder* holder = mPlaceHolders->FindPlaceHolder(handle);
        if (holder != nullptr) return holder->Stop();
    }
    return TbError(-1);
}

// 0x1001753c (136): the command executes against this body (virtual slot +8 of the command, the answer goes out through the same
// buffer); -1 without a manager or a list.
TbError TbAudioBody::SystemCommand(const TbSysCommand::Command& command) const {
    if (mManager != nullptr && static_cast<CAudioPlaceHolderList*>(mPlaceHolders) != nullptr) return command.Execute(this);
    return TbError(-1);
}

// 0x10017610 (296): -1 without a manager. The position and the velocity arguments are copied first (the copy constructors,
// 0x1000cb08); the four members are compared in the order position, forward, up, velocity, stopping at the first difference
// (operator!= 0x1000c920: a bytewise compare); on a difference all four are stored in that order and the manager gets the stored
// position, forward, up and velocity (TbSoundManager::SetListenerOrientation, 0x10027b60: the listing's decompiled text shows
// three arguments, the registers r5 to r8 hold four). The manager's answer is dropped; the answer is 0.
TbError TbAudioBody::SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& forward,
                                            const TbSoundPosition& up, const TbSoundPosition& velocity) {
    if (mManager == nullptr) return TbError(-1);
    const TbSoundPosition positionCopy(position);
    const TbSoundPosition velocityCopy(velocity);
    if (mPosition != positionCopy || mForward != forward || mUp != up || mVelocity != velocityCopy) {
        mPosition = positionCopy;
        mForward = forward;
        mUp = up;
        mVelocity = velocityCopy;
        mManager->SetListenerOrientation(mPosition, mForward, mUp, mVelocity);
    }
    return TbError(0);
}

// 0x1001781c (8) and 0x1001785c (8): `mr r3, r4; blr`. With this in r3 and the handle in r4 the Mac function answers the handle
// itself in r3 (no hidden result buffer: a TbError would have moved this to r4). The host's header declares a TbError.
TbError TbAudioBody::UpdateDistortion(void* handle, float) {
    return TbError(WordOf(handle));
}

TbError TbAudioBody::UpdateLowPass(void* handle, float) {
    return TbError(WordOf(handle));
}

// 0x10028110 (8)
TbSoundManager* TbAudioBody::GetSoundManager() const {
    return mManager;
}

// 0x1002814c (36): the CAutoPtr's operator T* (0x100172b4)
CAudioPlaceHolderList* TbAudioBody::GetPlaceHolderList() const {
    return mPlaceHolders;
}
