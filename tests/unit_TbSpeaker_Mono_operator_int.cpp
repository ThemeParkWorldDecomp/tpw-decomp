// Test library for small sound functions (unit v51075; libtpw_unit_TbSpeaker_Mono_operator_int.dylib,
// tests/test_TbSpeaker_Mono_operator_int.py), sound symbols shared:
//   src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp   the speaker conversions, CBank forwards, cleanup objects, sinit, math
//   src/TbSoundSampleStore.cpp                        TbSoundSampleStore::operator[] (the other functions of that file are not called)
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: the virtual-call glue 0x1002f154 (the speaker
// and the bank are fake objects whose slots log), the free 0x1002ea94, __register_global_object 0x1002eefc, pow 0x1002ee54, the
// three ClearCache and CChannelState::CChannelState. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CSprocketsModule.h"
#include "TbDynamicStringTemplate.h"
#include "CCodecFactory.h"
#include "TbGraphicFile.h"
#include "TbSoundCore.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundSampleStore.h"
#include "snd_handles.h"

using namespace calllog;

const char* NameText(const TbDynamicStringTemplate<char>& name);   // src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp

// ---- globals of other files, defined here
void* TbSpeaker::AutoDelete::_mpSpeaker = nullptr;
CChannelState CChannelState::mChannelState;
CAtomicValue::CAtomicValue() : mValue(0) {}

// link fakes: the pipe base and the channel's constructor (only linked, never run by the tests)
TbSoundSamplePipe* TbSoundSamplePipe::mFirstTbSoundSamplePipe = nullptr;
int32_t TbSoundSamplePipe::mCurrentTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mLowTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mHighTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mTotalTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mLevelTbSoundSamplePipe = 10;
TbSoundSamplePipe::TbSoundSamplePipe() : mPoolLink(nullptr), mNext(nullptr), mPrevious(nullptr), mHandle(nullptr) {}
TbSoundSamplePipe::~TbSoundSamplePipe() {}
TbSoundSamplePipe* TbSoundSamplePipe::Attatch(TbSoundSamplePipe*) { return nullptr; }
void TbSoundSamplePipe::Release() {}
void TbSoundSamplePipe::Detatch() {}
TbSoundSamplePipe* TbSoundSamplePipe::FindPipe(void*) { return nullptr; }
void TbSoundSamplePipe::GetInfo(TbSoundSampleInfo&) {}
uint32_t TbSoundSamplePipe::GetSampleRate() { return 0; }
int32_t TbSoundSamplePipe::GetDataChunk(void*, int32_t, int32_t&, bool) { return -1; }
void TbSoundSamplePipe::Restart() {}
TbError TbSoundSamplePipe::SetInfo(const TbSoundSampleInfo&) { return TbError(0); }
void* TbSoundSamplePipe::SetHandle(void* h) { return h; }
void TbSoundSamplePipe::Completed(bool) {}
void TbSoundSamplePipe::SetDataStart(void*) {}
void* TbSoundSamplePipe::operator new(std::size_t size) { return ::operator new(size); }
void TbSoundSamplePipe::operator delete(void* p, std::size_t) { ::operator delete(p); }
TbSoundPipeChannel::TbSoundPipeChannel() : mServiceBuffer(0x10u) {}
TbSoundPipeChannel::~TbSoundPipeChannel() {}
TbSoundVolumeControl::TbSoundVolumeControl() : mVolumes(nullptr), mCount(0) {}
uint32_t TbSoundSystemInfo::GetBuffersToUse() const { return mBuffersToUse; }
CLowest::CLowest() : mChannel(nullptr), mPriority(0) {}
void CLowest::operator()(uint32_t, TbSoundPipeChannel*) {}
CAutoCriticalSection::CAutoCriticalSection() : mValue(0) {}
CAutoCriticalSection::~CAutoCriticalSection() {}
CAutoCriticalSection::operator void**() { return nullptr; }

namespace {

int64_t Bits64(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    return bits;
}

// The fake speaker: slot +8, +0xc and +0x10 log the marker the PowerPC glue stub reads from r12.
struct FakeModule : TbSpeaker::Module {
    int32_t StereoValue() override {
        ev("vcall", {0x7008, lab(this)});
        return static_cast<int32_t>(ret("vcall"));
    }
    int32_t HeadphonesValue() override {
        ev("vcall", {0x700c, lab(this)});
        return static_cast<int32_t>(ret("vcall"));
    }
    int32_t MonoValue() override {
        ev("vcall", {0x7010, lab(this)});
        return static_cast<int32_t>(ret("vcall"));
    }
};
FakeModule gSpeaker;

// The fake bank: an object whose vtable holds the three slots the CBank forwards use (the slot numbers come from TbBank itself).
void* gBankVtable[64];
struct BankObject {
    void** vptr;
} gBankObj = {gBankVtable};
bool gBankReady = false;

template <class PMF>
size_t SlotOf(PMF pmf) {
    static_assert(sizeof(PMF) == 16, "pointer to member function");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof(words));
    size_t offset = (words[1] & 1) ? words[0] : words[0] - 1;
    return offset / sizeof(void*);
}

uint32_t FakeLength(void* self, uint32_t index) {
    ev("vcall", {0x7020, lab(self), static_cast<int64_t>(static_cast<int32_t>(index))});
    return static_cast<uint32_t>(ret("vcall"));
}
const char* FakeName(void* self, uint32_t index) {
    ev("vcall", {0x7024, lab(self), static_cast<int64_t>(static_cast<int32_t>(index))});
    return reinterpret_cast<const char*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("vcall"))));
}
bool FakeIsType(void* self, uint32_t type) {
    ev("vcall", {0x7030, lab(self), static_cast<int64_t>(static_cast<int32_t>(type))});
    return static_cast<uint32_t>(ret("vcall")) != 0;
}

void SetupBank() {
    if (gBankReady) return;
    gBankReady = true;
    gBankVtable[SlotOf(&TbBank::GetSampleLength)] = reinterpret_cast<void*>(&FakeLength);
    gBankVtable[SlotOf(&TbBank::GetSampleName)] = reinterpret_cast<void*>(&FakeName);
    gBankVtable[SlotOf(static_cast<bool (TbBank::*)(uint32_t) const>(&TbBank::IsType))] = reinterpret_cast<void*>(&FakeIsType);
}

}  // namespace

// ---- the fakes
// Referenced by other functions of src/TbSoundSampleStore.cpp, never called here.
TbError TbSoundCore::StopAndReleasePipe(TbSoundPipeChannel*, bool) { return TbError(0); }
TbSoundSamplePipe* CCodecFactory::CreateCodec(TbSoundPipeChannel*, TbSoundSampleInfo&) { return nullptr; }
TbSoundSamplePipe* CCodecFactory::CreateController(TbSoundSampleInfo&) { return nullptr; }
int32_t CCodecFactory::GetNextSampleFromBank(TbSampleHeader**, TbSoundSampleInfo&) { return 0; }
TbSoundSamplePipe* CCodecFactory::CreateCodecModifier(TbSoundSampleInfo&, TbSampleHeader*, TbSoundSamplePipe*) { return nullptr; }
CUniqueHandle::CUniqueHandle() : mValue(0) {}
CUniqueHandle::CUniqueHandle(void* value) : mValue(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value))) {}
CUniqueHandle::operator void*() const { return reinterpret_cast<void*>(static_cast<uintptr_t>(mValue)); }
uint32_t CUniqueHandle::GetIndex() const { return 0; }
void CUniqueHandle::SetIndex(uint16_t) {}
uint32_t CUniqueHandle::operator++() { return ++mValue; }
uint32_t CUniqueHandle::_mBitsPerCount = 26;
uint32_t TbSoundPipeChannel::GetNextHandle() { return 0; }
TbSoundSamplePipe* TbSoundPipeChannel::Attatch(TbSoundSamplePipe*) { return nullptr; }
int32_t TbSoundPipeChannel::GetDataChunk(void*, int32_t, int32_t&, bool) { return 0; }
TbError TbSoundPipeChannel::SetInfo(const TbSoundSampleInfo&) { return TbError(0); }
TbSoundSamplePipe* TbSoundPipeChannel::FindPipe(const CUniqueHandle&) { return nullptr; }
bool TbSoundPipeChannel::ReleasePipe(bool) { return false; }
void TbSoundPipeChannel::SetSampleHeader(const TbSampleHeader&) {}
void TbSoundPipeChannel::SetPriority(uint32_t) {}
uint32_t TbSoundPipeChannel::GetPriority() { return 0; }
void TbSoundPipeChannel::SetVolume(uint32_t) {}
uint32_t TbSoundPipeChannel::GetActualVolume() const { return 0; }
uint32_t TbSoundPipeChannel::GetSampleFlags() { return 0; }
TbSampleHeader* TbSoundPipeChannel::GetSampleHeader() { return nullptr; }
uint32_t TbSoundPipeChannel::ControlledBy(uint32_t) const { return 0; }
void* TbSoundPipeChannel::GetHandle() { return nullptr; }
uint32_t TbSoundPipeChannel::GetIndex() { return 0; }
void TbSoundPipeChannel::SetIndex(uint16_t) {}
TbError TbSoundPipeChannel::SetPipeInfo(void*, TbSoundSampleInfo&) { return TbError(0); }
void TbSoundPipeChannel::GetChannelInfo(uint32_t&, uint32_t&) {}
void* TbSoundPipeChannel::InitChannelInfo(TbSoundSampleInfo&, uint32_t&) { return nullptr; }
uint32_t TbSoundPipeChannel::FillServiceBuffer() { return 0; }
TbSoundSamplePipe* TbSoundPipeChannel::GetChannelUsed() { return nullptr; }
uint32_t TbSoundPipeChannel::GetVolume() { return 0; }
CCodecFactory::CCodecFactory() {}
CCodecFactory::~CCodecFactory() {}
void CChannelState::AddChannel() {}
void CChannelState::RemoveChannel() {}
int32_t CChannelState::GetNumberOfChannelsInUse() { return 0; }
CChannelState* CChannelState::Instance() { return &mChannelState; }
CChannelState::CChannelState() { ev("CChannelState", {lab(this)}); }

void USoundChannel_Free(void* channel) { ev("free", {lab(channel)}); }

void TbTimerCodec::ClearCache() { ev("ClearTimer", {0}); }
void _TbSoundSampleWAV::ClearCache() { ev("ClearWAV", {0}); }
void _TbSoundSampleWAVFile::ClearCache() { ev("ClearWAVFile", {0}); }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    int64_t tag = 0;
    if (destructor == reinterpret_cast<void*>(&DestructUserCallbackCleanup)) tag = 1;
    if (destructor == reinterpret_cast<void*>(&_CS_TbTimerCodec::Destruct)) tag = 2;
    if (destructor == reinterpret_cast<void*>(&_CS__TbSoundSampleWAVFile::Destruct)) tag = 3;
    ev("register", {lab(object), tag, lab(node)});
}

double MslPow(double a, double b) {
    int64_t x = Bits64(a), y = Bits64(b);
    ev("pow", {static_cast<int64_t>(static_cast<uint32_t>(static_cast<uint64_t>(x) >> 32)), static_cast<int64_t>(static_cast<uint32_t>(x)),
               static_cast<int64_t>(static_cast<uint32_t>(static_cast<uint64_t>(y) >> 32)), static_cast<int64_t>(static_cast<uint32_t>(y))});
    uint64_t hi = static_cast<uint32_t>(ret("powhi"));
    uint64_t lo = static_cast<uint32_t>(ret("powlo"));
    uint64_t bits = (hi << 32) | lo;
    double answer;
    std::memcpy(&answer, &bits, 8);
    return answer;
}

extern "C" {

uint64_t tpw_u_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    SetupBank();
    const Entry entries[] = {
        {"speaker", &gSpeaker},
        {"bank", &gBankObj},
        {"channel_state", &CChannelState::mChannelState},
        {"ucobj", &gCS_TbSoundUserCallbackObject},
        {"ucnode", &gCS_TbSoundUserCallbackNode},
        {"tcobj", &gCS_TbTimerCodecObject},
        {"tcnode", &gCS_TbTimerCodecNode},
        {"wfobj", &gCS_TbSoundSampleWAVFileObject},
        {"wfnode", &gCS_TbSoundSampleWAVFileNode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_u_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_u_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbRect));
    return n;
}

// ---- the functions
uint32_t tpw_u_speaker_op(int32_t kind) {
    TbSpeaker::AutoDelete::_mpSpeaker = &gSpeaker;
    int value;
    if (kind == 0) {
        TbSpeaker::Mono m;
        value = m;
    } else if (kind == 1) {
        TbSpeaker::Headphones h;
        value = h;
    } else {
        TbSpeaker::Stereo s;
        value = s;
    }
    return static_cast<uint32_t>(value);
}

void tpw_u_put_speaker(uint64_t value) { TbSpeaker::AutoDelete::_mpSpeaker = reinterpret_cast<void*>(static_cast<uintptr_t>(value)); }
void tpw_u_set_module_speaker(uint64_t value) {
    TbSpeaker::ConfigSetModuleSpeaker(reinterpret_cast<TbSpeaker::Module*>(static_cast<uintptr_t>(value)));
}
uint64_t tpw_u_get_speaker() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(TbSpeaker::AutoDelete::_mpSpeaker)); }

void tpw_u_sinit(int32_t which) {
    if (which == 0) __sinit_ChannelState_cpp();
    if (which == 1) __sinit_UserCallback_cpp();
    if (which == 2) __sinit_TimerCodec_cpp();
    if (which == 3) __sinit_TbSoundSampleWAVFile_cpp();
}

uint64_t tpw_u_bank(int32_t op, int32_t has, uint32_t index) {
    SetupBank();
    Bank::CBank* bank = static_cast<Bank::CBank*>(tpw::ArenaAllocZeroed(sizeof(Bank::CBank)));
    bank->mBank = has ? reinterpret_cast<TbBank*>(&gBankObj) : nullptr;
    if (op == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(bank->GetSampleName(index)));
    if (op == 1) return bank->GetSampleLength(index);
    return bank->IsType(index) ? 1 : 0;
}

// The channel's index in the store's array (the host channel is larger than the Mac's 0x60), or 0xffffffff when not a whole multiple.
uint32_t tpw_u_store_index(uint64_t base, uint32_t index) {
    TbSoundSampleStore* store = static_cast<TbSoundSampleStore*>(tpw::ArenaAllocZeroed(sizeof(TbSoundSampleStore)));
    store->mChannels.mData = reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(base));
    uint64_t result = static_cast<uint64_t>(reinterpret_cast<uintptr_t>((*store)[index]));
    uint64_t delta = result - base;
    if (delta % sizeof(TbSoundPipeChannel) != 0) return 0xffffffffu;
    return static_cast<uint32_t>(delta / sizeof(TbSoundPipeChannel));
}

uint64_t tpw_u_destruct(int32_t which, uint64_t self, int32_t flag) {
    int16_t f = static_cast<int16_t>(flag);
    if (which == 0) return reinterpret_cast<uintptr_t>(_CS_TbTimerCodec::Destruct(reinterpret_cast<_CS_TbTimerCodec*>(self), f));
    if (which == 1)
        return reinterpret_cast<uintptr_t>(_CS__TbSoundSampleWAVFile::Destruct(reinterpret_cast<_CS__TbSoundSampleWAVFile*>(self), f));
    return reinterpret_cast<uintptr_t>(_CS__TbSoundSampleWAV::Destruct(reinterpret_cast<_CS__TbSoundSampleWAV*>(self), f));
}

void tpw_u_refmgr(uint64_t block) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) CRefManager(); }

uint32_t tpw_u_static_istype(uint32_t type) {
    TbStaticFileBank* bank = static_cast<TbStaticFileBank*>(tpw::ArenaAllocZeroed(sizeof(TbStaticFileBank)));
    return bank->TbStaticFileBank::IsType(type) ? 1 : 0;
}

uint64_t tpw_u_nametext(uint32_t value) {
    TbDynamicStringTemplate<char>* text =
        static_cast<TbDynamicStringTemplate<char>*>(tpw::ArenaAllocZeroed(sizeof(TbDynamicStringTemplate<char>)));
    text->mData = reinterpret_cast<char*>(static_cast<uintptr_t>(value));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(NameText(*text)));
}

float tpw_u_powf(float a, float b) { return tbstd::powf(a, b); }
float tpw_u_fabs_f(float x) { return tbstd::fabs(x); }
float tpw_u_fabsf(float x) { return tbstd::fabsf(x); }
double tpw_u_fabs_d(double x) { return tbstd::fabs(x); }

// 0 when LB_MIN answers the first, 1 the second.
uint32_t tpw_u_lbmin(uint64_t a, uint64_t b) {
    const int* pa = reinterpret_cast<const int*>(static_cast<uintptr_t>(a));
    const int* pb = reinterpret_cast<const int*>(static_cast<uintptr_t>(b));
    const int& answer = LB_MIN<int, int>(*pa, *pb);
    return &answer == pa ? 0 : 1;
}

void tpw_u_rect(uint64_t block) { TbRectConstruct(reinterpret_cast<TbRect*>(static_cast<uintptr_t>(block))); }

}  // extern "C"
