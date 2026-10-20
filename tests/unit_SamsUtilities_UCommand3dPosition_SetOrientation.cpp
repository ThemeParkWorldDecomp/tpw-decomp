// Test library for the position commands, GetTag answers, speaker kinds and the static initialiser of CSprocketsModule.cpp (unit v51053;
// libtpw_unit_SamsUtilities_UCommand3dPosition_SetOrientation.dylib, tests/test_SamsUtilities_UCommand3dPosition_SetOrientation.py), sound program:
//   src/TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_SamsUtilities_UCommand3dPosition_SetOrientation.cpp
//
// The callees outside the unit are recording fakes: UHasFlags SetFlag and constructor, UCommand's base constructor, TbSoundPosition::Get,
// SendToChannel, UStatus::IfOk, the pool queue constructor and __register_global_object. The globals of other files the unit reads
// (the pool flags and the TVector of the cleanup destructor) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CSprocketsModule.h"
#include "CMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundPosition.h"

using namespace calllog;
using Pool = SamsUtilities::MpegFilePlayerPool;

// ---- globals of other files
uint8_t gPoolQueueBuilt;
uint8_t gPoolCleanupBuilt;
uint32_t gSinitDtor_1003352c[2];

namespace {
uint64_t gMarkBase;   // the object the offsets of the fakes are taken from (0: none)
uint64_t gMarkChan;   // the channel the test made
CSprocketsChannel* gChannelArray[2];

int64_t Off(const void* p) {
    uint64_t a = reinterpret_cast<uintptr_t>(p);
    return gMarkBase ? static_cast<int64_t>(a - gMarkBase) : 0;
}
}  // namespace

template <>
SamsUtilities::MpegFilePlayerPool::Queue::UInterruptSafeQueue() {
    if (on()) ev("QueueCtor", {lab(this)});
}
template <>
SamsUtilities::MpegFilePlayerPool::StaticCleanup::~StaticCleanup() {}
template <>
SamsUtilities::MpegFilePlayerPool::Queue SamsUtilities::MpegFilePlayerPool::gPoolQueue{};
template <>
SamsUtilities::MpegFilePlayerPool::StaticCleanup SamsUtilities::MpegFilePlayerPool::gPoolCleanup{};

// ---- the fakes
void UHasFlags_SetFlag(void* flags, uint32_t flag) { ev("SetFlag", {Off(flags), static_cast<int64_t>(flag)}); }
void UHasFlags_Construct(void* flags) { ev("UHasFlagsCtor", {Off(flags)}); }
namespace SamsUtilities {
void UCommand_ConstructBase(UCommand* command) { ev("UCommandCtor", {Off(command)}); }

UStatus SendToChannel(CSprocketsChannel* channel, UCommand& command) {
    int64_t vt = 0, d[3] = {0, 0, 0};
    uint32_t word;
    std::memcpy(&word, command.mStorage + 4, 4);
    vt = word;
    for (int i = 0; i < 3; i++) std::memcpy(&d[i], command.mStorage + 0x24 + 8 * i, 8);
    ev("Send", {reinterpret_cast<uintptr_t>(channel) == gMarkChan ? 1 : 0, vt, d[0], d[1], d[2]});
    return UStatus(static_cast<int32_t>(ret("Send")));
}

int32_t UStatus::IfOk(TbLegacyError ok, TbLegacyError failed) const {
    ev("IfOk", {mValue, ok.mValue, failed.mValue});
    return static_cast<int32_t>(ret("IfOk"));
}
}  // namespace SamsUtilities

void TbSoundPosition::Get(float& x, float& y, float& z) const {
    ev("Get", {0});
    uint32_t b[3] = {static_cast<uint32_t>(ret("GetX")), static_cast<uint32_t>(ret("GetY")), static_cast<uint32_t>(ret("GetZ"))};
    std::memcpy(&x, &b[0], 4);
    std::memcpy(&y, &b[1], 4);
    std::memcpy(&z, &b[2], 4);
}

CSprocketsChannel** CSprocketsModule::Channels() const { return gChannelArray; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_ssc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"queueBuilt", &gPoolQueueBuilt},
        {"cleanupBuilt", &gPoolCleanupBuilt},
        {"queue", &Pool::gPoolQueue},
        {"cleanup", &Pool::gPoolCleanup},
        {"dtor", gSinitDtor_1003352c},
        {"node", gSinit_1003f3b8},
        {"markBase", &gMarkBase},
        {"markChan", &gMarkChan},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ssc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ssc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SamsUtilities::UCommand, mStorage));
    out[n++] = static_cast<int64_t>(sizeof(SamsUtilities::UCommand));
    out[n++] = static_cast<int64_t>(sizeof(SamsUtilities::UCommand3dPosition));
    out[n++] = static_cast<int64_t>(sizeof(SamsUtilities::UStatus));
    return n;
}

void tpw_ssc_setchan(uint64_t chan) { gChannelArray[0] = reinterpret_cast<CSprocketsChannel*>(static_cast<uintptr_t>(chan)); }

uint64_t tpw_ssc_ctor(int32_t kind, uint64_t obj, uint32_t on) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    switch (kind) {
        case 0: new (p) SamsUtilities::UCommand3dPosition(on); break;
        case 1: new (p) SamsUtilities::UCommandListenerPosition(on); break;
        case 2: new (p) SamsUtilities::UCommandPosition(on); break;
        default: new (p) SamsUtilities::UCommandPreroll(on); break;
    }
    return obj;
}

void tpw_ssc_setter(int32_t kind, uint64_t obj, double x, double y, double z) {
    auto* c = reinterpret_cast<SamsUtilities::UCommand3dPosition*>(static_cast<uintptr_t>(obj));
    switch (kind) {
        case 0: c->SetLocation(x, y, z); break;
        case 1: c->SetVelocity(x, y, z); break;
        case 2: c->SetOrientation(x, y, z); break;
        default: c->SetUpVector(x, y, z); break;
    }
}

uint32_t tpw_ssc_tag(int32_t kind, uint64_t obj) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    using namespace SamsUtilities;
    switch (kind) {
        case 0: return static_cast<UCommandSpeakerType*>(p)->GetTag();
        case 1: return static_cast<UCommandListenerPosition*>(p)->GetTag();
        case 2: return static_cast<UCommandPreroll*>(p)->GetTag();
        case 3: return static_cast<UCommandVolume*>(p)->GetTag();
        case 4: return static_cast<UCommandPosition*>(p)->GetTag();
        case 5: return static_cast<UCommandPause*>(p)->GetTag();
        case 6: return static_cast<UCommandStop*>(p)->GetTag();
        case 7: return static_cast<UCommandSoundDistance*>(p)->GetTag();
        case 8: return static_cast<UCommandListenerScale*>(p)->GetTag();
        case 9: return static_cast<UCommandCPULoad*>(p)->GetTag();
        case 10: return static_cast<UCommandSoundType*>(p)->GetTag();
        case 11: return static_cast<UCommandLoadSound*>(p)->GetTag();
        default: return static_cast<UCommandPlay*>(p)->GetTag();
    }
}

uint32_t tpw_ssc_speaker(int32_t kind, uint64_t obj) {
    auto* s = reinterpret_cast<CSprocketsSpeaker*>(static_cast<uintptr_t>(obj));
    if (kind == 0) return s->Stereo();
    if (kind == 1) return static_cast<uint32_t>(s->Headphones());
    return static_cast<uint32_t>(s->Mono());
}

uint32_t tpw_ssc_isset(uint64_t obj) { return reinterpret_cast<SamsUtilities::UStatus*>(static_cast<uintptr_t>(obj))->IsSet() ? 1 : 0; }
void tpw_ssc_ustatus(uint64_t obj) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(obj))) SamsUtilities::UStatus(); }
void tpw_ssc_assign(uint64_t dst, uint64_t src) {
    *reinterpret_cast<SamsUtilities::UStatus*>(static_cast<uintptr_t>(dst)) =
        *reinterpret_cast<SamsUtilities::UStatus*>(static_cast<uintptr_t>(src));
}

int32_t tpw_ssc_speed(uint64_t module, float speed) {
    return reinterpret_cast<CSprocketsModule*>(static_cast<uintptr_t>(module))->CSprocketsModule::SetSpeedOfSound(speed).mValue;
}

int32_t tpw_ssc_velocity(uint64_t module, uint64_t position) {
    return reinterpret_cast<CSprocketsModule*>(static_cast<uintptr_t>(module))
        ->CSprocketsModule::SetListenerVelocity(*reinterpret_cast<const TbSoundPosition*>(static_cast<uintptr_t>(position))).mValue;
}

void tpw_ssc_sinit() { __sinit_CSprocketsModule_cpp(); }

}  // extern "C"
