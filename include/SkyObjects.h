// CSkyObjects (main app, 0x1015b5f0..0x1015b804): the things in a level's sky. PARTIAL;
// untranslated but for DestroySkyObjects: only what CGame needs to hold and destroy one.
#pragma once
#include <cstdint>

class CSkyObjects {
public:
    CSkyObjects();              // 0x1015b5f0 (empty)
    ~CSkyObjects();             // 0x1015b61c
    void InitSkyObjects();      // 0x1015b688
    // 0x1015b804 (src/game/park/SkyObjects.cpp): takes each of the sky objects out of the sky (gei_Sky) in turn.
    void DestroySkyObjects();

    // The sky objects' texture pointers, from +0 (4 bytes each on the Mac). The count is gMainBalance's NumSkyObjects (slot at 0x6e4);
    // the Mac allocates 0x10 bytes (four pointers), so the host holds four native ones; further slots are reached by index, as the Mac does.
    void* mObjects[4];
};
