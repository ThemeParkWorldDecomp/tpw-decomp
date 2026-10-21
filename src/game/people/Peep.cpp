// Peep.cpp: name proven by __sinit_Peep_cpp in the main application.

#include "analyser.h"
#include "CellBase.h"
#include "CMessageCentre.h"
#include "CPeep_SAVEVAR_2.h"
#include "CSubscriptionHeader.h"
#include "DateMessages.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "ESP_sprites.h"
#include "helpers/PeepHelpers.h"
#include "helpers/PersonHelpers.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "ppc_math.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbError.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "VehicleSteer.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// The park's date messages (main app, unit w129): the three constructors 0x10151c24, 0x10151c6c and 0x10151cb4, the GetType functions 0x10112de0, 0x10112da8
// and 0x10112d70, and the destructors 0x1011261c, 0x101125a4 and 0x1011252c. They live together because the native vtable needs a key function (GetType);
// the pattern is src/ThingMessages.cpp's.

int32_t CMsgEndOfYear::GetType() const { return 0xd; }
int32_t CMsgEndOfMonth::GetType() const { return 0xc; }
int32_t CMsgEndOfDay::GetType() const { return 0xb; }

// Unit v5556: CPeep::CPeep 0x10112e18 (432 bytes). The peep is reached by offset (the class has no members in the header).
namespace {
// The halfwords at 0x102a0d5a (initialised data): the peep's first sprite, picked by GetRand() % 5.
const uint16_t kPeepSpriteTable[5] = {60, 80, 100, 120, 140};

template <class T>
T& At(uint8_t* self, uint32_t offset) {
    return *reinterpret_cast<T*>(self + offset);
}
}  // namespace

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

// Unit v51034: CPerson::GetHeadSpriteID, SetSpotAnim, ScoreOnCloserInspection, FinalDestroy, LOADVAR<ESP_Sprites>, Pilot::~Pilot,
// CParkTime::GetFunnyTimeSinceGameTurn. The person is reached by Mac offset.

// 0x10113028: the Mac's deleting form; ~Vehicle runs after this empty body, and delete frees by itself (as ~Vehicle's note says).
Pilot::~Pilot() {}

// CPeep (Peep.cpp, Person.cpp): the destructor, the sprite members and FinalDestroy. The peep is reached by Mac offset.

namespace {
uint32_t& PWord(void* p, std::size_t off) { return *reinterpret_cast<uint32_t*>(PBytes(p) + off); }
CDelayedScript* ScriptOf(void* p) { return reinterpret_cast<CDelayedScript*>(PBytes(p) + 0xc); }
CThoughts* ThoughtsOf(void* p) { return reinterpret_cast<CThoughts*>(PBytes(p) + 0x30); }
}  // namespace

// 0x101130ac
void CPeep::FinalDestroy() {
    reinterpret_cast<CThing*>(this)->FinalDestroy();
    if (PWord(this, 0xc) != 0) ScriptOf(this)->KillSpriteScript();
    ReportAllEvents();
    ThoughtsOf(this)->FinalDestroy();
}

// CPeep::GetInterpolatedPeepPosition (0x101147e8) and CPeep::Serialise (0x10113138), both from Peep.cpp.

// Runtime glue 0x10239940, not translated anywhere yet: a weak empty stand-in so the launcher links (the Peep.cpp static initialiser needs it).
extern "C" __attribute__((weak)) void __register_global_object(void*, void*, void*) {}

namespace {

uint8_t* PeepBytes(CPeep* peep) { return reinterpret_cast<uint8_t*>(peep); }

// The Mac's int -> single conversion of the magic-number trick: the exact int, rounded to single.
float IntToSingle(int32_t value) { return static_cast<float>(static_cast<double>(value)); }

}  // namespace

// Declared before its first use: the definition is further down.
template<>
int32_t SAVEVAR<ESP_Sprites>(void* file, ESP_Sprites* value, const char* name, int32_t noLog);

// 0x10113138: the thing, the script, then (when the version is 0 or 1) the peep's own fields in the same order saving and loading, the vehicle and
// the thoughts; any failure answers 1. A load wraps the sprite id into the sprite set. At the end (also when the version is out of range) the peep
// walks to its saved destination again.
int32_t CPeep::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    uint8_t* self = PeepBytes(this);
    if (reinterpret_cast<CThing*>(this)->Serialise(file, save, version, unused) != 0) return 1;
    if (reinterpret_cast<CDelayedScript*>(self + 0xc)->Serialise(file, save, version, unused) != 0) return 1;

    auto* destX = reinterpret_cast<int16_t*>(self + 0x18);
    auto* destY = reinterpret_cast<int16_t*>(self + 0x1a);
    auto* adjustorSpeed = reinterpret_cast<uint16_t*>(self + 0xc4);
    auto* baseSpeed = reinterpret_cast<uint16_t*>(self + 0xc0);
    auto* count = reinterpret_cast<uint8_t*>(self + 0x2c);
    auto* sprite = reinterpret_cast<ESP_Sprites*>(self + 0x24);
    auto* lastMap = reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>*>(self + 0xcc);
    auto* previousSpeed = reinterpret_cast<float*>(self + 0xc8);
    auto* previousX = reinterpret_cast<MacLong*>(self + 0x190);
    auto* previousY = reinterpret_cast<MacLong*>(self + 0x194);
    auto* strandedTime = reinterpret_cast<MacULong*>(self + 0x198);
    auto* purposeSpeed = reinterpret_cast<uint16_t*>(self + 0xc2);
    auto* setDestOk = reinterpret_cast<int32_t*>(self + 0xd0);
    auto* spriteAngle = reinterpret_cast<MacULong*>(self + 0x1c);
    auto* spriteId = reinterpret_cast<uint32_t*>(self + 0x20);
    auto* underRide = reinterpret_cast<int32_t*>(self + 0x28);
    auto* vehicle = reinterpret_cast<Vehicle*>(self + 0xd4);
    auto* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);

    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVAR(file, destX, "mAccurateDestX", 0) != 0) return 1;
            if (SAVEVAR(file, destY, "mAccurateDestY", 0) != 0) return 1;
            if (SAVEVAR(file, adjustorSpeed, "mAdjustorSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, baseSpeed, "mBaseSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, count, "mCount", 0) != 0) return 1;
            if (SAVEVAR(file, sprite, "mESPSprite", 0) != 0) return 1;
            if (SAVEVARSTRUCT(file, lastMap, "mLastRecordedMapId", 0) != 0) return 1;
            if (vehicle->Serialise(file, save, version, unused) != 0) return 1;
            if (SAVEVAR(file, previousSpeed, "mPreviousSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, previousX, "mPreviousX", 0) != 0) return 1;
            if (SAVEVAR(file, previousY, "mPreviousY", 0) != 0) return 1;
            if (SAVEVAR(file, strandedTime, "mStrandedTime", 0) != 0) return 1;
            if (SAVEVAR(file, purposeSpeed, "mPurposeSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, setDestOk, "mSetDestSuccessfully", 0) != 0) return 1;
            if (SAVEVAR(file, spriteAngle, "mSpriteAngle", 0) != 0) return 1;
            if (SAVEVAR(file, spriteId, "mSpriteID", 0) != 0) return 1;
            if (SAVEVAR(file, underRide, "mSpriteUnderRideCtrl", 0) != 0) return 1;
            if (thoughts->Serialise(file, save, version, unused) != 0) return 1;
        }
    } else if (version < 2 && version >= 0) {
        if (LOADVAR(file, destX, "mAccurateDestX", 0) != 0) return 1;
        if (LOADVAR(file, destY, "mAccurateDestY", 0) != 0) return 1;
        if (LOADVAR(file, adjustorSpeed, "mAdjustorSpeed", 0) != 0) return 1;
        if (LOADVAR(file, baseSpeed, "mBaseSpeed", 0) != 0) return 1;
        if (LOADVAR(file, count, "mCount", 0) != 0) return 1;
        if (LOADVAR(file, sprite, "mESPSprite", 0) != 0) return 1;
        if (LOADVARSTRUCT(file, lastMap, "mLastRecordedMapId", 0) != 0) return 1;
        if (vehicle->Serialise(file, 0, version, unused) != 0) return 1;
        if (LOADVAR(file, previousSpeed, "mPreviousSpeed", 0) != 0) return 1;
        if (LOADVAR(file, previousX, "mPreviousX", 0) != 0) return 1;
        if (LOADVAR(file, previousY, "mPreviousY", 0) != 0) return 1;
        if (LOADVAR(file, strandedTime, "mStrandedTime", 0) != 0) return 1;
        if (LOADVAR(file, purposeSpeed, "mPurposeSpeed", 0) != 0) return 1;
        if (LOADVAR(file, setDestOk, "mSetDestSuccessfully", 0) != 0) return 1;
        if (LOADVAR(file, spriteAngle, "mSpriteAngle", 0) != 0) return 1;
        if (LOADVAR(file, spriteId, "mSpriteID", 0) != 0) return 1;
        const uint32_t sprites = static_cast<uint32_t>(ESP_GetNumberOfSprites(*sprite));
        *spriteId = *spriteId - (*spriteId / sprites) * sprites;
        if (LOADVAR(file, underRide, "mSpriteUnderRideCtrl", 0) != 0) return 1;
        if (thoughts->Serialise(file, 0, version, unused) != 0) return 1;
    }

    MapPos x, y;
    std::memcpy(&x, self + 0x18, sizeof(MapPos));
    std::memcpy(&y, self + 0x1a, sizeof(MapPos));
    if (SetDest(x, y) == 0) CTRACE(2, "Couldn't renav to my accurate dest!!");
    return 0;
}

// Unit v51064: twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (main app).

// 0x10113814: answers the old halfword at +0xcc and stores the cell the peep stands on (x byte + y byte * 128 + 1).
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CPeep::LastCellVisited() {
    uint8_t* self = Raw(this);
    uint16_t old = Fld_2<uint16_t>(self, 0xcc);
    Fld_2<uint16_t>(self, 0xcc) = static_cast<uint16_t>(Fld_2<uint8_t>(self, 4) + (Fld_2<uint8_t>(self, 6) * 128 + 1));
    return tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{old};
}

// Nine halfwords the Mac keeps from 0x102a0d64, two bytes apart (initial data 0; names invented): the "no cell" ids SetRandomDestination compares
// against. [0] 0x102a0d64 patrol check, [1] 0x102a0d66 queue check, [2] 0x102a0d68 exit check, [3..6] 0x102a0d6a..0x102a0d70 the four neighbours
// in the final choice, [7] 0x102a0d72 the random pick, [8] 0x102a0d74 the assert.
uint16_t gPeepNoCell[9];

namespace {

// The texts at 0x102439f5 + 0x104, + 0x14e, + 0x171, + 0x1b5, + 0x1ef.
const char kTextInsidePatrol[] = "*** I'm inside my patrol area but can't get anywhere else within it! ***\n";
const char kTextNowhere[] = "Peep can't SetRandomDest anywhere\n";
const char kTextNavigation[] = "Serious person navigation problem: no adjacent cells are reachable\n";
const char kTextNoDirection[] = "\n*** Could not find a direction, but there should be one\n";
const char kTextProblem[] = "Problem in SetRandomDest\n";

typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellId;

inline CellId Id(uint16_t cell) {
    CellId id;
    id.mIndex = cell;
    return id;
}

// The peep's cell: (y * 128 + 1 + x) in 16 bits, from the bytes at +4 and +6.
inline uint16_t PeepCell(const uint8_t* self) {
    return static_cast<uint16_t>(static_cast<uint32_t>(self[4]) + (static_cast<uint32_t>(self[6]) << 7) + 1u);
}

// The cell in the map grid (0x44 bytes each); the id 0 lands one before the grid.
inline CMapCell* CellAt(uint16_t cell) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorldMap) + static_cast<int64_t>(static_cast<int32_t>(cell) - 1) * 0x44);
}

inline bool IsStaffType(uint8_t type) {
    return type == 4 || type == 5 || type == 6 || type == 8 || type == 7;
}

inline uint16_t Step(uint16_t cell, const MapDelta& d) {
    return static_cast<uint16_t>(static_cast<uint32_t>(cell) + (static_cast<uint32_t>(d.mY) << 7) + static_cast<uint32_t>(d.mX));
}

}  // namespace

// 0x10113870: a random walk target. A stranded peep whose surroundings are unchanged only shows its bubble. Otherwise it follows up to 1-5
// cell steps along the paths (never straight back, skipping queue/exit/outside-patrol cells), then asks for a random spot in the last cell; a
// peep with no neighbours tries the eight rings around it, then five random cells within 5 of it.
int32_t CPeep::SetRandomDestination() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    CStaff* staffThis = reinterpret_cast<CStaff*>(this);
    uint32_t prev = 9999;
    uint16_t cur = PeepCell(self);
    int32_t outside = 0;

    if (IsStranded() && IsLocalAreaUnchangedSinceBecomingStranded()) {
        reinterpret_cast<CThoughts*>(self + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x11), false);
        return 0;
    }
    SetStranded(0);
    uint32_t rnd = gWorld->GetRand();
    const bool staff = IsStaffType(self[2]);
    int32_t counter = static_cast<int32_t>(rnd % 5u) + 1;
    CMapCell* here = CellAt(PeepCell(self));
    if (staff) {
        if (staffThis->InsidePatrolArea(Id(PeepCell(self))) == 0) {
            if (staffThis->FindDestInPatrolArea() != 0) return 1;
            outside = 1;
        }
    }

    if (here->NumberOfHVNeighbours() > 0) {
        while (true) {
            const int32_t old = counter;
            counter = old - 1;
            if (old == 0) break;

            CMapCell* cell = CellAt(cur);
            uint32_t nb = static_cast<uint32_t>(cell->GetNeighbours()) & 0x55u;
            int32_t n = cell->NumberOfHVNeighbours();
            uint16_t cand[4];
            cand[0] = (nb & 0x10) ? Step(cur, gMapDeltaPlusY) : 0;
            cand[1] = (nb & 0x04) ? Step(cur, gMapDeltaPlusX) : 0;
            cand[2] = (nb & 0x01) ? Step(cur, gMapDeltaMinusY) : 0;
            cand[3] = (nb & 0x40) ? Step(cur, gMapDeltaMinusX) : 0;

            if (staff && outside == 0) {
                for (int32_t i = 0; i < 4; ++i) {
                    if (cand[i] != gPeepNoCell[0] && staffThis->InsidePatrolArea(Id(cand[i])) == 0) {
                        --n;
                        cand[i] = 0;
                        if (n == 0) {
                            if (staffThis->FindDestInPatrolArea() != 0) return 1;
                            CTRACE(2, kTextInsidePatrol);
                            return 0;
                        }
                    }
                }
            }
            if (cell->IsPath()) {
                for (int32_t i = 0; i < 4; ++i) {
                    if (cand[i] != gPeepNoCell[1] && CellAt(cand[i])->IsQueue() != 0) {
                        --n;
                        cand[i] = 0;
                    }
                }
            }
            if (cell->IsQueue() != 0 && cell->IsEntrance() == 0) {
                if (cell->GetDirection() == 1) {
                    --n;
                    cand[2] = 0;
                } else if (cell->GetDirection() == 0x10) {
                    --n;
                    cand[0] = 0;
                } else if (cell->GetDirection() == 4) {
                    --n;
                    cand[1] = 0;
                } else if (cell->GetDirection() == 0x40) {
                    --n;
                    cand[3] = 0;
                }
            }
            for (int32_t i = 0; i < 4; ++i) {
                const uint16_t v = cand[i];
                CMapCell* exitCell = reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorld) + 0x2d8 +
                                                                 static_cast<int64_t>(static_cast<int32_t>(v) - 1) * 0x44);
                if (v != gPeepNoCell[2] && exitCell->IsExit() != 0) {
                    --n;
                    cand[i] = 0;
                }
            }

            uint32_t dir;
            if (n < 2) {
                if (cand[0] != gPeepNoCell[3]) {
                    dir = 0;
                } else if (cand[1] != gPeepNoCell[4]) {
                    dir = 1;
                } else if (cand[2] != gPeepNoCell[5]) {
                    dir = 2;
                } else if (cand[3] != gPeepNoCell[6]) {
                    dir = 3;
                } else {
                    CTRACE(2, kTextNowhere);
                    if (staff) {
                        if (staffThis->FindDestInPatrolArea() != 0) return 1;
                        CTRACE(2, kTextInsidePatrol);
                        return 0;
                    }
                    CTRACE(2, kTextNavigation);
                    reinterpret_cast<CThoughts*>(self + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x11), false);
                    SetStranded(1);
                    return 0;
                }
            } else {
                const uint32_t start = gWorld->GetRand() & 3u;
                uint32_t i = start;
                dir = start;
                while (i < start + 4 && (cand[dir] == gPeepNoCell[7] || dir == prev)) {
                    ++i;
                    dir = i & 3u;
                }
                if (i == start + 4) CTRACE(2, kTextNoDirection);
            }
            cur = cand[dir];
            GAME_ASSERTMSG(static_cast<int32_t>(gPeepNoCell[8] != cur), kTextProblem);
            prev = (dir + 2) & 3u;
            if (cur == PeepCell(self) && counter == 0) ++counter;
        }

        uint32_t ra = gWorld->GetRand() & 0x7fu;
        uint32_t rb = gWorld->GetRand() & 0x7fu;
        uint8_t a = static_cast<uint8_t>(ra);
        uint8_t b = static_cast<uint8_t>(rb);
        if (a < 5) a = 5; else if (a > 0x7b) a = 0x7b;
        if (b < 5) b = 5; else if (b > 0x7b) b = 0x7b;
        const uint32_t m = static_cast<uint32_t>(cur) - 1u;
        MapPos px;
        MapPos py;
        px.mCell = static_cast<uint8_t>(m & 0x7fu);
        px.mOffset = a;
        py.mCell = static_cast<uint8_t>(m >> 7);
        py.mOffset = b;
        int32_t result = SetDest(px, py);
        if (result != 0) *reinterpret_cast<int32_t*>(self + 0xd0) = 1;
        return result;
    }

    // No neighbour: the eight rings (cell itself, then the compass directions) of up to 3 cells, then five random cells near the peep.
    const uint16_t start = PeepCell(self);
    for (int32_t dir = 0; dir < 8; ++dir) {
        for (int32_t j = 0; j < 4; ++j) {
            int32_t dx = 0;
            int32_t dy = 0;
            switch (dir) {
                case 0: break;
                case 1: dx = j; dy = j; break;
                case 2: dx = j; break;
                case 3: dx = j; dy = -j; break;
                case 4: dy = -j; break;
                case 5: dx = -j; dy = -j; break;
                case 6: dx = -j; break;
                case 7: dx = -j; dy = j; break;
                default: break;
            }
            const uint16_t c = static_cast<uint16_t>(static_cast<uint32_t>(start) + (static_cast<uint32_t>(dy) << 7) + static_cast<uint32_t>(dx));
            const uint32_t m = static_cast<uint32_t>(c) - 1u;
            if (CMap::IsXYValid(static_cast<int32_t>(m & 0x7fu), static_cast<int32_t>(m >> 7)) != 0 && CellAt(c)->IsPath() != 0 &&
                SetDest(Id(c)) != 0) {
                *reinterpret_cast<int32_t*>(self + 0xd0) = 1;
                return 1;
            }
        }
    }
    uint32_t tries;
    for (tries = 0; tries < 5; ++tries) {
        const uint32_t r1 = gWorld->GetRand();
        const int32_t offX = static_cast<int32_t>(r1 % 11u) - 5;
        const uint32_t r2 = gWorld->GetRand();
        const int32_t offY = static_cast<int32_t>(r2 % 11u) - 5;
        const uint32_t m = static_cast<uint32_t>(PeepCell(self)) - 1u;
        const int32_t x = offX + static_cast<int32_t>(m & 0x7fu);
        const int32_t y = offY + static_cast<int32_t>(m >> 7);
        if (CMap::IsXYValid(x, y) != 0) {
            const uint16_t c = static_cast<uint16_t>((static_cast<uint32_t>(y) << 7) + 1u + static_cast<uint32_t>(x));
            if (SetDest(Id(c)) != 0) break;
        }
    }
    if (tries == 5) return 0;
    *reinterpret_cast<int32_t*>(self + 0xd0) = 1;
    return 1;
}

// 0x10114624
void CPeep::DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script) {
    if (PWord(this, 0xc) == 0)
        ScriptOf(this)->SpawnSpriteScript(script, static_cast<int32_t>(PWord(this, 0x24)), static_cast<int32_t>(PWord(this, 0x20)));
    float height = DetermineHFieldHeight(x, z, 1);
    float lifted = y - height;
    SCR_SetScriptPosition(static_cast<int32_t>(PWord(this, 0xc)), x, lifted, z, frame);
    if (!ScriptOf(this)->IsRunningThisScript(script)) ScriptOf(this)->SetSpriteScript(script);
}

// 0x10114758
void CPeep::HideMySprite() {
    ScriptOf(this)->KillSpriteScript();
    ThoughtsOf(this)->RemoveThoughtBubble();
    if (PBytes(this)[2] == 1 && PWord(this, 0x210) != 0) reinterpret_cast<CPerson*>(this)->MakeBalloonPop();
}

// 0x101147e8: the share is clamped (to -1..2, or to 0..1 when `ranged` is not 0; a NaN becomes the low end), the position is the pilot's
// current (x, y) mixed with the last one at +0x190 / +0x194 by a fused multiply-add, cut to an int, and scaled by 10 / 65536.
// (A NaN share is not clamped.)
void CPeep::GetInterpolatedPeepPosition(float t, int32_t ranged, float& x, float& y, float& z, uint32_t& stamp) {
    uint8_t* self = PeepBytes(this);
    float low, high;
    if (ranged != 0) {
        low = 0.0f;
        high = 1.0f;
    } else {
        low = -1.0f;
        high = 2.0f;
    }
    if (t < low) t = low;          // bge / ble test the LT / GT bits: a NaN passes both and stays a NaN
    else if (t > high) t = high;

    int32_t curX, curY;
    reinterpret_cast<Pilot*>(self + 0xd4)->get_pos(curX, curY);

    int32_t lastX, lastY, word;
    std::memcpy(&lastX, self + 0x190, 4);
    std::memcpy(&lastY, self + 0x194, 4);
    const float dx = IntToSingle(static_cast<int32_t>(static_cast<uint32_t>(curX) - static_cast<uint32_t>(lastX)));
    const float ax = IntToSingle(lastX);
    const float dy = IntToSingle(static_cast<int32_t>(static_cast<uint32_t>(curY) - static_cast<uint32_t>(lastY)));
    const float ay = IntToSingle(lastY);
    const int32_t ix = ppc_fctiwz(static_cast<double>(ppc_fmadds(t, dx, ax)));
    const int32_t iy = ppc_fctiwz(static_cast<double>(ppc_fmadds(t, dy, ay)));
    x = (10.0f * IntToSingle(ix)) / 65536.0f;
    y = 0.0f;
    z = (10.0f * IntToSingle(iy)) / 65536.0f;
    std::memcpy(&word, self + 0x1c, 4);
    stamp = static_cast<uint32_t>(word);
}

// Statics of Peep.cpp used by UpdatePositionInEngine (names invented): a guard byte and the value it protects, set the first time through.
uint8_t gPeepGuardC490;      // 0x1034c490
int32_t gPeepBalloonIndex;   // 0x1034c494: the balloon offset row in use
uint8_t gPeepGuardC488;      // 0x1034c488
float gPeepBalloonTime;      // 0x1034c48c: 0 to 1
uint8_t gPeepGuardC480;      // 0x1034c480
float gPeepBalloonRate;      // 0x1034c484: 0.1
uint8_t gPeepGuardC478;      // 0x1034c478
float gPeepBalloonScale;     // 0x1034c47c: 1.5
uint8_t gPeepGuardC470;      // 0x1034c470
float gPeepC474;             // 0x1034c474: 0.1 (written, never read)
uint8_t gPeepGuardC468;      // 0x1034c468
float gPeepBalloonDivisor;   // 0x1034c46c: 0.5
uint8_t gPeepGuardC460;      // 0x1034c460
float gPeepStepLag;          // 0x1034c464: 0.35
// Initialised data of Peep.cpp (names invented).
float gPeepScriptHeight = 2.5f;      // 0x102a0d98
float gPeepBalloonBase = 1.0f;       // 0x102a0d9c
uint32_t gPeepBalloonRows = 20;      // 0x102a0da0
// 0x102a0da4: 12 bytes a row, the balloon's x, y, z offset by frame (the image's data): it bobs up 0.2 and back over the 20 rows.
float gPeepBalloonOffsets[20][3] = {
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.02f, 0.0f}, {0.0f, 0.04f, 0.0f}, {0.0f, 0.06f, 0.0f},
    {0.0f, 0.08f, 0.0f}, {0.0f, 0.1f, 0.0f}, {0.0f, 0.12f, 0.0f}, {0.0f, 0.14f, 0.0f},
    {0.0f, 0.16f, 0.0f}, {0.0f, 0.18f, 0.0f}, {0.0f, 0.2f, 0.0f}, {0.0f, 0.18f, 0.0f},
    {0.0f, 0.16f, 0.0f}, {0.0f, 0.14f, 0.0f}, {0.0f, 0.12f, 0.0f}, {0.0f, 0.1f, 0.0f},
    {0.0f, 0.08f, 0.0f}, {0.0f, 0.06f, 0.0f}, {0.0f, 0.04f, 0.0f}, {0.0f, 0.0f, 0.0f},
};

namespace {

char* Bytes(void* p) { return static_cast<char*>(p); }
template <class T> T& At_2(void* p, int off) { return *reinterpret_cast<T*>(Bytes(p) + off); }

// Mac code addresses the sprite scripts are told apart by.
void* const kWalkScript = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1029f2ac));
void* const kCarryScript = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1029f948));
void* const kRunScript = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1029f308));

// divwu: the emulator answers 0 for a zero divisor.
uint32_t Divwu(uint32_t a, uint32_t b) { return b == 0 ? 0 : a / b; }

}  // namespace

void CPeep::UpdatePositionInEngine(float step) {
    if (gPeepGuardC490 == 0) { gPeepBalloonIndex = 0; gPeepGuardC490 = 1; }
    if (gPeepGuardC488 == 0) { gPeepBalloonTime = 0.0f; gPeepGuardC488 = 1; }
    if (gPeepGuardC480 == 0) { gPeepBalloonRate = 0.1f; gPeepGuardC480 = 1; }
    if (gPeepGuardC478 == 0) { gPeepBalloonScale = 1.5f; gPeepGuardC478 = 1; }
    if (gPeepGuardC470 == 0) { gPeepC474 = 0.1f; gPeepGuardC470 = 1; }
    if (gPeepGuardC468 == 0) { gPeepBalloonDivisor = 0.5f; gPeepGuardC468 = 1; }
    if (gPeepGuardC460 == 0) { gPeepStepLag = 0.35f; gPeepGuardC460 = 1; }

    const float d = step - gPeepStepLag;
    float f28;
    if (d < -1.0f) {
        f28 = -1.0f;
    } else {
        f28 = 2.0f;
        if (!(d > 2.0f)) f28 = d;
    }
    Pilot* pilot = reinterpret_cast<Pilot*>(Bytes(this) + 0xd4);
    const int32_t last190 = At_2<int32_t>(this, 0x190);
    const int32_t last194 = At_2<int32_t>(this, 0x194);
    int32_t px, py;
    pilot->get_pos(px, py);
    const float lx = static_cast<float>(last190), ly = static_cast<float>(last194);
    const int32_t ix = ppc_fctiwz(static_cast<double>(ppc_fmadds(f28, static_cast<float>(px - last190), lx)));
    const int32_t iy = ppc_fctiwz(static_cast<double>(ppc_fmadds(f28, static_cast<float>(py - last194), ly)));
    const float f30 = (10.0f * static_cast<float>(ix)) / 65536.0f;
    f28 = (10.0f * static_cast<float>(iy)) / 65536.0f;

    float f27;
    if (step < 0.0f) {
        f27 = 0.0f;
    } else {
        f27 = 1.0f;
        if (!(step > 1.0f)) f27 = step;
    }
    int32_t qx, qy;
    pilot->get_pos(qx, qy);
    const int32_t jx = ppc_fctiwz(static_cast<double>(ppc_fmadds(f27, static_cast<float>(qx - last190), lx)));
    const int32_t jy = ppc_fctiwz(static_cast<double>(ppc_fmadds(f27, static_cast<float>(qy - last194), ly)));
    f27 = (10.0f * static_cast<float>(jx)) / 65536.0f;
    const float f26 = (10.0f * static_cast<float>(jy)) / 65536.0f;

    if (At_2<int32_t>(this, 0x28) == 0) {
        SCR_SetScriptPosition(At_2<int32_t>(this, 0xc), f27, 0.0f, f26, static_cast<int32_t>(((static_cast<uint32_t>(At_2<int32_t>(this, 0x1c)) + 0x480u) >> 8) & 7));
    }
    if (IsStranded()) {
        const uint16_t cell = static_cast<uint16_t>(static_cast<uint8_t>(Bytes(this)[4]) + (static_cast<uint8_t>(Bytes(this)[6]) << 7) + 1);
        const int32_t cx = static_cast<int32_t>((static_cast<uint32_t>(cell) - 1u) & 0x7f);
        const int32_t cy = static_cast<int32_t>((static_cast<uint32_t>(cell) - 1u) >> 7);
        DC_AddSquareFaceToList(10.0f * static_cast<float>(cx), 1.0f, 10.0f * static_cast<float>(cy), 0, 1, 0, 1.0f, 0, 0, 0, static_cast<DYN_Type>(0));
    }
    if (At_2<int32_t>(this, 0xb4) != 0) {
        SCR_SetScriptPosition(At_2<int32_t>(this, 0xb4), f27, 0.0f + gPeepScriptHeight, f26, 0);
    }
    if (static_cast<uint8_t>(Bytes(this)[2]) == 1 && At_2<int32_t>(this, 0x210) != 0) {
        const uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        const uint32_t sum = static_cast<uint32_t>(gPeepBalloonIndex) + id;
        const uint32_t row = sum - Divwu(sum, gPeepBalloonRows) * gPeepBalloonRows;
        const float* t = gPeepBalloonOffsets[0] + row * 3;
        const float dz = std::fabs(f28 - f26);
        const float dx = std::fabs(f30 - f27);
        float q = (dx + dz) / gPeepBalloonDivisor;
        q = gPeepBalloonBase - q;
        const float base = 0.0f + q;
        const float sc = gPeepBalloonScale;
        reinterpret_cast<CPerson*>(this)->DrawBalloon(ppc_fmadds(sc, t[0], f30), ppc_fmadds(sc, t[1], base), ppc_fmadds(sc, t[2], f28), 0);
        const float time = gPeepBalloonTime;
        if (static_cast<double>(time) >= 1.0) {
            const uint32_t n = static_cast<uint32_t>(gPeepBalloonIndex) + 1u;
            gPeepBalloonIndex = static_cast<int32_t>(n - Divwu(n, gPeepBalloonRows) * gPeepBalloonRows);
            gPeepBalloonTime = 0.0f;
        } else {
            gPeepBalloonTime = time + gPeepBalloonRate;
        }
    }
}

int32_t CPeep::PeepWalks() {
    Pilot* pilot = reinterpret_cast<Pilot*>(Bytes(this) + 0xd4);
    int32_t x0, y0, x1, y1;
    pilot->get_pos(x0, y0);
    pilot->move();
    pilot->get_pos(x1, y1);
    const int16_t hx = static_cast<int16_t>(x1 >> 8), hy = static_cast<int16_t>(y1 >> 8);
    MapPos mx, my;
    mx.mCell = static_cast<uint8_t>(static_cast<uint16_t>(hx) >> 8);
    mx.mOffset = static_cast<uint8_t>(hx);
    my.mCell = static_cast<uint8_t>(static_cast<uint16_t>(hy) >> 8);
    my.mOffset = static_cast<uint8_t>(hy);
    reinterpret_cast<CThing*>(this)->Move(mx, my);
    SetStranded(0);
    if (At_2<int32_t>(this, 0x18c) != 0) return 2;
    if (gPeepC498 == pilot->is_finished_path()) return 0;
    const int32_t angle = LbMaths_ArcTan(-(x1 - x0), y1 - y0);
    At_2<uint32_t>(this, 0x1c) = static_cast<uint32_t>(angle) & 0xffffu;
    bool fast = false;
    if (At_2<uint16_t>(this, 0xc2) > gPersonTable[0]) {
        const uint8_t kind = static_cast<uint8_t>(Bytes(this)[2]);
        if (kind == 1 || kind == 7) fast = true;
    }
    void* code = kWalkScript;
    int32_t script = 1;
    if (static_cast<uint8_t>(Bytes(this)[2]) == 7) {
        if (reinterpret_cast<CGuard*>(this)->IsCarrying()) {
            code = kCarryScript;
            script = 0x17;
        }
    }
    CDelayedScript* ds = reinterpret_cast<CDelayedScript*>(Bytes(this) + 0xc);
    if (!fast) {
        if (ESP_RURunningThis(At_2<int32_t>(this, 0xc), code) == 0) ds->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(script));
    }
    if (fast) {
        if (ESP_RURunningThis(At_2<int32_t>(this, 0xc), kRunScript) == 0) ds->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(2));
    }
    const int32_t ddx = x1 - x0, ddy = y1 - y0;
    const int32_t sq = static_cast<int32_t>(static_cast<uint32_t>(ddx) * static_cast<uint32_t>(ddx) + static_cast<uint32_t>(ddy) * static_cast<uint32_t>(ddy));
    float rate = static_cast<float>(std::sqrt(static_cast<double>(sq))) / 26112.0f;
    if (!fast) rate = rate * 2.0f;
    ds->SetServiceInterval(rate);
    return 1;
}

// 0x1011524c
void CPeep::SetStandScript() {
    reinterpret_cast<CDelayedScript*>(Raw(this) + 0xc)->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3));
}

// CPeep (Peep.cpp), unit v5048: the stranded-area check, the particle effect and the event report.

namespace {

char* Bytes_2(void* p) { return static_cast<char*>(p); }
const char* Bytes_2(const void* p) { return static_cast<const char*>(p); }

}  // namespace

// 0x101152a4: starts the emitter of an effect at the script position of the peep (x and z shifted by ten per height field start cell, the height
// from the height field), the position scaled by 1024.
void CPeep::SpawnParticleEffect(int32_t effect) {
    float x, y, z;
    SCR_GetScriptPosition(*reinterpret_cast<const int32_t*>(Bytes_2(this) + 0xc), &x, &y, &z, nullptr);
    const float ten = 10.0f;
    x = ppc_fmadds(ten, static_cast<float>(gWorld->GetHeightfieldXStart()), x);
    z = ppc_fmadds(ten, static_cast<float>(gWorld->GetHeightfieldYStart()), z);
    y = DetermineHFieldHeightQuick(x, z);
    const float scale = 1024.0f;
    const float sx = scale * x;
    const float sy = scale * y;
    const float sz = scale * z;
    PAR_InitEmitter(effect, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz));
}

// Unit v5281: CPeep::PlaySound, IsInsidePark, Teleport, UpdateTurn, IsStranded, SetStranded and the two SetDest (Peep.cpp).
// The peep is reached by offset: cell x/y bytes at +4/+6, destination halfwords at +0x18/+0x1a, script at +0xc, three turn halfwords at
// +0xc0/+0xc2/+0xc4, a float at +0xc8, the Pilot at +0xd4, two positions at +0x190/+0x194, the stranded stamp at +0x198.

// 0x102a0e94: the null cell id (a halfword; zero at start).
uint16_t gPeepNullCellId;

namespace {

uint8_t* Bytes_3(void* p) { return static_cast<uint8_t*>(p); }
const uint8_t* Bytes_3(const void* p) { return static_cast<const uint8_t*>(p); }
uint16_t& Half(void* p, std::size_t off) { return *reinterpret_cast<uint16_t*>(Bytes_3(p) + off); }
uint32_t& Word(void* p, std::size_t off) { return *reinterpret_cast<uint32_t*>(Bytes_3(p) + off); }
Pilot* PilotOf(void* p) { return reinterpret_cast<Pilot*>(Bytes_3(p) + 0xd4); }
// A MapPos as the Mac's halfword: the cell byte high, the offset byte low (the original reads the pair as one big-endian halfword).
int32_t PosHalf(MapPos p) { return static_cast<int16_t>((p.mCell << 8) | p.mOffset); }
// The halfword at 0x102a0d5a + 4 (the third entry of {60, 80, 100, 120, 140}).
constexpr uint16_t kTurnDivisor = 100;

}  // namespace

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

// 0x101155bc
void CPeep::SetStranded(int32_t stranded) {
    if (stranded == 0) {
        Word(this, 0x198) = 0;
    } else {
        Word(this, 0x198) = reinterpret_cast<CMap*>(Bytes_3(gWorld) + 0x2d8)->MakeNewTimeStamp();
        tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = reinterpret_cast<CThing*>(this)->GetThingId();
        CTRACE(2, "Peep %d: stranded at time %d\n", static_cast<uint32_t>(id.mIndex), Word(this, 0x198));
    }
}

// 0x1011566c: true while the stranded stamp is set (a stamp from the future is cleared).
bool CPeep::IsStranded() {
    uint32_t now = reinterpret_cast<CMap*>(Bytes_3(gWorld) + 0x2d8)->MakeNewTimeStamp();
    if (now < Word(this, 0x198)) Word(this, 0x198) = 0;
    return Word(this, 0x198) != 0;
}

// 0x101156fc: the area around the cell the peep got stranded in (the back of its queue when it stands in one) has not been touched since the peep
// became stranded (+0x198): the newest stamp of the nine blocks around the cell is still below that turn.
bool CPeep::IsLocalAreaUnchangedSinceBecomingStranded() {
    const uint8_t cellX = static_cast<uint8_t>(Bytes_2(this)[4]);
    const uint8_t cellY = static_cast<uint8_t>(Bytes_2(this)[6]);
    const uint16_t cell = static_cast<uint16_t>(cellX + (cellY << 7) + 1);
    const CMapCell* mapCell = reinterpret_cast<const CMapCell*>(Bytes_2(gWorldMap) + (static_cast<int32_t>(cell) - 1) * 0x44);
    uint16_t start;
    if (mapCell->IsQueue()) {
        uint16_t walk = static_cast<uint16_t>(cellX + (cellY << 7) + 1);
        uint16_t last = 0;   // the listing leaves it unwritten when the walk starts at the end cell (never the case: that cell id is 0)
        while (walk != gStrandedQueueEnd) {
            last = walk;
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> in{walk};
            walk = CObject::GetQCellBehind(in).mIndex;
        }
        start = last;
    } else {
        start = static_cast<uint16_t>(cellX + (cellY << 7) + 1);
    }
    const uint32_t stranded = *reinterpret_cast<const uint32_t*>(Bytes_2(this) + 0x198);
    uint32_t newest = 0;
    for (int32_t i = 0; i < 9 && newest <= stranded; ++i) {
        const uint16_t neighbour = static_cast<uint16_t>(gNeighbourSteps[i].mDx + (gNeighbourSteps[i].mDy << 7) + start);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id{neighbour};
        CMap* map = reinterpret_cast<CMap*>(Bytes_2(gWorld) + 0x2d8);
        uint32_t stamp = map->GetTimeStamp(id);
        if (newest > stamp) stamp = newest;
        newest = stamp;
    }
    return stranded > newest;
}

// 0x101158c8
void CPeep::UpdateTurn() {
    uint32_t sum = static_cast<uint32_t>(Half(this, 0xc0)) + Half(this, 0xc2) + Half(this, 0xc4);
    float num = static_cast<float>(static_cast<double>(static_cast<int32_t>(sum)));
    float den = static_cast<float>(static_cast<double>(kTurnDivisor));
    float quotient = num / den;
    float speed = *reinterpret_cast<float*>(Bytes_3(this) + 0xc8);
    const float three = 3.0f;
    const float quarter = 0.25f;
    float result = ppc_fmadds(three, speed, quotient);
    result = result * quarter;
    *reinterpret_cast<float*>(Bytes_3(this) + 0xc8) = result;
    PilotOf(this)->set_speed(result);
    Half(this, 0xc4) = static_cast<uint16_t>(Half(this, 0xc4) * 99);
    Half(this, 0xc4) = static_cast<uint16_t>(Half(this, 0xc4) / 100u);
    PilotOf(this)->get_pos(reinterpret_cast<int32_t&>(Word(this, 0x190)), reinterpret_cast<int32_t&>(Word(this, 0x194)));
    reinterpret_cast<CDelayedScript*>(Bytes_3(this) + 0xc)->ProcessSpriteScripts();
}

// 0x101159d0
void CPeep::Teleport(MapPos x, MapPos y) {
    PilotOf(this)->reset(PosHalf(x) << 8, PosHalf(y) << 8, 0, 0);
    PilotOf(this)->get_pos(reinterpret_cast<int32_t&>(Word(this, 0x190)), reinterpret_cast<int32_t&>(Word(this, 0x194)));
    reinterpret_cast<CThing*>(this)->Move(x, y);
}

// 0x10115a80: true on a path, queue, entrance, exit or grass cell.
int32_t CPeep::IsInsidePark() const {
    const uint8_t* self = Bytes_3(this);
    uint16_t cell = static_cast<uint16_t>(self[4] + (self[6] << 7) + 1);
    CMap* map = reinterpret_cast<const CWorldSavvy*>(this)->GetMap();
    const CMapCell* mc = reinterpret_cast<const CMapCell*>(Bytes_3(map) + (static_cast<int32_t>(cell) - 1) * 0x44);
    if (mc->IsPath() || mc->IsQueue() || mc->IsEntrance() || mc->IsExit() || mc->IsGrass()) return 1;
    return 0;
}

// 0x10115b80: sends a sound event at the script position of the peep.
void CPeep::PlaySound(int32_t category, int32_t sound) {
    float x, y, z;
    int32_t mode;
    SCR_GetScriptPosition(static_cast<int32_t>(Word(this, 0xc)), &x, &y, &z, &mode);
    SOUND_SendEvent(0, category, sound, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
}

namespace {

// The texts at 0x102439f5 + offset.
const char kRule[] = "------------------------------------------------";                       // 0x244
const char kHeader[] = "This is what this person did (most recent last):";                      // 0x275
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x2a6
const char kDeleted[] = "I did something with a ride that has been deleted: ";                  // 0x2e9

}  // namespace

// 0x10115c3c: traces the peep's 32 most recent events, the oldest first. x, y and the ride type are kept from one event to the next (registers of the
// original that are only rewritten for an event with a ride): they start at 0 here, the original's start unwritten.
void CPeep::ReportAllEvents() const {
    CTRACE(2, kRule);
    CTRACE(2, kHeader);
    uint32_t x = 0, y = 0, type = 0;
    const CThoughts* thoughts = reinterpret_cast<const CThoughts*>(Bytes_2(this) + 0x30);
    for (int32_t i = 31; i >= 0; --i) {
        const CThoughtEvent* event = thoughts->GetRecentEvent(static_cast<uint32_t>(i));
        const uint16_t thing = event->mThing;
        if (thing != gNoThoughtThing) {
            reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            char* ride = Bytes_2(gThingArray[thing].mThing);
            if (ride == nullptr) {
                CTRACE(2, kDeleted);
                x = 0;
                y = 0;
                type = 0;
            } else {
                if (static_cast<uint8_t>(ride[2]) == 3) {
                    const uint16_t again = event->mThing;
                    reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                    type = *reinterpret_cast<const uint16_t*>(Bytes_2(gThingArray[again].mThing) + 0xe);
                }
                const uint16_t id1 = event->mThing;
                reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                {
                    const char* t = Bytes_2(gThingArray[id1].mThing);
                    const uint16_t cell = static_cast<uint16_t>(static_cast<uint8_t>(t[4]) + (static_cast<uint8_t>(t[6]) << 7) + 1);
                    x = static_cast<uint32_t>(static_cast<int32_t>(cell) - 1) & 0x7f;
                }
                const uint16_t id2 = event->mThing;
                reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                {
                    const char* t = Bytes_2(gThingArray[id2].mThing);
                    const uint16_t cell = static_cast<uint16_t>(static_cast<uint8_t>(t[4]) + (static_cast<uint8_t>(t[6]) << 7) + 1);
                    y = static_cast<uint32_t>(static_cast<int32_t>(cell) - 1) >> 7;
                }
            }
        }
        switch (event->mType) {
        case 1: CTRACE(2, "I found nothing to do. "); break;
        case 2: CTRACE(2, "I decided to visit ride at (%d, %d) of type %d. ", x, y, type); break;
        case 3: CTRACE(2, "I couldn't reach ride at (%d, %d) of type %d. ", x, y, type); break;
        case 4: CTRACE(2, "The ride at (%d, %d) of type %d was too exciting. ", x, y, type); break;
        case 5: CTRACE(2, "The ride at (%d, %d) of type %d was too boring. ", x, y, type); break;
        case 6: CTRACE(2, "The ride at (%d, %d) of type %d dismissed me from its queue. ", x, y, type); break;
        case 7: CTRACE(2, "I left the queue of ride at (%d, %d) of type %d, I got bored. ", x, y, type); break;
        case 8: CTRACE(2, "The ride at (%d, %d) of type %d was great!. ", x, y, type); break;
        case 9: CTRACE(2, "The ride at (%d, %d) of type %d was no good!. ", x, y, type); break;
        case 10: CTRACE(2, "The ride at (%d, %d) of type %d was too expensive! ", x, y, type); break;
        case 11: CTRACE(2, "I got a great costume from the ride at (%d, %d) of type %d. ", x, y, type); break;
        case 12: CTRACE(2, "I got a nice balloon from the ride at (%d, %d) of type %d. ", x, y, type); break;
        case 13: CTRACE(2, "The nasty man on the ride at (%d, %d) of type %d told me to get off! ", x, y, type); break;
        case 14: CTRACE(2, "I watched entertainer %d, he was top notch entertainment for sure. ", event->mThing); break;
        case 15: CTRACE(2, "I pranked. "); break;
        case 16: CTRACE(2, "I watched entertainer %d, he was top notch entertainment for sure. ", event->mThing); break;
        case 17: CTRACE(2, "I went to the toilet at (%d, %d). ", x, y); break;
        case 18: CTRACE(2, "I was sick! "); break;
        case 19: CTRACE(2, "It was time to go home. "); break;
        case 20: CTRACE(2, "Getting onto ride (%d, %d) of type %d. ", x, y, type); break;
        case 21: CTRACE(2, "There was no free queue space on ride (%d, %d) of type %d. ", x, y, type); break;
        case 22: CTRACE(2, "The queue for ride (%d, %d) of type %d moved, and I couldn't get to it any more. ", x, y, type); break;
        case 23: CTRACE(2, "On the way to my chosen ride I decided to pop into the ride at (%d, %d) of type %d", x, y, type); break;
        case 24: CTRACE(2, "I won on the sideshow at (%d, %d) of type %d! :D ", x, y, type); break;
        case 25: CTRACE(2, "I lost on the sideshow at (%d, %d) of type %d :( ", x, y, type); break;
        default: break;
        }
    }
    CTRACE(2, kRule);
}

// Unit SAVEVAR_2: SAVEVAR<ESP_Sprites> (0x101162d4) and __sinit_Peep_cpp (0x10116418), both from Peep.cpp.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

int32_t gPeepTbErrorZero;          // 0x102a0f04
int32_t gPeepC498;                 // 0x1034c498
int32_t gPeepC49C;                 // 0x1034c49c
uint32_t gPeepSinitNodes[9][3];    // 0x1034c3f4 .. 0x1034c454
char gPeepMapDeltaDtorVector[8];   // stand-in for 0x102663d0

// 0x101162d4: writes the sprite id as one word in file order (little-endian: the Mac reverses its bytes, the host's are already in that
// order, as SaveVar's SwapWord; an unconditional swap here wrote 8 as 0x08000000, which LOADVAR<ESP_Sprites> read back as a set out of
// range, v5L15). The result is tested as (result >= 0) == (zero word == 0), then 4 bytes written.
template<>
int32_t SAVEVAR<ESP_Sprites>(void* file, ESP_Sprites* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, static_cast<CSaveLoad::LogfileState>(0));
    uint32_t written = 0;
    uint32_t word = static_cast<uint32_t>(*reinterpret_cast<int32_t*>(value));
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where SAVEVARSTRUCT should be used instead");
    word = serialise::SwapWord(word);
    TbError result = LbFile_Write(file, &word, 4, &written);
    if (result.Succeeded() == (gPeepTbErrorZero == 0) && written == 4) return 0;
    GAME_ASSERTMSG(0, "File write failed");
    return 1;
}

// 0x10116418: the nine compass steps (the cell, then the eight neighbours), each registered with a destructor.
void __sinit_Peep_cpp() {
    static const int32_t kSteps[9][2] = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {0, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    gPeepC49C = 0;
    gPeepC498 = 0x10000;
    for (int32_t i = 0; i < 9; ++i) {
        gNeighbourSteps[i].mDx = kSteps[i][0];
        gNeighbourSteps[i].mDy = kSteps[i][1];
        __register_global_object(&gNeighbourSteps[i], gPeepMapDeltaDtorVector, gPeepSinitNodes[8 - i]);
    }
}
