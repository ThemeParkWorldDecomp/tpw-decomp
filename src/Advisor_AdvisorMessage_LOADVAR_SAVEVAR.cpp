// Unit v5L05: the save-file instances for AdvisorMessage, LOADVAR 0x1000f250 and SAVEVAR 0x1000f820 (Advisor.cpp on the Mac), which
// CAdvisorRequest::Serialise calls. A message is one unvalidated signed word, the shape of LOADVAR<int> and SAVEVAR<int> beside them.
#include "AdvisorQueue.h"
#include "Serialise.h"

// 0x1000f250 (236 bytes).
template<> int32_t LOADVAR<AdvisorMessage>(void* file, AdvisorMessage* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}

// 0x1000f820 (260 bytes).
template<> int32_t SAVEVAR<AdvisorMessage>(void* file, AdvisorMessage* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}
