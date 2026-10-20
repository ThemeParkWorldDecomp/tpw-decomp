#include <cstdint>
#include <cstring>
#include <new>
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ppc_math.h"
#include "CMessageCentre.h"
#include "LbSCBack.h"

// Unit v5556: CPeep::CPeep 0x10112e18 (432 bytes). The peep is reached by offset (the class has no members in the header).
namespace {
// The halfwords at 0x102a0d5a (initialised data): the peep's first sprite, picked by GetRand() % 5.
const uint16_t kPeepSpriteTable[5] = {60, 80, 100, 120, 140};

template <class T>
T& At(uint8_t* self, uint32_t offset) {
    return *reinterpret_cast<T*>(self + offset);
}
}  // namespace

#include "CSubscriptionHeader.h"
// The runtime's array constructor, defined weakly for the whole-program build (the unit test library supplies its own recording fake).
// The host passes no constructor: the elements here are the message centre's subscription headers.
// Each header's constructor builds an empty tree (count 0, root null), done here as zeroed bytes.
extern "C" __attribute__((weak)) void __construct_array(void* array, void*, void*, int32_t size, int32_t count) {
    std::memset(array, 0, static_cast<size_t>(size) * static_cast<size_t>(count));
}

CPeep::CPeep(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    ::new (static_cast<void*>(self)) CThing(cell, model);
    ::new (static_cast<void*>(self + 0xc)) CDelayedScript();
    At<uint32_t>(self, 0x1c) = 0;
    At<uint32_t>(self, 0x20) = 0;
    At<uint32_t>(self, 0x24) = 0;
    At<uint32_t>(self, 0x28) = 0;
    At<uint8_t>(self, 0x2c) = 0;
    ::new (static_cast<void*>(self + 0x30)) CThoughts();
    At<uint16_t>(self, 0xc0) = 0;
    At<uint16_t>(self, 0xc2) = 0;
    At<uint16_t>(self, 0xc4) = 0;
    At<float>(self, 0xc8) = 0.0f;
    At<uint16_t>(self, 0xcc) = 0;
    At<uint32_t>(self, 0xd0) = 0;
    ::new (static_cast<void*>(self + 0xd4)) Pilot();
    At<uint32_t>(self, 0x190) = 0;
    At<uint32_t>(self, 0x194) = 0;
    At<uint32_t>(self, 0x198) = 0;
    uint32_t rand = gWorld->GetRand();
    At<uint16_t>(self, 0xc0) = kPeepSpriteTable[rand % 5];
    At<uint16_t>(self, 0xc2) = gPersonTable[0];
    if (gMessageCentre == nullptr) {
        CMessageCentre* centre = static_cast<CMessageCentre*>(::operator new(0x1d0));
        __construct_array(centre, nullptr, nullptr, 0x10, 0x1d);
        gMessageCentre = centre;
    }
    gMessageCentre->Subscribe(*reinterpret_cast<CThing*>(this), static_cast<MsgType>(10));
    At<uint16_t>(self, 0xcc) = cell.mIndex;
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1u;
    Pilot* pilot = reinterpret_cast<Pilot*>(self + 0xd4);
    pilot->reset(static_cast<int32_t>(((v << 16) & 0x007f0000u) + 0x8000u),
                 static_cast<int32_t>(((v << 9) & 0xffff0000u) + 0x8000u), 0, 0);
    pilot->get_pos(At<int32_t>(self, 0x190), At<int32_t>(self, 0x194));
    At<uint32_t>(self, 0x28) = 0;
    At<uint32_t>(self, 0xd0) = 0;
    At<uint32_t>(self, 0x24) = 0;
}
