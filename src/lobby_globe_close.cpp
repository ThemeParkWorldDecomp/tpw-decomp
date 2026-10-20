// The online globe's end (main app, lobby_globe.cpp): the destructor and the two helpers that empty its lists (0x100aca4c,
// 0x100adf48, 0x100ae104) and Close (0x100ad50c), which does nothing. The vtable (0x1029e830) is emitted here, where the
// destructor is.
#include "lobby.h"

// The entries of both lists are destroyed one by one, then the lists, through their virtual destructors; the pointers are cleared
// after each. The base class's destructor follows. The lists are not tested for NULL by the helpers (a failed allocation in the
// constructor would crash them).
//
// HOST (unit v5L04, 2026-10-20): a derived destructor translated as the Mac laid it out carries this body inlined and then
// clears both pointers (CLBScreenComponentDownloadGlobe's, 0x100af330, unit v5553; the publish globe's, 0x100af1c8, will be
// the same); the Mac's code goes on to CLBScreenComponent's destructor directly, but C++ runs this one next, and reading
// mGlobes->mCount through the NULL pointer crashed the launcher at the lobby's close (the island gate). A NULL list is
// therefore left alone here. The non-NULL path is unchanged (LobbyExit's difftest; its NULL cases are in the untested list).
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {
    if (mGlobes != nullptr) {
        DeleteGlobeListEntries();
        delete mGlobes;
        mGlobes = nullptr;
    }
    if (mCityFlags != nullptr) {
        DeleteCityListEntries();
        delete mCityFlags;
        mCityFlags = nullptr;
    }
}

void CLBScreenComponentOnlineGlobe::Close() {}

// Takes the globes off the list from the head, deleting each one through its virtual destructor.
void CLBScreenComponentOnlineGlobe::DeleteGlobeListEntries() {
    while (mGlobes->mCount != 0) {
        CLBGlobe* globe = mGlobes->RemoveHead();
        if (globe != nullptr) delete globe;
    }
}

// The same for the city flags.
void CLBScreenComponentOnlineGlobe::DeleteCityListEntries() {
    while (mCityFlags->mCount != 0) {
        CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
        if (flag != nullptr) delete flag;
    }
}
