#include "AdvisorScoring.h"
#include "AdvisorQueue.h"
#include "Serialise.h"

// LOADVARSTRUCT<AdvisorHistory> 0x1000ecb4 (260 B): the file's 16 bytes, then the words at +0, +4 and +0xc byte-reversed.
// The 0x10 byte record is AdvisorMessageState here.
static void UnswapForFile(AdvisorMessageState& state) {
    static_assert(sizeof(AdvisorMessageState) == 0x10, "the record is 16 bytes");
    state.mLastSaid.mTimeSig = serialise::UnswapWord(state.mLastSaid.mTimeSig);
    state.mResponseIndex = serialise::UnswapWord(state.mResponseIndex);
    state.mKillCount = serialise::UnswapWord(state.mKillCount);
}

template<> int32_t LOADVARSTRUCT<AdvisorMessageState>(void* file, AdvisorMessageState* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);
}
