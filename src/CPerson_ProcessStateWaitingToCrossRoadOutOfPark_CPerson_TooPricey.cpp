// CPerson::TooPricey and CPerson::ExtraExcitementEffects (unit v5238). The person and the ride record are reached by Mac offset.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "MainBalance.h"
#include "ObjectControl.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

template <class T> T& At(void* base, size_t offset) { return *reinterpret_cast<T*>(static_cast<char*>(base) + offset); }
template <class T> T At(const void* base, size_t offset) { return *reinterpret_cast<const T*>(static_cast<const char*>(base) + offset); }

uint32_t Low8(float value) { return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xffu; }

// mulhw by 0x51eb851f, srawi 5, plus the sign bit: a signed divide by 100
int32_t Div100(int32_t x) {
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(static_cast<int32_t>(0x51eb851f)) * x) >> 32) >> 5;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

// mulhwu by 0x51eb851f, srwi 5: an unsigned divide by 100
uint32_t UDiv100(uint32_t x) { return static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851fu) * x) >> 32) >> 5; }

// min(100, max(0, field + amount)) with the compares of the listing (fcmpo: a NaN is neither above nor below)
void AddClamped(float& field, int32_t amount) {
    float old = field;
    float sum = static_cast<float>(amount) + old;
    if (sum > 100.0f) {
        field = 100.0f;
    } else if (sum < 0.0f) {
        field = 0.0f;
    } else {
        field = sum;
    }
}

void Offer(int32_t opinion, uint32_t value) {
    if (value >= 0x64) value = 0x64;
    gGameWorld->GetParkAnalyser()->OfferOpinionOn(opinion, static_cast<uint8_t>(value));
}

}  // namespace

// 0x10119c0c
void CPerson::ExtraExcitementEffects(const CObject* object) {
    char* balance = reinterpret_cast<char*>(&gMainBalance);
    if ((object->GetExcitementLevel(false) & 0xff) == 0) return;
    int32_t preferred = GetPreferredExcitement() & 0xff;
    int32_t diff = preferred - (object->GetExcitementLevel(false) & 0xff);
    if (diff < 0) diff = -diff;
    float& happy = At<float>(this, 0x19c);
    if (diff < 5) {
        AddClamped(happy, At<int32_t>(static_cast<const void*>(balance), 0x24));
    } else if (diff < 0xf) {
        AddClamped(happy, At<int32_t>(static_cast<const void*>(balance), 0x28));
    } else if (diff < 0x28) {
        AddClamped(happy, At<int32_t>(static_cast<const void*>(balance), 0x2c));
    }
    int32_t divisor = At<int32_t>(static_cast<const void*>(balance), 0x30);
    int32_t level = object->GetExcitementLevel(false) & 0xff;
    int32_t quotient = level / divisor;
    int32_t p = 100 - static_cast<int32_t>(Low8(At<float>(this, 0x1a8)));
    // mulhw by 0x66666667, srawi 3, plus the sign bit: a signed divide by 20
    int32_t q20 = static_cast<int32_t>((static_cast<int64_t>(0x66666667) * p) >> 32) >> 3;
    q20 += static_cast<int32_t>(static_cast<uint32_t>(q20) >> 31);
    AddClamped(At<float>(this, 0x1b0), quotient * q20);
}

// 0x10119eec
bool CPerson::TooPricey(CObject* object) const {
    uint32_t price = object->GetPricePerUse();
    if (price == 0) return false;
    const char* db = reinterpret_cast<const char*>(object->GetDBInfo());
    int32_t base = Div100(At<int32_t>(static_cast<const void*>(db), 0x140) * 0x73);
    uint32_t a = Low8(At<float>(this, 0x19c));
    uint32_t b = Low8(At<float>(this, 0x1b0));
    uint32_t c = Low8(At<float>(this, 0x1a4));
    uint32_t d = Low8(At<float>(this, 0x1a8));
    int32_t termA = Div100(At<int32_t>(static_cast<const void*>(db), 0x150) * static_cast<int32_t>(100 - a));
    int32_t termB = Div100(At<int32_t>(static_cast<const void*>(db), 0x14c) * static_cast<int32_t>(b));
    int32_t termC = Div100(At<int32_t>(static_cast<const void*>(db), 0x144) * static_cast<int32_t>(c));
    int32_t termD = Div100(At<int32_t>(static_cast<const void*>(db), 0x148) * static_cast<int32_t>(d));
    int32_t factor = termA + (termC + termD + 100 - termB);
    uint32_t value = UDiv100(static_cast<uint32_t>(base * factor));
    (void)object->GetPricePerUse();
    uint32_t chance = object->GetChanceOfWinning() & 0xff;
    uint32_t prize = 0;
    if (At<int32_t>(static_cast<const void*>(object->GetDBInfo()), 0x7a8) == 2) prize = object->GetPrizeValue();
    ObjectControl* control = object->GetControl();
    int32_t winnings = Div100(static_cast<int32_t>(chance * prize));
    uint32_t controlFactor = static_cast<uint32_t>(control->mRipOffOK + 100);
    uint32_t happyFactor = Low8(At<float>(this, 0x19c)) + 100;
    value = (value + static_cast<uint32_t>(winnings)) * controlFactor;
    value = UDiv100(value);
    value = UDiv100(value * happyFactor);

    uint32_t opinion = (value - price) * 10 + 0x32;
    int32_t type = At<int32_t>(static_cast<const void*>(object->GetDBInfo()), 0x7a8);
    if (type == 2) {
        Offer(9, opinion);
    } else if (type == 1) {
        switch (At<int32_t>(static_cast<const void*>(db), 0x158)) {
            case 1: Offer(1, opinion); break;
            case 2: Offer(2, opinion); break;
            case 3: Offer(3, opinion); break;
            case 4: Offer(4, opinion); break;
            default: break;
        }
        switch (At<int32_t>(static_cast<const void*>(db), 0x15c)) {
            case 1: Offer(6, opinion); break;
            case 2: Offer(7, opinion); break;
            default: break;
        }
    }
    if (price > value) return true;
    return At<uint32_t>(this, 0x1a0) < price;
}
