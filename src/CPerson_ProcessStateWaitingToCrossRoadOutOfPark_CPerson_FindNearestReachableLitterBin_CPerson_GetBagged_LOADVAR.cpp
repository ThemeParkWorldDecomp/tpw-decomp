#include "ThingReceivers.h"
#include "Serialise.h"

// 0x1011b7cc, 236 bytes
template<> int32_t LOADVAR<CPerson::PersonState>(void* file, CPerson::PersonState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
