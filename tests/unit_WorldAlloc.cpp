// Test entry points for CWorld::Allocate with a fake AllocateThingHeap (library unit_WorldAlloc: World_alloc.cpp, BlockSave.cpp,
// ExitErr.cpp). The real AllocateThingHeap always answers 1, so the error branch only shows with a fake that answers 0 (the case's
// queue "heap"). The save table and the exit error are the real objects; this library has no world, only an address to call on.
#include <cstdint>
#include <cstring>

#include "calllog.h"

#include "BlockSave.h"
#include "ExitErr.h"
#include "World.h"

using namespace calllog;

namespace {
alignas(16) unsigned char gDummy[16];   // the world the function is called on (Allocate only passes the address on)
template <class T>
T* Ptr(uint32_t word) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(word));
}
// The address of the dummy world as label 1, 0 for NULL, else the raw low 32 bits (the case's random words).
int64_t Canon(const void* pointer) {
    if (pointer == nullptr) return 0;
    if (pointer == gDummy) return 1;
    return static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu);
}
}  // namespace

CBlockSave gBlockSave;

// The fake records the world it was called with (the dummy is label 1) and answers the queue (default 1).
bool CWorld::AllocateThingHeap() {
    ev("AllocateThingHeap", {lab(this)});
    return ret("heap", 1) != 0;
}

extern "C" {

void* tpw_wa_dummy() { return gDummy; }

// 7 words: the two save entries (data, size, data, size) and the exit error (mError, mType, mValue); and its text.
void tpw_wa_set_state(const uint32_t* w, const uint8_t* text) {
    gBlockSave.mBlocks[0].mData = Ptr<void>(w[0]);
    gBlockSave.mBlocks[0].mSize = static_cast<int32_t>(w[1]);
    gBlockSave.mBlocks[1].mData = Ptr<void>(w[2]);
    gBlockSave.mBlocks[1].mSize = static_cast<int32_t>(w[3]);
    gExitError.mError = static_cast<int32_t>(w[4]);
    gExitError.mType = static_cast<CExitError::ErrorType>(w[5]);
    gExitError.mValue = w[6];
    std::memcpy(gExitError.mText, text, sizeof gExitError.mText);
}

void tpw_wa_get_state(int64_t* out, uint8_t* text) {
    out[0] = Canon(gBlockSave.mBlocks[0].mData);
    out[1] = static_cast<uint32_t>(gBlockSave.mBlocks[0].mSize);
    out[2] = Canon(gBlockSave.mBlocks[1].mData);
    out[3] = static_cast<uint32_t>(gBlockSave.mBlocks[1].mSize);
    out[4] = static_cast<uint32_t>(gExitError.mError);
    out[5] = static_cast<uint32_t>(gExitError.mType);
    out[6] = gExitError.mValue;
    std::memcpy(text, gExitError.mText, sizeof gExitError.mText);
}

int32_t tpw_wa_call() { return reinterpret_cast<CWorld*>(gDummy)->Allocate() ? 1 : 0; }

}  // extern "C"
