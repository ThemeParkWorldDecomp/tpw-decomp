// Unit v51056: CUIGrapher's setters (0x101fd798 .. 0x101fd97c), CFSLoadSave's Save/Hidden wrappers and destructor, the ui_mainmenu.cpp
// small functions and static initialiser, the TPEmbed.cpp small functions and the bthread / Connection / vector destructors.
#include <cstdint>
#include <cstring>
#include <new>
#include "FinanceGraphPanelMsg.h"
#include "LbDrawPrimitives.h"
#include "UI_Base.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "platform/msl.h"
#include "AdvisorScoring.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "GameSys.h"
#include "LbSCBack.h"
#include "TPGlobals.h"
#include "UI_pump.h"
#include "lobby.h"
#include "online.h"
#include "ui_menu.h"
#include "ui_mainmenu.h"

// ---- globals of the files here (bss, zero at start)
uint32_t gTPEmbedWordA;                 // 0x10396aa0
uint32_t gTPEmbedWordB;                 // 0x10396aa4
alignas(8) uint8_t gMainMenuText[0x14];       // 0x10396a5c
alignas(8) uint8_t gMainMenuTextNode[12];     // 0x10396a70
const char16_t gMainMenuLiteral[2] = {0x0070, 0x0075};   // 0x1024f694 (only the first word is known)

// ---- CUIGrapher
void CUIGrapher::SetXRange(int32_t a, int32_t b, int32_t c) {
    mF28 = c;
    mF18 = a;
    mF20 = b;
}

void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) {
    mF2C = c;
    mF1C = a;
    mF24 = b;
}

void CUIGrapher::SetDrawArea(int32_t a, int32_t b, int32_t c, int32_t d) {
    mF08 = a;
    mF0C = b;
    mF10 = c;
    mF14 = d;
    mF04 = mF14 - mF0C;
    mF00 = mF10 - mF08;
}

// The colour (five bytes) is passed in r4 and the top byte of r5 and copied byte for byte (a word and a byte).
void CUIGrapher::SetXAxisStyle(TbColour colour) { std::memcpy(mXAxis, &colour, 5); }
void CUIGrapher::SetYAxisStyle(TbColour colour) { std::memcpy(mYAxis, &colour, 5); }
void CUIGrapher::SetGridStyle(TbColour colour) { std::memcpy(mGrid, &colour, 5); }
void CUIGrapher::SetBGStyle(TbColour colour) { std::memcpy(mBG, &colour, 5); }

// ---- CFSLoadSave
void CFSLoadSave::LoadHidden(const TbDynamicStringTemplate<char16_t>& name) { LoadInternal(name, gTPGlobals.mF44AC); }
void CFSLoadSave::SaveHidden(const TbDynamicStringTemplate<char16_t>& name) { SaveInternal(name, gTPGlobals.mF44AC); }
void CFSLoadSave::Save(const TbDynamicStringTemplate<char16_t>& name) { SaveInternal(name, gTPGlobals.mF4420); }

CFSLoadSave* CFSLoadSave::Destroy(CFSLoadSave* self, int16_t flag) {
    if (self) {
        v51056_callee::LoadSaveListDtor(self);
        if (flag > 0) ftp_callee::OperatorDelete(self);
    }
    return self;
}

// ---- ui_mainmenu.cpp
void __sinit_ui_mainmenu_cpp() {
    mainmenu_callee::WideStringCtor(gMainMenuText, gMainMenuLiteral);
    __register_global_object(gMainMenuText, reinterpret_cast<void*>(&UIMain_DestroyWString), gMainMenuTextNode);
}

void MainLobbyMenuPostOnlineLoginCallbackFn(int32_t error, const void* context) {}

int32_t MainLobbyMenuConfirmQuit_OK(void* context) {
    gLobby->SendQuit();
    return 1;
}

int32_t BankruptGameMenuConfirmRestart_OK(void* context) {
    GS_SetCurrentGameStatus(GS_RESTART_LEVEL);
    HideMainMenu();
    UI_ByeByeBankrupt();
    return 1;
}

int32_t MainGameMenuConfirmRestart_OK(void* context) {
    GS_SetCurrentGameStatus(GS_RESTART_LEVEL);
    HideMainMenu();
    return 1;
}

// ---- TPEmbed.cpp and the thread helpers
void __sinit_TPEmbed_cpp() {
    gTPEmbedWordB = 0;
    gTPEmbedWordA = 0x10000;
}

uint32_t bthread_create(void* (*start)(void*), void* arg) {
    uint32_t id;
    NS_MacDoze::_beginthreadex(nullptr, 0, start, arg, 0, &id);
    return id;
}

bthread_mutex* bthread_mutex::Construct(bthread_mutex* self) {
    NS_MacDoze::InitializeCriticalSection(reinterpret_cast<void**>(self));
    return self;
}

int32_t bthread_cond::signal() {
    NS_MacDoze::SetEvent(mHandle);
    return 0;
}

namespace argdeque_callee {
void VectorDtor(void* vec, int16_t flag) {
    if (vec) {
        online_callee::ArgumentVectorTearDown(vec);
        if (flag > 0) ftp_callee::OperatorDelete(vec);
    }
}
}  // namespace argdeque_callee

void* operator_new(uint32_t size, void* where) { return where; }

Connection* Connection::Destroy(Connection* self, int16_t flag) {
    if (self) {
        self->mF00 = 0x102b1610u;
        ftp_callee::CSockDtor(reinterpret_cast<FTPClient*>(self), 0);
        if (flag > 0) ftp_callee::OperatorDelete(self);
    }
    return self;
}
