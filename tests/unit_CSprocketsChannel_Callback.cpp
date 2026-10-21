// Test library for the Sound Sprockets channel (unit v5263; libtpw_unit_CSprocketsChannel_Callback.dylib, tests/test_CSprocketsChannel_Callback.py), main app:
//   src/sound/PlaceHolderSentence.cpp   CSprocketsChannel: constructor, destructor, Cleanup, Callback, Play, Set3D
//
// The callees outside the unit are recording fakes: the USoundChannel base and Sound Manager imports, the command constructors and
// SetImmediate (a constructor writes a tag word into its command, so the later events name which command they carry), the send glue,
// the description's pointer operator and the pipe's StreamerCallback (answers from the queues).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CSprocketsModule.h"
#include "TbSoundPipeChannel.h"

using namespace calllog;

namespace {

int64_t P32(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }

uint32_t Tag(const SamsUtilities::UCommand& c) {
    uint32_t t;
    std::memcpy(&t, c.mStorage, 4);
    return t;
}

void SetTag(SamsUtilities::UCommand* c, uint32_t t) { std::memcpy(c->mStorage, &t, 4); }

}  // namespace

// ---- the fakes
void USoundChannel_Construct(CSprocketsChannelMac* channel, int32_t size) {
    ev("BaseCtor", {static_cast<int64_t>(lab(channel)), static_cast<int64_t>(size)});
}
void USoundChannel_Destruct(CSprocketsChannelMac* channel, int16_t flag) {
    ev("BaseDtor", {static_cast<int64_t>(lab(channel)), static_cast<int64_t>(flag)});
}
void USoundChannel_Callback(CSprocketsChannelMac* channel, SndCommand* command) {
    ev("BaseCallback", {static_cast<int64_t>(lab(channel)), static_cast<int64_t>(command->cmd), static_cast<int64_t>(command->param1),
                        static_cast<int64_t>(static_cast<uint32_t>(command->param2))});
}
void USoundChannel_Free(void* channel) { ev("Free", {static_cast<int64_t>(lab(channel))}); }
void SndDoImmediate_(void* sndChannel, const SndCommand* command) {
    ev("SndDoImmediate", {P32(sndChannel), static_cast<int64_t>(command->cmd), static_cast<int64_t>(command->param1),
                          static_cast<int64_t>(static_cast<uint32_t>(command->param2))});
}
void DisposeBuffer_(void* buffer) { ev("Dispose", {P32(buffer)}); }

bool TbSoundPipeChannel::StreamerCallback(void* buffer, uint32_t size, void* pipe) {
    ev("Streamer", {P32(buffer), static_cast<int64_t>(size), P32(pipe)});
    return ret("Streamer") != 0;
}

namespace SamsUtilities {
UCommandPlay::UCommandPlay() {
    SetTag(this, 0xA1A1A1A1u);
    ev("PlayCtor", {});
}
UCommandSoundType::UCommandSoundType(SoundType type) {
    SetTag(this, 0xB2000000u + static_cast<uint32_t>(type));
    ev("SoundTypeCtor", {static_cast<int64_t>(type)});
}
void UCommand::SetImmediate(uint8_t immediate) {
    ev("SetImmediate", {static_cast<int64_t>(Tag(*this)), static_cast<int64_t>(immediate)});
}
UStatus SendToChannel(CSprocketsChannelMac* channel, UCommand& command) {
    ev("Send", {static_cast<int64_t>(lab(channel)), static_cast<int64_t>(Tag(command))});
    return UStatus{0};
}
template <>
UReferenceCounter<USoundDescription>::operator USoundDescription*() const {
    ev("DescPtr", {});
    return reinterpret_cast<USoundDescription*>(static_cast<uintptr_t>(ret("DescPtr")));
}
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the fields the Mac addresses by offset, and the object size.
int32_t tpw_cs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mVtable4));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mVtable8));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mVtable10));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mSndChannel));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mPipe));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mBuffer));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mBufferSize));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, m3D));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mFlag49));
    out[n++] = static_cast<int64_t>(sizeof(CSprocketsChannelMac));
    return n;
}

uint64_t tpw_cs_ctor(uint64_t object) {
    CSprocketsChannelMac* c = new (reinterpret_cast<void*>(static_cast<uintptr_t>(object))) CSprocketsChannelMac();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c));
}
uint64_t tpw_cs_dtor(uint64_t object, int16_t flag) {
    CSprocketsChannelMac* c = reinterpret_cast<CSprocketsChannelMac*>(static_cast<uintptr_t>(object));
    if (flag > 0)
        delete c;
    else
        c->~CSprocketsChannelMac();
    return object;
}
void tpw_cs_cleanup(uint64_t object) { reinterpret_cast<CSprocketsChannelMac*>(static_cast<uintptr_t>(object))->Cleanup(); }
void tpw_cs_callback(uint64_t object, uint64_t command) {
    reinterpret_cast<CSprocketsChannelMac*>(static_cast<uintptr_t>(object))->Callback(reinterpret_cast<SndCommand*>(static_cast<uintptr_t>(command)));
}
void tpw_cs_play(uint64_t object) { reinterpret_cast<CSprocketsChannelMac*>(static_cast<uintptr_t>(object))->Play(); }
void tpw_cs_set3d(uint64_t object, uint8_t on) { reinterpret_cast<CSprocketsChannelMac*>(static_cast<uintptr_t>(object))->Set3D(on); }

}  // extern "C"
