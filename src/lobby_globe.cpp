// The lobby's online globe screens (main app of SimTheme Park 2.04b3): the base class's constructor (0x100ac90c) and the
// download and publish globes' Init (0x100ae2d0, 0x100aeac8). The rest of the three classes is untranslated.
#include <new>

#include "lobby.h"

// Two lists on the heap (the Mac tests each allocation for NULL before running the list's constructor, which is what a
// nothrow new does), then nine floats. The words 0x34, 0x44..0x6b and 0x70 are not written. A failed allocation leaves
// its field NULL and the other is still tried.
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {
    mGlobes = new (std::nothrow) TbTList<CLBGlobe*, CLBGlobe*>;
    mCityFlags = new (std::nothrow) TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>;
    mF1C = 0.4f;
    mF20 = 0.15f;
    mF24 = 0.5f;
    mF38 = 300.0f;
    mF28 = 0.2f;
    mF2C = 0.075f;
    mF30 = 0.25f;
    mF3C = 150.0f;
    mF40 = 1.5f;
    mF78 = 0;
    mF74 = 0;
}

// The download and publish globes each clear the word at +0x70 and do nothing else (not the base class's Init).
void CLBScreenComponentDownloadGlobe::Init() { mF70 = 0; }

void CLBScreenComponentPublishGlobe::Init() { mF70 = 0; }
