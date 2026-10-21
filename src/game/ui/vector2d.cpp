// vector2d.cpp: name proven by __sinit_vector2d_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "helpers/ThingHelpers.h"
#include "MapCell.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "tb_rain.h"
#include "TbDynamicStringTemplate.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "vector2d.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// The constructors of the things a new level makes (unit w122a, main app): CThing::CThing (Thing.cpp) and the constructors of CStaffHQ,
// CMechanicHQ, CAdvisor, CTagSystem, CResearchLab, CWeather and CUIMsgReceiver. The six leaf constructors they build their parts with
// (CTrainingCentre, CWeatherForecast, CStaffHQ::StrikeType, AdvisorHistory (host: AdvisorMessageState, written in CAdvisor's loop),
// CResearchProject, CResearchCategory) are inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every derived constructor builds its CThing first (the host holds it as the first member, which is constructed before the others) with
// map cell id 1 and the model of its class, then its own fields, then subscribes to the message centre. The Mac's constructors leave most
// bytes of the object alone (a new object keeps whatever the allocator gave); so do these, and no member has an initialiser.

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

// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.

uint32_t gVector2dSinitHigh;        // 0x1034c700
uint32_t gVector2dSinitLow;         // 0x1034c704
void CUIMsgReceiver::FinalDestroy() { mBase.FinalDestroy(); }

int32_t CUIMsgReceiver::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return mBase.Serialise(file, save, version, unused) != 0 ? 1 : 0;
}

// 0x10130998 (364 bytes): the message's type (virtual slot 2) picks a UIInterface call through a jump table over types 7 to 28; the
// other types do nothing. The Mac reads the word at +8 as a uint16 id (types 7, 8, 10, 15, 27, 28) or as an int (types 18, 19); the
// host reads them through the message classes that have those fields.
void CUIMsgReceiver::ReceiveMessage(const CMsg& message) {
    UIInterface* ui = &gUIInterface;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    switch (message.GetType()) {
        case 7:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->RideBrokenDown(id);
            break;
        case 8:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->RideCondemned(id);
            break;
        case 10:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->RideDeleted(id);
            break;
        case 15: {
            const CMsgStaffRoomEvent& m = static_cast<const CMsgStaffRoomEvent&>(message);
            id.mIndex = m.mThingId;
            ui->StaffRoomEvent(id, m.mArriving);
            break;
        }
        case 18:
            ui->ResearchCategoryComplete(static_cast<const CMsgResearchCategoryUpdated&>(message).mCategory);
            break;
        case 19: {
            int32_t value = static_cast<int32_t>(static_cast<const CMsgResearchCategoryUpdated&>(message).mCategory);
            if (value == 5) {
                ui->EnableResearchButton(true);
            } else if (value == 6) {
                ui->EnableResearchButton(false);
            }
            break;
        }
        case 27:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->ThingDeath(id);
            break;
        case 28:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->KidArrived(id);
            break;
        default:
            break;
    }
}

// vector2d.cpp (decompiled from SimTheme Park 2.04b3, functions at 0x10130b40..0x10130e8c).
// Arithmetic is 32-bit two's complement wrapping, as the original PowerPC code: the
// project builds with -fwrapv so signed overflow is defined and matches.

Vector2d g_vector2d_xaxis(0x10000, 0);

uint32_t xaxis_circle_intersect(Vector2d& p, Fixed r) {
    int32_t x = p.x;
    int32_t y = p.y;
    // (x >> 18)^2 + (y >> 18)^2 : arithmetic shifts, plain 32-bit multiplies
    int32_t a = (x >> 18) * (x >> 18) + (y >> 18) * (y >> 18);
    int32_t c = a - FixMul(r, r);
    int32_t minus2x = x * -2;
    int32_t disc = FixMul(minus2x, minus2x) - c * 4;
    if (disc <= 0) return 0x80000000u;
    uint32_t s = isqrt(static_cast<uint32_t>(disc));
    return static_cast<uint32_t>(x * 2 - static_cast<int32_t>(s)) >> 1;
}

int32_t line_circle_intersect(Vector2d& p1, Vector2d& d, Vector2d& centre, Fixed r) {
    Vector2d f = p1 - centre;
    int32_t a = FixMul(d.y, d.y) + FixMul(d.x, d.x);
    int32_t b = (FixMul(d.y, f.y) + FixMul(d.x, f.x)) * 2;
    int32_t c = (FixMul(f.y, f.y) + FixMul(f.x, f.x)) - FixMul(r, r);
    int32_t disc = FixMul(b, b) - FixMul(a, c) * 4;
    if (disc <= 0) return INT32_MIN;
    int32_t s = static_cast<int32_t>(isqrt(static_cast<uint32_t>(disc)));
    int32_t root1 = s - b;
    int32_t root2 = -(b + s);
    return ((root2 > root1) ? root1 : root2) >> 1;
}

Vector2d* Vector2d_Dtor(Vector2d* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

void __sinit_vector2d_cpp() {
    gVector2dSinitLow = 0;
    gVector2dSinitHigh = 0x10000;
}
