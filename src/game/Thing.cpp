// Thing.cpp: name proven by __sinit_Thing_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "helpers/ThingHelpers.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "Mouse.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "RegionEffectCell.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "StaffDB.h"
#include "tb_rain.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

// The constructors of the things a new level makes (unit w122a, main app): CThing::CThing (Thing.cpp) and the constructors of CStaffHQ,
// CMechanicHQ, CAdvisor, CTagSystem, CResearchLab, CWeather and CUIMsgReceiver. The six leaf constructors they build their parts with
// (CTrainingCentre, CWeatherForecast, CStaffHQ::StrikeType, AdvisorHistory (host: AdvisorMessageState, written in CAdvisor's loop),
// CResearchProject, CResearchCategory) are inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every derived constructor builds its CThing first (the host holds it as the first member, which is constructed before the others) with
// map cell id 1 and the model of its class, then its own fields, then subscribes to the message centre. The Mac's constructors leave most
// bytes of the object alone (a new object keeps whatever the allocator gave); so do these, and no member has an initialiser.

// 0x1012c57c (84 bytes, Thing.cpp), model 0xc: nothing of its own but Init (which only subscribes).
CTagSystem::CTagSystem() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xc)) { Init(); }

// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.

uint32_t gThingSinitHigh;           // 0x1034c6f0
uint32_t gThingSinitLow;            // 0x1034c6f4
void CTagSystem::FinalDestroy() { mBase.FinalDestroy(); }

int32_t CTagSystem::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return mBase.Serialise(file, save, version, unused) != 0 ? 1 : 0;
}

// The seven thing-loop entry points (unit w123a, main app). External dispatch,
// death-message support, random/season providers and tree insertion remain callees.

void ThingLoopWorldPending(CWorld*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>)
    __asm__("__ZN6CWorld22AddToPendingDeleteListE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE");   // 0x1013b154
// 0x1012c6b8, 556 bytes. The body remains untouched.
void CTagSystem::Init() {
    for (int32_t type : {7, 12, 20, 24, 25, 19}) SubscribeThing(mBase, type);
}

// Unit v5L39: the program image's table (382 entries; it was all zero here, so no tag was ever found). Rows are the entry's eight
// words; the first is the tag id, which GetElement checks against the slot first.
TagTableElement gTagTable[0x17e] = {   // 0x102a12c0
    {{0, 0x17f, 0}, 2, {0, 0, 0x7530}, 0},   // 0x000
    {{1, 0x17f, 0}, 2, {0, 0, 0x7530}, 0},   // 0x001
    {{2, 0x17f, 0}, 6, {0, 0, 0x7530}, 0},   // 0x002
    {{3, 4, 0x20000}, 7, {0x18, 0, 0x7530}, 0},   // 0x003
    {{4, 3, 0x20000}, 8, {0x18, 0, 0x7530}, 0},   // 0x004
    {{5, 0x17f, 0x20000}, 9, {0xa, 0, 0x7530}, 0},   // 0x005
    {{6, 0x17f, 0x20000}, 0xa, {0xa, 0, 0x7530}, 0},   // 0x006
    {{7, 0x17f, 0x20000}, 0xb, {0xe, 0, 0x7530}, 0},   // 0x007
    {{8, 0x17f, 0x20000}, 0xc, {0xe, 0, 0x7530}, 0},   // 0x008
    {{9, 0x17f, 0x20000}, 0xd, {0xe, 0, 0x7530}, 0},   // 0x009
    {{0xa, 0x17f, 0x20000}, 0xe, {0xd, 0, 0x7530}, 0},   // 0x00a
    {{0xb, 0x17f, 0x20000}, 0xf, {0xd, 0, 0x7530}, 0},   // 0x00b
    {{0xc, 0x17f, 0x20000}, 0x10, {0xd, 0, 0x7530}, 0},   // 0x00c
    {{0xd, 0x17f, 0x20000}, 0x11, {0x10, 0, 0x7530}, 0},   // 0x00d
    {{0xe, 0x17f, 0x20000}, 0x12, {0x10, 0, 0x7530}, 0},   // 0x00e
    {{0xf, 0x17f, 0x20000}, 0x13, {0x10, 0, 0x7530}, 0},   // 0x00f
    {{0x10, 0x17f, 0x20000}, 0x14, {0xf, 0, 0x7530}, 0},   // 0x010
    {{0x11, 0x17f, 0x20000}, 0x15, {0x11, 0, 0x7530}, 0},   // 0x011
    {{0x12, 0x17f, 0x20000}, 0x16, {0x11, 0, 0x7530}, 0},   // 0x012
    {{0x13, 0x17f, 0x20000}, 0x17, {0x11, 0, 0x7530}, 0},   // 0x013
    {{0x14, 0x17f, 0}, 0x18, {0, 0, 0x7530}, 0},   // 0x014
    {{0x15, 0x17f, 0}, 0x19, {0, 0, 0x7530}, 0},   // 0x015
    {{0x16, 0x17f, 0}, 0x1a, {0, 0, 0x7530}, 0},   // 0x016
    {{0x17, 0x17f, 0}, 0x1b, {0, 0, 0x7530}, 0},   // 0x017
    {{0x18, 0x17f, 0}, 0x1c, {0xc, 0, 0x7530}, 0},   // 0x018
    {{0x19, 0x17f, 0}, 0x1d, {0, 0, 0x7530}, 0},   // 0x019
    {{0x1a, 0x17f, 0}, 0x1e, {0, 0, 0x7530}, 0},   // 0x01a
    {{0x1b, 0x17f, 0}, 0x1e, {0, 0, 0x7530}, 0},   // 0x01b
    {{0x1c, 0x17f, 0}, 0x1f, {0, 0, 0x7530}, 0},   // 0x01c
    {{0x1d, 0x17f, 0}, 0x20, {0, 0, 0x7530}, 0},   // 0x01d
    {{0x1e, 0x17f, 0x20000}, 0x21, {0x15, 0, 0x7530}, 0},   // 0x01e
    {{0x1f, 0x17f, 0x20000}, 0x22, {0x15, 0, 0x7530}, 0},   // 0x01f
    {{0x20, 0x17f, 0x20000}, 0x23, {0x15, 0, 0x7530}, 0},   // 0x020
    {{0x21, 0x17f, 0x20000}, 0x24, {0x15, 0, 0x7530}, 0},   // 0x021
    {{0x22, 0x17f, 0x20000}, 0x25, {0x15, 0, 0x7530}, 0},   // 0x022
    {{0x23, 0x17f, 0x10000}, 0x26, {0, 0, 0x7530}, 0},   // 0x023
    {{0x24, 0x17f, 0x10000}, 0x27, {0, 0, 0x7530}, 0},   // 0x024
    {{0x25, 0x17f, 0x10000}, 0x28, {0, 0, 0x7530}, 0},   // 0x025
    {{0x26, 0x17f, 0x10000}, 0x29, {0, 0, 0x7530}, 0},   // 0x026
    {{0x27, 0x17f, 0x10000}, 0x2a, {0, 0, 0x7530}, 0},   // 0x027
    {{0x28, 0x23, 0x10000}, 0x2b, {0, 0, 0x7530}, 0},   // 0x028
    {{0x29, 0x24, 0x10000}, 0x2c, {0, 0, 0x7530}, 0},   // 0x029
    {{0x2a, 0x25, 0x10000}, 0x2d, {0, 0, 0x7530}, 0},   // 0x02a
    {{0x2b, 0x26, 0x10000}, 0x2e, {0, 0, 0x7530}, 0},   // 0x02b
    {{0x2c, 0x27, 0x10000}, 0x2f, {0, 0, 0x7530}, 0},   // 0x02c
    {{0x2d, 0x28, 0x10000}, 0x30, {4, 0, 0x7530}, 0},   // 0x02d
    {{0x2e, 0x29, 0x10000}, 0x31, {4, 0, 0x7530}, 0},   // 0x02e
    {{0x2f, 0x2a, 0x10000}, 0x32, {4, 0, 0x7530}, 0},   // 0x02f
    {{0x30, 0x2b, 0x10000}, 0x33, {4, 0, 0x7530}, 0},   // 0x030
    {{0x31, 0x2c, 0x10000}, 0x34, {4, 0, 0x7530}, 0},   // 0x031
    {{0x32, 0x2d, 0x10000}, 0x35, {0, 0, 0x7530}, 0},   // 0x032
    {{0x33, 0x2e, 0x10000}, 0x36, {0, 0, 0x7530}, 0},   // 0x033
    {{0x34, 0x2f, 0x10000}, 0x37, {0, 0, 0x7530}, 0},   // 0x034
    {{0x35, 0x30, 0x10000}, 0x38, {0, 0, 0x7530}, 0},   // 0x035
    {{0x36, 0x31, 0x10000}, 0x39, {0, 0, 0x7530}, 0},   // 0x036
    {{0x37, 0x2d, 0x10000}, 0x3a, {0, 0, 0x7530}, 0},   // 0x037
    {{0x38, 0x2e, 0x10000}, 0x3b, {0, 0, 0x7530}, 0},   // 0x038
    {{0x39, 0x2f, 0x10000}, 0x3c, {0, 0, 0x7530}, 0},   // 0x039
    {{0x3a, 0x30, 0x10000}, 0x3d, {0, 0, 0x7530}, 0},   // 0x03a
    {{0x3b, 0x31, 0x10000}, 0x3e, {0, 0, 0x7530}, 0},   // 0x03b
    {{0x3c, 0x17f, 0x20000}, 0x3f, {3, 0, 0x7530}, 0},   // 0x03c
    {{0x3d, 0x17f, 0x20000}, 0x40, {3, 0, 0x7530}, 0},   // 0x03d
    {{0x3e, 0x17f, 0x20000}, 0x41, {9, 0, 0x7530}, 0},   // 0x03e
    {{0x3f, 0x17f, 0x20000}, 0x42, {0x13, 0, 0x7530}, 0},   // 0x03f
    {{0x40, 0x17f, 0x20000}, 0x43, {9, 0, 0x7530}, 0},   // 0x040
    {{0x41, 0x17f, 0x20000}, 0x44, {0x13, 0, 0x7530}, 0},   // 0x041
    {{0x42, 0x17f, 0x20000}, 0x45, {0x13, 0, 0x7530}, 0},   // 0x042
    {{0x43, 0x17f, 0x20000}, 0x46, {0, 0, 0x7530}, 0},   // 0x043
    {{0x44, 0x17f, 0x10000}, 0x47, {3, 0, 0x7530}, 2},   // 0x044
    {{0x45, 0x17f, 0x10000}, 0x47, {3, 0, 0x7530}, 2},   // 0x045
    {{0x46, 0x44, 0x10000}, 0x48, {3, 0, 0x7530}, 2},   // 0x046
    {{0x47, 0x44, 0x10000}, 0x49, {3, 0, 0x7530}, 2},   // 0x047
    {{0x48, 0x44, 0x10000}, 0x4a, {3, 0, 0x7530}, 2},   // 0x048
    {{0x49, 0x17f, 0x20000}, 0x4b, {0xa, 0, 0x7530}, 0},   // 0x049
    {{0x4a, 0x17f, 0x20000}, 0x4c, {0x13, 0, 0x7530}, 0},   // 0x04a
    {{0x4b, 0x17f, 0x20000}, 0x4d, {0xa, 0, 0x7530}, 0},   // 0x04b
    {{0x4c, 0x17f, 0x20000}, 0x4e, {0xa, 0, 0x7530}, 0},   // 0x04c
    {{0x4d, 0x17f, 0x20000}, 0x4f, {0xb, 0, 0x7530}, 0},   // 0x04d
    {{0x4e, 0x17f, 0x20000}, 0x50, {0x13, 0, 0x7530}, 0},   // 0x04e
    {{0x4f, 0x17f, 0x20000}, 0x51, {0xb, 0, 0x7530}, 0},   // 0x04f
    {{0x50, 0x17f, 0x20000}, 0x52, {0xb, 0, 0x7530}, 0},   // 0x050
    {{0x51, 0x17f, 0x20000}, 0x53, {0xc, 0, 0x7530}, 0},   // 0x051
    {{0x52, 0x17f, 0x20000}, 0x54, {0, 0, 0x7530}, 0},   // 0x052
    {{0x53, 0x17f, 0x20000}, 0x55, {0xc, 0, 0x7530}, 0},   // 0x053
    {{0x54, 0x17f, 0x20000}, 0x56, {0xc, 0, 0x7530}, 0},   // 0x054
    {{0x55, 0x17f, 0x20000}, 0x56, {0xc, 0, 0x7530}, 0},   // 0x055
    {{0x56, 0x54, 0x20000}, 0x57, {0xc, 0, 0x7530}, 0},   // 0x056
    {{0x57, 0x54, 0x20000}, 0x58, {0xc, 0, 0x7530}, 0},   // 0x057
    {{0x58, 0x17f, 0x20000}, 0x59, {3, 0, 0x7530}, 0},   // 0x058
    {{0x59, 0x17f, 0x20000}, 0x5a, {3, 0, 0x7530}, 0},   // 0x059
    {{0x5a, 0x17f, 0x20000}, 0x5b, {3, 0, 0x7530}, 0},   // 0x05a
    {{0x5b, 0x17f, 0x20000}, 0x5c, {3, 0, 0x7530}, 0},   // 0x05b
    {{0x5c, 0x17f, 0x20000}, 0x5d, {0xc, 0, 0x7530}, 0},   // 0x05c
    {{0x5d, 0x5e, 0x20000}, 0x5e, {0x18, 0, 0x7530}, 0},   // 0x05d
    {{0x5e, 0x5d, 0x20000}, 0x5f, {0x18, 0, 0x7530}, 0},   // 0x05e
    {{0x5f, 0x3f, 0}, 0x72, {9, 0, 0x7530}, 2},   // 0x05f
    {{0x60, 0x41, 0}, 0x73, {0x13, 0, 0x7530}, 2},   // 0x060
    {{0x61, 0x4a, 0}, 0x74, {0xa, 0, 0x7530}, 2},   // 0x061
    {{0x62, 0x4e, 0}, 0x75, {0xb, 0, 0x7530}, 2},   // 0x062
    {{0x63, 0x52, 0}, 0x76, {0xc, 0, 0x7530}, 2},   // 0x063
    {{0x64, 0x17f, 0x10000}, 0x77, {0, 0, 0x7530}, 0},   // 0x064
    {{0x65, 0x17f, 0x30000}, 0x78, {0, 0, 0}, 0},   // 0x065
    {{0x66, 0x17f, 0x30000}, 0xb4, {0, 0, 0}, 5},   // 0x066
    {{0x67, 0x17f, 0x30000}, 0xb5, {0, 0, 0}, 6},   // 0x067
    {{0x68, 0x17f, 0x30000}, 0xb6, {0, 0, 0}, 0},   // 0x068
    {{0x69, 0x17f, 0x30000}, 0xb7, {0, 0, 0}, 0},   // 0x069
    {{0x6a, 0x17f, 0x30000}, 0xb8, {0, 0, 0}, 7},   // 0x06a
    {{0x6b, 0x17f, 0x30000}, 0xb9, {0, 0, 0}, 8},   // 0x06b
    {{0x6c, 0x17f, 0x30000}, 0xba, {0, 0, 0}, 0},   // 0x06c
    {{0x6d, 0x17f, 0x30000}, 0xbb, {0, 0, 0}, 0},   // 0x06d
    {{0x6e, 0x17f, 0x30000}, 0xbc, {0, 0, 0}, 0},   // 0x06e
    {{0x6f, 0x17f, 0x30000}, 0xbd, {0, 0, 0}, 0},   // 0x06f
    {{0x70, 0x17f, 0x30000}, 0xbe, {0, 0, 0}, 0},   // 0x070
    {{0x71, 0x17f, 0x30000}, 0xbf, {0, 0, 0}, 0},   // 0x071
    {{0x7a, 0x17f, 0x20000}, 0x79, {0, 0, 0x7530}, 0},   // 0x072
    {{0x7b, 0x17f, 0x20000}, 0x7a, {0, 0, 0x7530}, 0},   // 0x073
    {{0x7c, 0x17f, 0x20000}, 0x7b, {0x12, 0, 0x7530}, 0},   // 0x074
    {{0x7d, 0x17f, 0x10000}, 0x7c, {0, 0, 0x7530}, 0},   // 0x075
    {{0x7e, 0x7d, 0x10000}, 0x7d, {0, 0, 0x7530}, 0},   // 0x076
    {{0x7f, 0x7e, 0x10000}, 0x7e, {0, 0, 0x7530}, 0},   // 0x077
    {{0x80, 0x7f, 0x10000}, 0x7f, {0, 0, 0}, 0},   // 0x078
    {{0x81, 0x17f, 0x20000}, 0x80, {0x19, 0, 0x7530}, 0},   // 0x079
    {{0x82, 0x17f, 0x20000}, 0x81, {0x15, 0, 0x7530}, 0},   // 0x07a
    {{0x83, 0x17f, 0x20000}, 0x82, {0x15, 0, 0x7530}, 0},   // 0x07b
    {{0x84, 0x17f, 0x20000}, 0x83, {0x15, 0, 0x7530}, 0},   // 0x07c
    {{0x85, 0x17f, 0x20000}, 0x84, {0x15, 0, 0x7530}, 0},   // 0x07d
    {{0x86, 0x17f, 0x20000}, 0x85, {0x15, 0, 0x7530}, 0},   // 0x07e
    {{0x87, 0x17f, 0}, 0x86, {3, 0, 0x7530}, 0},   // 0x07f
    {{0x88, 0x17f, 0x20000}, 0x87, {0x15, 0, 0x7530}, 0},   // 0x080
    {{0x89, 0x17f, 0x20000}, 0x88, {0x15, 0, 0x7530}, 0},   // 0x081
    {{0x8a, 0x17f, 0x20000}, 0x89, {0x15, 0, 0x7530}, 0},   // 0x082
    {{0x8b, 0x17f, 0x20000}, 0x8a, {0x15, 0, 0x7530}, 0},   // 0x083
    {{0x8c, 0x17f, 0x20000}, 0x8b, {0x15, 0, 0x7530}, 0},   // 0x084
    {{0x8d, 0x17f, 0x20000}, 0x8c, {0x15, 0, 0x7530}, 0},   // 0x085
    {{0x8e, 0x17f, 0x20000}, 0x8d, {0x15, 0, 0x7530}, 0},   // 0x086
    {{0x8f, 0x17f, 0x20000}, 0x8e, {0x15, 0, 0x7530}, 0},   // 0x087
    {{0x90, 0x17f, 0x20000}, 0x8f, {0x15, 0, 0x7530}, 0},   // 0x088
    {{0x91, 0x17f, 0x20000}, 0x90, {0x15, 0, 0x7530}, 0},   // 0x089
    {{0x92, 0x17f, 0}, 0x91, {0, 0, 0x7530}, 0},   // 0x08a
    {{0x93, 0x17f, 0x10000}, 0x92, {4, 0, 0x7530}, 0},   // 0x08b
    {{0x94, 0x17f, 0x10000}, 0x93, {4, 0, 0x7530}, 0},   // 0x08c
    {{0x95, 0x17f, 0x10000}, 0x94, {4, 0, 0x7530}, 0},   // 0x08d
    {{0x96, 0x97, 0x10000}, 0x95, {0, 0, 0x7530}, 0},   // 0x08e
    {{0x97, 0x96, 0x10000}, 0x96, {0, 0, 0x7530}, 0},   // 0x08f
    {{0x98, 0x17f, 0}, 0x97, {0xe, 0, 0x7530}, 2},   // 0x090
    {{0x99, 0x17f, 0}, 0x98, {0, 0, 0x7530}, 0},   // 0x091
    {{0x9a, 0x17f, 0x10000}, 0x99, {0xe, 0, 0x7530}, 0},   // 0x092
    {{0x9b, 0x17f, 0}, 0x9a, {0x13, 0, 0x7530}, 0},   // 0x093
    {{0x9c, 0x17f, 0}, 0x9a, {0x13, 0, 0x7530}, 0},   // 0x094
    {{0x9d, 0x17f, 0}, 0x9a, {0x13, 0, 0x7530}, 0},   // 0x095
    {{0x9e, 0x17f, 0}, 0x9a, {0x13, 0, 0x7530}, 0},   // 0x096
    {{0x9f, 0x17f, 0}, 0x9b, {4, 0, 0x7530}, 0},   // 0x097
    {{0xa0, 0x17f, 0}, 0x9c, {4, 0, 0x7530}, 0},   // 0x098
    {{0xa1, 0x17f, 0}, 0x9d, {4, 0, 0x7530}, 0},   // 0x099
    {{0xa2, 0x17f, 0}, 0x9e, {4, 0, 0x7530}, 0},   // 0x09a
    {{0xa3, 0x17f, 0}, 0x9f, {3, 0, 0x7530}, 0},   // 0x09b
    {{0xa5, 0x17f, 0x20000}, 0xa0, {0, 0, 0x7530}, 0},   // 0x09c
    {{0xa6, 0x17f, 0x20000}, 0xa1, {0, 0, 0x7530}, 0},   // 0x09d
    {{0xa7, 0x17f, 0x20000}, 0xa2, {0, 0, 0x7530}, 0},   // 0x09e
    {{0xa8, 0x17f, 0x20000}, 0xa3, {0, 0, 0x7530}, 0},   // 0x09f
    {{0xa9, 0x17f, 0x20000}, 0xa4, {0, 0, 0x7530}, 0},   // 0x0a0
    {{0xaa, 0x17f, 0x20000}, 0xa5, {0, 0, 0x7530}, 0},   // 0x0a1
    {{0xab, 0x17f, 0x20000}, 0xa6, {0, 0, 0x7530}, 0},   // 0x0a2
    {{0xac, 0x17f, 0x20000}, 0xa7, {3, 0, 0x7530}, 0},   // 0x0a3
    {{0xad, 0x17f, 0x20000}, 0xa8, {3, 0, 0x7530}, 0},   // 0x0a4
    {{0xae, 0x17f, 0x20000}, 0xa9, {3, 0, 0x7530}, 0},   // 0x0a5
    {{0xaf, 0x17f, 0x20000}, 0xaa, {3, 0, 0x7530}, 0},   // 0x0a6
    {{0xb0, 0x17f, 0x20000}, 0xab, {3, 0, 0x7530}, 0},   // 0x0a7
    {{0xb1, 0x17f, 0x20000}, 0xac, {0, 0, 0x7530}, 0},   // 0x0a8
    {{0xb2, 0x17f, 0x20000}, 0xad, {0, 0, 0x7530}, 0},   // 0x0a9
    {{0xb3, 0x17f, 0x20000}, 0xae, {0, 0, 0x7530}, 0},   // 0x0aa
    {{0xb4, 0x17f, 0x20000}, 0xaf, {0, 0, 0x7530}, 0},   // 0x0ab
    {{0xb5, 0x17f, 0x20000}, 0xb0, {3, 0, 0x7530}, 0},   // 0x0ac
    {{0xf7, 0x17f, 0x40000}, 0xc0, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0ad
    {{0xf8, 0x17f, 0x40000}, 0xc4, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0ae
    {{0xf9, 0x17f, 0x40000}, 0xc8, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0af
    {{0xfa, 0x17f, 0x40000}, 0xcc, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b0
    {{0xfb, 0x17f, 0x40000}, 0xd0, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b1
    {{0xfc, 0x17f, 0x40000}, 0xd4, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b2
    {{0xfd, 0x17f, 0x40000}, 0xd8, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b3
    {{0xfe, 0x17f, 0x40000}, 0xdc, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b4
    {{0xff, 0x17f, 0x40000}, 0xe0, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0b5
    {{0x100, 0x17f, 0x40000}, 0xe4, {0x1a, 0x1b, 0x15f90}, 0xb},   // 0x0b6
    {{0x101, 0x17f, 0x40000}, 0xe8, {0x1a, 0x1b, 0x15f90}, 0xb},   // 0x0b7
    {{0x102, 0x17f, 0x40000}, 0xec, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b8
    {{0x103, 0x17f, 0x40000}, 0xf0, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0b9
    {{0x104, 0x17f, 0x40000}, 0xf4, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0ba
    {{0x105, 0x17f, 0x40000}, 0xf8, {0x1a, 0x1b, 0x15f90}, 0xf},   // 0x0bb
    {{0x106, 0x17f, 0x40000}, 0xfc, {0x1a, 0x1b, 0x15f90}, 0xc},   // 0x0bc
    {{0x107, 0x17f, 0x40000}, 0x100, {0x1a, 0x1b, 0x15f90}, 0xb},   // 0x0bd
    {{0x108, 0x17f, 0x40000}, 0x104, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0be
    {{0x109, 0x17f, 0x40000}, 0x108, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0bf
    {{0x10a, 0x17f, 0x40000}, 0x10c, {0x1a, 0x1b, 0x15f90}, 0xb},   // 0x0c0
    {{0x10b, 0x17f, 0x40000}, 0x114, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0c1
    {{0x10c, 0x17f, 0x40000}, 0x118, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0c2
    {{0x10d, 0x17f, 0x40000}, 0x11c, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0c3
    {{0x10e, 0x17f, 0x40000}, 0x120, {0x1a, 0x1b, 0x15f90}, 9},   // 0x0c4
    {{0x10f, 0x17f, 0x40000}, 0x124, {0x1a, 0x1b, 0x15f90}, 0xd},   // 0x0c5
    {{0x110, 0x17f, 0x40000}, 0x128, {0x1a, 0x1b, 0x15f90}, 0xe},   // 0x0c6
    {{0x111, 0x17f, 0x40000}, 0x130, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0c7
    {{0x112, 0x132, 0x40000}, 0x134, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0c8
    {{0x113, 0x133, 0x40000}, 0x138, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0c9
    {{0x114, 0x17f, 0x40000}, 0x13c, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0ca
    {{0x115, 0x135, 0x40000}, 0x140, {0x1a, 0x1b, 0x15f90}, 0xe},   // 0x0cb
    {{0x116, 0x17f, 0x40000}, 0x144, {0x1a, 0x1b, 0x15f90}, 0xe},   // 0x0cc
    {{0x117, 0x138, 0x40000}, 0x148, {0x1a, 0x1b, 0x15f90}, 0xa},   // 0x0cd
    {{0x118, 0x118, 0x40000}, 0xc1, {0, 0, 0}, 0x10},   // 0x0ce
    {{0x119, 0x119, 0x40000}, 0xc5, {0, 0, 0}, 0x10},   // 0x0cf
    {{0x11a, 0x11a, 0x40000}, 0xc9, {0, 0, 0}, 0x10},   // 0x0d0
    {{0x11b, 0x11b, 0x40000}, 0xcd, {0, 0, 0}, 0x10},   // 0x0d1
    {{0x11c, 0x11c, 0x40000}, 0xd1, {0, 0, 0}, 0x10},   // 0x0d2
    {{0x11d, 0x11d, 0x40000}, 0xd5, {0, 0, 0}, 0x10},   // 0x0d3
    {{0x11e, 0x11e, 0x40000}, 0xd9, {0, 0, 0}, 0x10},   // 0x0d4
    {{0x11f, 0x11f, 0x40000}, 0xdd, {0, 0, 0}, 0x10},   // 0x0d5
    {{0x120, 0x120, 0x40000}, 0xe1, {0, 0, 0}, 0x11},   // 0x0d6
    {{0x121, 0x121, 0x40000}, 0xe5, {0, 0, 0}, 0x12},   // 0x0d7
    {{0x122, 0x122, 0x40000}, 0xe9, {0, 0, 0}, 0x12},   // 0x0d8
    {{0x123, 0x123, 0x40000}, 0xed, {0, 0, 0}, 0x10},   // 0x0d9
    {{0x124, 0x124, 0x40000}, 0xf1, {0, 0, 0}, 0x10},   // 0x0da
    {{0x125, 0x125, 0x40000}, 0xf5, {0, 0, 0}, 0x10},   // 0x0db
    {{0x126, 0x126, 0x40000}, 0xf9, {0, 0, 0}, 0x16},   // 0x0dc
    {{0x127, 0x127, 0x40000}, 0xfd, {0, 0, 0}, 0x13},   // 0x0dd
    {{0x128, 0x128, 0x40000}, 0x101, {0, 0, 0}, 0x12},   // 0x0de
    {{0x129, 0x129, 0x40000}, 0x105, {0, 0, 0}, 0x10},   // 0x0df
    {{0x12a, 0x12a, 0x40000}, 0x109, {0, 0, 0}, 0x10},   // 0x0e0
    {{0x12b, 0x12b, 0x40000}, 0x10d, {0, 0, 0}, 0x12},   // 0x0e1
    {{0x12c, 0x12c, 0x40000}, 0x115, {0, 0, 0}, 0x10},   // 0x0e2
    {{0x12d, 0x12d, 0x40000}, 0x119, {0, 0, 0}, 0x10},   // 0x0e3
    {{0x12e, 0x12e, 0x40000}, 0x11d, {0, 0, 0}, 0x11},   // 0x0e4
    {{0x12f, 0x12f, 0x40000}, 0x121, {0, 0, 0}, 0x10},   // 0x0e5
    {{0x130, 0x130, 0x40000}, 0x125, {0, 0, 0}, 0x14},   // 0x0e6
    {{0x131, 0x131, 0x40000}, 0x129, {0, 0, 0}, 0x15},   // 0x0e7
    {{0x132, 0x132, 0x40000}, 0x131, {0, 0, 0}, 0x11},   // 0x0e8
    {{0x133, 0x133, 0x40000}, 0x135, {0, 0, 0}, 0x11},   // 0x0e9
    {{0x134, 0x134, 0x40000}, 0x139, {0, 0, 0}, 0x11},   // 0x0ea
    {{0x135, 0x135, 0x40000}, 0x13d, {0, 0, 0}, 0x11},   // 0x0eb
    {{0x136, 0x136, 0x40000}, 0x141, {0, 0, 0}, 0x15},   // 0x0ec
    {{0x137, 0x137, 0x40000}, 0x145, {0, 0, 0}, 0x15},   // 0x0ed
    {{0x138, 0x138, 0x40000}, 0x149, {0, 0, 0}, 0x11},   // 0x0ee
    {{0x139, 0x118, 0x40000}, 0xc2, {0, 0, 0x7530}, 0x17},   // 0x0ef
    {{0x13a, 0x119, 0x40000}, 0xc6, {0, 0, 0x7530}, 0x17},   // 0x0f0
    {{0x13b, 0x11a, 0x40000}, 0xca, {0, 0, 0x7530}, 0x17},   // 0x0f1
    {{0x13c, 0x11b, 0x40000}, 0xce, {0, 0, 0x7530}, 0x17},   // 0x0f2
    {{0x13d, 0x11c, 0x40000}, 0xd2, {0, 0, 0x7530}, 0x17},   // 0x0f3
    {{0x13e, 0x11d, 0x40000}, 0xd6, {0, 0, 0x7530}, 0x17},   // 0x0f4
    {{0x13f, 0x11e, 0x40000}, 0xda, {0, 0, 0x7530}, 0x17},   // 0x0f5
    {{0x140, 0x11f, 0x40000}, 0xde, {0, 0, 0x7530}, 0x17},   // 0x0f6
    {{0x141, 0x120, 0x40000}, 0xe2, {0, 0, 0x7530}, 0x18},   // 0x0f7
    {{0x142, 0x121, 0x40000}, 0xe6, {0, 0, 0x7530}, 0x19},   // 0x0f8
    {{0x143, 0x122, 0x40000}, 0xea, {0, 0, 0x7530}, 0x19},   // 0x0f9
    {{0x144, 0x123, 0x40000}, 0xee, {0, 0, 0x7530}, 0x17},   // 0x0fa
    {{0x145, 0x124, 0x40000}, 0xf2, {0, 0, 0x7530}, 0x17},   // 0x0fb
    {{0x146, 0x125, 0x40000}, 0xf6, {0, 0, 0x7530}, 0x17},   // 0x0fc
    {{0x147, 0x126, 0x40000}, 0xfa, {0, 0, 0x7530}, 0x1d},   // 0x0fd
    {{0x148, 0x127, 0x40000}, 0xfe, {0, 0, 0x7530}, 0x1a},   // 0x0fe
    {{0x149, 0x128, 0x40000}, 0x102, {0, 0, 0x7530}, 0x19},   // 0x0ff
    {{0x14a, 0x129, 0x40000}, 0x106, {0, 0, 0x7530}, 0x17},   // 0x100
    {{0x14b, 0x12a, 0x40000}, 0x10a, {0, 0, 0x7530}, 0x17},   // 0x101
    {{0x14c, 0x12b, 0x40000}, 0x10e, {0, 0, 0x7530}, 0x19},   // 0x102
    {{0x14d, 0x12c, 0x40000}, 0x116, {0, 0, 0x7530}, 0x17},   // 0x103
    {{0x14e, 0x12d, 0x40000}, 0x11a, {0, 0, 0x7530}, 0x17},   // 0x104
    {{0x14f, 0x12e, 0x40000}, 0x11e, {0, 0, 0x7530}, 0x18},   // 0x105
    {{0x150, 0x12f, 0x40000}, 0x122, {0, 0, 0x7530}, 0x17},   // 0x106
    {{0x151, 0x130, 0x40000}, 0x126, {0, 0, 0x7530}, 0x1b},   // 0x107
    {{0x152, 0x131, 0x40000}, 0x12a, {0, 0, 0x7530}, 0x1c},   // 0x108
    {{0x153, 0x132, 0x40000}, 0x132, {0, 0, 0x7530}, 0x18},   // 0x109
    {{0x154, 0x133, 0x40000}, 0x136, {0, 0, 0x7530}, 0x18},   // 0x10a
    {{0x155, 0x134, 0x40000}, 0x13a, {0, 0, 0x7530}, 0x18},   // 0x10b
    {{0x156, 0x135, 0x40000}, 0x13e, {0, 0, 0x7530}, 0x18},   // 0x10c
    {{0x157, 0x136, 0x40000}, 0x142, {0, 0, 0x7530}, 0x1c},   // 0x10d
    {{0x158, 0x137, 0x40000}, 0x146, {0, 0, 0x7530}, 0x1c},   // 0x10e
    {{0x159, 0x138, 0x40000}, 0x14a, {0, 0, 0x7530}, 0x18},   // 0x10f
    {{0x15a, 0x118, 0x40000}, 0xc3, {0, 0, 0x7530}, 0x1e},   // 0x110
    {{0x15b, 0x119, 0x40000}, 0xc7, {0, 0, 0x7530}, 0x1e},   // 0x111
    {{0x15c, 0x11a, 0x40000}, 0xcb, {0, 0, 0x7530}, 0x1e},   // 0x112
    {{0x15d, 0x11b, 0x40000}, 0xcf, {0, 0, 0x7530}, 0x1e},   // 0x113
    {{0x15e, 0x11c, 0x40000}, 0xd3, {0, 0, 0x7530}, 0x1e},   // 0x114
    {{0x15f, 0x11d, 0x40000}, 0xd7, {0, 0, 0x7530}, 0x1e},   // 0x115
    {{0x160, 0x11e, 0x40000}, 0xdb, {0, 0, 0x7530}, 0x1e},   // 0x116
    {{0x161, 0x11f, 0x40000}, 0xdf, {0, 0, 0x7530}, 0x1e},   // 0x117
    {{0x162, 0x120, 0x40000}, 0xe3, {0, 0, 0x7530}, 0x1f},   // 0x118
    {{0x163, 0x121, 0x40000}, 0xe7, {0, 0, 0x7530}, 0x20},   // 0x119
    {{0x164, 0x122, 0x40000}, 0xeb, {0, 0, 0x7530}, 0x20},   // 0x11a
    {{0x165, 0x123, 0x40000}, 0xef, {0, 0, 0x7530}, 0x1e},   // 0x11b
    {{0x166, 0x124, 0x40000}, 0xf3, {0, 0, 0x7530}, 0x1e},   // 0x11c
    {{0x167, 0x125, 0x40000}, 0xf7, {0, 0, 0x7530}, 0x1e},   // 0x11d
    {{0x168, 0x126, 0x40000}, 0xfb, {0, 0, 0x7530}, 0x24},   // 0x11e
    {{0x169, 0x127, 0x40000}, 0xff, {0, 0, 0x7530}, 0x21},   // 0x11f
    {{0x16a, 0x128, 0x40000}, 0x103, {0, 0, 0x7530}, 0x20},   // 0x120
    {{0x16b, 0x129, 0x40000}, 0x107, {0, 0, 0x7530}, 0x1e},   // 0x121
    {{0x16c, 0x12a, 0x40000}, 0x10b, {0, 0, 0x7530}, 0x1e},   // 0x122
    {{0x16d, 0x12b, 0x40000}, 0x10f, {0, 0, 0x7530}, 0x20},   // 0x123
    {{0x16e, 0x12c, 0x40000}, 0x117, {0, 0, 0x7530}, 0x1e},   // 0x124
    {{0x16f, 0x12d, 0x40000}, 0x11b, {0, 0, 0x7530}, 0x1e},   // 0x125
    {{0x170, 0x12e, 0x40000}, 0x11f, {0, 0, 0x7530}, 0x1f},   // 0x126
    {{0x171, 0x12f, 0x40000}, 0x123, {0, 0, 0x7530}, 0x1e},   // 0x127
    {{0x172, 0x130, 0x40000}, 0x127, {0, 0, 0x7530}, 0x22},   // 0x128
    {{0x173, 0x131, 0x40000}, 0x12b, {0, 0, 0x7530}, 0x23},   // 0x129
    {{0x174, 0x132, 0x40000}, 0x133, {0, 0, 0x7530}, 0x1f},   // 0x12a
    {{0x175, 0x133, 0x40000}, 0x137, {0, 0, 0x7530}, 0x1f},   // 0x12b
    {{0x176, 0x134, 0x40000}, 0x13b, {0, 0, 0x7530}, 0x1f},   // 0x12c
    {{0x177, 0x135, 0x40000}, 0x13f, {0, 0, 0x7530}, 0x1f},   // 0x12d
    {{0x178, 0x136, 0x40000}, 0x143, {0, 0, 0x7530}, 0x23},   // 0x12e
    {{0x179, 0x137, 0x40000}, 0x147, {0, 0, 0x7530}, 0x23},   // 0x12f
    {{0x17a, 0x138, 0x40000}, 0x14b, {0, 0, 0x7530}, 0x1f},   // 0x130
    {{0x17b, 0x17f, 0x50000}, 0xb1, {0, 0, 0x7530}, 0},   // 0x131
    {{0x17c, 0x17f, 0x50000}, 0xb3, {0, 0, 0x7530}, 0},   // 0x132
    {{0x17d, 0x17f, 0x50000}, 0xb2, {0, 0, 0x7530}, 0},   // 0x133
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x134
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x135
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x136
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x137
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x138
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x139
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x13a
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x13b
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x13c
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x13d
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x13e
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x13f
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x140
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x141
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x142
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x143
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x144
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x145
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x146
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x147
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x148
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x149
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x14a
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x14b
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x14c
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x14d
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x14e
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x14f
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x150
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x151
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x152
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x153
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x154
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x155
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x156
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x157
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x158
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x159
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x15a
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x15b
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x15c
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x15d
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x15e
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x15f
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x160
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x161
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x162
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x163
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x164
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x165
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x166
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x167
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x168
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x169
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x16a
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x16b
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x16c
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x16d
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x16e
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x16f
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x170
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x171
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x172
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x173
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x174
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x175
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x176
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x177
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x178
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x179
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x17a
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x17b
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x17c
    {{0, 0, 0}, 0, {0, 0, 0}, 0},   // 0x17d
};
namespace {

const char kNotFound[] = "CTagTable - not found %d\n";                    // 0x10246616
const char kTextA[] = "TAG SYSTEM says: Postcard sent\n";                                     // 0x10246451
const char kTextB[] = "TAG SYSTEM says: Postcard to outbox\n";                                      // 0x10246471
const char kTextC[] = "TAG SYSTEM says: Postcard failed\n";                                      // 0x10246496
const char kBadMessage[] = "CTagSystem::ReceiveMessage has encountered an unknown (to it) message type (%d).\n";                                  // 0x102464b8

int16_t ElementShort(const TagTableElement* e) { return *reinterpret_cast<const int16_t*>(&e->mF00[2]); }

}  // namespace

// 0x1012c90c
void CTagSystem::ReceiveMessage(const CMsg& message) {
    CTagParams params;
    params.mId = 0x17f;
    params.mF4 = 0x17f;
    params.mText = nullptr;
    params.mTime = 0;
    params.mFrame = 0;
    params.mF14 = 0;
    params.mF18 = 0;
    params.mF1C = 0;
    params.mF20 = 0;
    params.mShow = 0;
    char16_t* text = reinterpret_cast<char16_t*>(mF0C);
    const CTagTable* table = reinterpret_cast<const CTagTable*>(mF0C + 0x800);   // this + 0x80c

    switch (message.GetType()) {
    case 0x18: {
        const CMsgTagShow& m = static_cast<const CMsgTagShow&>(message);
        int32_t id = m.mTag;
        const TagTableElement* element = table->GetElement(id);
        if (element == nullptr) return;
        uint16_t thing = m.mThing;
        int32_t time = m.mTime;
        if (time < 0) time = static_cast<int32_t>(element->mF10[2]);
        params.mId = static_cast<uint32_t>(id);
        params.mF4 = static_cast<int32_t>(element->mF00[1]);
        params.mFrame = ElementShort(element);
        FormatTagText(element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{thing}, m.mValue, nullptr);
        params.mText = text;
        params.mTime = time;
        params.mF1C = thing;
        params.mF14 = static_cast<int32_t>(element->mF10[0]);
        params.mF18 = static_cast<int32_t>(element->mF10[1]);
        params.mF20 = static_cast<int32_t>(m.mValue);
        gUIInterface.MessageTag(params);
        return;
    }
    case 0x19: {
        const CMsgChallenge& m = static_cast<const CMsgChallenge&>(message);
        int32_t id = m.mTagId;
        const TagTableElement* element = table->GetElement(id);
        if (element == nullptr) return;
        params.mTime = static_cast<int32_t>(element->mF10[2]);
        params.mId = static_cast<uint32_t>(id);
        params.mF4 = static_cast<int32_t>(element->mF00[1]);
        params.mFrame = ElementShort(element);
        FormatTagText(element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, &m);
        params.mText = text;
        params.mF14 = static_cast<int32_t>(element->mF10[0]);
        params.mF18 = static_cast<int32_t>(element->mF10[1]);
        gUIInterface.MessageTag(params);
        return;
    }
    case 0x13: {
        int32_t code = static_cast<const CMsgTagSimple&>(message).mCode;
        int32_t id = 0x17f;
        if (code == 8) {
            SamsUtilities::UDebug(kTextB);
            id = 0x17c;
        } else if (code < 8) {
            if (code >= 7) {
                SamsUtilities::UDebug(kTextA);
                id = 0x17b;
            }
        } else if (code < 10) {
            SamsUtilities::UDebug(kTextC);
            id = 0x17d;
        }
        if (id == 0x17f) return;
        const TagTableElement* element = table->GetElement(id);
        if (element == nullptr) return;
        params.mTime = static_cast<int32_t>(element->mF10[2]);
        params.mId = static_cast<uint32_t>(id);
        params.mF4 = static_cast<int32_t>(element->mF00[1]);
        params.mFrame = ElementShort(element);
        FormatTagText(element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, nullptr);
        params.mText = text;
        params.mF14 = static_cast<int32_t>(element->mF10[0]);
        params.mF18 = static_cast<int32_t>(element->mF10[1]);
        gUIInterface.MessageTag(params);
        return;
    }
    default:
        TRACE(kBadMessage, message.GetType());
        return;
    }
}

// CTagSystem::FormatTagText (unit v5626), cut from the neighbourhood of src/ThingLoop.cpp.

uint16_t gTagNoThingId;    // 0x102a1214
uint16_t gTagNoRideType;   // 0x102a1216

namespace {

// The texts of the string pool at 0x10246451 (narrow) and 0x1024659a (wide).
const char kThingAssert[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";               // 0x1024650a
const char kNoRideTrace[] = "Object id is IDNULL in CTagSystem::FormatTagText()   (TVT_UNICODE_OBJNAME)\n";       // 0x1024654d
const char16_t kEmptyText[] = u"";                                                                                // 0x1024659a
const char16_t kThingText[] = u"A string needs to be put in here";                                               // 0x1024659c
const char16_t kPlainText[] = u"String and number goes here";                                                    // 0x102465de

// gText's resource pointers 5 (Mac 0x560) and 17 (Mac 0x590): the tag texts and the staff names.
const TbITable* TagStrings() { return static_cast<const TbITable*>(gText.mResourcePointers[5]); }
const TbITable* StaffStrings() { return static_cast<const TbITable*>(gText.mResourcePointers[17]); }

// Integer balance values by their Mac offsets in gMainBalance (0x750, 0x754, 0x760, 0x764, 0x768).
int32_t Balance(uint32_t macOffset) { return gMainBalance.mValues[(macOffset - 8) / 4].mInteger; }

// The staff name of the challenge's staff type (0 to 4: string 1, 3, 5, 7 or 9); another type leaves `name` as it was
// (uninitialised, as on the Mac).
void StaffName(const CMsgChallenge* challenge, char16_t* name) {
    switch (challenge->mStaff) {
    case 0: StaffStrings()->FormatString(name, 0x40, 1); break;
    case 1: StaffStrings()->FormatString(name, 0x40, 3); break;
    case 2: StaffStrings()->FormatString(name, 0x40, 5); break;
    case 3: StaffStrings()->FormatString(name, 0x40, 7); break;
    case 4: StaffStrings()->FormatString(name, 0x40, 9); break;
    default: break;
    }
}

TbVTag* V(TbVTag& tag) { return &tag; }

}  // namespace

// 0x1012cc3c, 4308 bytes. The integer tags carry the number's magnitude and sign on the Mac (TbIntegerVTag's constructor);
// the host tag keeps the signed number.
int32_t CTagSystem::FormatTagText(const TagTableElement* element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                                  uint32_t value, const CMsgChallenge* challenge) {
    char16_t* out = reinterpret_cast<char16_t*>(mF0C);
    if (element == nullptr) {
        swprintf(out, 0x400, kEmptyText);
        return 0;
    }
    const CMsgChallenge* c = challenge;
    switch (element->mFormat) {
    case 0:   // 0x1012ccc4: the text alone
        TagStrings()->FormatString(out, 0x400, element->mTextId);
        break;
    case 1: {   // 0x1012cce0: a thing that still exists
        if (thing.mIndex == gTagNoThingId) return 0;
        uint16_t index = thing.mIndex;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingAssert);
        if (gThingArray[index].mThing == nullptr) return 0;
        swprintf(out, 0x400, kThingText);
        break;
    }
    case 2: {   // 0x1012cd60: the ride type in the low half of `value`
        uint16_t type = static_cast<uint16_t>(value);
        if (type == gTagNoRideType) {
            TRACE(kNoRideTrace);
            return 0;
        }
        TbUniStringVTag name(0, gRideDatabase.GetTypeName(type));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(name));
        break;
    }
    case 3: {   // 0x1012cdf4: `value` as a number
        TbIntegerVTag number(1, static_cast<int32_t>(value));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 4:   // 0x1012cddc
        swprintf(out, 0x400, kPlainText);
        break;
    case 5: {   // 0x1012ce54
        TbIntegerVTag number(1, Balance(0x750));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 6: {   // 0x1012ceb8
        TbIntegerVTag number(1, Balance(0x754));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 7: {   // 0x1012cf1c
        TbIntegerVTag number(1, Balance(0x760));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 8: {   // 0x1012cf80
        TbIntegerVTag first(1, Balance(0x764));
        TbIntegerVTag second(2, Balance(0x768));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(first), V(second));
        break;
    }
    case 9:
    case 11: {   // 0x1012d030
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag time(2, c->mTargetTime);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(time));
        break;
    }
    case 10:
    case 14: {   // 0x1012d12c
        char16_t ride[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag time(2, c->mTargetTime);
        TbUniStringVTag name(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(time), V(name));
        break;
    }
    case 12:
    case 13: {   // 0x1012d274
        char16_t staff[0x40];
        StaffName(c, staff);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag time(2, c->mTargetTime);
        TbUniStringVTag name(6, staff);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(time), V(name));
        break;
    }
    case 15: {   // 0x1012d450
        char16_t ride[0x40];
        char16_t ride2[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        gRideDatabase.GetTypeName(c->mObj2, ride2, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag time(2, c->mTargetTime);
        TbUniStringVTag second(7, ride2);
        TbUniStringVTag first(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(time), V(first), V(second));
        break;
    }
    case 16:
    case 18: {   // 0x1012d594
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TbIntegerVTag days(5, c->mDays);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(sofar), V(days));
        break;
    }
    case 17:
    case 21: {   // 0x1012d6dc
        char16_t ride[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TbIntegerVTag days(5, c->mDays);
        TbUniStringVTag name(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(sofar), V(days), V(name));
        break;
    }
    case 19:
    case 20: {   // 0x1012d874
        char16_t staff[0x40];
        StaffName(c, staff);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TbIntegerVTag days(5, c->mDays);
        TbUniStringVTag name(6, staff);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(sofar), V(days), V(name));
        break;
    }
    case 22: {   // 0x1012daa0
        char16_t ride[0x40];
        char16_t ride2[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        gRideDatabase.GetTypeName(c->mObj2, ride2, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag days(5, c->mDays);
        TbUniStringVTag second(7, ride2);
        TbUniStringVTag first(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(days), V(first), V(second));
        break;
    }
    case 23: case 24: case 25: case 26: case 27: case 28: case 29: {   // 0x1012dbe4
        TbIntegerVTag prize(3, c->mPrize);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize));
        break;
    }
    case 30: case 31: case 32: case 33: case 34: case 35: case 36: {   // 0x1012dc4c
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(target), V(sofar));
        break;
    }
    default:   // above 0x24 (unsigned)
        break;
    }
    return 1;
}

// 0x1012dd94
const TagTableElement* CTagTable::GetElement(int32_t id) const {
    if (static_cast<uint32_t>(id) - 0x17e < 2) return nullptr;
    const TagTableElement* element = reinterpret_cast<const TagTableElement*>(reinterpret_cast<const uint8_t*>(gTagTable) + static_cast<int64_t>(id) * 0x20);
    if (id != static_cast<int32_t>(element->mF00[0])) {
        for (int32_t i = 0; i < 0x17e; i++) {
            element = &gTagTable[i];
            if (id == static_cast<int32_t>(element->mF00[0])) return element;
        }
        CTRACE(3, kNotFound, id);
        return nullptr;
    }
    return element;
}

// 0x1012de58 (236 bytes, Thing.cpp). The thing is entered in the world's table (CWorld::AddThingToThingList on the global world, whose
// answer, the row, becomes its id) and in the thing list of its map cell. The cell index is 1 based and computed in 32 bits: 0 gives
// 0xffffffff, the cell before the array. The stored y byte is the low byte of the shifted index, the TRACE argument the whole value.
CThing::CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, ThingModel model) {
    mThingId = 0;
    mKind = static_cast<uint8_t>(model);
    mDead = 0;
    mCellPrev = 0;
    mCellNext = 0;
    mThingId = gWorld->AddThingToThingList(this).mIndex;
    mCellXSub = 0x80;
    const uint32_t index = static_cast<uint32_t>(cell.mIndex) - 1u;
    const uint32_t x = index & 0x7fu;
    const uint32_t y = index >> 7;
    mCellX = static_cast<uint8_t>(x);
    mCellYSub = 0x80;
    mCellY = static_cast<uint8_t>(y);
    // The cell is 0x44 bytes (CMapCell) from the map's base; the multiply keeps its low 32 bits, read as signed.
    CMapCell* const mapCell = reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gWorldMap) +
                                                           static_cast<intptr_t>(static_cast<int32_t>(index * 0x44u)));
    mapCell->InsertThing(GetThingId());
    TRACE("Creating thing at [%d,%d]\n", static_cast<int32_t>(x), static_cast<int32_t>(y));
}

// The world's save (main app of SimTheme Park 2.04b3, World.cpp; unit w205): CWorld::Save, the two SAVEVAR instances it needs for its own
// enums (CWorld::WorldState 0x1014182c and CThing::ThingModel 0x10141980), the staff and mechanic headquarters getters, the macro layer's
// destructor and CThing's empty serialise constructor. The two chunk reader destructors of the same range are in src/ChunkFile.cpp with their
// classes. CWorld::Load and the matching LOADVAR instances are the next unit.
//
// The save is a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list of the thing table:
// for each used entry the next entry's index, the thing's kind and the thing itself, written by its own Serialise by the kind. Every callee
// answers 0 for success; the first nonzero answer ends Save with 0 at once (nothing is called after it), and a save that wrote everything
// answers 1. The strings are at 0x10247498 + the offset in the comment.

// 0x1013dce0 (4 bytes): a bare return; the thing's fields are left as they were (the loader fills them in).
CThing::CThing(const CSerialise&) {}

// The world's load (main app of SimTheme Park 2.04b3, World.cpp; unit w207): CWorld::Load, the two LOADVAR instances it needs for its own
// enums (CWorld::WorldState 0x101415b4 and CThing::ThingModel 0x101416f0) and CThing::ForceThingId (Thing.cpp). The matching save is
// src/World_save.cpp (unit w205).
//
// The load reads what Save wrote: a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list
// of the thing table. It starts by resetting the table (InitThingHeap). The used list is rebuilt from the head's index and each entry's next
// index, and for every used entry the load MAKES the thing of the entry's kind in place (operator new of the kind's size, the part of the
// class's constructor the Mac inlines here, the kind byte, the entry's index as the thing's id) before the thing's own Serialise reads it.
// The Mac's operator new is the game heap, so the host takes every thing's block from the arena (unit v5L31): the lists keep a thing's
// address in a 32-bit word.
// Every callee answers 0 for success; the first nonzero answer ends Load with 0 at once (nothing is called after it, and a thing that was
// made is not entered in its row), and a load that read everything relinks the free list, switches the sprite drawing by the world state
// and answers 1. The strings are at 0x10247498 + the offset in the comment.

// 0x1012dfa8 (16 bytes). The loader makes the thing's id from the table row it sits in (the row's index, a halfword).
void CThing::ForceThingId(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { mThingId = id.mIndex; }

// CThing::GetThingId 0x1012e1f8 and CThing::ReceiveMessage 0x1012e470 (main app, unit w107; the inventory files them under Thing.cpp).

// The Mac returns the two-byte tag through a hidden pointer and copies the half-word at +0 into it.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const { return {mThingId}; }

// 0x1012e230, 536 bytes. The kind is unsigned and dispatch is direct, without a dead test.
void CThing::Process() {
    switch (mKind) {
    case 1:
        reinterpret_cast<CPerson*>(this)->UpdateTurn();
        reinterpret_cast<CPerson*>(this)->ModelState();
        break;
    case 3:
        reinterpret_cast<CObject*>(this)->UpdateTurn();
        reinterpret_cast<CObject*>(this)->ModelState();
        break;
    case 4:
        reinterpret_cast<CMechanic*>(this)->UpdateTurn();
        reinterpret_cast<CMechanic*>(this)->ModelState();
        break;
    case 5:
        reinterpret_cast<CHandyman*>(this)->UpdateTurn();
        reinterpret_cast<CHandyman*>(this)->ModelState();
        break;
    case 6:
        reinterpret_cast<CEntertainer*>(this)->UpdateTurn();
        reinterpret_cast<CEntertainer*>(this)->ModelState();
        break;
    case 7:
        reinterpret_cast<CGuard*>(this)->UpdateTurn();
        reinterpret_cast<CGuard*>(this)->ModelState();
        break;
    case 8:
        reinterpret_cast<CResearcher*>(this)->UpdateTurn();
        reinterpret_cast<CResearcher*>(this)->ModelState();
        break;
    case 11:
        if (TheGameType().mType != kGameTypeOnline) reinterpret_cast<CAdvisor*>(this)->UpdateTurn();
        break;
    case 13:
        reinterpret_cast<CParkAnalyser*>(this)->UpdateTurn();
        break;
    case 15:
        reinterpret_cast<CWeather*>(this)->UpdateTurn();
        break;
    case 18:
        if (TheGameType().mType == kGameTypeOnline) {
            reinterpret_cast<COnlinePerson*>(this)->UpdateTurn();
            reinterpret_cast<COnlinePerson*>(this)->ModelState();
        } else {
            CTRACE(3, "We probably shouldn't be encountering MODEL_ONLINE_PERSONs in the normal world tick should we... (?)\n");
        }
        break;
    case 19:
        if (TheGameType().mType != kGameTypeOnline) reinterpret_cast<CChallengeHQ*>(this)->UpdateTurn();
        break;
    default:
        break;
    }
}

// A switch on the kind byte through a jump table at 0x102a42d0; each case hands the thing and the message, unchanged, to the class's
// ReceiveMessage (a direct call). Kinds 0 and 2 and everything above 0x13 do nothing.
void CThing::ReceiveMessage(const CMsg& message) {
    switch (mKind) {
    case 1: reinterpret_cast<CPerson*>(this)->ReceiveMessage(message); break;
    case 3: reinterpret_cast<CObject*>(this)->ReceiveMessage(message); break;
    case 4: reinterpret_cast<CMechanic*>(this)->ReceiveMessage(message); break;
    case 5: reinterpret_cast<CHandyman*>(this)->ReceiveMessage(message); break;
    case 6: reinterpret_cast<CEntertainer*>(this)->ReceiveMessage(message); break;
    case 7: reinterpret_cast<CGuard*>(this)->ReceiveMessage(message); break;
    case 8: reinterpret_cast<CResearcher*>(this)->ReceiveMessage(message); break;
    case 9: reinterpret_cast<CStaffHQ*>(this)->ReceiveMessage(message); break;
    case 10: reinterpret_cast<CMechanicHQ*>(this)->ReceiveMessage(message); break;
    case 11: reinterpret_cast<CAdvisor*>(this)->ReceiveMessage(message); break;
    case 12: reinterpret_cast<CTagSystem*>(this)->ReceiveMessage(message); break;
    case 13: reinterpret_cast<CParkAnalyser*>(this)->ReceiveMessage(message); break;
    case 14: reinterpret_cast<CResearchLab*>(this)->ReceiveMessage(message); break;
    case 15: reinterpret_cast<CWeather*>(this)->ReceiveMessage(message); break;
    case 16: reinterpret_cast<CBankAccount*>(this)->ReceiveMessage(message); break;
    case 17: reinterpret_cast<CUIMsgReceiver*>(this)->ReceiveMessage(message); break;
    case 18: reinterpret_cast<COnlinePerson*>(this)->ReceiveMessage(message); break;
    case 19: reinterpret_cast<CChallengeHQ*>(this)->ReceiveMessage(message); break;
    default: break;
    }
}

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdTag;

// 0x1012e5b4: moves the thing to the cells of two MapPos; a move inside the same cell only stores the positions.
void CThing::Move(MapPos x, MapPos y) {
    if (x.mCell == mCellX && y.mCell == mCellY) {
        mCellX = x.mCell;
        mCellXSub = x.mOffset;
        mCellY = y.mCell;
        mCellYSub = y.mOffset;
        return;
    }
    GAME_ASSERTMSG(static_cast<uint32_t>(x.mCell) + static_cast<uint32_t>(y.mCell) * 0x80 <= 0x4000,
                   "Attempt to move thing to invalid cell");
    ThingIdTag id;
    id.mIndex = mThingId;
    CMapCell* cells = reinterpret_cast<CMapCell*>(gWorldMap);
    uint16_t oldCell = static_cast<uint16_t>(mCellX + (mCellY * 0x80 + 1));
    cells[static_cast<int32_t>(oldCell) - 1].RemoveThing(id);
    mCellX = x.mCell;
    mCellXSub = x.mOffset;
    mCellY = y.mCell;
    mCellYSub = y.mOffset;
    uint16_t newCell = static_cast<uint16_t>(mCellX + (mCellY * 0x80 + 1));
    cells[static_cast<int32_t>(newCell) - 1].InsertThing(id);
}

// 0x1012e728, 568 bytes. Post first, dispatch the freshly read kind, mark dead,
// queue the freshly read id, then set kind 2. Even assertion paths reach the tail.
void CThing::KillMe() {
    CMsgDeath message({mThingId});
    void* deathVptr;
    std::memcpy(&deathVptr, static_cast<const void*>(&message), sizeof deathVptr);
    Centre()->PostMessage(message);
    // The Mac's inlined nondeleting destructor repeats the death vptr store.
    // Restore the native vptr too, without a destructor call or a Mac address.
    std::memcpy(static_cast<void*>(&message), &deathVptr, sizeof deathVptr);
    switch (mKind) {
    case 1: reinterpret_cast<CPerson*>(this)->FinalDestroy(); break;
    case 3: reinterpret_cast<CObject*>(this)->FinalDestroy(); break;
    case 4: reinterpret_cast<CMechanic*>(this)->FinalDestroy(); break;
    case 5: reinterpret_cast<CHandyman*>(this)->FinalDestroy(); break;
    case 6: reinterpret_cast<CEntertainer*>(this)->FinalDestroy(); break;
    case 7: reinterpret_cast<CGuard*>(this)->FinalDestroy(); break;
    case 8: reinterpret_cast<CResearcher*>(this)->FinalDestroy(); break;
    case 9: reinterpret_cast<CStaffHQ*>(this)->FinalDestroy(); break;
    case 10: reinterpret_cast<CMechanicHQ*>(this)->FinalDestroy(); break;
    case 11: reinterpret_cast<CAdvisor*>(this)->FinalDestroy(); break;
    case 12: reinterpret_cast<CTagSystem*>(this)->FinalDestroy(); break;
    case 13: reinterpret_cast<CParkAnalyser*>(this)->FinalDestroy(); break;
    case 14: reinterpret_cast<CResearchLab*>(this)->FinalDestroy(); break;
    case 15: reinterpret_cast<CWeather*>(this)->FinalDestroy(); break;
    case 16: reinterpret_cast<CBankAccount*>(this)->FinalDestroy(); break;
    case 17: reinterpret_cast<CUIMsgReceiver*>(this)->FinalDestroy(); break;
    case 18: reinterpret_cast<COnlinePerson*>(this)->FinalDestroy(); break;
    case 19: reinterpret_cast<CChallengeHQ*>(this)->FinalDestroy(); break;
    case 2: GAME_ASSERTMSG(0, "Trying to delete an already-deleted thing\n"); break;
    default: GAME_ASSERTMSG(0, "Dunno how to delete this CThing\n"); break;
    }
    mDead = 1;
    ThingLoopWorldPending(gWorld, {mThingId});
    mKind = 2;
}

// CMsgDeath (main app, unit w123c): the constructor 0x10152264, GetType 0x1012ec14 and destructor 0x1012e984. The inventory files them under
// RideDB.cpp and Thing.cpp (unreliable). The three live together because the native vtable needs a key function (GetType) and the
// message is made by CThing::KillMe.

// The Mac's destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does that).
CMsgDeath::~CMsgDeath() {}

// 0x1012e9f8: takes the thing out of its cell, frees a ride's instance, clears the selection global for people and staff
// (kinds 1, 4 to 8 and 0x12) and drops its subscriptions (the message centre is made on first use).
void CThing::FinalDestroy() {
    ThingIdTag id;
    id.mIndex = mThingId;
    CMapCell* cells = reinterpret_cast<CMapCell*>(gWorldMap);
    uint16_t cell = static_cast<uint16_t>(mCellX + (mCellY * 0x80 + 1));
    cells[static_cast<int32_t>(cell) - 1].RemoveThing(id);
    if (mKind == 3) DestroyRideInstance(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x1c));
    bool person = mKind == 1 || (mKind >= 4 && mKind <= 8) || mKind == 0x12;
    if (person && this == gESPF20) gESPF20 = nullptr;
    if (gMessageCentre == nullptr) {
        void* mem = staffhq_callee::OperatorNew(0x1d0);
        if (mem != nullptr) staffhq_callee::ConstructSubscriptions(mem);
        gMessageCentre = static_cast<CMessageCentre*>(mem);
    }
    gMessageCentre->UnsubscribeAll(*this);
}

namespace {

// The Mac's mX and mY are the byte pairs (cell, sub) read as one big-endian short; the templates take a host short. PairToShort turns
// the pair into that short in place and ShortToPair back, so the templates see the field's own address and a value they leave stays.
void PairToShort(uint8_t* b) {
    const int16_t v = static_cast<int16_t>((b[0] << 8) | b[1]);
    std::memcpy(b, &v, 2);
}

void ShortToPair(uint8_t* b) {
    int16_t v;
    std::memcpy(&v, b, 2);
    b[0] = static_cast<uint8_t>(static_cast<uint16_t>(v) >> 8);
    b[1] = static_cast<uint8_t>(v);
}

}  // namespace

// 0x1012e010: the position and the cell list links; versions 0 and 1 only; a load clears the dead flag.
int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    int16_t* px = reinterpret_cast<int16_t*>(&mCellX);
    int16_t* py = reinterpret_cast<int16_t*>(&mCellY);
    ThingIdTag* child = reinterpret_cast<ThingIdTag*>(&mCellNext);
    ThingIdTag* parent = reinterpret_cast<ThingIdTag*>(&mCellPrev);
    int32_t failed;
    if (save != 0) {
        if (version < 2 && version >= 0) {
            PairToShort(&mCellX);
            failed = SAVEVAR<int16_t>(file, px, "mX", 0);
            ShortToPair(&mCellX);
            if (failed != 0) return 1;
            PairToShort(&mCellY);
            failed = SAVEVAR<int16_t>(file, py, "mY", 0);
            ShortToPair(&mCellY);
            if (failed != 0) return 1;
            if (SAVEVARSTRUCT<ThingIdTag>(file, child, "mMapChild", 0) != 0) return 1;
            if (SAVEVARSTRUCT<ThingIdTag>(file, parent, "mMapParent", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            PairToShort(&mCellX);
            failed = LOADVAR<int16_t>(file, px, "mX", 0);
            ShortToPair(&mCellX);
            if (failed != 0) return 1;
            PairToShort(&mCellY);
            failed = LOADVAR<int16_t>(file, py, "mY", 0);
            ShortToPair(&mCellY);
            if (failed != 0) return 1;
            if (LOADVARSTRUCT<ThingIdTag>(file, child, "mMapChild", 0) != 0) return 1;
            if (LOADVARSTRUCT<ThingIdTag>(file, parent, "mMapParent", 0) != 0) return 1;
            mDead = 0;
        }
    }
    return 0;
}

int32_t CMsgDeath::GetType() const { return 0x1b; }

void __sinit_Thing_cpp() {
    gThingSinitLow = 0;
    gThingSinitHigh = 0x10000;
}
