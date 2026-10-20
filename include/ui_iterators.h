#ifndef TPW_UI_ITERATORS_H
#define TPW_UI_ITERATORS_H

#include "analyser.h"

// Unit w227 (src/ui_iterators.cpp). Enumerator names are invented from the selections.
enum UIIteratorTypes : int32_t {
    kUIIterStaff = 0x1, kUIIterHandymen = 0x2, kUIIterMechanics = 0x4,
    kUIIterGuards = 0x8, kUIIterEntertainers = 0x10, kUIIterResearchers = 0x20,
    kUIIterVisitors = 0x40, kUIIterRides = 0x80, kUIIterSideshows = 0x100,
    kUIIterShops = 0x200, kUIIterFeatures = 0x400, kUIIterPlainFeatures = 0x800,
    kUIIterBogs = 0x1000, kUIIterUpgrades = 0x2000,
    kUIIterNonTrackUpgrades = 0x4000, kUIIterStaffRooms = 0x8000
};

int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)); // 0x101f7db0, 3240 bytes
CThing* NextObjectOfType(UIObjType type, CThing* thing); // 0x101f8aa0, 292 bytes
CThing* NextUpgrade(CThing* thing); // 0x101f8bfc, 264 bytes
CThing* NextNonTrackUpgrade(CThing* thing); // 0x101f8d30, 292 bytes
CThing* NextBog(CThing* thing); // 0x101f8e88, 304 bytes
CThing* NextFeatureNotBogOrStaffRoom(CThing* thing); // 0x101f8fe0, 312 bytes
CThing* NextStaffRoom(CThing* thing); // 0x101f9154, 232 bytes
CThing* UI_IterateThingNext(UIIteratorTypes type, CThing* thing); // 0x101f9268, 1452 bytes
CThing* PreviousObjectOfType(UIObjType type, CThing* thing); // 0x101f9858, 404 bytes
CThing* PreviousUpgrade(CThing* thing); // 0x101f9a28, 376 bytes
CThing* PreviousNonTrackUpgrade(CThing* thing); // 0x101f9bd0, 404 bytes
CThing* PreviousBog(CThing* thing); // 0x101f9d9c, 416 bytes
CThing* PreviousFeatureNotBogOrStaffRoom(CThing* thing); // 0x101f9f68, 424 bytes
CThing* PreviousStaffRoom(CThing* thing); // 0x101fa150, 344 bytes
CThing* UI_IterateThingPrevious(UIIteratorTypes type, CThing* thing); // 0x101fa2d8, 1112 bytes
CThing* UI_WrapIterateThingNext(UIIteratorTypes type, CThing* thing); // 0x101fa778, 64 bytes
CThing* UI_WrapIterateThingPrevious(UIIteratorTypes type, CThing* thing); // 0x101fa800, 64 bytes

// __sinit_ui_iterators_cpp, 0x101fa88c (28 bytes). Names invented; constant initialisers on the host.
extern uint32_t gUIIterFileWord10396A4C;
extern uint32_t gUIIterFileWord10396A50;

#endif
