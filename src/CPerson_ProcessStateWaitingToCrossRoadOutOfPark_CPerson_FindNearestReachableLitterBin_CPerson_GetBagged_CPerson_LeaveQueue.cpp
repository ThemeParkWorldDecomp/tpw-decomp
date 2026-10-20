// Unit v5954: the person's walk through the ticket booth: CPerson::LeaveQueue, ProcessStatePayingAtBooth, ProcessStateWaitingForParkToOpen,
// ProcessStateWalkingToTicketBooths and FeelingAboutEntryFee. The person is reached by Mac offset.
#include <cstdint>
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "ppc_math.h"

// 0x102a0d58: the third halfword of the person sprite table (the header declares gPersonTable with two; the Mac reads the one after them
// when the bus is coming).
uint16_t gPersonTableBusSprite = 0x32;

namespace {

// The texts at the TOC base 0x10244aa8 (+0x43, +0x6c, +0xb4, +0xc4, +0xea, +0x10a, +0x12e, +0x14e).
const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kBigProblem[] = "Big problem...no way this should happen\n";
const char kLonghand[] = "Working out how exciting park is longhand - should do this in Analyser\n";
const char kGpAf[] = "GP: %d, AF: %d\n";
const char kBusComing[] = "The bus is coming!  RUUUUUUUUUUN!!!!\n";
const char kTooExpensive[] = "Person: park far too expensive\n";
const char kText10a[] = "Person: park on the expensive side\n";
const char kText12e[] = "Person: park on the cheap side\n";
const char kText14e[] = "Person: your admission fee is about right\n";

template <class T>
inline T& At(void* base, uint32_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

inline uint16_t CellOf(void* person) {
    uint32_t x = At<uint8_t>(person, 4);
    uint32_t y = At<uint8_t>(person, 6);
    return static_cast<uint16_t>(x + (y << 7) + 1);
}

}  // namespace

// 0x1011dbf4
void CPerson::LeaveQueue() {
    uint16_t ride = At<uint16_t>(this, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    CObject* object = static_cast<CObject*>(gThingArray[ride].mThing);
    auto id = reinterpret_cast<CThing*>(this)->GetThingId();
    object->RemovePersonFromQueue(id);
    DismissFromQueue();
    SetState(static_cast<PersonState>(6));
}

// 0x1011c63c
int32_t CPerson::FeelingAboutEntryFee() {
    CTRACE(2, kLonghand);
    CParkAnalyser* analyser = gWorld->GetParkAnalyser();
    uint32_t points = static_cast<uint32_t>(analyser->GetTotalParkAttractionPoints());
    uint32_t base = static_cast<uint32_t>(gMainBalance.mValues[13].mInteger);   // 0x3c
    uint32_t rnd = gWorld->GetRand();
    uint32_t modulus = (base >> 1) + 1;
    uint32_t remainder = rnd - (rnd / modulus) * modulus;
    uint32_t divisor = base + remainder;
    uint32_t quotient = divisor == 0 ? points : points / divisor;   // divwu by 0 is undefined on the hardware; the emulator answers the dividend
    CBankAccount* bank = gWorld->GetBankAccount();
    uint32_t balance = At<uint32_t>(bank, 0x118);
    uint32_t limit = static_cast<uint32_t>(gMainBalance.mValues[14].mInteger);   // 0x40
    uint32_t feeling = quotient + limit;
    CTRACE(2, kGpAf, static_cast<int32_t>(feeling), static_cast<int32_t>(balance));
    if (!(feeling > limit || balance > limit)) return 3;
    float fFeeling = static_cast<float>(feeling);
    float fBalance = static_cast<float>(balance);
    float c44 = gMainBalance.mValues[15].mFloat;
    float c48 = gMainBalance.mValues[16].mFloat;
    float c4c = gMainBalance.mValues[17].mFloat;
    float product = fFeeling * c48;
    if (fBalance >= product) {
        float product2 = fFeeling * c4c;
        if (fBalance >= product2) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[33].mInteger));   // 0x8c
            return 0;
        }
        gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[34].mInteger));   // 0x90
        return 1;
    }
    float product3 = fFeeling * c44;
    if (fBalance <= product3) {
        gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[35].mInteger));   // 0x94
        return 2;
    }
    gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[36].mInteger));   // 0x98
    return 3;
}

// 0x1011c88c
void CPerson::ProcessStateWalkingToTicketBooths() {
    bool bus = false;
    if (gWorld->ArrivalVehicle_IsSeagoing() == 0) {
        if (gWorld->ArrivalVehicle_GetStatus() == 3) {
            CTRACE(2, kBusComing);
            At<uint16_t>(this, 0xc2) = gPersonTableBusSprite;
            bus = true;
        }
    }
    if (!bus) {
        auto id = reinterpret_cast<CThing*>(this)->GetThingId();
        if ((id.mIndex & 3) == 0) At<uint16_t>(this, 0xc2) = gPersonTable[1];
        else At<uint16_t>(this, 0xc2) = gPersonTable[0];
    }
    int32_t walk = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walk == 0) {
        if (gWorld->IsParkClosed() != 0) SetState(static_cast<PersonState>(3));
        else SetState(static_cast<PersonState>(4));
    } else if (walk == 2) {
        GAME_ASSERTMSG(0, kBigProblem);
    }
}

// 0x1011c9f8
void CPerson::ProcessStateWaitingForParkToOpen() {
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    if (gWorld->IsParkClosed() == 0 && gWorld->ParkGates_GetStatus() == 1) {
        if (At<uint32_t>(this, 0x204) == 0) {
            CMap* map = gWorldMap;
            uint16_t booth = map->GetTicketBoothPos(0).mIndex;
            if (CellOf(this) == booth) {
                peep->SetDest(map->GetTicketBoothPos(0));
            } else {
                uint16_t booth1 = map->GetTicketBoothPos(1).mIndex;
                if (CellOf(this) == booth1) {
                    peep->SetDest(map->GetTicketBoothPos(1));
                } else {
                    uint32_t rnd = gWorld->GetRand();
                    peep->SetDest(map->GetTicketBoothPos(static_cast<int32_t>(rnd & 1)));
                }
            }
            At<uint32_t>(this, 0x200) = 0;
            SetState(static_cast<PersonState>(2));
        } else {
            int32_t cell = CellOf(this);
            uint16_t who = At<uint16_t>(gWorldMap, static_cast<uint32_t>((cell - 1) * 0x44 + 0x24));
            auto id = reinterpret_cast<CThing*>(this)->GetThingId();
            if (who == id.mIndex) {
                uint32_t rnd = gWorld->GetRand();
                peep->SetDest(gWorldMap->GetParkEntrancePos(static_cast<int32_t>(rnd & 1)));
                SetState(static_cast<PersonState>(5));
            }
        }
        return;
    }
    if (At<uint32_t>(this, 0x200) == 0) {
        At<int32_t>(this, 0x188) = 1;
        uint32_t rnd = gWorld->GetRand();
        peep->SetDest(gWorldMap->GetCrossingParkSidePos(static_cast<int32_t>(rnd & 1)));
        SetState(static_cast<PersonState>(0x12));
        At<uint32_t>(this, 0x1bc) = 0;
    } else {
        At<uint32_t>(this, 0x200) = At<uint32_t>(this, 0x200) - 1;
    }
}

// 0x1011cce4
void CPerson::ProcessStatePayingAtBooth() {
    uint32_t wait = At<uint32_t>(this, 0x200);
    if (wait != 0) {
        At<uint32_t>(this, 0x200) = wait - 1;
        return;
    }
    int32_t feeling = FeelingAboutEntryFee();
    bool pay = false;
    if (feeling == 2) {
        CTRACE(2, kText12e);
        RaiseHappiness(static_cast<Amount>(1));
        pay = true;
    } else if (feeling < 2) {
        if (feeling == 0) {
            CTRACE(2, kTooExpensive);
            At<int32_t>(this, 0x188) = 1;
            CMap* map = gWorldMap;
            uint32_t rnd = gWorld->GetRand();
            reinterpret_cast<CPeep*>(this)->SetDest(map->GetCrossingParkSidePos(static_cast<int32_t>(rnd & 1)));
            reinterpret_cast<CThoughts*>(reinterpret_cast<uint8_t*>(this) + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
            SetState(static_cast<PersonState>(0x12));
            At<uint32_t>(this, 0x1bc) = 0;
            return;
        }
        if (feeling < 0) return;
        CTRACE(2, kText10a);
        uint32_t rnd = gWorld->GetRand();
        At<uint32_t>(this, 0x200) = rnd % 0x32 + 0x32;
        reinterpret_cast<CThoughts*>(reinterpret_cast<uint8_t*>(this) + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
        LowerHappiness(static_cast<Amount>(1));
        int32_t happiness = ppc_fctiwz(static_cast<double>(At<float>(this, 0x19c)));
        if ((static_cast<uint32_t>(happiness) & 0xffu) != 0) return;
        SetState(static_cast<PersonState>(0x12));
        At<uint32_t>(this, 0x1bc) = 0;
        return;
    } else if (feeling >= 4) {
        return;
    }
    (void)pay;
    CTRACE(2, kText14e);
    gWorld->GetBankAccount()->PayAdmission();
    At<uint32_t>(this, 0x204) = 1;
    SetState(static_cast<PersonState>(3));
}
