// Test library for the small sound-library functions of unit v51113 (libtpw_unit_TbMapStreamer_FixUpCatagoryIndex.dylib,
// tests/test_TbMapStreamer_FixUpCatagoryIndex.py), sound library:
//   src/sound/CSprocketsModule.cpp, src/sound/TbMapStreamer.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TpBody.cpp
//
// The callees outside the unit are fakes that record what the PowerPC stubs of the module record and answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "CSprocketsModule.h"
#include "TbAudioBody.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipe.h"
#include "bfl_FileSystem.h"

using namespace calllog;

namespace {
alignas(16) char gInstance[0x100];
CSprocketsChannel** gChannels = nullptr;

float FloatFromBits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
uint32_t BitsOfFloat(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}
}  // namespace

// ---- the fakes
void TbMapStreamer::CCatFixupTable::Unload() { ev("Unload", {lab(this)}); }
TbMapStreamer* TbMapStreamer::Instance() {
    ev("Instance", {0});
    return reinterpret_cast<TbMapStreamer*>(gInstance);
}
TbError TbMapStreamer::RemoveCatagory(uint32_t category) {
    ev("Remove", {lab(this), static_cast<int64_t>(category)});
    return TbError(static_cast<int32_t>(ret("Remove")));
}
void CSprocketsModule::Tick() { ev("Tick", {lab(this)}); }
CSprocketsChannel** CSprocketsModule::Channels() const { return gChannels; }

namespace SamsUtilities {
UStatus SendToChannel(CSprocketsChannel* channel, UCommand& command) {
    uint32_t word;
    std::memcpy(&word, command.mStorage + 4, 4);
    ev("Send", {lab(channel), static_cast<int64_t>(word)});
    return UStatus(static_cast<int32_t>(ret("send")));
}
int32_t UStatus::IfOk(TbLegacyError ok, TbLegacyError failed) const {
    ev("IfOk", {static_cast<int64_t>(mValue), static_cast<int64_t>(ok.mValue), static_cast<int64_t>(failed.mValue)});
    return static_cast<int32_t>(ret("IfOk"));
}
void UCommand_ConstructOn(UCommand*, uint32_t on) { ev("UCommand", {static_cast<int64_t>(on)}); }
}  // namespace SamsUtilities

void TbSoundSamplePipe::ClearCache() { ev("ClearCache", {0}); }
void _TbSoundCodecLoop::ClearCache() { ev("ClearCache2", {0}); }
void USoundChannel_Free(void* channel) { ev("Free", {lab(channel)}); }

// TbAudioBody's complete destructor, faked at its symbol so that no vtable is needed here.
extern "C" void tpw_fake_audiobody_dtor(TbAudioBody* self) __asm__("__ZN11TbAudioBodyD1Ev");
extern "C" void tpw_fake_audiobody_dtor(TbAudioBody* self) { ev("AudioBodyDtor", {lab(self), 0}); }

// The base file storage (the Mac's constructor glue 0x1002eecc): the constructor is recorded and touches only the vtable word; the
// virtual functions are empty (they give the class its vtable here, Close being the key function).
namespace bfl {
namespace FileSystem {
CFileStorage::CFileStorage() { ev("CFileStorage", {lab(this)}); }
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(0); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(0); }
TbError CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(0); }
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
const char* CFileStorage::GetRootPath() { return nullptr; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }
CFileStorage::~CFileStorage() {}
}  // namespace FileSystem
}  // namespace bfl

namespace {
struct Probe : CAudioFileSystem {
    Bank::CBank* GenerateBank(const TbSoundBankInfo&, CAudioComponent*) override { return nullptr; }
};
}  // namespace

extern "C" {

uint64_t tpw_ms_addr(const char* name) {
    if (std::strcmp(name, "inst") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gInstance));
    return 0;
}

uint64_t tpw_ms_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// ---- the map streamer
uint64_t tpw_ms_make() { return tpw_ms_alloc(sizeof(TbMapStreamer) + 16); }
uint64_t tpw_ms_fixup_addr(uint64_t obj) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<TbMapStreamer*>(obj)->mFixup));
}
uint32_t tpw_ms_fixup(uint32_t count, uint64_t data, uint32_t index) {
    TbMapStreamer* m = reinterpret_cast<TbMapStreamer*>(tpw_ms_make());
    m->mFixup.mCount = count;
    m->mFixup.mData = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(data));
    return m->FixUpCatagoryIndex(index);
}
void tpw_ms_unload(uint64_t obj) { reinterpret_cast<TbMapStreamer*>(obj)->UnloadCatgoryFixupTable(); }
uint64_t tpw_ms_stringtext(uint32_t length, uint64_t text) {
    SamsUtilities::UString* s = reinterpret_cast<SamsUtilities::UString*>(tpw_ms_alloc(sizeof(SamsUtilities::UString) + 16));
    s->mLength = length;
    s->mText = reinterpret_cast<char*>(static_cast<uintptr_t>(text));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(TbMapStreamerFixup::StringText(s)));
}

// ---- constructors: 1 when the vtable word was stored (the block starts as 0xAA bytes)
uint32_t tpw_ms_disc_ctor(uint64_t obj) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    new (p) bfl::FileSystem::CDiscFileStorage();
    uint64_t first;
    std::memcpy(&first, p, 8);
    return first != 0xAAAAAAAAAAAAAAAAull ? 1 : 0;
}
uint32_t tpw_ms_audiofs_ctor(uint64_t obj) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    new (p) Probe();
    uint64_t first;
    std::memcpy(&first, p, 8);
    return first != 0xAAAAAAAAAAAAAAAAull ? 1 : 0;
}
void tpw_ms_error_ctor(uint64_t obj) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(obj))) TbError(); }

// ---- CSprocketsModule: the module block (mBuffers) and the channel array the fake Channels() answers
uint64_t tpw_sp_setup(uint32_t count, uint64_t channels) {
    gChannels = reinterpret_cast<CSprocketsChannel**>(static_cast<uintptr_t>(channels));
    CSprocketsModule* m = reinterpret_cast<CSprocketsModule*>(tpw_ms_alloc(sizeof(CSprocketsModule) + 16));
    m->mBuffers = count;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}
int32_t tpw_sp_restart_channel(uint64_t module, uint32_t channel) {
    return reinterpret_cast<CSprocketsModule*>(module)->CSprocketsModule::RestartChannel(channel).mValue;
}
int32_t tpw_sp_restart_all(uint64_t module) { return reinterpret_cast<CSprocketsModule*>(module)->CSprocketsModule::RestartAll().mValue; }
void tpw_sp_command_play(uint64_t block, uint32_t on) {
    new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) SamsUtilities::UCommandPlay(on);
}

// ---- TpBody
int32_t tpw_tp_simple(int32_t which, uint64_t body, uint64_t arg) {
    TpBody* b = reinterpret_cast<TpBody*>(body);
    switch (which) {
    case 0: return b->Init(reinterpret_cast<const char*>(static_cast<uintptr_t>(arg))).mValue;
    case 1: return b->Close().mValue;
    case 2: return b->SetFades(static_cast<uint32_t>(arg)).mValue;
    case 3: return b->SetCatagoryReverb(static_cast<uint32_t>(arg)).mValue;
    default: return b->DeregisterCatagoryBanks(static_cast<uint32_t>(arg)).mValue;
    }
}
void tpw_tp_tick(uint64_t body) { reinterpret_cast<TpBody*>(body)->Tick(); }
uint64_t tpw_tp_destruct(uint64_t body, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(TpBody::Destruct(reinterpret_cast<TpBody*>(body), static_cast<int16_t>(flag))));
}

// ---- cleanup objects
uint64_t tpw_cs_pipe(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(_CS_TbSoundSamplePipe::Destruct(reinterpret_cast<_CS_TbSoundSamplePipe*>(self), static_cast<int16_t>(flag))));
}
uint64_t tpw_cs_loop(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(_CS__TbSoundCodecLoop::Destruct(reinterpret_cast<_CS__TbSoundCodecLoop*>(self), static_cast<int16_t>(flag))));
}

// ---- square roots (float bits in and out)
uint32_t tpw_sq_sqrtf(uint32_t bits) { return BitsOfFloat(tbstd::sqrtf(FloatFromBits(bits))); }
uint32_t tpw_sq_sqrt(uint32_t bits) { return BitsOfFloat(tbstd::sqrtOfFloat(FloatFromBits(bits))); }

}  // extern "C"
