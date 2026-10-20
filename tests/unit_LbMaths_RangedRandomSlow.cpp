// Test library for the small sound-program functions of unit v51112 (libtpw_unit_LbMaths_RangedRandomSlow.dylib,
// tests/test_LbMaths_RangedRandomSlow.py), sound program:
//   src/AudioPlaceHolder_LbMaths_Random_LbMaths_RangedRandomSlow.cpp, src/AudioPlaceHolder_LbMaths_Random.cpp (LbMaths_Random),
//   src/TbSoundSampleStore.cpp (CChannelIterator::operator++).
//
// Callees outside the unit are recording fakes: the runtime glue (register, seed, new[], delete[], delete), FillWaveFormatEx, the
// CFileStorage base destructor, the access object's Write. Replaced global operators log only while a wrapper has armed them.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AudioPlaceHolder.h"
#include "LbTime.h"
#include "TbMpegBase.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundSampleStore.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "snd_containers.h"

namespace tpw {
inline void* ArenaAllocZeroed(std::size_t n) { return std::calloc(1, n ? n : 1); }
}  // namespace tpw

using namespace calllog;


int32_t CAudioPlaceHolder::mRandomSeed;

namespace {
bool gArmed = false;

struct Arm {
    Arm() { gArmed = true; }
    ~Arm() { gArmed = false; }
};

// The access object: the same virtual order as CAccessItem (destructor pair, Read, Write).
struct FakeAccess {
    virtual ~FakeAccess() {}
    virtual int32_t Read(void*, uint32_t) { return 0; }
    virtual int32_t Write(const void* source, uint32_t size) {
        ev("Write", {lab(this), lab(const_cast<void*>(source)), static_cast<int64_t>(size)});
        return static_cast<int32_t>(ret("Write"));
    }
};
FakeAccess gAccess;
uint8_t gBuf[64];
}  // namespace

// ---- operators, armed only inside the wrappers that test allocation -----------------------------------------------------------
extern "C" void* SoundHeapNewPointers(uint32_t count) {
    ev("new[]", {static_cast<int64_t>(count) * 4 / 4});
    return tpw::ArenaAllocZeroed(count ? count * sizeof(void*) : 4);
}
extern "C" void SoundHeapDeleteArray(void* p) { ev("delete[]", {p != nullptr ? 1 : 0}); }
extern "C" void SoundHeapDelete(void* p) { ev("delete", {p != nullptr ? 1 : 0}); }

// ---- the fakes ------------------------------------------------------------------------------------------------------------------
_CS__TbSoundSampleWAV* _CS__TbSoundSampleWAV::Destruct(_CS__TbSoundSampleWAV* self, int16_t) { return self; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    int64_t id = -1;
    if (destructor == reinterpret_cast<void*>(&_CS__TbSoundSampleWAV::Destruct)) id = 1;
    if (destructor == reinterpret_cast<void*>(&DestructStandInMPEG2)) id = 2;
    if (destructor == reinterpret_cast<void*>(&DestructStandInCallback)) id = 3;
    if (destructor == reinterpret_cast<void*>(&DestructStandInCodecLoop)) id = 4;
    ev("register", {lab(object), id, lab(node)});
}

uint32_t LbTime_GetClock() { return 0; }   // called only by the source's weak host glue, which the fake below replaces

extern "C" int32_t MslSeedGlue1002eaac() {
    ev("seed", {0});
    return static_cast<int32_t>(ret("seed"));
}

void CFormat::FillWaveFormatEx() { ev("Fill", {lab(this)}); }

namespace bfl::FileSystem {
// The base destructor is faked, with every virtual of the class so that its vtable is emitted here.
CFileStorage::~CFileStorage() { ev("~CFileStorage", {1, 0}); }
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
}  // namespace bfl::FileSystem

extern "C" {

uint64_t tpw_v_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"seed", &CAudioPlaceHolder::mRandomSeed},
        {"wavobj", &gCS_TbSoundSampleWAVObject},
        {"wavnode", &gCS_TbSoundSampleWAVNode},
        {"mpegobj", &gCS_TbSoundSampleMPEG2Object},
        {"mpegnode", &gCS_TbSoundSampleMPEG2Node},
        {"cbobj", &gCS_TbSoundSampleCallbackObject},
        {"cbnode", &gCS_TbSoundSampleCallbackNode},
        {"loopobj", &gCS_TbSoundCodecLoopObject},
        {"loopnode", &gCS_TbSoundCodecLoopNode},
        {"acc", &gAccess},
        {"buf", gBuf},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// sizeof(TbSoundPipeChannel), offsetof(CFile, mImplementation)
int32_t tpw_v_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbSoundPipeChannel));
    out[n++] = static_cast<int64_t>(offsetof(bfl::FileSystem::CFile, mImplementation));
    return n;
}

uint32_t tpw_v_rrs(uint64_t seed, uint32_t range) {
    return LbMaths_RangedRandomSlow(*reinterpret_cast<int32_t*>(seed), range);
}
uint32_t tpw_v_brrs(uint64_t seed, uint32_t range) {
    return bfl::maths::RangedRandomSlow(*reinterpret_cast<int32_t*>(seed), range);
}
uint32_t tpw_v_brand(uint64_t seed) { return bfl::maths::Random(*reinterpret_cast<int32_t*>(seed)); }

uint64_t tpw_v_lbmax_ul(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(&LbMaxULong(*reinterpret_cast<uint32_t*>(a), *reinterpret_cast<uint32_t*>(b)));
}
uint64_t tpw_v_lbmin_f(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(&LbMinFloat(*reinterpret_cast<float*>(a), *reinterpret_cast<float*>(b)));
}
uint64_t tpw_v_lbmin_ui(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(&LbMinUInt(*reinterpret_cast<uint32_t*>(a), *reinterpret_cast<uint32_t*>(b)));
}
uint64_t tpw_v_lbmax_l(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(&LbMaxLong(*reinterpret_cast<int32_t*>(a), *reinterpret_cast<int32_t*>(b)));
}
uint64_t tpw_v_lbmin_i(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(&LbMinInt(*reinterpret_cast<int32_t*>(a), *reinterpret_cast<int32_t*>(b)));
}

int32_t tpw_v_rotr(int32_t value, int32_t amount) { return SamsUtilities::UBinary::RotateRight<int32_t>(value, amount); }
float tpw_v_logf(float x) { return tbstd::logf(x); }

void tpw_v_sinit_wav() { __sinit_TbSoundSampleWAV_cpp(); }
void tpw_v_sinit_mpeg() { __sinit_TbSoundSampleMPEG2_cpp(); }
void tpw_v_sinit_cb() { __sinit_TbSoundSampleCallback_cpp(); }
void tpw_v_sinit_loop() { __sinit_TbSoundCodecLoop_cpp(); }
void tpw_v_sinit_apl() { __sinit_AudioPlaceHolder_cpp(); }

uint64_t tpw_v_tbrect(uint64_t dest, uint64_t source) {
    return reinterpret_cast<uintptr_t>(TbRectAssign(reinterpret_cast<TbRect*>(dest), reinterpret_cast<const TbRect*>(source)));
}
uint64_t tpw_v_opnew(uint32_t size, uint64_t where) {
    return reinterpret_cast<uintptr_t>(OperatorNewPlacementSound(size, reinterpret_cast<void*>(where)));
}
uint64_t tpw_v_itinc(uint64_t it) {
    return reinterpret_cast<uintptr_t>(TbSoundSampleStoreSound::IteratorIncrement(
        reinterpret_cast<TbSoundSampleStoreSound::ChannelIterator*>(it), 0x60));
}
uint64_t tpw_v_fmt(uint64_t cf) { return reinterpret_cast<uintptr_t>(CFormatToWaveFormatEx(reinterpret_cast<CFormat*>(cf))); }
int32_t tpw_v_write(uint64_t file, uint64_t source, uint32_t size) {
    return CFileWriteMain(reinterpret_cast<bfl::FileSystem::CFile*>(file), reinterpret_cast<const void*>(source), size);
}
uint64_t tpw_v_dfs(uint64_t self, int16_t flag) {
    Arm arm;
    return reinterpret_cast<uintptr_t>(
        bfl::FileSystem::CDiscFileStorage::DestroySound(reinterpret_cast<bfl::FileSystem::CDiscFileStorage*>(self), flag));
}
uint64_t tpw_v_aapc(uint64_t self, uint32_t count) {
    Arm arm;
    return reinterpret_cast<uintptr_t>(CAutoArrayPtrPlaceHolderCtor(reinterpret_cast<CAutoArrayPtr<CAudioPlaceHolder*>*>(self), count));
}
uint64_t tpw_v_aapd(uint64_t self, int16_t flag) {
    Arm arm;
    return reinterpret_cast<uintptr_t>(CAutoArrayPtrPlaceHolderDtor(reinterpret_cast<CAutoArrayPtr<CAudioPlaceHolder*>*>(self), flag));
}
uint64_t tpw_v_aapo(uint64_t self) {
    return reinterpret_cast<uintptr_t>(CAutoArrayPtrPlaceHolderGet(reinterpret_cast<CAutoArrayPtr<CAudioPlaceHolder*>*>(self)));
}

}  // extern "C"
