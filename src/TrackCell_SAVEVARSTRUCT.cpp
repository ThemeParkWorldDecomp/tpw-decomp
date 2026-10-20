// SAVEVARSTRUCT<CThoughts::EventHistType> (0x1012f9a0, 260 bytes): saves a four-byte entry of two 16-bit fields, each swapped for the file.
#include "ThingReceivers.h"
#include "Serialise.h"

template<> int32_t SAVEVARSTRUCT<CThoughts::EventHistType>(void* file, CThoughts::EventHistType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    CThoughts::EventHistType copy;
    copy.mFirst = value->mFirst;
    copy.mSecond = value->mSecond;
    GAME_ASSERTMSG(1, "should be a struct");
    copy.mFirst = serialise::SwapWord(copy.mFirst);
    copy.mSecond = serialise::SwapWord(copy.mSecond);
    if (!LbFile_Write(file, &copy, 4, &written).Succeeded() || written != 4) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}
