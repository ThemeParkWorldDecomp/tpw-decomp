// Unit v5281: CPeep::PlaySound, IsInsidePark, Teleport, UpdateTurn, IsStranded, SetStranded and the two SetDest (Peep.cpp).
// The peep is reached by offset: cell x/y bytes at +4/+6, destination halfwords at +0x18/+0x1a, script at +0xc, three turn halfwords at
// +0xc0/+0xc2/+0xc4, a float at +0xc8, the Pilot at +0xd4, two positions at +0x190/+0x194, the stranded stamp at +0x198.
#include <cstdint>
#include <cstring>

#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ppc_math.h"

// 0x102a0e94: the null cell id (a halfword; zero at start).
uint16_t gPeepNullCellId;

namespace {

uint8_t* Bytes(void* p) { return static_cast<uint8_t*>(p); }
const uint8_t* Bytes(const void* p) { return static_cast<const uint8_t*>(p); }
uint16_t& Half(void* p, std::size_t off) { return *reinterpret_cast<uint16_t*>(Bytes(p) + off); }
uint32_t& Word(void* p, std::size_t off) { return *reinterpret_cast<uint32_t*>(Bytes(p) + off); }
Pilot* PilotOf(void* p) { return reinterpret_cast<Pilot*>(Bytes(p) + 0xd4); }
// A MapPos as the Mac's halfword: the cell byte high, the offset byte low (the original reads the pair as one big-endian halfword).
int32_t PosHalf(MapPos p) { return static_cast<int16_t>((p.mCell << 8) | p.mOffset); }
// The halfword at 0x102a0d5a + 4 (the third entry of {60, 80, 100, 120, 140}).
constexpr uint16_t kTurnDivisor = 100;

}  // namespace

// 0x10115b80: sends a sound event at the script position of the peep.
void CPeep::PlaySound(int32_t category, int32_t sound) {
    float x, y, z;
    int32_t mode;
    SCR_GetScriptPosition(static_cast<int32_t>(Word(this, 0xc)), &x, &y, &z, &mode);
    SOUND_SendEvent(0, category, sound, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
}

// 0x10115a80: true on a path, queue, entrance, exit or grass cell.
int32_t CPeep::IsInsidePark() const {
    const uint8_t* self = Bytes(this);
    uint16_t cell = static_cast<uint16_t>(self[4] + (self[6] << 7) + 1);
    CMap* map = reinterpret_cast<const CWorldSavvy*>(this)->GetMap();
    const CMapCell* mc = reinterpret_cast<const CMapCell*>(Bytes(map) + (static_cast<int32_t>(cell) - 1) * 0x44);
    if (mc->IsPath() || mc->IsQueue() || mc->IsEntrance() || mc->IsExit() || mc->IsGrass()) return 1;
    return 0;
}

// 0x101159d0
void CPeep::Teleport(MapPos x, MapPos y) {
    PilotOf(this)->reset(PosHalf(x) << 8, PosHalf(y) << 8, 0, 0);
    PilotOf(this)->get_pos(reinterpret_cast<int32_t&>(Word(this, 0x190)), reinterpret_cast<int32_t&>(Word(this, 0x194)));
    reinterpret_cast<CThing*>(this)->Move(x, y);
}

// 0x101158c8
void CPeep::UpdateTurn() {
    uint32_t sum = static_cast<uint32_t>(Half(this, 0xc0)) + Half(this, 0xc2) + Half(this, 0xc4);
    float num = static_cast<float>(static_cast<double>(static_cast<int32_t>(sum)));
    float den = static_cast<float>(static_cast<double>(kTurnDivisor));
    float quotient = num / den;
    float speed = *reinterpret_cast<float*>(Bytes(this) + 0xc8);
    const float three = 3.0f;
    const float quarter = 0.25f;
    float result = ppc_fmadds(three, speed, quotient);
    result = result * quarter;
    *reinterpret_cast<float*>(Bytes(this) + 0xc8) = result;
    PilotOf(this)->set_speed(result);
    Half(this, 0xc4) = static_cast<uint16_t>(Half(this, 0xc4) * 99);
    Half(this, 0xc4) = static_cast<uint16_t>(Half(this, 0xc4) / 100u);
    PilotOf(this)->get_pos(reinterpret_cast<int32_t&>(Word(this, 0x190)), reinterpret_cast<int32_t&>(Word(this, 0x194)));
    reinterpret_cast<CDelayedScript*>(Bytes(this) + 0xc)->ProcessSpriteScripts();
}

// 0x1011566c: true while the stranded stamp is set (a stamp from the future is cleared).
bool CPeep::IsStranded() {
    uint32_t now = reinterpret_cast<CMap*>(Bytes(gWorld) + 0x2d8)->MakeNewTimeStamp();
    if (now < Word(this, 0x198)) Word(this, 0x198) = 0;
    return Word(this, 0x198) != 0;
}

// 0x101155bc
void CPeep::SetStranded(int32_t stranded) {
    if (stranded == 0) {
        Word(this, 0x198) = 0;
    } else {
        Word(this, 0x198) = reinterpret_cast<CMap*>(Bytes(gWorld) + 0x2d8)->MakeNewTimeStamp();
        tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = reinterpret_cast<CThing*>(this)->GetThingId();
        CTRACE(2, "Peep %d: stranded at time %d\n", static_cast<uint32_t>(id.mIndex), Word(this, 0x198));
    }
}

// 0x101154f0: walks to a position unless stranded with the local area unchanged.
int32_t CPeep::SetDest(MapPos x, MapPos y) {
    if (IsStranded() && IsLocalAreaUnchangedSinceBecomingStranded()) return 0;
    SetStranded(0);
    Half(this, 0x18) = static_cast<uint16_t>(PosHalf(x));
    Half(this, 0x1a) = static_cast<uint16_t>(PosHalf(y));
    PilotOf(this)->new_target(PosHalf(x) << 8, PosHalf(y) << 8);
    // The original answers what new_target leaves in r3; the header declares it void, so 1 is answered.
    return 1;
}

// 0x10115400: walks to a map cell.
int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    GAME_ASSERTMSG(gPeepNullCellId != cell.mIndex, "Trying to SetDest to IDNULL\n");
    int32_t t = static_cast<int32_t>(cell.mIndex) - 1;
    MapPos x, y;
    x.mCell = static_cast<uint8_t>(t & 0x7f);
    x.mOffset = 0x80;
    y.mCell = static_cast<uint8_t>(t >> 7);
    y.mOffset = 0x80;
    return SetDest(x, y);
}
