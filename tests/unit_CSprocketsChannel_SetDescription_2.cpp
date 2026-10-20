// Test library for CSprocketsChannelMac::SetDescription, the UReferenceCounter<USoundDescription> copy constructor and destructor,
// TbMapStreamer::CCatFixupTable::Unload / LoadCatgoryFixupTable / SetDroppable and TbStaticFileBank (unit v51032;
// libtpw_unit_CSprocketsChannel_SetDescription_2.dylib, tests/test_CSprocketsChannel_SetDescription_2.py), sound program:
//   src/TbSysCommand_TpBody_Load_CSprocketsChannel_SetDescription_2.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read (pointers as labels) and answer from the
// test's queues; the blocks they hand out are set by the test (tpw_sd_setblock).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "CSprocketsModule.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {
uint64_t gBlocks[8];   // what the allocation fakes hand out: [0] the buffer, [1] the description

uint32_t Hash(const char* text, uint32_t h) {
    for (; *text != 0; ++text) h = h * 31u + static_cast<uint32_t>(static_cast<uint8_t>(*text));
    return h;
}
}  // namespace

// ---- fakes
namespace SamsUtilities {
// UDebug(format, text): the hash of the format and, when it has a %s, of the text.
void UDebug(const char* format, ...) {
    uint32_t h = Hash(format, 7);
    if (std::strstr(format, "%s") != nullptr) {
        va_list args;
        va_start(args, format);
        const char* text = va_arg(args, const char*);
        va_end(args);
        h = Hash(text, h);
    }
    ev("UDebug", {static_cast<int64_t>(h & 0x7fffffffu)});
}
void* UAllocatorMake(int32_t size) {
    ev("UAllocatorMake", {size});
    return ret("UAllocatorMake") != 0 ? reinterpret_cast<void*>(gBlocks[1]) : nullptr;
}
void USoundBuffer_Construct(void* object, void* buffer, uint32_t size, uint8_t bits, double rate, uint32_t compression,
                            int32_t channels) {
    uint64_t rateBits;
    std::memcpy(&rateBits, &rate, 8);
    ev("USoundBuffer", {lab(object), lab(buffer), size, bits, static_cast<int64_t>(rateBits >> 32),
                        static_cast<int64_t>(rateBits & 0xffffffffu), compression, channels});
}
void USoundDescriptionCounter_Make(UReferenceCounter<USoundDescription>* self, USoundDescription* description) {
    ev("CounterMake", {lab(description)});
    self->mObject = description;
}
void UReferenceCounted_AddReference(UReferenceCounted* object) { ev("AddReference", {lab(object)}); }
uint32_t UReferenceCounted::RemoveReference() {
    ev("RemoveReference", {lab(this)});
    return 0;
}
UReferenceCounted::~UReferenceCounted() {}
UCommandLoadSound::UCommandLoadSound(const UReferenceCounter<USoundDescription>& description) {
    ev("CmdCtor", {lab(description.mObject)});
}
UCommandLoadSound* UCommandLoadSound::Destruct(int16_t deleteFlag) {
    ev("CmdDtor", {deleteFlag});
    return this;
}
UStatus SendToChannel(CSprocketsChannelMac* channel, UCommand&) {
    ev("Send", {lab(channel)});
    return UStatus{0};
}
}  // namespace SamsUtilities

void CSprocketsChannelMac::Cleanup() { ev("Cleanup", {lab(this)}); }
void* AllocBuffer_(uint32_t bytes) {
    ev("AllocBuffer", {bytes});
    return ret("AllocBuffer") != 0 ? reinterpret_cast<void*>(gBlocks[0]) : nullptr;
}
void DisposeBuffer_(void* buffer) { ev("Dispose", {lab(buffer)}); }
void USoundChannel_Free(void* channel) { ev("Free", {lab(channel)}); }

TbError TbMapStreamer::CCatFixupTable::Load(bfl::FileSystem::CFile& file) {
    ev("Load", {lab(this), lab(&file)});
    return TbError(0);
}
void TbMapStreamer::ReShuffleSoundForDroppable(CSoundData* sound) { ev("Reshuffle", {lab(this), lab(sound)}); }
TbFileBank::~TbFileBank() { ev("~TbFileBank", {lab(this)}); }
// Only the vtable of TbFileBank (emitted here with its destructor) names these.
void TbFileBank::Deregister() {}
TbError TbFileBank::RegisterBank(const char*, uint32_t) { return TbError(0); }
TbError TbFileBank::RetrieveSample(TbSampleHeader**, uint32_t) { return TbError(0); }
const char* TbFileBank::GetSampleName(uint32_t) { return nullptr; }
bool TbFileBank::IsType(uint32_t) const { return false; }
// The bases' members the same vtables and the inline reference manager name; nothing here runs.
bfl::FileSystem::CFile::~CFile() {}
void bfl::FileSystem::CFile::Close() {}
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t bfl::FileSystem::CFile::GetSize() { return 0; }
int32_t bfl::FileSystem::CFile::GetPos() { return 0; }
void* bfl::FileSystem::CFile::MapFile() { return nullptr; }
TbError bfl::FileSystem::CFile::Flush() { return TbError(0); }
int32_t bfl::FileSystem::CFile::GetMacFileReference() { return 0; }
int32_t TbRefRelease::AddRef() { return 0; }
int32_t TbRefRelease::Release() { return 0; }
TbRefRelease::~TbRefRelease() {}
TbBank::~TbBank() {}
int32_t TbBank::GetNumberOfSamples() { return 0; }
uint32_t TbBank::GetSampleLength(uint32_t) { return 0; }
const char* TbBank::GetSampleName(uint32_t) { return nullptr; }
uint32_t TbBank::GetSamplePosition(uint32_t) { return 0; }
bfl::FileSystem::CFile* TbBank::GetHandle() { return nullptr; }
bool TbBank::IsStreaming() const { return false; }
TbError TbBank::Initialise(TbRefRelease**) { return TbError(0); }
// TbStaticFileBank's vtable (emitted here) names IsType, unit v51075's (src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp).
bool TbStaticFileBank::IsType(uint32_t) const { return false; }

extern "C" {

void tpw_sd_setblock(uint32_t index, uint64_t address) { gBlocks[index] = address; }

uint64_t tpw_sd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets the module writes by Mac offset (HOST_LAYOUT).
int32_t tpw_sd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(CSprocketsChannelMac, mBuffer);
    out[n++] = offsetof(CSprocketsChannelMac, mBufferSize);
    out[n++] = offsetof(TbMapStreamer::CCatFixupTable, mData);
    out[n++] = offsetof(TbMapStreamer::CCatFixupTable, mCount);
    out[n++] = offsetof(TbMapStreamer, mFixup);
    out[n++] = offsetof(TbMapStreamer, mCatagories);
    out[n++] = offsetof(CCatagoryContainer, mData);
    out[n++] = offsetof(TbStaticFileBank, mData);
    out[n++] = offsetof(TbStaticFileBank, mCache);
    out[n++] = offsetof(TbMapStreamer, mCatagories) + offsetof(CResize<CCatagoryContainer*>, mSize);
    return n;
}

// Ptr32 fields hold offsets from the arena base.
uint64_t tpw_sd_arenabase() { return (uint64_t)(uintptr_t)tpw::gArenaBase; }

void tpw_sd_setdesc(uint64_t channel, uint64_t header) {
    reinterpret_cast<CSprocketsChannelMac*>(channel)->SetDescription(*reinterpret_cast<TbSampleHeader*>(header));
}
void tpw_sd_copy(uint64_t self, uint64_t source) {
    SamsUtilities::USoundDescriptionCounter_Copy(reinterpret_cast<SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*>(self),
                                                 reinterpret_cast<SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*>(source));
}
void tpw_sd_counter_dtor(uint64_t self, int32_t flag) {
    SamsUtilities::USoundDescriptionCounter_Destruct(
        reinterpret_cast<SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*>(self), static_cast<int16_t>(flag));
}
void tpw_sd_unload(uint64_t fixup) { reinterpret_cast<TbMapStreamer::CCatFixupTable*>(fixup)->Unload(); }
uint32_t tpw_sd_loadcat(uint64_t streamer, uint64_t file) {
    return static_cast<uint32_t>(reinterpret_cast<TbMapStreamer*>(streamer)->LoadCatgoryFixupTable(
        reinterpret_cast<bfl::FileSystem::CFile*>(file)));
}
void tpw_sd_setdroppable(uint64_t streamer, uint32_t value) {
    TbMapStreamer* m = reinterpret_cast<TbMapStreamer*>(streamer);
    m->SetDroppable((value & 0xff) != 0);
}
void tpw_sd_sfb_dereg(uint64_t bank) { reinterpret_cast<TbStaticFileBank*>(bank)->TbStaticFileBank::Deregister(); }
uint64_t tpw_sd_sfb_dtor(uint64_t bank, int32_t flag) {
    return reinterpret_cast<uintptr_t>(reinterpret_cast<TbStaticFileBank*>(bank)->Destruct(static_cast<int16_t>(flag)));
}

}  // extern "C"
