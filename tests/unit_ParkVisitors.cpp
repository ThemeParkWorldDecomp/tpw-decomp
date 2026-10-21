// Differential library for unit w154, the park analyser's visitor counts and opinions (src/game/park/analyser.cpp).
//
// Every callee is a fake that records the same event as the PowerPC side's stub: GAME_ASSERTMSG, the world accessor (CWorldSavvy::GetWorld), CWorldSavvy::GetMap,
// CWorld::GetUsedThingHead and GetNextUsedThing, CPeep::IsInsidePark, CParkAnalyser::GatesOpen, CMap::GetTicketBoothPos, CParkTime::GetFunnyTimeForGameTurns,
// the analyser value's Get and Set and the histories (SetAtEnd, GetAverageOfLast, HowFarBack, GetValue; reported with their offset in the analyser). The
// answers come from queues.
//
// The world is NOT a block of its Mac size. In "walk" mode only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail); in "cells" mode the map cells
// (gCells, 0x44 bytes a cell, a pad before them) are real and the world pointer is the cells' address minus 0x2d8. The things are raw blocks (gThings).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "History.h"
#include "MapCell.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "ui_main.h"

using namespace calllog;

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kAnalyserSize = 0x21c10;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kCellPad = 0x100;
constexpr int32_t kCellBytes = 0x4000 * 0x44;
constexpr int32_t kThingSlots = 8;
constexpr int32_t kThingSize = 0x448;

alignas(16) unsigned char gTail[kTailSize];
alignas(16) unsigned char gAnalyserBuf[kGuard + kAnalyserSize + kGuard];
alignas(16) unsigned char gThings[kThingSlots][kThingSize];
alignas(16) unsigned char gCellBlock[kCellPad + kCellBytes + kCellPad];
CWorld* gWorldPtr;

unsigned char* AnalyserBase() { return gAnalyserBuf + kGuard; }
unsigned char* Cells() { return gCellBlock + kCellPad; }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(AnalyserBase()); }
int64_t Off(const void* inside) { return static_cast<int64_t>(reinterpret_cast<const unsigned char*>(inside) - AnalyserBase()); }

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorldPtr;
}

CMap* CWorldSavvy::GetMap() const {
    ev("GetMap", {lab(this)});
    return reinterpret_cast<CMap*>(Cells());
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head", 0));
    return result;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next", 0));
    return result;
}

int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("inside", 0));
}

bool CParkAnalyser::GatesOpen() const {
    ev("GatesOpen", {lab(this)});
    return ret("gates", 0) != 0;
}

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("GetTicketBoothPos", {lab(this), which});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("booth", 0));
    return result;
}

TbTimeDiff CParkTime::GetFunnyTimeForGameTurns(uint32_t turns) const {
    ev("GetFunnyTimeForGameTurns", {lab(this), turns});
    TbTimeDiff span;
    span.mTicks = ret("ticks", 0);
    return span;
}

template <typename T>
T CParkAnalyserVal<T>::Get() const {
    ev("ValGet", {Off(this)});
    return static_cast<T>(ret("get", 0));
}
template <typename T>
void CParkAnalyserVal<T>::Set(T value) {
    ev("ValSet", {Off(this), static_cast<int64_t>(value)});
}
template int CParkAnalyserVal<int>::Get() const;
template void CParkAnalyserVal<int>::Set(int);

template <class T, int N>
void CHistory<T, N>::SetAtEnd(T value) {
    ev("HistSetAtEnd", {Off(this), static_cast<int64_t>(value)});
}
template <class T, int N>
void CHistory<T, N>::GetAverageOfLast(T& out, int32_t count) const {
    ev("HistAverage", {Off(this), count});
    out = static_cast<T>(ret("avg", 0));
}
template <class T, int N>
int32_t CHistory<T, N>::HowFarBack() const {
    ev("HistHowFarBack", {Off(this)});
    return static_cast<int32_t>(ret("far", 0));
}
template <class T, int N>
bool CHistory<T, N>::GetValue(T& out, int32_t back) const {
    ev("HistGetValue", {Off(this), back});
    out = static_cast<T>(ret("hv", 0));
    return ret("hvok", 0) != 0;
}
template void CHistory<unsigned char, 50>::SetAtEnd(unsigned char);
template void CHistory<unsigned char, 50>::GetAverageOfLast(unsigned char&, int32_t) const;
template int32_t CHistory<unsigned char, 50>::HowFarBack() const;
template void CHistory<int, 50>::SetAtEnd(int);
template bool CHistory<int, 144>::GetValue(int&, int32_t) const;

// ---- the entries of the test
extern "C" {
void* tpw_pv_addr(int32_t which) {
    switch (which) {
        case 0: return AnalyserBase();
        case 1: return gTail;
        case 2: return Cells();
        case 3: return gCellBlock;
        default: return gThings[which - 10];   // 10 .. 17
    }
}
// mode 0: the world is the tail's block, mode 1: the world is the cells' block
void* tpw_pv_mode(int32_t mode) {
    unsigned char* base = mode == 0 ? gTail - kTail : Cells() - 0x2d8;
    gWorldPtr = reinterpret_cast<CWorld*>(base);
    gGameWorld = gWorldPtr;
    return base;
}
void tpw_pv_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
int32_t tpw_pv_call(int32_t which, int32_t a, int32_t b) {
    CParkAnalyser* p = Analyser();
    const CParkAnalyser* c = p;
    switch (which) {
        case 0: p->OfferOpinionOn(a, static_cast<uint8_t>(b)); return 0;
        case 1: p->IveMisbehaved(a); return 0;
        case 2: p->PersonLeftPark(a); return 0;
        case 3: return c->GetVisitorsInPark(static_cast<uint32_t>(a));
        case 4: return c->NumberOfVisitorsOutsidePark();
        case 5: return c->NumberOfVisitorsNeedingToilet(static_cast<uint8_t>(a));
        case 6: return c->NumberOfHungryVisitors(static_cast<uint8_t>(a));
        case 7: return c->NumberOfThirstyVisitors(static_cast<uint8_t>(a));
        case 8: return c->MisbehavingVisitorsLastMonth(a != 0);
        case 9: return c->NumberOfVisitorsSinceBeginning();
        case 10: return c->NumberOfVisitorsInLastMonths(a);
        case 11: return c->NumberOfVisitorsWithBalloons();
        case 12: return c->NumberOfVisitorsWithCostumes();
        case 13: return c->PercentageOfVisitorsWithHappinessOver(a);
        case 14: return c->PercentageOfVisitorsWithHappinessUnder(a);
        case 15: return c->GetAverageOpinionOn(a);
        case 16: return c->HaveCompleteSetOfOpinions(a) ? 1 : 0;
        default: {
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
            cell.mIndex = static_cast<uint16_t>(a);
            return c->GetAverageHappinessAroundPoint(cell, static_cast<uint32_t>(b));
        }
    }
}
float tpw_pv_happy(uint32_t arg) { return Analyser()->GetHappinessLevel(arg); }
float tpw_pv_shim(int32_t which) { return CParkAnalyser_GetHappinessLevel(Analyser(), which); }
}
