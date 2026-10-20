// The online globe's window: Show, Hide, zoom, city flags (0x100ad788 .. 0x100ae228).
#define TPW_GLOBE_SHOWHIDE_REAL 1
#include <cstring>

#include "lobby.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TPSound.h"
#include "tb_rain.h"
#include "tb_resource_display.h"

// The globe's vtable is emitted with its key function, the destructor: src/lobby_globe_CLBGlobe_dtor.cpp (lead 2026-10-20, v51031).

namespace {
float ReadF(const uint8_t* p) {
    float f;
    std::memcpy(&f, p, 4);
    return f;
}
void WriteF(uint8_t* p, float f) { std::memcpy(p, &f, 4); }
void WriteI(uint8_t* p, int32_t v) { std::memcpy(p, &v, 4); }
}  // namespace

// mF0C + 4, + 8, + 12 are the Mac's 0x10, 0x14, 0x18; mF44 + 0x14 is 0x58 (zoomed in), mF44 + 0x24 is 0x68.
void CLBScreenComponentOnlineGlobe::Show() {
    if (IsVisible() != 0) return;
    mVisible = 1;
    WriteF(mF44 + 0x00, 0.0f);
    WriteF(mF44 + 0x04, 0.0f);
    WriteF(mF44 + 0x08, 0.0f);
    WriteF(mF44 + 0x0c, 0.0f);
    WriteF(mF44 + 0x10, gLobby->mGlobals->mGlobeRadiusOut);
    gLobbyView->mFov = 60.0f;
    gei_Rain->SetRaindrops(0);
    gLobbyView->SetFogOff();
    WriteI(mF44 + 0x14, 0);
    SortOutZoomConstants();
    gei_Display.mFlags |= 0x2000000;
    gLobbyView->SetGlobalAmbientLight(1, 0x20202000);
    uint32_t color = 0xffffff00;
    gLobbyView->SetGlobalDirectionalLight(1, &color, nullptr, 1);
    std::memcpy(mF34, &mF38, 4);
    float f = ReadF(mF34);
    gLobbyView->mLightUpOffset = f;
    gLobbyView->mLightRightOffset = -f;
    gLobbyView->mLightDownward = 0;
    gLobbyScreens->mAdvisor->mHasTimeout = 0;
    gLobbyView->mSkyTarget[0] = 0xf30000;
    gLobbyView->mSkyTarget[1] = 0xcb0000;
    gLobbyView->mSkyTarget[2] = 0xbf0000;
    mF74 = SOUND_SendEvent(0, gLobbyThunderBank, 2, 0, 0, 0);
    mF78 = SOUND_SendEvent(0, gLobbyThunderBank, 3, 0, 0, 0);
    CLBGlobe* globe = new CLBGlobe;
    globe->mMesh = gLobbyMeshes->InstanceMeshType("data:lobby:globe", "globe", 1, 1, nullptr, nullptr);
    globe->mMesh->Show();
    globe->mScreen = this;
    globe->mScreen->RegisterGlobe(globe);
    mGlobeObj = globe;
    WriteI(mF44 + 0x24, 0);
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->StopProcessing();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->mCityList->GetList(PostGetCityListCallbackFunction, this);
}

void CLBScreenComponentOnlineGlobe::Hide() {
    if (IsVisible() == 0) return;
    mVisible = 0;
    RemoveWorldLobbyController();
    DeleteGlobeListEntries();
    DeleteCityListEntries();
    gLobbyMeshes->DeleteMeshInstanceListEntries();
    gei_Display.mFlags &= 0xFDFFFFFFu;
    SOUND_FadeSoundEffect(mF74);
    SOUND_FadeSoundEffect(mF78);
    mF78 = 0;
    mF74 = 0;
}

void CLBScreenComponentOnlineGlobe::GlobeZoomIn() {
    WriteI(mF44 + 0x14, 1);
    HideWorldLobbySats();
    SortOutZoomConstants();
}

void CLBScreenComponentOnlineGlobe::GlobeZoomOut() {
    WriteI(mF44 + 0x14, 0);
    ShowWorldLobbySats();
    SortOutZoomConstants();
}

void CLBScreenComponentOnlineGlobe::SortOutZoomConstants() {
    int32_t zoomed;
    std::memcpy(&zoomed, mF44 + 0x14, 4);
    if (zoomed != 0) {
        WriteF(mF0C + 4, mF28);
        WriteF(mF0C + 8, mF2C);
        WriteF(mF0C + 12, mF30);
    } else {
        WriteF(mF0C + 4, mF1C);
        WriteF(mF0C + 8, mF20);
        WriteF(mF0C + 12, mF24);
    }
}

void CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>* cities) {
    TbPtrList<CFSOnlineCity>::CNode* node = cities->mHead;
    while (node != nullptr) {
        CFSOnlineCity* city = node->mData;
        node = node->mNext;
        if (IsCityWanted(city) != 0) {
            CLBGlobeCityFlag* flag = new CLBGlobeCityFlag(this, city);
            (void)flag;
        }
    }
    WriteI(mF44 + 0x24, 1);
}

void CLBScreenComponentOnlineGlobe::UnRegister(CLBGlobeCityFlag* flag) {
    POSITION position = mCityFlags->Find(flag);
    if (position != nullptr) mCityFlags->RemoveAt(position);
}
