// The constructors of the things a new level makes (unit w122a, main app): CThing::CThing (Thing.cpp) and the constructors of CStaffHQ,
// CMechanicHQ, CAdvisor, CTagSystem, CResearchLab, CWeather and CUIMsgReceiver. The six leaf constructors they build their parts with
// (CTrainingCentre, CWeatherForecast, CStaffHQ::StrikeType, AdvisorHistory (host: AdvisorMessageState, written in CAdvisor's loop),
// CResearchProject, CResearchCategory) are inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every derived constructor builds its CThing first (the host holds it as the first member, which is constructed before the others) with
// map cell id 1 and the model of its class, then its own fields, then subscribes to the message centre. The Mac's constructors leave most
// bytes of the object alone (a new object keeps whatever the allocator gave); so do these, and no member has an initialiser.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "tb_rain.h"

namespace {

// The cell tag every derived constructor passes: map cell id 1 (cell x 0, y 0), built in the high half of r4 on the Mac.
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> FirstCell() { return {1}; }

// The block the Mac repeats before EACH Subscribe call (not once): the message centre (global 0x1037f6c4) is created when it is NULL. A
// NULL answer of operator new is stored (the centre stays NULL, no headers are built) and Subscribe then runs with a NULL centre; the
// next block tries again. The 29 headers of 0x10 bytes are built by __construct_array on the Mac and by the member array here.
CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

void Subscribe(CThing& thing, int32_t type) { MessageCentreForSubscribe()->Subscribe(thing, static_cast<MsgType>(type)); }

}  // namespace

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

// 0x1012a7fc (304 bytes, StaffHQ.cpp), model 9. mTraining (0x1012ffb0) and mStrikes (0x1012a950) are built by their own constructors.
CStaffHQ::CStaffHQ() : mBase(FirstCell(), static_cast<CThing::ThingModel>(9)) {
    mF60 = 0;
    mF64 = 0;
    Subscribe(mBase, 0xc);
    Subscribe(mBase, 0xe);
}

// 0x1010521c (176 bytes, MechanicHQ.cpp), model 10.
CMechanicHQ::CMechanicHQ() : mBase(FirstCell(), static_cast<CThing::ThingModel>(10)) {
    mF0C = 0;
    Subscribe(mBase, 7);
}

// 0x10009f9c (172 bytes, Advisor.cpp), model 0xb. The eight requests of the queue (0x1000a0b4, the valid byte) and mLastTutorialRequest
// (the inlined store of +0x16e4) are built by CAdvisorRequest's constructor. mPlayStartTime (+0xd8) is written by Init only.
CAdvisor::CAdvisor() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xb)) {
    std::memset(mF0C, 0, sizeof mF0C);
    std::memset(mFD4, 0, sizeof mFD4);
    mPlayDuration = 0;
    // AdvisorHistory::AdvisorHistory (0x1000a06c) for each of the 0x15f records: +0, +4 (-1), the byte +8 and +0xc; +9 to +0xb stay.
    for (AdvisorMessageState& state : mMessageState) {
        state.mLastSaid.mTimeSig = 0;
        state.mResponseIndex = -1;
        state.mSaidOnce = 0;
        state.mKillCount = 0;
    }
    Init();
}

// 0x1012c57c (84 bytes, Thing.cpp), model 0xc: nothing of its own but Init (which only subscribes).
CTagSystem::CTagSystem() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xc)) { Init(); }

// 0x10121ad0 (128 bytes, ResearchLab.cpp), model 0xe: the 256 projects (0x10123abc, empty) and the five categories (0x101242a0).
CResearchLab::CResearchLab() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xe)) {}

// 0x10137da0 (376 bytes, World.cpp), model 0xf. The wind direction is first cleared (the constant at 0x102653a0 is 0.0f) and set to 90
// degrees after the subscriptions; GameLoad of the global rain runs without a NULL test, as on the Mac.
CWeather::CWeather() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xf)) {
    mF14 = 0;
    mF10 = 0;
    mF1C = 0;
    mF18 = 0;
    mWindDirection = 0.0f;
    Subscribe(mBase, 1);
    Subscribe(mBase, 2);
    mF0C = FindNewQuality();
    mF64 = 0;
    mF34 = 0;
    mF30 = 0;
    mF38 = 0;
    mF44 = 0;
    mF3C = 0;
    mF40 = 0;
    mF54 = 0;
    mWindDirection = 90.0f;
    mWindForce = 0.5f;
    gei_Rain->GameLoad();
}

// 0x101305b0 (756 bytes, vector2d.cpp by the inventory), model 0x11: only the eight subscriptions.
CUIMsgReceiver::CUIMsgReceiver() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0x11)) {
    Subscribe(mBase, 0x12);
    Subscribe(mBase, 0xa);
    Subscribe(mBase, 7);
    Subscribe(mBase, 8);
    Subscribe(mBase, 0x1b);
    Subscribe(mBase, 0xf);
    Subscribe(mBase, 0x13);
    Subscribe(mBase, 0x1c);
}
