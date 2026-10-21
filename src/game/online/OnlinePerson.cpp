// OnlinePerson.cpp: name proven by __sinit_OnlinePerson_cpp in the main application.

#include "analyser.h"
#include "CMsg.h"
#include "Debug.h"
#include "email.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MslWStringTree.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "ThingReceivers.h"
#include "TPGlobals.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

// Unit v51059: CParser's set helpers and a handful of small library/destructor functions of the online code (main app, addresses in comments).

// 0x10212f58
void COnlinePerson::FinalDestroy() {
    reinterpret_cast<CPeep*>(this)->FinalDestroy();
}

// 0x10212fb0
void COnlinePerson::ReceiveMessage(const CMsg& message) {
    (void)message;
}

// COnlinePerson (an online avatar, a CPeep with extra fields), main app, unit v5133:
//   UpdateTurn 0x10212ff0, ModelState 0x10213080, IsThisMyName 0x10213124, SetDest(cell) 0x102131e0, SetDest(x, y) 0x102132a0,
//   Teleport 0x1021334c, ProcessWalkingToDest 0x1021341c.
// The object is reached by Mac offset: +4 and +6 the MapPos pair (x, y), +0xc the CDelayedScript, +0x19c the name (UTF-16),
// +0x39c the state word (0 idle, 1 and 2 walking to a destination, 3 idle again after a stuck walk), +0x3a0 the "is the player's own avatar" byte.

int32_t wcscmp(const char16_t* a, const char16_t* b);   // the MSL import (glue 0x10238c98)

namespace {

uint8_t* Bytes(COnlinePerson* p) { return reinterpret_cast<uint8_t*>(p); }
int32_t& State(COnlinePerson* p) { return *reinterpret_cast<int32_t*>(Bytes(p) + 0x39c); }
CPeep* Peep(COnlinePerson* p) { return reinterpret_cast<CPeep*>(p); }
CDelayedScript* Script(COnlinePerson* p) { return reinterpret_cast<CDelayedScript*>(Bytes(p) + 0xc); }
CDelayedScript::ScriptIdx Idx(int32_t n) { return static_cast<CDelayedScript::ScriptIdx>(n); }

const char kStuck[] = "The person has become stuck on their way to the destination\n";   // 0x10250c52

}  // namespace

// 0x10212ff0
void COnlinePerson::UpdateTurn() {
    Peep(this)->UpdateTurn();
    if (Bytes(this)[0x3a0] != 0) Peep(this)->SpawnParticleEffect(0x35);
    else Peep(this)->SpawnParticleEffect(0x39);
}

// 0x10213080
void COnlinePerson::ModelState() {
    switch (State(this)) {
        case 0: ProcessIdle(); break;
        case 1: ProcessWalkingToDest(); break;
        case 2: ProcessWalkingToDest(); break;
        case 3: ProcessIdle(); break;
        default: break;
    }
}

// 0x10213124
bool COnlinePerson::IsThisMyName(std::u16string& name) const {
    return wcscmp(name.c_str(), reinterpret_cast<const char16_t*>(reinterpret_cast<const uint8_t*>(this) + 0x19c)) == 0;
}

// 0x102131e0
int32_t COnlinePerson::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    if (Peep(this)->SetDest(cell) == 0) return 0;
    State(this) = 1;
    Script(this)->SetSpriteScript(Idx(1));
    return 1;
}

// 0x102132a0
int32_t COnlinePerson::SetDest(MapPos x, MapPos y) {
    if (Peep(this)->SetDest(x, y) == 0) return 0;
    State(this) = 1;
    Script(this)->SetSpriteScript(Idx(1));
    return 1;
}

// 0x1021334c
void COnlinePerson::Teleport(MapPos x, MapPos y) {
    State(this) = 0;
    Script(this)->SetSpriteScript(Idx(3));
    Peep(this)->Teleport(x, y);
}

// 0x102133e4
void COnlinePerson::ProcessIdle() {}

// 0x1021341c
void COnlinePerson::ProcessWalkingToDest() {
    const int32_t walk = Peep(this)->PeepWalks();
    if (walk == 0) {
        State(this) = 0;
        Script(this)->SetSpriteScript(Idx(3));
        if (Bytes(this)[0x3a0] != 0) {
            if (gOnlineControl == nullptr) {
                void* block = online_callee::OperatorNew(0xe8c);
                if (block != nullptr) new (block) COnlineControl();
                gOnlineControl = static_cast<COnlineControl*>(block);
            }
            MapPos x, y;
            std::memcpy(&x, Bytes(this) + 4, sizeof(MapPos));
            std::memcpy(&y, Bytes(this) + 6, sizeof(MapPos));
            gOnlineControl->UpdateAvatarPos(x, y);
        }
    } else if (walk == 2) {
        CTRACE(3, kStuck);
        State(this) = 3;
        Script(this)->SetSpriteScript(Idx(5));
    }
}

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

uint32_t gOnlinePersonWord20;                          // 0x10396d20
uint32_t gOnlinePersonWord24;                          // 0x10396d24

// 0x10213548
void __sinit_OnlinePerson_cpp() {
    gOnlinePersonWord24 = 0;
    gOnlinePersonWord20 = 0x10000;
}
