// The weak stand-ins of the unit library tpw_unit_std_basic_string_assign_2
// (cmake/units/std_basic_string_assign_2.cmake), as src/ui_Postcard_panel_std_basic_string_assign_2.cpp of before
// the regroup (v5L30b) defined them, unchanged. The regroup put each function into the src file of its original
// source file, where these defaults would sit next to the real definitions, so it dropped them; the unit library
// compiles them from here, so that it builds what it built before.

#include "ui_Postcard.h"
#include <cstring>
#include <new>
#include "Postcard.h"
#include "Game.h"
#include "FS_Online.h"
#include "Text.h"
#include "TbITable.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "HLSprite.h"
#include "Advisor.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"
#include "ui_ime.h"
#include "std_basic_streambuf_wchar.h"
#include "ui_DrawObject.h"
#include "TPEngine.h"
#include "FS_OnlineList.h"

// 0x1017a6e4 (288): the server list's destructor body (the vtable store aside): the stored answer released, the records removed and
// destroyed, the list destroyed. The delete flag is the C++ delete.
// Lead 2026-10-20 (v51129): weak, because src/CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp
// (v51110) defines the same specialization for the derived destructor 0x1017a908, the same code; libtpw links that one, the unit
// test library takes this one.
template<> __attribute__((weak)) CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::~CFSOnlineSystemList() {
    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) onlinelist2_callee::DestroyServerInfos(mReceived->mInfos);
            mReceived->mInfos = nullptr;
            onlinelist2_callee::FreeBlock(mReceived);
        }
        mReceived = nullptr;
    }
    while (mList->GetCount() != 0) {
        CFSOnlineServer* server = onlinelist2_callee::ServerRemoveHead(mList);
        if (server != nullptr) onlinelist2_callee::DestroyServer(server);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) onlinelist2_callee::DestroyServerList(mList);
}
