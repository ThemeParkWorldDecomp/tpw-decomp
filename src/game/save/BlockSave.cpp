// BlockSave.cpp: name proven by __sinit_BlockSave_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "BlockSave.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "FS_Online.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "lobby.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "TbDynamicStringTemplate.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <new>

// The message centre's post and the message and subscription objects it uses (main app, unit w107): CMessageCentre::PostMessage
// 0x1015256c, the CMsgEvent constructor 0x10151df8 and destructor 0x100f39cc (and GetType 0x100f4470, which its vtable needs), the
// CSubscriptionHeader constructor 0x1000a3f8 and destructor 0x101522e0. The inventory files them under RideDB.cpp and BlockSave.cpp.

int32_t CMsgEvent::GetType() const { return 0x13; }

// CBlockSave (main app of SimTheme Park 2.04b3; unit w113): the save block table.

// 0x100f46ec (20 bytes): entry `block` (the Mac's table has 8-byte entries, index << 3) gets the pointer, then the size. The name is
// never read. The object has room for two entries; a larger block number would overwrite the next global on the Mac.
void CBlockSave::RegisterSaveBlock(SaveBlock block, void* data, int32_t size, const char* name) {
    mBlocks[block].mData = data;
    mBlocks[block].mSize = size;
}

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

void __sinit_BlockSave_cpp() {}
