#include "World.h"
#include <cstddef>
#include <cstdint>
#include <new>
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Mouse.h"
#include "Serialise.h"
#include "StaffDB.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "WorldThings.h"

// __sinit_World_cpp (0x10141c14, 348 bytes): World.cpp's static initialiser. Declarations are local to this file.
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// New globals (Mac addresses in comments; names invented).
uint32_t gWorldWord1037ec00;   // 0x1037ec00, set to 0x10000
uint32_t gWorldWord1037ec04;   // 0x1037ec04, set to 0
char gSinitCtorVecThingEntry[8];   // 0x10266db8, TVector of CWorld::_CThingArrayEntry::_CThingArrayEntry
char gSinitDtorVecPendingList[8];  // 0x10266db0, TVector of TbTList<...>::~TbTList
char gSinitNodeList[16];       // 0x1034cbe8, registration node of gPendingDeleteList
char gSinitNodeBus[16];        // 0x1034cb8c
char gSinitNodeSeaplane[16];   // 0x1034cb6c
char gSinitNodeFerry[16];      // 0x1034cb4c
char gSinitNodeGates[16];      // 0x1034cb2c
char gSinitNodeLights[16];     // 0x1034cb0c
char gSinitNodeEnd[16];        // 0x1034caec

typedef TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> PendingList;

// 0x10141d94 (28 bytes, translated elsewhere): declared so that the generic constructor is not instantiated here.
template<> PendingList::TbTList();

void __sinit_World_cpp() {
    void* wdtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    gWorldWord1037ec04 = 0;
    gWorldWord1037ec00 = 0x10000;
    __construct_array(gThingArray, gSinitCtorVecThingEntry, nullptr, 0x14, 0x2800);
    PendingList* list = new (&gPendingDeleteList) PendingList();
    __register_global_object(list, gSinitDtorVecPendingList, gSinitNodeList);
    TbDynamicStringTemplate<char16_t>* s;
    s = new (&gArrivalBusName) TbDynamicStringTemplate<char16_t>(u"bus");
    __register_global_object(s, wdtor, gSinitNodeBus);
    s = new (&gArrivalSeaplaneName) TbDynamicStringTemplate<char16_t>(u"seaplane");
    __register_global_object(s, wdtor, gSinitNodeSeaplane);
    s = new (&gArrivalFerryName) TbDynamicStringTemplate<char16_t>(u"Ferry");
    __register_global_object(s, wdtor, gSinitNodeFerry);
    s = new (&gWorldGatesName) TbDynamicStringTemplate<char16_t>(u"gates");
    __register_global_object(s, wdtor, gSinitNodeGates);
    s = new (&gWorldLightsName) TbDynamicStringTemplate<char16_t>(u"lights");
    __register_global_object(s, wdtor, gSinitNodeLights);
    s = new (&gWorldEndName) TbDynamicStringTemplate<char16_t>(u"End");
    __register_global_object(s, wdtor, gSinitNodeEnd);
    gWinningAnimThing = 0;
}
