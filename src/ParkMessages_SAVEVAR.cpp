// SAVEVAR<ResearchCat> (0x10124680, 260 B): saves the research category, a 32-bit enum, through the shared serialise::SaveVar body.
#include "Serialise.h"
#include "ThingReceivers.h"

template<> int32_t SAVEVAR<ResearchCat>(void* file, ResearchCat* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}
