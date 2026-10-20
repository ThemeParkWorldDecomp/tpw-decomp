// Unit v5937: SAVEVARSTRUCT<AdvisorHistory> 0x1000f96c, CAdvisorScoring::LastMinuteCheck 0x100102d8, CAdvisorRequest::Serialise 0x1000e568.
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "analyser.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Debug.h"

// 0x1000f96c (296 bytes). The Mac builds a 16-byte copy on the stack (+0, +4, the byte at +8, +0xc; the three padding bytes are stack
// garbage there and zero here), puts the three words in file order (SwapWord, as LOADVARSTRUCT<AdvisorMessageState>'s UnswapWord: the host
// does not reverse them, v5L15) and writes it. The result test is (result >= 0) == (zero word == 0): the zero
// word at 0x1027dec8 never changes, so it is Succeeded().
template<> int32_t SAVEVARSTRUCT<AdvisorMessageState>(void* file, AdvisorMessageState* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(0x10, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    uint32_t copy[4] = {0, 0, 0, 0};
    const uint8_t* source = reinterpret_cast<const uint8_t*>(value);
    std::memcpy(&copy[0], source + 0, 4);
    std::memcpy(&copy[1], source + 4, 4);
    reinterpret_cast<uint8_t*>(copy)[8] = source[8];
    std::memcpy(&copy[3], source + 0xc, 4);
    GAME_ASSERTMSG(1, "should be a struct");
    copy[0] = serialise::SwapWord(copy[0]);
    copy[1] = serialise::SwapWord(copy[1]);
    copy[3] = serialise::SwapWord(copy[3]);
    TbError result = LbFile_Write(file, copy, 0x10, &written);
    if (result.Succeeded() && written == 0x10) return 0;
    GAME_ASSERTMSG(0, "File write failed");
    return 1;
}

// 0x100102d8 (276 bytes). The record is always the one at the message's own index; the search for another record with the number only
// decides whether there is a record at all (the original's quirk).
int32_t CAdvisorScoring::LastMinuteCheck(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& thingId, uint32_t& value) {
    AdvisorMessageRecord* record;
    if (static_cast<uint32_t>(message - 0x15f) < 2u) {
        record = nullptr;
    } else {
        record = &gAdvisorMessages[message];
        if (message != record->mMessage) {
            int32_t i = 0;
            for (; i < 0x15f; i++) {
                if (message == gAdvisorMessages[i].mMessage) break;
            }
            if (i >= 0x15f) {
                CTRACE(3, "Not found: %d\n", static_cast<int>(message));
                record = nullptr;
            }
        }
    }
    if (record == nullptr) return -1;
    mWord4 = 0;
    mWord8 = 0;
    int32_t result;
    if (IsThisValidInTheCurrentMode(message)) result = (this->*(record->mScorer))();
    else result = -1;
    thingId.mIndex = mWord4;
    value = mWord8;
    return result;
}

// 0x1000e568 (756 bytes).
int32_t CAdvisorRequest::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version >= 2 || version < 0) return 0;
    typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;
    ThingId* thingId = reinterpret_cast<ThingId*>(&mThingId);
    MacULong* secondary = reinterpret_cast<MacULong*>(&mSecondary);
    bool* valid = reinterpret_cast<bool*>(&mValid);
    bool* overrideOnlyOnce = reinterpret_cast<bool*>(&mOverrideOnlyOnce);
    bool* scoreOverridden = reinterpret_cast<bool*>(&mScoreOverridden);
    if (save != 0) {
        if (SAVEVARSTRUCT<ThingId>(file, thingId, "mThingId", 0) != 0) return 1;
        if (SAVEVAR<MacULong>(file, secondary, "mSecondary", 0) != 0) return 1;
        if (SAVEVAR<AdvisorMessage>(file, &mMessage, "mMessage", 0) != 0) return 1;
        if (SAVEVAR<int32_t>(file, &mScore, "mScore", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, valid, "mValid", 0) != 0) return 1;
        if (SAVEVAR<int32_t>(file, &mResponseNo, "mResponseNo", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, overrideOnlyOnce, "mOverrideOnlyOnce", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, scoreOverridden, "mScoreOverridden", 0) != 0) return 1;
    } else {
        if (LOADVARSTRUCT<ThingId>(file, thingId, "mThingId", 0) != 0) return 1;
        if (LOADVAR<MacULong>(file, secondary, "mSecondary", 0) != 0) return 1;
        if (LOADVAR<AdvisorMessage>(file, &mMessage, "mMessage", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mScore, "mScore", 0) != 0) return 1;
        if (LOADVAR<bool>(file, valid, "mValid", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mResponseNo, "mResponseNo", 0) != 0) return 1;
        if (LOADVAR<bool>(file, overrideOnlyOnce, "mOverrideOnlyOnce", 0) != 0) return 1;
        if (LOADVAR<bool>(file, scoreOverridden, "mScoreOverridden", 0) != 0) return 1;
    }
    return 0;
}
