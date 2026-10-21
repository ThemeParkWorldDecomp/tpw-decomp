// Fakes and test entry points for unit w79f, the audio object the game holds (libtpw_unit_AudioBody.dylib, driven by
// tests/test_AudioBody.py in the program "sound symbols shared"; see calllog.h):
//   src/sound/AudioComponent.cpp, src/sound/TbAudioBody.cpp, src/sound/TbSysCommand.cpp  TbAudioBody, CAudioComponentFactory, CreateAudioObject (the unit under test)
// Real: the component chain (src/sound/AudioComponent.cpp), TbRefRelease (w78), the info and position records (w77) and the bullfrog
// strings. Fakes, each recording what the original's callee is given (the PowerPC side records the same from its stubs):
//   TbSoundManager (w79e) and the TbSoundCore under it (w79b): every member the body calls records and answers a queue; the
//       manager is a fake so that every branch of the body is reachable (a real manager's Init always answers 0)
//   TbMapStreamer: Instance (the raw storage the case built), SetSoundManager, RegisterCatagory, RemoveCatagory; TbEventMapper::MapEvent
//   CAudioPlaceHolderList (w79g1): constructor, destructor, FindPlaceHolder, CreatePlaceHolder, UpdatePlaceHolder and
//       UpdatePlaceHolderParameter; CAudioPlaceHolder (w79g2): a fake placeholder (Stop, the priority record, the flags),
//       SetSoundManager
//   HookedComponent, a CAudioComponent whose Init records and answers a queue; HookedBody, a body whose CreateSystemChain records
//       and installs nothing (the case presets the manager and the component); TestCommand and TestEvent
//
// Labels: 1 the body, 2 the manager, 3 the component, 4 the list, 5 the placeholder, 6 the streamer, 7 the sound (CEventData), 8 the
// event, 9 the command, 10 the config, 12..15 the body's position, forward, up and velocity, 21.. the catagory data, 31.. the
// containers, 41..44 the argument positions, 100 and up the objects the code under test makes ("new" events record the label
// only: the host's sizes are not the Mac's).
//
// tpw_ab_run drives one operation on a world it builds from the parameter words; see the P_ and O_ enums.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "TbAudioBody.h"
#include "TbMapStreamer.h"
#include "calllog.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

bool gTrack;   // "new" events are recorded only while the code under test runs

// The runtime's new: blocks are filled with 0xA5 (arrays with zeros: a grown catagory slot reads as NULL, the PowerPC side
// does the same); every non-array block gets a "new" event with its label.
void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, array ? 0 : 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack || array) return;
    ev("new", {newlab(block)});
}

// Blocks the runtime frees are kept (no address is reused while a stale pointer to it is compared).
bool KeepFreed(void*, bool) {
    return true;
}

}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) Allocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

namespace {

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

void* Cookie(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

float BitsToFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

// The three words of a position as the Mac stores them (no float arithmetic: a NaN keeps its bits).
void PosWords(const TbSoundPosition& p, uint32_t* w) {
    std::memcpy(w, &p, 12);
}

void SetPos(TbSoundPosition& p, const uint32_t* w) {
    std::memcpy(static_cast<void*>(&p), w, 12);
}

const char* Text(const TbDynamicStringTemplate<char>& s) {
    return s.mData != nullptr ? s.mData : "";
}

// The words of a system info the body defines: the three words of the constructor, the quality double the body copies into
// +0xc and +0x10, the mask, the buffers to use, the window handle and +0x40 (the others are stack garbage in the original).
void RecInfo(const char* name, int64_t first, const TbSoundSystemInfo& i) {
    ev(name, {first, i.mField0, i.mField4, i.mField8, i.mFieldC, i.mField10, i.mMask, i.mBuffersToUse, Low32(i.mHWnd), i.mField40});
}

void RecPositions(const char* name, int64_t first, const TbSoundPosition& a, const TbSoundPosition& b) {
    uint32_t x[3], y[3];
    PosWords(a, x);
    PosWords(b, y);
    ev(name, {first, x[0], x[1], x[2], y[0], y[1], y[2]});
}

}  // namespace

// The string library's printf-style formatters are not used here (the bullfrog library supplies them on the Mac).
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- the manager and the core under it: fakes (w79e, w79b) -----------------------------------------------------------------

TbSoundCore::TbSoundCore() : CAudioComponent(), mModule(nullptr), mStore(nullptr), mCopyInit() {}
TbSoundCore::~TbSoundCore() {}
TbError TbSoundCore::Init(const TbSoundSystemInfo& info) {
    RecInfo("Core.Init", lab(this), info);
    return TbError(static_cast<int32_t>(ret("Core.Init", 0)));
}
TbError TbSoundCore::GetSystemInfo(TbSoundSystemInfo&) { return TbError(-77); }
TbError TbSoundCore::GetSampleInfo(void*, TbSoundSampleInfo&) { return TbError(-77); }
void* TbSoundCore::NewSample(void*, TbSoundSampleInfo&) { return nullptr; }
void* TbSoundCore::NewControl(void*, TbSoundSampleInfo&) { return nullptr; }
TbError TbSoundCore::SystemCommand(const SystemCommand::Command&) { return TbError(-77); }
TbSoundSystemModule* TbSoundCore::GetSystemModule() const { return nullptr; }
TbSoundSampleStore* TbSoundCore::GetSampleStore() const { return nullptr; }
CMemoryCopyInit::CMemoryCopyInit() {}
CMemoryCopyInit::~CMemoryCopyInit() {}

TbSoundManager::TbSoundManager() : TbSoundCore() {
    mListenerPosition = nullptr;
    mAudioSystem = nullptr;
    ev("Manager.ctor", {lab(this)});
}
TbSoundManager::~TbSoundManager() {
    ev("Manager.~", {lab(this)});
}
TbError TbSoundManager::Init(const char* path) {
    ev("Manager.Init", {lab(this)}, {path != nullptr ? path : "(null)"});
    return TbError(static_cast<int32_t>(ret("Manager.Init", 0)));
}
float TbSoundManager::GetRolloffFactorToMinMaxDistance(float, float) { return 0.0f; }
TbDynamicStringTemplate<char>& TbSoundManager::GetSoundPath() {
    ev("Manager.GetSoundPath", {lab(this)});
    return mSoundPath;
}
void TbSoundManager::SetListenerPosition(const TbSoundPosition& position) {
    ev("Manager.SetListenerPosition", {lab(this), lab(&position)});
    mListenerPosition = &position;
}
void TbSoundManager::SetAudioSystem(CAudioComponent* component) {
    ev("Manager.SetAudioSystem", {lab(this), lab(component)});
    mAudioSystem = component;
}
bool TbSoundManager::VolumeControlIsOn(uint8_t mask) {
    ev("Manager.VolumeControlIsOn", {lab(this), mask});
    return ret("Manager.VolumeOn", 0) != 0;
}
TbError TbSoundManager::SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& front, const TbSoundPosition& top,
                                               const TbSoundPosition& velocity) {
    RecPositions("Manager.SetListenerOrientation.a", lab(this), position, front);
    RecPositions("Manager.SetListenerOrientation.b", lab(this), top, velocity);
    ev("Manager.SetListenerOrientation", {lab(&position), lab(&front), lab(&top), lab(&velocity)});
    return TbError(static_cast<int32_t>(ret("Manager.Orient", 0)));
}

// ---- the streamer and the mapper --------------------------------------------------------------------------------------------

namespace {
alignas(16) uint8_t gStreamerBytes[sizeof(TbMapStreamer) + 16];
TbMapStreamer* Streamer() {
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}

alignas(16) uint8_t gContainerBytes[3][sizeof(CCatagoryContainer) + 16];
alignas(16) uint8_t gCatagoryDataBytes[3][sizeof(CCatagoryData) + 16];
alignas(16) uint8_t gSoundBytes[sizeof(CEventData) + 16];
CEventData* Sound() {
    return reinterpret_cast<CEventData*>(gSoundBytes);
}
}  // namespace

TbMapStreamer* TbMapStreamer::Instance() {
    return Streamer();
}
void TbMapStreamer::SetSoundManager(TbSoundManager* manager) {
    ev("Streamer.SetSoundManager", {lab(this), lab(manager)});
}
TbError TbMapStreamer::RegisterCatagory(const char* path, const char* name, uint32_t& handle) {
    ev("Streamer.Register", {lab(this), handle}, {path != nullptr ? path : "", name != nullptr ? name : ""});
    handle = static_cast<uint32_t>(ret("Streamer.handle", 0));
    return TbError(static_cast<int32_t>(ret("Streamer.Register", 0)));
}
TbError TbMapStreamer::RemoveCatagory(uint32_t category) {
    ev("Streamer.Remove", {lab(this), category});
    return TbError(static_cast<int32_t>(ret("Streamer.Remove", 0)));
}

// The library's event interface: the game's object arrives unchanged, so the fields are read straight from it (no virtual call).
CEventData* TbEventMapper::MapEvent(CCatagoryData* data, const sound::TbAudioEvent& event) {
    const TbAudioEvent& game = reinterpret_cast<const TbAudioEvent&>(event);
    ev("MapEvent", {lab(data), lab(&event), static_cast<uint32_t>(game.mCategory), static_cast<uint32_t>(game.mEvent)});
    return ret("Map", 0) != 0 ? Sound() : nullptr;
}

// ---- the placeholder list and the placeholders -------------------------------------------------------------------------------

namespace {
// A placeholder the list finds: the flags and the priority record are set by the case.
class FakeHolder : public CAudioPlaceHolder {
public:
    TbError Stop() override {
        ev("Holder.Stop", {lab(this)});
        return TbError(static_cast<int32_t>(ret("Holder.Stop", 0)));
    }
};
FakeHolder* gHolder;
}  // namespace

CAudioPlaceHolder::~CAudioPlaceHolder() {}
TbError CAudioPlaceHolder::Stop() { return TbError(-77); }
uint32_t CAudioPlaceHolder::CDistPriority::GetBasePriority() const { return mBasePriority; }
void CAudioPlaceHolder::SetSoundManager(TbSoundManager* manager) {
    ev("Holder.SetSoundManager", {lab(manager)});
}
// Paste into unit_AudioBody.cpp after "CAudioPlaceHolder::SetSoundManager" (w79f's section "the placeholder list and the
// placeholders"). With w79g2's real header, FakeHolder's vtable names every virtual of the class and its constructor, so the
// unit library needs a definition of each; none of them is called by TbAudioBody. TbSndCallback has two out-of-line virtuals
// (its vtable and typeinfo are emitted where SoundCallback is defined).
TbSndCallback::TbSndCallback() {}
void TbSndCallback::SoundCallback(void*, void*) {}
void TbSndCallback::CompletedCallback(void*, void*, int32_t) {}
CAudioPlaceHolder::CAudioPlaceHolder() : mEvent(nullptr), mSound(nullptr), mSample(nullptr), mEndTime(0), mHandle(nullptr),
    mCatagory(nullptr), mListHandle(nullptr), mFlags(0), mNext(nullptr), mPrevious(nullptr), mPauseTime(0), mMaxDistance(0.0f),
    mLastSound(-1), mLastSample(-1), mVolumeControlIndex(0), mFade(0.0f), mVolume(0), mPitch(0), mExtra(nullptr) {}
void CAudioPlaceHolder::CompletedCallback(void*, void*, int32_t) {}
void CAudioPlaceHolder::Release() {}
TbError CAudioPlaceHolder::Init(CCatagoryContainer*, void*, const TbSoundPosition&, float) { return TbError(0); }
TbError CAudioPlaceHolder::PlaySample(uint32_t) { return TbError(0); }
TbError CAudioPlaceHolder::Pause() { return TbError(0); }
TbError CAudioPlaceHolder::Restart() { return TbError(0); }
float CAudioPlaceHolder::ChangeDistPriority() { return 0.0f; }
TbError CAudioPlaceHolder::Update(const TbSoundPosition&, const TbSoundPosition&) { return TbError(0); }
void CAudioPlaceHolder::UpdateParameter(uint8_t, uint32_t) {}
void CAudioPlaceHolder::UpdateParameterDepandants() {}
bool CAudioPlaceHolder::ChooseRandomSound(bool) { return false; }
CSampleData* CAudioPlaceHolder::ChooseRandomSample(bool) { return nullptr; }
bool CAudioPlaceHolder::GetParameterValue(uint32_t, uint32_t&) { return false; }
bool CAudioPlaceHolder::FillSampleInfo(uint32_t, TbSoundSampleInfo&) { return false; }
bool CAudioPlaceHolder::FillBasicSampleInfo(TbSoundSampleInfo&) { return false; }
int32_t CAudioPlaceHolder::GetPsuedoTime() { return 0; }
uint32_t CAudioPlaceHolder::GetClassID() { return 0; }

// w79g1's real class holds a critical section and the priority array by value: the fake constructor gives the array no data (its
// destructor then frees nothing) and the two small members of the critical section are defined here (the real ones are in
// src/sound/TbSoundCodecLoop.cpp, which this unit does not link).
CAutoCriticalSection::CAutoCriticalSection() : mValue(0) {}
CAutoCriticalSection::~CAutoCriticalSection() = default;
CAudioPlaceHolderList::CAudioPlaceHolderList(uint32_t channels) : mPriorityList(nullptr) {
    ev("List.ctor", {lab(this), channels});
}
CAudioPlaceHolderList::~CAudioPlaceHolderList() {
    ev("List.~", {lab(this)});
}
CAudioPlaceHolder* CAudioPlaceHolderList::FindPlaceHolder(void* handle) {
    ev("List.Find", {lab(this), Low32(handle)});
    return ret("List.Find", 0) != 0 ? gHolder : nullptr;
}
void* CAudioPlaceHolderList::CreatePlaceHolder(CCatagoryContainer* container, void* sound, const sound::TbAudioEvent& event) {
    ev("List.Create", {lab(this), lab(container), lab(sound), lab(&event)});
    return Cookie(ret("List.Create", 0));
}
uint32_t CAudioPlaceHolderList::UpdatePlaceHolder(void* handle, const TbSoundPosition& position, const TbSoundPosition& velocity) {
    RecPositions("List.Update", Low32(handle), position, velocity);
    return static_cast<uint32_t>(ret("List.Update", 0));
}
uint32_t CAudioPlaceHolderList::UpdatePlaceHolderParameter(void* handle, uint8_t parameter, uint32_t value) {
    ev("List.UpdateParameter", {lab(this), Low32(handle), parameter, value});
    return static_cast<uint32_t>(ret("List.UpdateParameter", 0));
}

// ---- the other objects of the world -----------------------------------------------------------------------------------------

namespace {

// A component whose Init (slot +0x38) records the info and answers a queue.
class HookedComponent : public CAudioComponent {
public:
    TbError Init(const TbSoundSystemInfo& info) override {
        RecInfo("Comp.Init", lab(this), info);
        return TbError(static_cast<int32_t>(ret("Comp.Init", 0)));
    }
};

// A body whose CreateSystemChain records and installs nothing: the case has put the manager and the component in place.
class HookedBody : public TbAudioBody {
public:
    TbError CreateSystemChain(const CAudioConfig& config) override {
        uint64_t bits;
        std::memcpy(&bits, &config.mQuality, 8);
        ev("Body.CreateSystemChain", {lab(this), lab(&config), static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xFFFFFFFFu),
                                      static_cast<uint32_t>(config.mChannels)});
        return TbError(0);
    }
};

class TestCommand : public TbSysCommand::Command {
public:
    TbError Execute(const TbAudioBody* body) const override {
        ev("Cmd.Execute", {lab(this), lab(body)});
        return TbError(static_cast<int32_t>(ret("Cmd.Execute", 0)));
    }
};

class TestEvent : public TbAudioEvent {
public:
    TestEvent(int32_t category, int32_t event) : TbAudioEvent(category, event, TbSoundPosition(), 0.0f) {}
    int32_t GetCategory() const override {
        ev("Event.GetCategory", {lab(this)});
        return mCategory;
    }
};

enum Param {
    P_MODE = 0, P_MGR = 1, P_COMP = 2, P_LIST = 3, P_MREF = 4, P_CREF = 5, P_CHAIN = 6, P_POS = 8, P_ARGPOS = 20, P_A = 32,
    P_NCAT = 40, P_KIND = 41, P_F08 = 44, P_SFLAGS = 47, P_SPRIO = 48, P_EVCAT = 49, P_EVEV = 50, P_HFLAGS = 52, P_HPRIO = 53,
    P_QUALITY_HI = 54, P_QUALITY_LO = 55
};
enum Out {
    O_RESULT = 0, O_BODYPOS = 1, O_MGR = 13, O_COMP = 14, O_LIST = 15, O_AUDIO = 16, O_LISTENER = 17, O_MREF = 18, O_CREF = 19,
    O_HFLAGS = 20, O_NCATS = 21, O_HANDLE = 22, O_NEXT = 23, O_PREV = 24, O_OUTPTR = 25, O_X = 26
};
enum Op {
    kTbAudioSystemCtor = 0, kCtor, kDtor, kCapCtor, kCapDtor, kCapArrow, kCapConv, kChain, kInit, kRelease, kDereg, kQualify,
    kRegister, kSend, kCanBeDropped, kUpdateHandle, kUpdateParameter, kStop, kSystemCommand, kOrient, kUpdateDistortion,
    kUpdateLowPass, kGetSoundManager, kGetPlaceHolderList, kCreateCore, kCreateComponent, kCreate, kIsSlash
};

std::string gText;       // the qualified path or another text the operation produced

// The body of the world: made with the log off; the list gets label 4.
TbAudioBody* gBody;
TbSoundManager* gManager;
CAudioComponent* gComponent;
CAudioConfig* gConfig;

void Labels(TbAudioBody* body) {
    tpw_cl_label(1, body);
    tpw_cl_label(12, &body->mPosition);
    tpw_cl_label(13, &body->mForward);
    tpw_cl_label(14, &body->mUp);
    tpw_cl_label(15, &body->mVelocity);
}

}  // namespace

extern "C" {

const char* tpw_ab_text() {
    return gText.c_str();
}

int32_t tpw_ab_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbAudioBody));
        case 1: return static_cast<int32_t>(sizeof(TbSoundManager));
        case 2: return static_cast<int32_t>(sizeof(CAudioComponent));
        case 3: return static_cast<int32_t>(sizeof(TbSoundCore));
    }
    return 0;
}

// One operation. p: the parameter words (P_ enum), names: six 64-byte texts (0 and 1 the case's texts, 3 the Init name, 4 the
// manager's sound path), out: the results (O_ enum).
void tpw_ab_run(int32_t op, const uint32_t* p, const char* names, uint32_t* out) {
    std::memset(out, 0, 64 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;
    gText.clear();
    gBody = nullptr;
    gManager = nullptr;
    gComponent = nullptr;

    // ---- the world, made with the log off
    // the streamer's catagory deque and the containers
    new (&Streamer()->mCatagories) CResize<CCatagoryContainer*>();
    tpw_cl_label(6, Streamer());
    const uint32_t ncat = p[P_NCAT];
    if (ncat != 0) {
        Streamer()->mCatagories.mData = new CCatagoryContainer*[ncat];
        Streamer()->mCatagories.mSize = ncat;
        for (uint32_t i = 0; i < ncat && i < 3; ++i) {
            Streamer()->mCatagories.mData[i] = nullptr;
            const uint32_t kind = p[P_KIND + i];
            if (kind == 0) continue;
            auto* container = reinterpret_cast<CCatagoryContainer*>(gContainerBytes[i]);
            std::memset(gContainerBytes[i], 0xA5, sizeof gContainerBytes[i]);
            container->mData = nullptr;
            tpw_cl_label(31 + i, container);
            if (kind == 2) {
                auto* data = reinterpret_cast<CCatagoryData*>(gCatagoryDataBytes[i]);
                std::memset(gCatagoryDataBytes[i], 0xA5, sizeof gCatagoryDataBytes[i]);
                data->mF08 = static_cast<uint16_t>(p[P_F08 + i]);
                container->mData = data;
                tpw_cl_label(21 + i, data);
            }
            Streamer()->mCatagories.mData[i] = container;
        }
    }
    CEventData* sound = Sound();
    std::memset(gSoundBytes, 0xA5, sizeof gSoundBytes);
    sound->mFlags = static_cast<uint16_t>(p[P_SFLAGS]);
    sound->mF0C = p[P_SPRIO];
    tpw_cl_label(7, sound);
    // the placeholder the list finds
    gHolder = new FakeHolder();
    gHolder->mFlags = p[P_HFLAGS];
    gHolder->mPriority.mDistance = static_cast<uint16_t>(p[P_HPRIO] >> 16);
    gHolder->mPriority.mBasePriority = static_cast<uint16_t>(p[P_HPRIO]);
    tpw_cl_label(5, gHolder);

    // the config: the double (as two words) and the channel count
    const uint64_t qbits = (static_cast<uint64_t>(p[P_QUALITY_HI]) << 32) | p[P_QUALITY_LO];
    double quality;
    std::memcpy(&quality, &qbits, 8);
    CAudioConfig config(quality, static_cast<int32_t>(p[P_A + 7]));
    gConfig = &config;
    tpw_cl_label(10, &config);

    TestEvent event(static_cast<int32_t>(p[P_EVCAT]), static_cast<int32_t>(p[P_EVEV]));
    tpw_cl_label(8, &event);
    TestCommand command;
    tpw_cl_label(9, &command);
    TbSoundPosition args[4];
    for (int i = 0; i < 4; ++i) {
        SetPos(args[i], p + P_ARGPOS + 3 * i);
        tpw_cl_label(41 + i, &args[i]);
    }
    TbSoundPosition bodyPos[4];

    alignas(16) static uint8_t rawBytes[sizeof(TbAudioBody) + 64];
    alignas(16) static uint8_t capBytes[64];
    const uint32_t a0 = p[P_A], a1 = p[P_A + 1], a2 = p[P_A + 2], a3 = p[P_A + 3];

    TbAudioBody* body = nullptr;
    const bool noBody = op == kTbAudioSystemCtor || op == kCtor || op == kCapCtor || op == kCapDtor || op == kCapArrow ||
                        op == kCapConv || op == kCreateCore || op == kCreateComponent || op == kCreate || op == kIsSlash;
    if (!noBody) {
        body = p[P_MODE] == 1 && (op == kInit) ? new HookedBody() : new TbAudioBody();
        gBody = body;
        Labels(body);
        tpw_cl_label(4, body->mPlaceHolders.mData);
        if (p[P_LIST] == 0) body->mPlaceHolders.mData = nullptr;
        for (int i = 0; i < 4; ++i) bodyPos[i] = TbSoundPosition();
        SetPos(body->mPosition, p + P_POS);
        SetPos(body->mForward, p + P_POS + 3);
        SetPos(body->mUp, p + P_POS + 6);
        SetPos(body->mVelocity, p + P_POS + 9);
        if (p[P_MGR] != 0) {
            gManager = new TbSoundManager();
            tpw_cl_label(2, gManager);
            gManager->mRefCount = static_cast<int32_t>(p[P_MREF]);
            gManager->mSoundPath = names + 64 * 4;
            body->mManager = gManager;
        }
        if (p[P_COMP] != 0) {
            gComponent = p[P_COMP] == 2 ? new HookedComponent() : new CAudioComponent();
            tpw_cl_label(3, gComponent);
            gComponent->mRefCount = static_cast<int32_t>(p[P_CREF]);
            body->mComponent = gComponent;
        }
        if (p[P_CHAIN] != 0 && gManager != nullptr && gComponent != nullptr) {
            gComponent->Chain(gManager);
            gManager->mRefCount = static_cast<int32_t>(p[P_MREF]);
            gComponent->mRefCount = static_cast<int32_t>(p[P_CREF]);
        }
    }

    // the owner pointer of the CAutoPtr operations: a list (a1 nonzero) or NULL, held by a CAutoPtr on the heap (label 13)
    CAutoPtr<CAudioPlaceHolderList>* cap = nullptr;
    if (op == kCapDtor || op == kCapArrow || op == kCapConv) {
        CAudioPlaceHolderList* list = a1 != 0 ? new CAudioPlaceHolderList(0xc) : nullptr;
        if (list != nullptr) tpw_cl_label(4, list);
        cap = new CAutoPtr<CAudioPlaceHolderList>(list);
        tpw_cl_label(13, cap);
    }
    TbDynamicStringTemplate<char> qualified(names + 64 * 1);

    // ---- the call
    tpw_cl_on = 1;
    gTrack = true;
    int64_t result = 0;
    uint32_t x[8] = {};
    bool counts = true;
    bool state = true;
    switch (op) {
        case kTbAudioSystemCtor:
            x[0] = 1;   // the vptr is checked on the PowerPC side; the host's base constructor is implicit
            state = false;
            counts = false;
            break;
        case kCtor: {
            std::memset(rawBytes, 0xA5, sizeof rawBytes);
            body = new (rawBytes) TbAudioBody();
            gBody = body;
            Labels(body);
            x[0] = 1;   // the vptrs and the bytes beyond the object are checked on the PowerPC side
            counts = false;
            break;
        }
        case kDtor:
            if (static_cast<int16_t>(a0) > 0) {
                delete body;
            } else {
                body->~TbAudioBody();
            }
            state = false;
            break;
        case kCapCtor: {
            auto* cap = new (capBytes) CAutoPtr<CAudioPlaceHolderList>(static_cast<CAudioPlaceHolderList*>(Cookie(a0)));
            x[0] = Low32(cap->mData);
            counts = false;
            state = false;
            break;
        }
        case kCapDtor: {
            if (static_cast<int16_t>(a0) > 0) {
                delete cap;
            } else {
                cap->~CAutoPtr();
            }
            counts = false;
            state = false;
            break;
        }
        case kCapArrow:
        case kCapConv: {
            result = op == kCapArrow ? lab(cap->operator->()) : lab(static_cast<CAudioPlaceHolderList*>(*cap));
            counts = false;
            state = false;
            break;
        }
        case kChain:
            body->CreateSystemChain(config);
            gManager = body->mManager;
            gComponent = body->mComponent;
            break;
        case kInit:
            result = static_cast<int32_t>(body->Init(names + 64 * 3, config));
            break;
        case kRelease:
            result = static_cast<int32_t>(body->Release());
            state = false;
            break;
        case kDereg:
            result = static_cast<int32_t>(static_cast<const TbAudioBody*>(body)->DeregisterCatagoryBanks(a0));
            break;
        case kQualify:
            body->QualifyPath(names, qualified);
            gText = Text(qualified);
            break;
        case kRegister: {
            uint32_t handle = a0;
            result = static_cast<int32_t>(body->RegisterCatagory(names, names + 64 * 1, handle));
            out[O_HANDLE] = handle;
            break;
        }
        case kSend:
            result = body->SendEvent(static_cast<int32_t>(a0), event);
            break;
        case kCanBeDropped:
            result = body->CanBeDroppedDueToVolControl(a0);
            break;
        case kUpdateHandle:
            result = body->UpdateHandle(static_cast<int32_t>(a0), args[0], args[1]);
            break;
        case kUpdateParameter:
            result = body->UpdateParameter(static_cast<int32_t>(a0), static_cast<uint8_t>(a1), a2);
            break;
        case kStop:
            result = static_cast<int32_t>(static_cast<const TbAudioBody*>(body)->StopHandle(Cookie(a0)));
            break;
        case kSystemCommand:
            result = static_cast<int32_t>(static_cast<const TbAudioBody*>(body)->SystemCommand(command));
            break;
        case kOrient:
            result = static_cast<int32_t>(body->SetListenerOrientation(args[0], args[1], args[2], args[3]));
            break;
        case kUpdateDistortion:
            result = static_cast<int32_t>(body->UpdateDistortion(Cookie(a0), BitsToFloat(a1)));
            break;
        case kUpdateLowPass:
            result = static_cast<int32_t>(body->UpdateLowPass(Cookie(a0), BitsToFloat(a1)));
            break;
        case kGetSoundManager:
            result = lab(body->GetSoundManager());
            break;
        case kGetPlaceHolderList:
            result = lab(body->GetPlaceHolderList());
            break;
        case kCreateCore: {
            TbSoundCore* core = CAudioComponentFactory::CreateCore();
            result = lab(core);
            x[0] = lab(core->mModule);
            x[1] = lab(core->mStore);
            x[2] = lab(core->mNext);
            x[3] = lab(core->mPrevious);
            x[4] = core->mId;
            x[5] = core->mRefCount;
            counts = false;
            state = false;
            break;
        }
        case kCreateComponent: {
            CAudioComponent* component = CAudioComponentFactory::CreateComponent();
            result = lab(component);
            x[2] = lab(component->mNext);
            x[3] = lab(component->mPrevious);
            x[4] = component->mId;
            x[5] = component->mRefCount;
            counts = false;
            state = false;
            break;
        }
        case kCreate: {
            TbAudioSystem* audio = static_cast<TbAudioSystem*>(Cookie(0xA5A5A5A5u));
            result = static_cast<int32_t>(CreateAudioObject(names + 64 * 3, &audio, config));
            out[O_OUTPTR] = static_cast<uint32_t>(lab(audio));
            body = static_cast<TbAudioBody*>(audio);
            counts = false;
            if (body == nullptr) {
                state = false;
            } else {
                gBody = body;
                gManager = body->mManager;
                gComponent = body->mComponent;
            }
            break;
        }
        case kIsSlash:
            result = IsSlash(static_cast<char>(a0));
            counts = false;
            state = false;
            break;
        default:
            break;
    }
    gTrack = false;

    out[O_RESULT] = static_cast<uint32_t>(result);
    if (state && body != nullptr && op != kDtor && op != kRelease) {
        PosWords(body->mPosition, out + O_BODYPOS);
        PosWords(body->mForward, out + O_BODYPOS + 3);
        PosWords(body->mUp, out + O_BODYPOS + 6);
        PosWords(body->mVelocity, out + O_BODYPOS + 9);
        out[O_MGR] = static_cast<uint32_t>(lab(body->mManager));
        out[O_COMP] = static_cast<uint32_t>(lab(body->mComponent));
        out[O_LIST] = static_cast<uint32_t>(lab(body->mPlaceHolders.mData));
        if (gManager != nullptr) {
            out[O_AUDIO] = static_cast<uint32_t>(lab(gManager->mAudioSystem));
            out[O_LISTENER] = static_cast<uint32_t>(lab(gManager->mListenerPosition));
            out[O_PREV] = static_cast<uint32_t>(lab(gManager->mPrevious));
        }
        if (gComponent != nullptr) out[O_NEXT] = static_cast<uint32_t>(lab(gComponent->mNext));
    }
    if (op == kCtor && body != nullptr) {
        PosWords(body->mPosition, out + O_BODYPOS);
        PosWords(body->mForward, out + O_BODYPOS + 3);
        PosWords(body->mUp, out + O_BODYPOS + 6);
        PosWords(body->mVelocity, out + O_BODYPOS + 9);
        out[O_MGR] = static_cast<uint32_t>(lab(body->mManager));
        out[O_COMP] = static_cast<uint32_t>(lab(body->mComponent));
        out[O_LIST] = static_cast<uint32_t>(lab(body->mPlaceHolders.mData));
    }
    if (counts) {
        if (gManager != nullptr) out[O_MREF] = static_cast<uint32_t>(gManager->mRefCount);
        if (gComponent != nullptr) out[O_CREF] = static_cast<uint32_t>(gComponent->mRefCount);
    }
    out[O_HFLAGS] = gHolder->mFlags;
    out[O_NCATS] = Streamer()->mCatagories.mSize;
    std::memcpy(out + O_X, x, sizeof x);
    (void)a3;
    (void)bodyPos;
}

}  // extern "C"
