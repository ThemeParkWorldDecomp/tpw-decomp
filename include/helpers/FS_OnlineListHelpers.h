// FS_OnlineListHelpers.h: file-local helpers that src/game/online/FS_OnlineList.cpp shares with AdvisorScoring.cpp and
// FS_Online.cpp. Before the regroup, the functions using them shared a source file (1 file), which had one copy of
// each; that copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "FS_OnlineList.h"

// The release of the stored answer that every GetList here and the destructors inline: the count drops by one, at zero the infos are
// destroyed and freed and the answer freed (operator delete only); the pointer is cleared whatever the count was.
template<class LIST, class RECEIVED> static inline void ReleaseReceived(CFSOnlineSystemList<LIST, RECEIVED>* self) {
    RECEIVED* received = self->mReceived;
    if (received != nullptr) {
        --received->mRefCount;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        self->mReceived = nullptr;
    }
}
