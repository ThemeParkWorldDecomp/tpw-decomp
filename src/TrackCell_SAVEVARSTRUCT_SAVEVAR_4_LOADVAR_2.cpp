#include "ThingReceivers.h"
#include "Serialise.h"

// 0x1012f700, 236 B. The twin of SAVEVAR<CThoughts::ThoughtType>: one word read, swapped in place. The TbError comparison is the
// (result >= 0) idiom against the zero word at 0x102a4404.
template<> int32_t LOADVAR<CThoughts::ThoughtType>(void* file, CThoughts::ThoughtType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    if (!LbFile_Read(file, value, 4, &read).Succeeded() || read != 4) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where LOADVARSTRUCT should be used instead");
    *value = serialise::UnswapWord(*value);
    return 0;
}
