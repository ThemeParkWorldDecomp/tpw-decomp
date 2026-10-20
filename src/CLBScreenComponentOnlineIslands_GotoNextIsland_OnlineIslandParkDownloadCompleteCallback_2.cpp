// Unit v51046: nine small functions of the lobby, the interface and the ESP sprites (main app).
//   OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
//   CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
//   CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
//   CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.
#define TPW_GLOBE_SHOWHIDE_REAL   // the download globe's Show calls the online globe's real Show (0x100ad788), not the inline base
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "lobby.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "World.h"
#include "tb_printf_opengl.h"

void UIMain_DestroyString(void* object);   // 0x103bcd38, the narrow string destructor glue
extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count);   // runtime glue 0x10238758
namespace mainmenu_callee {
CFSOnlineSystem* NewOnlineSystem();        // operator new(0x404), then CFSOnlineSystem::CFSOnlineSystem 0x1016f7d0
}

// ---- the online islands screen's callbacks

// 0x100b4420 (TVector 0x10266748): error 0 runs the screen's slot 0x4c, any other error its DownloadFailFunction.
void OnlineIslandParkDownloadCompleteCallback(int32_t error, const void* context) {
    CLBScreenComponentOnlineIslands* screen =
        const_cast<CLBScreenComponentOnlineIslands*>(static_cast<const CLBScreenComponentOnlineIslands*>(context));
    if (error == 0) {
        OnlineIslandsSlot4c(screen);
    } else {
        screen->DownloadFailFunction();
    }
}

// 0x100b3e28 (TVector 0x10266778): error 0 makes the islands of the park list, any other error backs out.
void PostGetTop10ParksListCallbackFunction(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* parks) {
    CLBScreenComponentOnlineIslands* screen =
        const_cast<CLBScreenComponentOnlineIslands*>(static_cast<const CLBScreenComponentOnlineIslands*>(context));
    if (error == 0) {
        screen->CreateParks(parks);
    } else {
        screen->HandleOnlineBacktrack();
    }
}

// ---- the islands screen

uint32_t CLBScreenComponentIslands::ProcessCancelIslands() {
    uint32_t answer = 0;
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);   // the original does not test mCurrent
        answer = 1;
        mCameraState = 0;
    }
    return answer;
}

// ---- the download globe

uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() {
    uint32_t answer = 0;
    // The Mac's "if (gOnlineSystem == 0) make it" is inlined twice (the second test can never be true).
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    if (gOnlineSystem->mF384 != 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
        gOnlineSystem->StopProcessing();
        HandleOnlineBacktrack();   // virtual slot 0x4c of the globe (0x100ae9c0 here)
        answer = 1;
    }
    return answer;
}

void CLBScreenComponentDownloadGlobe::Show() {
    if (IsVisible() == 0) {
        CreateWorldLobbyController(false);
        CreateOnlineCityInfoOverlay();
        CLBScreenComponentOnlineGlobe::Show();
        gLobbyScreens->mAdvisor->ClearAll();
    }
}

// ---- the frame rate

void CIntrface::DrawFPSCounter(float fps) {
    char* text = reinterpret_cast<char*>(mF10);   // this + 0x10, 0x14 bytes on the Mac
    std::sprintf(text, "FPS: %4g\n", static_cast<double>(fps));   // 0x1023de3e
    gOpenGLPrintf.printf_xy(0, 0, text);                           // the engine's gei_Printf
}

// ---- ESP sprites

int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites) {
    const ESP_CSprite* sprite = gESPSprites[sprites + gESPGroups[set].mFirst];
    uint32_t count = sprite->mActionCount;
    uint32_t number = gGameWorld->GetRand() >> 2;
    uint32_t quotient = count != 0 ? number / count : 0;   // divwu by 0 answers 0 in the emulator
    return static_cast<int32_t>(number - quotient * count);
}

// ---- static initialisers

namespace {
const char16_t kWideText[] = u"upload";   // 0x1023eac4
}

alignas(16) uint8_t gLobbyParserWide[0x30];
uint8_t gLobbyParserWideNode[12];
uint8_t gLobbyFolderNode[12];
uint8_t gLobbyDescriptorFileNode[12];

void __sinit_lobby_parser_cpp() {
    void* wideDestroy = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* narrowDestroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof gLobbyParserWide, "storage");
    auto* wide = ::new (gLobbyParserWide) TbDynamicStringTemplate<char16_t>(kWideText);
    __register_global_object(wide, wideDestroy, gLobbyParserWideNode);
    auto* folder = ::new (&gLobbyFolder) TbDynamicStringTemplate<char>("data:lobby:");        // 0x1023eaad
    __register_global_object(folder, narrowDestroy, gLobbyFolderNode);
    auto* file = ::new (&gLobbyDescriptorFile) TbDynamicStringTemplate<char>("lobby.txt");    // 0x1023eab9
    __register_global_object(file, narrowDestroy, gLobbyDescriptorFileNode);
}

// Stand-ins for the Mac's words (the imports' addresses are not callable here).
void HLSpriteListCtorGlue(void*) {}
void HLSpriteListDtorGlue(void*) {}
uint32_t gHLSpriteArrayDtor[2];
uint32_t gESPFileInfoDtor[2];
uint8_t gHLSpriteArrayNode[12];
uint8_t gESPGlobalListNode[12];
uint8_t gESPFileInfoNode[12];

void __sinit_HLSprite_cpp() {
    __construct_array(gHLSpriteLists, reinterpret_cast<void*>(&HLSpriteListCtorGlue),
                      reinterpret_cast<void*>(&HLSpriteListDtorGlue), 0x28, 0x10);
    __register_global_object(nullptr, gHLSpriteArrayDtor, gHLSpriteArrayNode);
    CSpriteList* globalList = ::new (&gESPGlobalList) CSpriteList();
    __register_global_object(globalList, reinterpret_cast<void*>(&HLSpriteListDtorGlue), gESPGlobalListNode);
    ::new (&gESPFileInfo) bfl::FileSystem::CFileInfo();   // the name pointer at +0x20 to the buffer at +0x24, which starts empty
    __register_global_object(&gESPFileInfo, gESPFileInfoDtor, gESPFileInfoNode);
}
