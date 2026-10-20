#include "ThingReceivers.h"
#include "Serialise.h"

// 0x1012f844, 260 B. The comparison against the data word at 0x102a43fc is the TbError idiom (see TbError::Succeeded).
template<> int32_t SAVEVAR<CThoughts::ThoughtType>(void* file, CThoughts::ThoughtType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    uint32_t copy = static_cast<uint32_t>(*value);
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where SAVEVARSTRUCT should be used instead");
    copy = serialise::SwapWord(copy);
    if (!LbFile_Write(file, &copy, 4, &written).Succeeded() || written != 4) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}
