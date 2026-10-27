// FlyingRide.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "HLSprite.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "std_basic_filebuf.h"
#include "std_basic_istream.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "TrackCell.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v5216: HeightAtPos_NAV (0x10074230) and GetNextHoldingWaypoint (0x10074458).

// 0x1029d2bc (initially 0x36b0, name invented): the lowest height a holding waypoint takes.
int32_t gFlyHoldMax = 0x36b0;

namespace {

int32_t FlyField(void* base, int32_t offset) {
    return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + offset);
}

}  // namespace

// The fly bases are native pointers here; the Mac's are 32-bit. Fields at +0x14 x, +0x18 height, +0x1c z.
int32_t HeightAtPos_NAV(int32_t x, int32_t z) {
    if (!(x < gFlyZoneD + 20000 && z < gFlyZoneA + 20000 && x >= gFlyZoneB - 20000 && z >= gFlyZoneC - 20000))
        return 100000;
    for (int32_t i = 1; i < 100; i++) {
        void* base = gFlyBases[i];
        if (base == nullptr) continue;
        // (v / 32) rounded toward zero: srawi then addze
        const int32_t dx = (x - FlyField(base, 0x14)) / 32;
        const int32_t dz = (z - FlyField(base, 0x1c)) / 32;
        const int32_t sum = dx * dx + dz * dz;
        const int32_t dist = static_cast<int32_t>(std::sqrt(static_cast<double>(sum)));
        if (static_cast<int32_t>(static_cast<uint32_t>(dist) << 5) < 10000) return 30000;
    }
    if (!(x < gFlyZoneD && z < gFlyZoneA && x >= gFlyZoneB && z >= gFlyZoneC)) return 0;
    int32_t cx = x / 300 / 10;
    int32_t cz = z / 300 / 10;
    if (cx < 0) cx = 0;
    if (cz < 0) cz = 0;
    if (cx >= gHeightfieldWidth) cx = gHeightfieldWidth - 1;
    if (cz >= gHeightfieldHeight) cz = gHeightfieldHeight - 1;
    const float h = GetWorldGSquareHeight(cx, cz, nullptr);
    return static_cast<int32_t>(h) * 300;
}

void GetNextHoldingWaypoint(FLYBOY* fly) {
    uint8_t* p = reinterpret_cast<uint8_t*>(fly);
    fly->mPrevX58 = fly->mX4c;
    fly->mPrevH5c = fly->mH50;
    fly->mPrevZ60 = fly->mZ54;
    void* base = gFlyBases[fly->m0];
    const int32_t limit = FlyField(base, 0x18) + 5000;
    fly->mH50 = gFlyHoldMax < limit ? limit : gFlyHoldMax;
    uint32_t* state = reinterpret_cast<uint32_t*>(p + 0x88);
    *state = (*state + 1) & 3;
    base = gFlyBases[fly->m0];
    const int32_t bx = FlyField(base, 0x14);
    const int32_t bz = FlyField(base, 0x1c);
    switch (*state) {
        case 0: fly->mX4c = bx + 25000; fly->mZ54 = bz + 25000; break;
        case 1: fly->mX4c = bx + 25000; fly->mZ54 = bz - 25000; break;
        case 2: fly->mX4c = bx - 25000; fly->mZ54 = bz - 25000; break;
        default: fly->mX4c = bx - 25000; fly->mZ54 = bz + 25000; break;
    }
}

// GetNextWaypoint 0x100745ec (1920 bytes): moves a fly's target one 20000-unit cell on and sets the height it flies at.

namespace {

inline int32_t Abs32(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// Snaps to the middle of a 20000 cell (truncating divide, so a negative value lands one cell lower).
inline int32_t SnapCell(int32_t v) {
    int32_t base = (v / 20000) * 20000;
    if (v < 0) base -= 20000;
    return base + 10000;
}

// Ground height under a point, in the Mac's 300-unit steps; 0 outside the fly zone.
int32_t GroundAt(int32_t x, int32_t z) {
    if (x < gFlyZoneD && z < gFlyZoneA && x >= gFlyZoneB && z >= gFlyZoneC) {
        int32_t gx = (x / 300) / 10;
        int32_t gz = (z / 300) / 10;
        if (gx < 0) gx = 0;
        if (gz < 0) gz = 0;
        if (gx >= gHeightfieldWidth) gx = gHeightfieldWidth - 1;
        if (gz >= gHeightfieldHeight) gz = gHeightfieldHeight - 1;
        return static_cast<int32_t>(GetWorldGSquareHeight(gx, gz, nullptr)) * 300;
    }
    return 0;
}

inline int32_t Sign(int32_t d) { return d == 0 ? 0 : (d > 0 ? 1 : -1); }

}  // namespace

void GetNextWaypoint(FLYBOY* fly) {
    int32_t stepX, stepZ;
    if (Abs32(fly->mX4c - fly->mPrevX58) > Abs32(fly->mZ54 - fly->mPrevZ60)) {
        stepX = Sign(fly->mX4c - fly->mPrevX58) * 20000;
        stepZ = 0;
    } else {
        stepX = 0;
        stepZ = Sign(fly->mZ54 - fly->mPrevZ60) * 20000;
    }
    fly->mPrevX58 = fly->mX4c;
    fly->mPrevH5c = fly->mH50;
    fly->mPrevZ60 = fly->mZ54;
    int32_t limit = fly->m8 + 5000;
    const int32_t rnd = Abs32(static_cast<int32_t>(gGameWorld->GetRand()));
    if (rnd % 4 != 0 && HeightAtPos_NAV(fly->mX4c + stepX, fly->mZ54 + stepZ) < limit) {
        fly->mX4c = SnapCell(fly->mX4c);
        fly->mZ54 = SnapCell(fly->mZ54);
        fly->mX4c += stepX;
        fly->mZ54 += stepZ;
    } else {
        for (;;) {
            if (HeightAtPos_NAV(fly->mX4c + stepZ, fly->mZ54 + stepX) < limit) {
                fly->mX4c += stepZ;
                fly->mZ54 += stepX;
                break;
            }
            if (HeightAtPos_NAV(fly->mX4c - stepZ, fly->mZ54 - stepX) < limit) {
                fly->mX4c -= stepZ;
                fly->mZ54 -= stepX;
                break;
            }
            if (HeightAtPos_NAV(fly->mX4c + stepX, fly->mZ54 + stepZ) < limit) {
                fly->mX4c += stepX;
                fly->mZ54 += stepZ;
                break;
            }
            if (HeightAtPos_NAV(fly->mX4c - stepX, fly->mZ54 - stepZ) < limit) {
                fly->mX4c -= stepX;
                fly->mZ54 -= stepZ;
                break;
            }
            limit += 5000;
        }
        fly->mX4c = SnapCell(fly->mX4c);
        fly->mZ54 = SnapCell(fly->mZ54);
    }
    const int32_t target = GroundAt(fly->mX4c, fly->mZ54);
    const int32_t own = GroundAt(fly->mX4, fly->mZC);
    fly->mH50 = (target < own ? own : target) + 5000;
    if (fly->mH50 < gFlyMinHeight) fly->mH50 = gFlyMinHeight;
}

// FLY_CreateBase 0x100753e8 (352 bytes) and EmployEric 0x10074d9c (1208 bytes): a new fly base and the starting of one car of it.
// A fly base is the 0x122c-byte block of gFlyBases (layout: HLSprite.h, sFlyBase); the code reads it by Mac offset in 32-bit words.

namespace {

// The texts at 0x1023dc85 (the first, then +0x2f and +0x72 of it).
const char kFlyMessageAnimFailed[] = "FLY: Failed to trigger a flying car anim (%ld)";
const char kFlyMessageThingZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kFlyMessageNoHeadNodes[] = "FLY: No headnodes on car mesh";

// A 32-bit word of a block at a Mac byte offset.
inline int32_t& Wd(uint8_t* block, uint32_t offset) { return *reinterpret_cast<int32_t*>(block + offset); }

// The Mac's labs on a 32-bit long (wraps for the most negative number).
inline int32_t Labs32(uint32_t value) {
    int32_t v = static_cast<int32_t>(value);
    return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
}

inline int32_t Rand(CWorld* world) { return Labs32(world->GetRand()); }

}  // namespace

// Starts car `slotNo` of base `index`: the first free slot (word 0x80 is 0) of the 20 gets its first position (outside the fly zone, at a
// random edge), a sound, a ride instance of the thing's ride type and its animation count. Answers 1, or 0 when all 20 slots are in use.
int32_t EmployEric(int32_t index) {
    void** const entry = &gFlyBases[index];
    uint8_t* const base = static_cast<uint8_t*>(*entry);
    int32_t k = 0;
    uint8_t* p = nullptr;
    for (;; ++k) {
        if (k >= 0x14) return 0;
        p = base + k * 0xe4 + 0x5c;
        if (Wd(p, 0x80) == 0) break;
    }
    Wd(p, 0xe0) = 0;
    Wd(p, 0) = index;
    Wd(p, 0x18) = 0;
    Wd(p, 0x14) = 0;
    Wd(p, 0x74) = 0;
    Wd(p, 0x70) = 0;
    Wd(p, 0x78) = 0;
    int32_t r = Rand(gGameWorld);
    Wd(p, 0x7c) = Wd(base, 0x28) + (r - (r / 100) * 100);
    Wd(p, 0x64) = 0;
    Wd(p, 0x68) = 0;
    Wd(p, 0x6c) = 0;
    Wd(p, 0x80) = 4;
    Wd(p, 0xb8) = 0;
    Wd(p, 0xb4) = 0;
    Wd(p, 0x8c) = 0;
    Wd(p, 0xc4) = 0;
    Wd(p, 0xc8) = 1;
    Wd(p, 0xc0) = 0;
    Wd(p, 0x50) = 0x36b0;
    if ((Rand(gGameWorld) & 1) != 0) {
        const int32_t m = Rand(gGameWorld) * 100;
        const int32_t span = gFlyZoneD - gFlyZoneB;
        Wd(p, 0x4c) = gFlyZoneB + (m - (m / span) * span);
        if ((Rand(gGameWorld) & 1) != 0)
            Wd(p, 0x54) = gFlyZoneA + 0x10000 - 0x5038;
        else
            Wd(p, 0x54) = gFlyZoneC - 0x10000 + 0x5038;
    } else {
        const int32_t m = Rand(gGameWorld) * 100;
        const int32_t span = gFlyZoneA - gFlyZoneC;
        Wd(p, 0x54) = gFlyZoneC + (m - (m / span) * span);
        if ((Rand(gGameWorld) & 1) != 0)
            Wd(p, 0x4c) = gFlyZoneD + 0x10000 - 0x5038;
        else
            Wd(p, 0x4c) = gFlyZoneB - 0x10000 + 0x5038;
    }
    Wd(p, 4) = Wd(p, 0x4c);
    Wd(p, 8) = Wd(p, 0x50);
    Wd(p, 0xc) = Wd(p, 0x54);
    Wd(p, 0x84) = 0;
    Wd(p, 0x58) = Wd(p, 0x4c) - 0x4e20;
    Wd(p, 0x5c) = Wd(p, 0x50);
    Wd(p, 0x60) = Wd(p, 0x54);
    Wd(base, 0xc) = Wd(base, 0xc) + 1;
    Wd(p, 0xcc) = -1;
    Wd(p, 0xd0) = -1;
    Wd(p, 0xd4) = -1;
    Wd(p, 0xe0) = SND_SendIndirectedEvent(Wd(base, 0x54), 0, gSoundBanks[7], 0, Wd(p, 4) / 300, Wd(p, 8) / 300, Wd(p, 0xc) / 300);
    Wd(p, 0xd8) = Rand(gGameWorld);
    if (Wd(base, 0xc) == 1) {
        Wd(p, 0x80) = 1;
        Wd(p, 4) = Wd(base, 0x14);
        Wd(p, 8) = Wd(base, 0x18);
        Wd(p, 0xc) = Wd(base, 0x1c);
        Wd(p, 0x7c) = 0;
        Wd(p, 0x10) = Wd(base, 0x40);
        Wd(base, 0x3c) = 1;
    }
    uint16_t thingNo;
    std::memcpy(&thingNo, base + 0x58, 2);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kFlyMessageThingZero);
    uint16_t rideType;
    std::memcpy(&rideType, static_cast<uint8_t*>(gThingArray[thingNo].mThing) + 0xe, 2);
    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{rideType});
    const uint32_t rideNo = info->mSubRideTypes[0];   // the Mac's +0x7b8, by name: the host's record is laid out wider
    Wd(p, 0xc0) = static_cast<int32_t>(InstanceRide(rideNo, 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0));
    uint8_t* q = static_cast<uint8_t*>(*entry) + k * 0xe4 + 0x5c;
    if (Wd(q, 0xc0) != 0 && (Wd(q, 0xcc) != 0 || Wd(q, 0xd0) != 0 || Wd(q, 0xd4) != 0)) {
        int32_t result;
        if (Wd(q, 0xc0) == 0)
            result = 0;
        else
            result = static_cast<int32_t>(SetCurRideAnimDuration(gRideAnimSets[static_cast<uint32_t>(Wd(q, 0xc0))], 0, 0, 0, 1.0f, 0));
        Wd(q, 0xcc) = 0;
        Wd(q, 0xd0) = 0;
        Wd(q, 0xd4) = 0;
        GAME_ASSERTMSG(result, kFlyMessageAnimFailed, 0);
    }
    int32_t count = 0;
    for (;;) {
        const uint32_t found =
            FindDataNodeIndexByID(gRideAnimSets[static_cast<uint32_t>(Wd(p, 0xc0))], 0x80, static_cast<uint32_t>(count + 1));
        if (found == 0xffffffffu) break;
        ++count;
    }
    Wd(p, 0xdc) = count;
    if (count == 0) GAME_ASSERTMSG(0, kFlyMessageNoHeadNodes);
    return 1;
}

// Unit v51054: small MSL helpers of the locale / streambuf / filebuf / ios_base::failure group (main app).
// Each function is a few instructions; the callees outside the unit are declared in include/std_basic_filebuf.h.

int32_t FLY_GetCameraMesh(int32_t tour);   // 0x10075278 (declared in rsse.h)

namespace v51054 {

// The four use_facet<F>(locale) wrappers pass the locale on to std::__use_facet<F> (the second argument is the
// uninitialised stack word the original loads; nothing reads it).
void* UseFacet_CtypeChar(const UseFacetLocale* locale) {          // 0x10073934
    return ::UseFacetCtypeChar(locale);
}
void* UseFacet_CodecvtCharChar(const UseFacetLocale* locale) {    // 0x1007245c
    return ::UseFacetCodecvtCharChar(locale);
}
// std::locale::facet::~facet with the hidden delete flag (signed 16 bit).
UseFacetObj* FacetDestroy(UseFacetObj* self, int16_t flag) {      // 0x10072df0
    if (self != nullptr) {
        self->mVtable = 0x1029d2a0u;
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

// basic_filebuf<char>::unshift: flushes the shift sequence of the codecvt into the file.
bool FilebufCharUnshift(BasicFilebufChar* self) {                 // 0x10072a94
    int32_t result = 1;
    while (result == 1) {
        char buffer[4];
        char* next = buffer;
        const char* p = buffer;
        void* cvt = reinterpret_cast<void*>(static_cast<uintptr_t>(self->mCodecvt));
        void* state = reinterpret_cast<uint8_t*>(self) + 0x28;
        result = filebuf_unshift_callee::CodecvtUnshift(cvt, state, buffer, buffer + 4, &next);
        if (result == 2) return false;
        if (result >= 0 && result < 2) {
            while (p < next) {
                int32_t c = static_cast<int8_t>(*p++);
                if (filebuf_wseek_callee::Fputc(c, self->mFile) == -1) return false;
            }
        }
    }
    return true;
}

// basic_filebuf<char>::sync: -1 when there is no file or fflush fails.
int32_t FilebufCharSync(BasicFilebufChar* self) {                 // 0x100703e4
    bool failed = true;
    if (self->mFile != 0) {
        if (filebuf_unshift_callee::Fflush(self->mFile) == 0) failed = false;
    }
    return failed ? -1 : 0;
}

// std::ios_base::getloc: copies the locale (two words) and counts the new reference.
void IosBaseGetloc(LocaleRefPtr* out, const IosBaseLocalePtr* ios) {   // 0x10072a34
    const LocaleRefPtr* src = ios->mLocale;
    out->mImp = src->mImp;
    out->mCount = src->mCount;
    if (out->mCount != nullptr) *out->mCount = *out->mCount + 1;
}

void CharArrayAllocDestroy(void* alloc, LocaleCharArray* arr) {   // 0x1007272c
    (void)alloc;
    if (arr != nullptr) locale_imp_callee::Delete(arr->mData);
}

// Answers the array itself when the count reaches zero, else NULL. A count of 0 is first made 1 (so it drops to 0).
LocaleCharArray* CharArrayRemoveRef(LocaleCharArray* arr) {       // 0x10072688
    uint32_t refs = arr->mRefs;
    if (refs == 0) refs = 1;
    arr->mRefs = refs;
    refs = arr->mRefs - 1;
    arr->mRefs = refs;
    if (refs == 0) return arr;
    return nullptr;
}

uint32_t FacetVectorSize(const MslPtrVector* vec) {               // 0x10072618
    return vec->mSize;
}

// Address of element i (the original steps 4 bytes per element).
void** FacetVectorIndex(MslPtrVector* vec, uint32_t i) {          // 0x100725a0
    return reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(vec->mData) + (i << 2));
}

void CharArrayAllocDeallocate(void* alloc, void* arr, uint32_t n) {   // 0x10072364
    (void)alloc;
    (void)n;
    locale_imp_callee::Delete(arr);
}

// std::_RefCountedPtr<__locale_imp>::_RefCountedPtr(const _RefCountedPtr&): copies both words, counts the reference.
LocaleRefPtr* LocaleRefPtrCopy(LocaleRefPtr* self, const LocaleRefPtr* other) {   // 0x10072030
    self->mImp = other->mImp;
    self->mCount = other->mCount;
    if (self->mCount != nullptr) *self->mCount = *self->mCount + 1;
    return self;
}

// basic_streambuf<char> defaults.
FposInt StreambufSeekoff(BasicStreambufChar* self, int32_t off, int32_t dir, uint32_t mode) {   // 0x10070b60
    (void)self; (void)off; (void)dir; (void)mode;
    FposInt r;
    r.off = -1;
    r.state = 0;
    return r;
}
FposInt StreambufSeekpos(BasicStreambufChar* self, int32_t off, int32_t state, uint32_t mode) { // 0x10070ad8
    (void)self; (void)off; (void)state; (void)mode;
    FposInt r;
    r.off = -1;
    r.state = 0;
    return r;
}
int32_t StreambufOverflow(BasicStreambufChar* self, int32_t c) { (void)self; (void)c; return -1; }    // 0x100707b0
int32_t StreambufPbackfail(BasicStreambufChar* self, int32_t c) { (void)self; (void)c; return -1; }   // 0x10070754
int32_t StreambufUnderflow(BasicStreambufChar* self) { (void)self; return -1; }                       // 0x10070634
int32_t StreambufShowmanyc(BasicStreambufChar* self) { (void)self; return 0; }                        // 0x100705d8
int32_t StreambufSync(BasicStreambufChar* self) { (void)self; return 0; }                             // 0x10070584
BasicStreambufChar* StreambufSetbuf(BasicStreambufChar* self, char* buffer, int32_t n) {              // 0x10070530
    (void)buffer; (void)n;
    return self;
}

// std::ios_base::failure::~failure with the hidden delete flag.
IME::MslExceptionObject* IosFailureDtor(IME::MslExceptionObject* self, int16_t flag) {   // 0x1007020c
    if (self != nullptr) {
        self->mVtbl = gIosFailureVtbl;
        IME::MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = IME::gMslExcVtblC;
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

const char* IosFailureWhat(const IME::MslExceptionObject* self) {   // 0x100702c8
    return self->mRc.mPtr;
}

}  // namespace v51054

// FLY_GetCameraMesh: word 0x11c of the fly base `tour` (1 to 99), else 0.
int32_t FLY_GetCameraMesh(int32_t tour) {   // 0x10075278
    if (tour > 0 && tour < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[tour]);
        if (base != nullptr) return *reinterpret_cast<int32_t*>(base + 0x11c);
    }
    return 0;
}

// HLSprite.cpp range of the main app (SimTheme Park 2.04b3): FLY_Init (0x100752d4, 244 bytes), the start of the flies. The
// globals are in src/hlsprite_flies.cpp.

// The first call (gFlyInitialised is 0) marks the flies running, empties the 100 base slots and makes the table of sines the
// flies' wings flap by: 4,096 words, entry i = (int)(1024 * sin(PI * 2i / 4096) + 0.5), with the game's own PI of 3.14159
// (not the exact one), so the table is 1024 sin(i * 2 PI' / 4096) rounded to the nearest. Later calls (before FLY_Close) do
// nothing. The multiply and add of the entry is one fused fmadd on the Mac.
void FLY_Init() {
    if (gFlyInitialised != 0) return;
    gFlyInitialised = 1;
    gFlyActive = 1;
    for (int32_t i = 0; i < 100; ++i) gFlyBases[i] = nullptr;
    gFlySinTable = LbMem_Alloc(0x4000);
    int32_t* table = static_cast<int32_t*>(gFlySinTable);
    for (int32_t i = 0; i < 0x1000; ++i) {
        const double angle = (3.14159 * static_cast<double>(i << 1)) / 4096.0;
        table[i] = ppc_fctiwz(ppc_fmadd(1024.0, std::sin(angle), 0.5));
    }
}

// Makes a fly base in the first free slot of gFlyBases (1 to 99): answers its index, or -1 when all are in use. `thingWord` carries the thing
// id in its high half. Like the original, a failed allocation is not checked.
int32_t FLY_CreateBase(uint32_t thingWord, int32_t handle, int32_t x, int32_t y, int32_t z, int32_t angle, int32_t a) {
    int32_t i = 1;
    while (i < 100 && gFlyBases[i] != nullptr) ++i;
    if (i >= 100) return -1;
    uint8_t* base = static_cast<uint8_t*>(LbMem_Alloc(0x122c));
    gFlyBases[i] = base;
    if (base != nullptr) {
        Wd(base, 0x2c) = 0;
        Wd(base, 0x14) = x;
        Wd(base, 0x18) = y;
        Wd(base, 0x1c) = z;
        Wd(base, 0) = 1;
        Wd(base, 4) = 1;
        Wd(base, 0xc) = 0;
        Wd(base, 0x10) = 300;
        Wd(base, 0x3c) = 0;
        Wd(base, 0x40) = 0x400 - angle;
        Wd(base, 0x44) = 0;
        Wd(base, 0x2c) = 1;
        Wd(base, 0x38) = 1;
        Wd(base, 0x48) = 0;
        Wd(base, 0x20) = 0x400;
        Wd(base, 0x24) = 0x400;
        const uint16_t thingNo = static_cast<uint16_t>(thingWord >> 16);
        std::memcpy(base + 0x58, &thingNo, 2);
        Wd(base, 0x4c) = a;
        Wd(base, 0x54) = handle;
        for (int32_t s = 0; s < 0x14; ++s) Wd(base, s * 0xe4 + 0xdc) = 0;
        Wd(base, 0x28) = 300;
        Wd(base, 0x50) = 5;
        EmployEric(i);
    }
    Wd(base, 0x30) = Wd(base, 0x4c) != 0 ? 0x9c4 : 0x1388;
    Wd(base, 0x34) = 1;
    return i;
}

// HLSprite.cpp range of the main app (SimTheme Park 2.04b3), unit w133: FLY_SetFlyZone (0x100755a0, 180 bytes).

int32_t gFlyZoneB = 0x11170;   // 0x1029d2b0
int32_t gFlyZoneA = 0x222e0;   // 0x1029d2b4
int32_t gFlyZoneD = 0x2e630;   // 0x1029d2ac
int32_t gFlyZoneC = 0x7530;    // 0x1029d2b8

// The fly zone from the hoarding extents: LLCR_GetHoardingExtents writes the four limits (to B, C, D and A, in that argument order), then
// B and C become (v * 10 + 0x23) * 300 and D and A (v * 10 - 0x23) * 300 (32-bit wrap-around).
void FLY_SetFlyZone() {
    LLCR_GetHoardingExtents(&gFlyZoneB, &gFlyZoneC, &gFlyZoneD, &gFlyZoneA);
    gFlyZoneB = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneB) * 10u + 0x23u) * 300u);
    gFlyZoneC = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneC) * 10u + 0x23u) * 300u);
    gFlyZoneD = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneD) * 10u - 0x23u) * 300u);
    gFlyZoneA = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneA) * 10u - 0x23u) * 300u);
}

// FLY_Tick 0x1007567c (9288 bytes, unit v5880): one tick of every fly of every fly base 1 to 99 (HLSprite.cpp).
// A base is LbMem_Alloc(0x122c) bytes of 32-bit words (include/HLSprite.h, sFlyBase); fly i starts at +0x5c + i * 0xe4 (the
// FLYBOY that GetNextWaypoint and GetNextHoldingWaypoint take; its state word at +0x80 is sFlySlot::mActive). Fields are read by
// their Mac offsets. The state is a jump table at 0x1029d2d0 (state 0 and above 9 do nothing); then the common motion, the
// avoidance of the other flies and the steering, and the sound position.
// Kept from the original: the turn delta folds by 0xfff (not 0x1000), the avoidance loop starts at the fly itself (only the
// flies after it are looked at), the landed test is made twice, the GAME_ASSERTMSG condition is SetCurRideAnim's r3.

// Initialised data of HLSprite.cpp (names invented): the lowest target heights of the approach (state 4 to 5), of the turn
// towards the base (state 4 to 6) and of the take-off (state 1 to 2).
int32_t gFlyApproachMinHeight = 0x36b0;   // 0x1029d2c4
int32_t gFlyTurnMinHeight = 0x36b0;       // 0x1029d2c8
int32_t gFlyTakeOffMinHeight = 0x36b0;    // 0x1029d2cc

namespace {

// The assertion text at 0x1023dc85.
const char kFlyAnimText[] = "FLY: Failed to trigger a flying car anim (%ld)";

int32_t& FW(uint8_t* p, uint32_t off) { return *reinterpret_cast<int32_t*>(p + off); }

int32_t SinT(int32_t a) { return static_cast<const int32_t*>(gFlySinTable)[static_cast<uint32_t>(a) & 0xfffu]; }
int32_t CosT(int32_t a) { return static_cast<const int32_t*>(gFlySinTable)[(static_cast<uint32_t>(a) + 0x400u) & 0xfffu]; }

int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t Shl(int32_t a, int n) { return static_cast<int32_t>(static_cast<uint32_t>(a) << n); }

// The Mac's labs on a 32-bit long (the most negative number stays negative).
int32_t Labs_2(int32_t v) { return v < 0 ? Sub(0, v) : v; }

// (int)sqrt((double)(a * a + b * b)), the sum in 32 bits.
int32_t Root2(int32_t a, int32_t b) {
    return ppc_fctiwz(std::sqrt(static_cast<double>(Add(Mul(a, a), Mul(b, b)))));
}

// The distance to the target in the ground plane, by 32nds, as the original: ((x - tx) / 32, (z - tz) / 32), root, times 32.
int32_t TargetDistance(uint8_t* fly) {
    return Shl(Root2(Sub(FW(fly, 4), FW(fly, 0x4c)) / 32, Sub(FW(fly, 0xc), FW(fly, 0x54)) / 32), 5);
}

// 652 * atan2(dz, dx), truncated: the heading in 4096ths of a turn.
int32_t Heading(int32_t dz, int32_t dx) {
    return ppc_fctiwz(652.0 * std::atan2(static_cast<double>(dz), static_cast<double>(dx)));
}

// The animation change the original inlines: only when the fly has a ride and its current one differs.
void SetFlyAnim(uint8_t* fly, uint32_t anim, uint32_t loop) {
    if (static_cast<uint32_t>(FW(fly, 0xc0)) == 0) return;
    if (FW(fly, 0xcc) == static_cast<int32_t>(anim) && FW(fly, 0xd0) == 0 && FW(fly, 0xd4) == static_cast<int32_t>(loop)) return;
    const uint32_t ride = static_cast<uint32_t>(FW(fly, 0xc0));
    int32_t done = 0;
    if (ride != 0) done = static_cast<int32_t>(SetCurRideAnimDuration(gRideAnimSets[ride], anim, 0, loop, 1.0f, 0));
    FW(fly, 0xcc) = static_cast<int32_t>(anim);
    FW(fly, 0xd0) = 0;
    FW(fly, 0xd4) = static_cast<int32_t>(loop);
    GAME_ASSERTMSG(done, kFlyAnimText, anim);
}

// The engine sound's parameter 10 (the event code asked even when the fly has no sound).
void SetFlySound(uint8_t* base, uint8_t* fly, uint32_t value) {
    const int32_t code = RSSE_GetSoundEventCode(FW(base, 0x54), 10);
    if (FW(fly, 0xe0) != 0) FW(fly, 0xe0) = SOUND_SetParamValue(FW(fly, 0xe0), static_cast<uint32_t>(code), value);
}

// Flying: animation 9 with sound 0x3c when +0xb8 is set, else 5 with 0x1e.
void FlyingAnim(uint8_t* base, uint8_t* fly) {
    if (FW(fly, 0xb8) != 0) {
        SetFlyAnim(fly, 9, 1);
        SetFlySound(base, fly, 0x3c);
    } else {
        SetFlyAnim(fly, 5, 1);
        SetFlySound(base, fly, 0x1e);
    }
}

int32_t Rand_2() { return Labs_2(static_cast<int32_t>(gGameWorld->GetRand())); }

}  // namespace

void FLY_Tick() {
    for (int32_t index = 1; index < 100; ++index) {
        uint8_t* const base = static_cast<uint8_t*>(gFlyBases[index]);
        if (base == nullptr) continue;
        if (FW(base, 0xc) < FW(base, 0)) EmployEric(index);
        for (int32_t i = 0; i < 20; ++i) {
            uint8_t* const fly = base + 0x5c + static_cast<uint32_t>(i) * 0xe4u;
            if (FW(fly, 0x80) == 0) continue;
            FW(fly, 0x1c) = FW(fly, 4);
            FW(fly, 0x20) = FW(fly, 8);
            FW(fly, 0x24) = FW(fly, 0xc);
            FW(fly, 0x28) = FW(fly, 0x10);
            FW(fly, 0x2c) = FW(fly, 0x14);
            FW(fly, 0x30) = FW(fly, 0x18);
            if (i > 0 && FW(base, 0xc) > FW(base, 0) && FW(base, 0xc) > 1) FW(fly, 0xb4) = 1;
            FLYBOY* const boy = reinterpret_cast<FLYBOY*>(fly);
            switch (static_cast<uint32_t>(FW(fly, 0x80))) {
            case 8:   // 0x100757b4: holding near the base
                FlyingAnim(base, fly);
                if (FW(fly, 0x8c) == 0 && FW(fly, 0xb4) != 0) {
                    FW(fly, 0x80) = 4;
                    GetNextHoldingWaypoint(boy);
                    FW(fly, 0x50) = 0x36b0;
                }
                if (TargetDistance(fly) < 5000) GetNextHoldingWaypoint(boy);
                if (FW(base, 0x2c) != 0) break;
                FW(base, 0x2c) = 1;
                if (FW(base, 0x44) != 0) {
                    FW(fly, 0xbc) = FW(base, 0x40);
                    FW(fly, 0x80) = 5;
                    FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(Mul(Sub(0, FW(base, 0x30)), 6), CosT(FW(fly, 0xbc))) / 1024);
                    const int32_t h = Add(FW(base, 0x18), 5000);
                    FW(fly, 0x50) = gFlyApproachMinHeight < h ? h : gFlyApproachMinHeight;
                    FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(Mul(Sub(0, FW(base, 0x30)), 6), SinT(FW(fly, 0xbc))) / 1024);
                } else {
                    FW(fly, 0xbc) = Heading(Sub(FW(base, 0x1c), FW(fly, 0xc)), Sub(FW(base, 0x14), FW(fly, 4)));
                    FW(fly, 0x80) = 6;
                    FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(Sub(0, FW(base, 0x30)), CosT(FW(fly, 0xbc))) / 1024);
                    const int32_t h = Add(FW(base, 0x18), 5000);
                    FW(fly, 0x50) = gFlyTurnMinHeight < h ? h : gFlyTurnMinHeight;
                    FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(Sub(0, FW(base, 0x30)), SinT(FW(fly, 0xbc))) / 1024);
                }
                break;
            case 5:   // 0x10075c40: the approach
                FlyingAnim(base, fly);
                if (TargetDistance(fly) < 5000) {
                    FW(fly, 0x80) = 6;
                    FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(Sub(0, FW(base, 0x30)), CosT(FW(fly, 0xbc))) / 1024);
                    FW(fly, 0x50) = Add(FW(base, 0x18), 5000);
                    FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(Sub(0, FW(base, 0x30)), SinT(FW(fly, 0xbc))) / 1024);
                }
                if (FW(fly, 0xb4) != 0 && FW(fly, 0x8c) == 0) {
                    FW(fly, 0x80) = 4;
                    FW(fly, 0x50) = 0x36b0;
                    FW(base, 0x2c) = 0;
                }
                break;
            case 1: {   // 0x10075f38: landed
                FW(fly, 0x14) = FW(fly, 0x14) / 2;
                SetFlySound(base, fly, 0x50);
                if (FW(base, 0x3c) != 0) break;
                if (FW(fly, 0xb8) != 0) break;
                const int32_t keep = FW(base, 0x38);
                if (keep == 0 && i == 0 && FW(base, 0xc) == 1) break;
                if (keep == 0 && i == 0 && FW(base, 0xc) == 1) break;   // the original tests it twice
                FW(fly, 0x80) = 2;
                const int32_t r = Rand_2();
                FW(fly, 0x7c) = Add(FW(base, 0x28), r % 100);
                FW(fly, 0x84) = FW(base, 0x10);
                const int32_t a = FW(base, 0x44) != 0 ? FW(base, 0x40) : FW(fly, 0x10);
                FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(CosT(a), 20000) / 1024);
                FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(SinT(a), 20000) / 1024);
                const int32_t tx = FW(fly, 0x4c);
                const int32_t tz = FW(fly, 0x54);
                int32_t ground;
                if (tx >= gFlyZoneD || tz >= gFlyZoneA || tx < gFlyZoneB || tz < gFlyZoneC) {
                    ground = 0;
                } else {
                    int32_t cx = tx / 300 / 10;
                    int32_t cz = tz / 300 / 10;
                    if (cx < 0) cx = 0;
                    if (cz < 0) cz = 0;
                    if (cx >= gHeightfieldWidth) cx = gHeightfieldWidth - 1;
                    if (cz >= gHeightfieldHeight) cz = gHeightfieldHeight - 1;
                    ground = Mul(ppc_fctiwz(GetWorldGSquareHeight(cx, cz, nullptr)), 300);
                }
                const int32_t above = Add(ground, 5000);
                const int32_t climb = Add(FW(base, 0x18), 5000);
                const int32_t h = climb < above ? above : climb;
                FW(fly, 0x50) = gFlyTakeOffMinHeight < h ? h : gFlyTakeOffMinHeight;
                break;
            }
            case 9:   // 0x10076260: leaving; gone at the target
                FlyingAnim(base, fly);
                if (TargetDistance(fly) >= 5000) break;
                FW(fly, 0x80) = 0;
                if (static_cast<uint32_t>(FW(fly, 0xc4)) != 0) {
                    PAR_KillEmitter(FW(fly, 0xc4));
                    FW(fly, 0xc4) = 0;
                }
                FW(fly, 0xc4) = 0;
                DestroyRideInstance(static_cast<uint32_t>(FW(fly, 0xc0)));
                FW(fly, 0xc0) = 0;
                FW(base, 0xc) = FW(base, 0xc) - 1;
                if (FW(fly, 0xe0) != 0) SOUND_StopSoundEffect(FW(fly, 0xe0));
                FW(fly, 0xe0) = 0;
                FW(fly, 0xe0) = 0;
                break;
            case 2: {   // 0x10076548: the take-off climb
                SetFlyAnim(fly, 4, 1);
                SetFlySound(base, fly, 0xf);
                if (FW(fly, 8) < FW(fly, 0x50)) break;
                FW(base, 0x2c) = 0;
                if (FW(fly, 0xb4) != 0) {
                    FW(fly, 0x80) = 4;
                    break;
                }
                FW(fly, 0x80) = 3;
                const int32_t x = FW(fly, 0x4c);
                int32_t gx = Mul(x / 20000, 20000);
                if (x < 0) gx = Sub(gx, 20000);
                FW(fly, 0x58) = Sub(gx, 20000);
                FW(fly, 0x5c) = FW(fly, 0x50);
                const int32_t z = FW(fly, 0x54);
                int32_t gz = Mul(z / 20000, 20000);
                if (z < 0) gz = Sub(gz, 20000);
                FW(fly, 0x60) = gz;
                break;
            }
            case 3:   // 0x10076718: wandering over the park
                FlyingAnim(base, fly);
                if (FW(fly, 0xb8) != 0 && FW(base, 0x34) == 3) FW(fly, 0x84) = 0;
                if (FW(fly, 0xb8) != 0) FW(fly, 0x7c) = FW(base, 0x28) / 2;
                if (FW(fly, 0x84) <= 0 || FW(fly, 0xb4) != 0) {
                    FW(fly, 0x80) = 4;
                    GetNextWaypoint(boy);
                    FW(fly, 0x50) = 0x36b0;
                    break;
                }
                if (TargetDistance(fly) < 5000) GetNextWaypoint(boy);
                break;
            case 4:   // 0x100769e0: the turn towards the base
                FlyingAnim(base, fly);
                if (TargetDistance(fly) >= 5000) break;
                if (FW(fly, 0xb4) != 0 && FW(fly, 0x8c) == 0) {
                    FW(fly, 0x80) = 9;
                    FW(fly, 0x50) = 0x36b0;
                    if ((Rand_2() & 1) != 0) {
                        const int32_t m = Mul(Rand_2(), 100);
                        const int32_t span = Sub(gFlyZoneD, gFlyZoneB);
                        FW(fly, 0x4c) = Add(gFlyZoneB, Sub(m, Mul(m / span, span)));
                        if ((Rand_2() & 1) != 0)
                            FW(fly, 0x54) = Add(gFlyZoneA, 0x10000 - 0x5038);
                        else
                            FW(fly, 0x54) = Add(gFlyZoneC, -0x10000 + 0x5038);
                    } else {
                        const int32_t m = Mul(Rand_2(), 100);
                        const int32_t span = Sub(gFlyZoneA, gFlyZoneC);
                        FW(fly, 0x54) = Add(gFlyZoneC, Sub(m, Mul(m / span, span)));
                        if ((Rand_2() & 1) != 0)
                            FW(fly, 0x4c) = Add(gFlyZoneD, 0x10000 - 0x5038);
                        else
                            FW(fly, 0x4c) = Add(gFlyZoneB, -0x10000 + 0x5038);
                    }
                    FW(fly, 0x7c) = Shl(FW(base, 0x28), 1);
                } else {
                    FW(fly, 0x80) = 8;
                    GetNextHoldingWaypoint(boy);
                }
                break;
            case 6:   // 0x10076db4: the final approach; then down to the base
                FlyingAnim(base, fly);
                if (TargetDistance(fly) >= 5000) break;
                FW(fly, 0x80) = 7;
                FW(fly, 0x7c) = FW(base, 0x4c) != 0 ? FW(base, 0x28) / 2 : FW(base, 0x28);
                FW(fly, 0x4c) = FW(base, 0x14);
                FW(fly, 0x50) = FW(base, 0x18);
                FW(fly, 0x54) = FW(base, 0x1c);
                break;
            case 7:   // 0x10077044: landing
                if (FW(fly, 8) <= FW(base, 0x18)) {
                    FW(fly, 8) = FW(base, 0x18);
                    FW(fly, 0x80) = 1;
                    FW(base, 0x3c) = 1;
                    FW(fly, 0x18) = 0;
                    FW(fly, 0x7c) = 0;
                    FW(fly, 0x74) = 0;
                    if (FW(fly, 0x8c) > 0) FW(fly, 0x8c) = Sub(0, FW(fly, 0x8c));
                    SetFlyAnim(fly, 3, 0);
                    SetFlySound(base, fly, 0x50);
                } else {
                    SetFlyAnim(fly, 6, 0);
                    SetFlySound(base, fly, FW(fly, 0xb8) != 0 ? 0x3c : 0x1e);
                }
                break;
            default:
                break;
            }

            // 0x1007728c: the motion.
            FW(fly, 0x78) = Mul(FW(fly, 0x78), FW(fly, 0x80) == 1 ? 500 : 900) / 1024;
            FW(fly, 0x70) = Mul(FW(fly, 0x70), 31) / 32;
            FW(fly, 0x84) = Sub(FW(fly, 0x84), 1);
            {
                const int32_t s = FW(fly, 0x7c);
                const int32_t t = Mul(s, FW(base, 0x50)) / 10;
                FW(fly, 0x78) = Add(FW(fly, 0x78), Add(s, t) / 10);
            }
            FW(fly, 0x64) = Mul(FW(fly, 0x78), CosT(FW(fly, 0x10))) / 1024;
            FW(fly, 0x6c) = Mul(FW(fly, 0x78), SinT(FW(fly, 0x10))) / 1024;
            FW(fly, 4) = Add(FW(fly, 4), FW(fly, 0x64));
            FW(fly, 0xc) = Add(FW(fly, 0xc), FW(fly, 0x6c));

            if (FW(fly, 0x80) == 1) {
                if (FW(fly, 0x14) > 0) FW(fly, 0x14) = Sub(FW(fly, 0x14), 0x32);
            } else {
                // The other flies (from this one on) that are flying: push apart in height and speed.
                for (int32_t j = i; j < 20; ++j) {
                    if (j == i) continue;
                    uint8_t* const other = base + 0x5c + static_cast<uint32_t>(j) * 0xe4u;
                    const int32_t st = FW(other, 0x80);
                    if (st == 0 || st == 1) continue;
                    const int32_t d2 = Shl(Root2(Sub(FW(fly, 4), FW(other, 4)) / 32, Sub(FW(fly, 0xc), FW(other, 0xc)) / 32), 5);
                    if (d2 < Mul(FW(base, 0x20), 15000) / 1024 &&
                        Labs_2(Sub(FW(other, 8), FW(fly, 8))) < Mul(FW(base, 0x20), 0xc80) / 1024) {
                        if (FW(fly, 0x80) == 7) {
                            FW(other, 0x74) = Add(FW(other, 0x74), 0x28);
                        } else if (FW(other, 0x80) == 7) {
                            FW(fly, 0x74) = Add(FW(fly, 0x74), 0x28);
                        } else {
                            if (FW(fly, 0x78) > FW(other, 0x78))
                                FW(other, 0x78) = Sub(FW(other, 0x78), FW(other, 0x78) / 16);
                            else
                                FW(fly, 0x78) = Sub(FW(fly, 0x78), FW(fly, 0x78) / 16);
                            if (FW(fly, 8) > FW(other, 8))
                                FW(fly, 0x74) = Add(FW(fly, 0x74), 0x28);
                            else if (i < j)
                                FW(other, 0x74) = Add(FW(other, 0x74), 0x28);
                        }
                    }
                    const int32_t dx = Sub(FW(fly, 4), FW(other, 4)) / 32;
                    const int32_t dy = Sub(FW(fly, 8), FW(other, 8)) / 32;
                    const int32_t dz = Sub(FW(fly, 0xc), FW(other, 0xc)) / 32;
                    const int32_t sum = Add(Mul(dx, dx), Add(Mul(dy, dy), Mul(dz, dz)));
                    const int32_t d3 = Shl(ppc_fctiwz(std::sqrt(static_cast<double>(sum))), 5);
                    if (d3 < Mul(FW(base, 0x20), 3000) / 1024) {
                        if (FW(fly, 8) > FW(other, 8))
                            FW(fly, 8) = Add(FW(fly, 8), 500);
                        else
                            FW(other, 8) = Add(FW(other, 8), 500);
                        FW(fly, 0x18) = Add(FW(fly, 0x18), 1000);
                        FW(other, 0x18) = Sub(FW(other, 0x18), 1000);
                    }
                }

                // Steering towards the target.
                const int32_t heading = Heading(Sub(FW(fly, 0x54), FW(fly, 0xc)), Sub(FW(fly, 0x4c), FW(fly, 4)));
                int32_t turn = static_cast<int32_t>(static_cast<uint32_t>(Sub(heading, FW(fly, 0x10))) & 0xfffu);
                if (turn >= 0x800) turn -= 0xfff;
                FW(fly, 0x70) = Add(turn, FW(fly, 0x70)) / 2;
                FW(fly, 0x10) = static_cast<int32_t>(static_cast<uint32_t>(Add(FW(fly, 0x10), FW(fly, 0x70) / 10)) & 0xfffu);

                if (FW(fly, 0x80) == 7) {
                    if (FW(fly, 0x14) < 0x3c) FW(fly, 0x14) = FW(fly, 0x14) + 2;
                    const int32_t d = Root2(Sub(FW(fly, 4), FW(fly, 0x4c)) / 32, Sub(FW(fly, 0xc), FW(fly, 0x54)) / 32);
                    const int32_t lift = Shl(d, FW(base, 0x4c) != 0 ? 6 : 5);
                    FW(fly, 8) = Add(FW(base, 0x18), Sub(Add(Mul(FW(fly, 8), 7), lift), 3000)) / 8;
                } else {
                    const int32_t k = Labs_2(Sub(FW(fly, 0x50), FW(fly, 8))) > 10000 ? 3 : 1;
                    int32_t v = Sub(FW(fly, 0x50), FW(fly, 8)) / 20;
                    if (v > 0x10) v = 0x10;
                    if (v < -0x10) v = -0x10;
                    FW(fly, 0x74) = Add(FW(fly, 0x74), Mul(v, k));
                    if (FW(fly, 0x74) > 0x200) FW(fly, 0x74) = 0x200;
                    if (FW(fly, 0x74) < -0x200) FW(fly, 0x74) = -0x200;
                    FW(fly, 0x74) = Mul(FW(fly, 0x74), 7) / 8;
                    FW(fly, 8) = Add(FW(fly, 8), Mul(FW(fly, 0x74), 3));
                    FW(fly, 0x14) = Mul(FW(base, 0x24), FW(fly, 0x74)) / 1024;
                }

                // The roll follows the turn rate, eased by eighths on the short way round.
                const int32_t rate = FW(fly, 0x70);
                int32_t roll = static_cast<int32_t>(
                    static_cast<uint32_t>(Mul(FW(base, 0x24), Sub(Sub(0, rate) / 2, rate / 4)) / 1024) & 0xfffu);
                const int32_t cur = static_cast<int32_t>(static_cast<uint32_t>(FW(fly, 0x18)) & 0xfffu);
                const int32_t diff = roll - cur;
                if (diff > 0x800)
                    roll -= 0x1000;
                else if (diff < -0x800)
                    roll += 0x1000;
                FW(fly, 0x18) = static_cast<int32_t>(static_cast<uint32_t>((roll + cur * 7) / 8) & 0xfffu);
            }

            if (FW(fly, 0xe0) != 0)
                FW(fly, 0xe0) = SOUND_MoveSoundEffect(FW(fly, 0xe0), FW(fly, 4) / 300, FW(fly, 8) / 300, FW(fly, 0xc) / 300);
        }
    }
}

// FLY_Draw (HLSprite.cpp, 0x10077ae4, 1776 bytes): moves every active slot of the fly bases one step and poses its ride and emitter.

// The flies' working registers (bss; the original names are unknown): position x y z, then three angles (twelve-bit units).
float gFlyDrawPosX;      // 0x102da960
float gFlyDrawPosY;      // 0x102da95c
float gFlyDrawPosZ;      // 0x102da958
float gFlyDrawAngA;      // 0x102da954
float gFlyDrawAngB;      // 0x102da950
float gFlyDrawAngC;      // 0x102da94c
uint32_t gFlyDrawOrigin[3];   // 0x1029d2f8

namespace {

inline int32_t& W_4(uint8_t* p, uint32_t off) { return *reinterpret_cast<int32_t*>(p + off); }
inline float& Fl(uint8_t* p, uint32_t off) { return *reinterpret_cast<float*>(p + off); }

sMatrix* RideMatrix(int32_t ride) {
    tag_sNode* node = gRideAnimSets[ride]->mHeads->mHeader->mF78.get();
    return &node->mMatrix;
}

// The angle step: (new + old * (k - 1)) / k on twelve-bit angles, taking the short way round; k is 4 or 8.
float Blend(float target, float current, int32_t k) {
    const int32_t a = FLY_FloatToInt(target);
    const int32_t b = FLY_FloatToInt(current);
    const int32_t lo = a & 0xfff;
    int32_t hi = b & 0xfff;
    const int32_t diff = hi - lo;
    if (diff > 0x800) hi -= 0x1000;
    else if (diff < -0x800) hi += 0x1000;
    const int32_t sum = hi + lo * (k - 1);
    return static_cast<float>((sum / k) & 0xfff);
}

}  // namespace

void FLY_Draw(float step) {
    if (step > 2.0f) step = 2.0f;
    for (int32_t i = 1; i < 100; ++i) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[i]);
        if (base == nullptr) continue;
        for (int32_t n = 0; n < 20; ++n) {
            uint8_t* p = base + static_cast<uint32_t>(n) * 0xe4 + 0x5c;
            if (W_4(p, 0x80) == 0) continue;
            if (W_4(p, 0xc8) != 0) {
                gFlyDrawPosX = static_cast<float>(W_4(p, 4)) / 300.0f;
                gFlyDrawPosY = static_cast<float>(W_4(p, 8)) / 300.0f;
                gFlyDrawPosZ = static_cast<float>(W_4(p, 0xc)) / 300.0f;
                gFlyDrawAngA = static_cast<float>(W_4(p, 0x10));
                gFlyDrawAngB = static_cast<float>(W_4(p, 0x18));
                gFlyDrawAngC = static_cast<float>(W_4(p, 0x14));
                W_4(p, 0xc8) = 0;
            } else {
                gFlyDrawPosX = ppc_fmadds(step, static_cast<float>(W_4(p, 4) - W_4(p, 0x1c)), static_cast<float>(W_4(p, 0x1c))) / 300.0f;
                gFlyDrawPosY = ppc_fmadds(step, static_cast<float>(W_4(p, 8) - W_4(p, 0x20)), static_cast<float>(W_4(p, 0x20))) / 300.0f;
                gFlyDrawPosZ = ppc_fmadds(step, static_cast<float>(W_4(p, 0xc) - W_4(p, 0x24)), static_cast<float>(W_4(p, 0x24))) / 300.0f;
                int32_t t = (W_4(p, 0x10) - W_4(p, 0x28)) & 0xfff;
                if (t >= 0x800) t -= 0xfff;
                gFlyDrawAngA = ppc_fmadds(step, static_cast<float>(t), static_cast<float>(W_4(p, 0x28)));
                t = (W_4(p, 0x18) - W_4(p, 0x30)) & 0xfff;
                if (t >= 0x800) t -= 0xfff;
                gFlyDrawAngB = ppc_fmadds(step, static_cast<float>(t), static_cast<float>(W_4(p, 0x30)));
                t = (W_4(p, 0x14) - W_4(p, 0x2c)) & 0xfff;
                if (t >= 0x800) t -= 0xfff;
                gFlyDrawAngC = ppc_fmadds(step, static_cast<float>(t), static_cast<float>(W_4(p, 0x2c)));
                gFlyDrawPosX = ppc_fmadds(3.0f, Fl(p, 0x34), gFlyDrawPosX) * 0.25f;
                gFlyDrawPosY = ppc_fmadds(3.0f, Fl(p, 0x38), gFlyDrawPosY) * 0.25f;
                gFlyDrawPosZ = ppc_fmadds(3.0f, Fl(p, 0x3c), gFlyDrawPosZ) * 0.25f;
                gFlyDrawAngA = Blend(Fl(p, 0x48), gFlyDrawAngA, 4);
                gFlyDrawAngB = Blend(Fl(p, 0x40), gFlyDrawAngB, 4);
                gFlyDrawAngC = Blend(Fl(p, 0x44), gFlyDrawAngC, 8);
            }
            Fl(p, 0x34) = gFlyDrawPosX;
            Fl(p, 0x38) = gFlyDrawPosY;
            Fl(p, 0x3c) = gFlyDrawPosZ;
            Fl(p, 0x48) = gFlyDrawAngA;
            Fl(p, 0x40) = gFlyDrawAngB;
            Fl(p, 0x44) = gFlyDrawAngC;
            float v[3];
            v[0] = gFlyDrawOrigin[0];
            v[1] = gFlyDrawOrigin[1];
            v[2] = gFlyDrawOrigin[2];
            const int32_t ride = W_4(p, 0xc0);
            if (static_cast<uint32_t>(ride) == 0) continue;
            SetLMatrixIdentityNoTrans(RideMatrix(ride));
            sMatrix* m = RideMatrix(ride);
            m->m[3][0] = gFlyDrawPosX;
            m->m[3][1] = gFlyDrawPosY;
            m->m[3][2] = gFlyDrawPosZ;
            const float pi = 3.1415927f;
            v[0] = pi * (2.0f * -gFlyDrawAngB) / 4096.0f;
            v[2] = pi + pi * (2.0f * (3072.0f - gFlyDrawAngA)) / 4096.0f;
            v[1] = pi * (2.0f * (4.0f * -gFlyDrawAngC)) / 4096.0f;
            RotateLMatrixXYZNoTrans(RideMatrix(ride), static_cast<eRotateParams>(0xe), v);
            if (static_cast<uint32_t>(W_4(p, 0xc4)) == 0) continue;
            const uint32_t index = FindDataNodeIndexByID(gRideAnimSets[ride], 0x100, 1);
            if (index == 0xffffffffu) continue;
            sEmbeddedSet* set = static_cast<sEmbeddedSet*>(gRideAnimSets[ride]->mHeads->mF28);
            uint8_t* block = set->mRecords[index].mBlock;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (block != nullptr) {
                x = Fl(block, 0x30);
                y = Fl(block, 0x34);
                z = Fl(block, 0x38);
            }
            const int32_t ix = static_cast<int32_t>(x);
            const int32_t iy = static_cast<int32_t>(y);
            const int32_t iz = static_cast<int32_t>(z);
            PAR_SetEmitterPos(W_4(p, 0xc4), static_cast<int32_t>(static_cast<uint32_t>(ix) << 10),
                              static_cast<int32_t>(static_cast<uint32_t>(iy) << 10), static_cast<int32_t>(static_cast<uint32_t>(iz) << 10));
        }
    }
}

// HLSprite.cpp (main app 0x100781f4, unit w114): FLY_RemoveBase, the removal of one fly base.

// Removes the fly base `index` (1 to 99; anything else, or an empty slot of gFlyBases, does nothing). Each of the base's 20 slots with a
// nonzero active word is torn down: |count| times (counting down to 1) the ride's head with the number ((n + offset) mod divisor) + 1 is
// found in the ride set's embedded data (mask 0x80) and disabled, when the divisor and the ride are not 0; then the emitter is killed
// and cleared, the ride instance destroyed and cleared, the base's live count goes down by one and the sound is stopped and cleared.
// Then the base is freed and its slot cleared. A slot with a zero active word is skipped whole.
//
// Quirks kept: the emitter and the sound words are cleared twice (the second store of each is unconditional); the active word is never
// cleared; the head number's remainder is a signed one (divw), the sum wraps in 32 bits; |count| is the C library's labs, so INT_MIN
// loops 2^32 times (and the remainder of INT_MIN by -1 is undefined): neither is tested. The base pointer is read from the table again
// at each use, as the original does.
void FLY_RemoveBase(int32_t index) {
    if (index <= 0 || index >= 100 || gFlyBases[index] == nullptr) return;
    for (int32_t slot = 0; slot < kFlySlotCount; ++slot) {
        sFlyBase* base = static_cast<sFlyBase*>(gFlyBases[index]);
        sFlySlot* s = base->Slot(slot);
        if (s->mActive == 0) continue;
        uint32_t n = s->mCount < 0 ? 0u - static_cast<uint32_t>(s->mCount) : static_cast<uint32_t>(s->mCount);
        for (; n != 0; n = n - 1) {
            if (s->mDivisor != 0) {
                const int32_t sum = static_cast<int32_t>(n + static_cast<uint32_t>(s->mOffset));
                const int32_t quotient = sum / s->mDivisor;
                const int32_t remainder =
                    static_cast<int32_t>(static_cast<uint32_t>(sum) - static_cast<uint32_t>(quotient) * static_cast<uint32_t>(s->mDivisor));
                if (s->mRide != 0) {
                    const uint32_t head = FindDataNodeIndexByID(gRideAnimSets[s->mRide], 0x80, static_cast<uint32_t>(remainder) + 1);
                    if (head != 0xffffffff) DisableRideHead(gRideAnimSets[s->mRide]->mHeads, head);
                }
            }
        }
        if (s->mEmitter != 0) {
            PAR_KillEmitter(static_cast<int32_t>(s->mEmitter));
            s->mEmitter = 0;
        }
        s->mEmitter = 0;
        DestroyRideInstance(s->mRide);
        s->mRide = 0;
        base->mLive = base->mLive - 1;
        if (s->mSound != 0) SOUND_StopSoundEffect(static_cast<int32_t>(s->mSound));
        s->mSound = 0;
        s->mSound = 0;
    }
    LbMem_Free(gFlyBases[index]);
    gFlyBases[index] = nullptr;
}

// HLSprite.cpp range of the main app (SimTheme Park 2.04b3): the flies' shutdown. FLY_Init (0x100752d4, untranslated) makes the
// sine table and the bases; the globals are defined here with the first function that uses them.

int32_t gFlyActive = 0;           // 0x102daaf4
int32_t gFlyInitialised = 0;      // 0x102daaf8
void* gFlySinTable = nullptr;     // 0x102daafc
void* gFlyBases[100] = {};        // 0x102da964

// Frees the sine table and removes the bases 1 to 99 (not 0), when FLY_Init has run; then FLY_Init may run again.
void FLY_Close() {
    if (gFlyInitialised != 0) {
        LbMem_Free(gFlySinTable);
        gFlySinTable = nullptr;
        for (int32_t i = 1; i < 100; ++i) FLY_RemoveBase(i);
        gFlyInitialised = 0;
    }
}

// The fly bases' peep handling and repair state (unit v5980): FLY_ForceExtractPeep 0x1007861c, FLY_ExtractPeep 0x1007848c,
// FLY_AddPeep 0x100787c8, FLY_SetRepairState 0x10078b40. A fly base is words only; offsets below are the Mac's.

using processinstruction_callee::Labs;

namespace {

int32_t& W_2(uint8_t* p, uint32_t off) { return *reinterpret_cast<int32_t*>(p + off); }

constexpr uint32_t kSlotCount = 0x14;
constexpr uint32_t kStride = 0xe4;

// The head the slot's ride shows for peep number n: the mod of (n + offset) by the divisor, plus 1, found in the ride's set (0xffffffff: none).
// Answers true with the index when the slot has a divisor and a ride (the callers act only then).
bool HeadFor(uint8_t* slot, int32_t n, uint32_t* index, tag_sRideAnimSet** set) {
    const int32_t divisor = W_2(slot, 0x138);
    if (divisor == 0) return false;
    const int32_t sum = n + W_2(slot, 0x134);
    const int32_t rem = sum - (sum / divisor) * divisor;
    const uint32_t ride = static_cast<uint32_t>(W_2(slot, 0x11c));
    if (ride == 0) return false;
    *set = gRideAnimSets[ride];
    *index = FindDataNodeIndexByID(*set, 0x80, static_cast<uint32_t>(rem + 1));
    return *index != 0xffffffffu;
}

}  // namespace

int32_t FLY_ExtractPeep(int32_t fly) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr) {
            for (uint32_t i = 0; i < kSlotCount; ++i) {
                const uint32_t s = i * kStride;
                if (W_2(base, s + 0xdc) == 1) {
                    const int32_t count = W_2(base, s + 0xe8);
                    if (count >= 0) return 0;
                    uint32_t index;
                    tag_sRideAnimSet* set;
                    if (HeadFor(base + s, -count, &index, &set)) DisableRideHead(set->mHeads, index);
                    W_2(base, s + 0xe8) = W_2(base, s + 0xe8) + 1;
                    W_2(base, 0x48) -= 1;
                    return W_2(base, s + 0xec + static_cast<uint32_t>(-W_2(base, s + 0xe8)) * 4);
                }
            }
        }
    }
    return 0;
}

int32_t FLY_ForceExtractPeep(int32_t fly) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr) {
            for (uint32_t i = 0; static_cast<int32_t>(i) < static_cast<int32_t>(kSlotCount); ++i) {
                const uint32_t s = i * kStride;
                if (W_2(base, s + 0xe8) > 0) {
                    const int32_t n = Labs(W_2(base, s + 0xe8));
                    uint32_t index;
                    tag_sRideAnimSet* set;
                    if (HeadFor(base + s, n, &index, &set)) DisableRideHead(set->mHeads, index);
                    int32_t& count = W_2(base, s + 0xe8);
                    if (count > 0) count = count - 1; else count = count + 1;
                    W_2(base, 0x48) -= 1;
                    const int32_t m = Labs(W_2(base, s + 0xe8));
                    return W_2(base, s + 0xec + static_cast<uint32_t>(m) * 4);
                }
            }
        }
    }
    return 0;
}

int32_t FLY_AddPeep(int32_t fly, int32_t peep) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr) {
            for (uint32_t i = 0; i < kSlotCount; ++i) {
                const uint32_t s = i * kStride;
                if (W_2(base, s + 0xdc) == 1) {
                    const int32_t count = W_2(base, s + 0xe8);
                    if (count < 0) return 0;
                    W_2(base, s + 0xec + static_cast<uint32_t>(count) * 4) = peep;
                    W_2(base, s + 0xe8) = W_2(base, s + 0xe8) + 1;
                    uint32_t index;
                    tag_sRideAnimSet* set;
                    if (W_2(base, s + 0x138) != 0) {
                        // the head number uses the count after the increment, read back from the base
                        if (HeadFor(base + s, W_2(base, s + 0xe8), &index, &set)) EnableRideHead(set->mHeads, index, static_cast<uint32_t>(peep));
                    }
                    W_2(base, 0x48) += 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

// FLY ride-base accessors on gFlyBases (HLSprite.cpp): the fly handle indexes the table (1 to 99), a NULL slot is ignored.
// The base structure has no declared layout; fields are read by Mac offset (all words).

namespace {

inline int32_t& W_3(void* base, int32_t offset) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + offset); }

inline void* BaseOf(int32_t fly) {
    if (fly <= 0 || fly >= 100) return nullptr;
    return gFlyBases[fly];
}

}  // namespace

// 0x10078958: void; time / 64 truncating toward zero, then clamps the 20 entries' word at 0xe0.
void FLY_SetRunTime(int32_t fly, int32_t time) {
    void* b = BaseOf(fly);
    if (b == nullptr) return;
    int32_t t = time / 64;
    W_3(b, 0x10) = t;
    for (int32_t i = 0; i < 0x14; ++i) {
        int32_t& v = W_3(static_cast<uint8_t*>(b) + i * 0xe4, 0xe0);
        if (v > t) v = t;
    }
}
// 0x100789ec: void in the original; signed divide by 10 (mulhw trick).
void FLY_SetRunSpeed(int32_t fly, int32_t speed) {
    void* b = BaseOf(fly);
    if (b == nullptr) return;
    W_3(b, 0x50) = speed / 10;
}

// 0x10078a58: void in the original.
int32_t FLY_SetOpenClosed(int32_t fly, int32_t a) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    if (a != 0) {
        W_3(b, 0) = W_3(b, 4);
        W_3(b, 0x38) = 1;
        return 0;
    }
    if (W_3(b, 0x34) == 3) return 0;
    W_3(b, 0) = 1;
    for (int32_t i = 0; i < 0x14; ++i) {
        uint8_t* e = static_cast<uint8_t*>(b) + i * 0xe4;
        if (W_3(e, 0xdc) != 0) W_3(e, 0xe0) = 0;
        if (W_3(e, 0xdc) == 1) {
            int32_t v = W_3(e, 0xe8);
            if (v > 0) W_3(e, 0xe8) = -v;
        }
        W_3(b, 0x38) = 0;
    }
    return 0;
}

namespace {

int32_t ThirdOf(int32_t v) { return v / 300; }
int32_t Scaled(int32_t v) { return static_cast<int32_t>(static_cast<uint32_t>(v) << 10) / 300; }

void StartCrash(uint8_t* base) {
    W_2(base, 0x114) = 1;
    W_2(base, 0xe0) = 0;
    if (W_2(base, 0x120) != 0) PAR_KillEmitter(W_2(base, 0x120));
    const int32_t a = Scaled(W_2(base, 0x60));
    const int32_t b = Scaled(W_2(base, 0x64));
    const int32_t c = Scaled(W_2(base, 0x68));
    W_2(base, 0x120) = static_cast<int32_t>(PAR_InitEmitter(0x28, a, b, c));
}

}  // namespace

int32_t FLY_SetRepairState(int32_t fly, int32_t state) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr && state != W_2(base, 0x34)) {
            W_2(base, 0x34) = state;
            if (state == 3) {
                StartCrash(base);
            } else if (state < 3) {
                if (state == 1) {
                    W_2(base, 0) = W_2(base, 4);
                    if (W_2(base, 0x114) != 0) {
                        const uint32_t ride = static_cast<uint32_t>(W_2(base, 0x11c));
                        if (ride != 0 && (W_2(base, 0x128) != 10 || W_2(base, 0x12c) != 0 || W_2(base, 0x130) != 0)) {
                            const int32_t answer = static_cast<int32_t>(SetCurRideAnimDuration(gRideAnimSets[ride], 10, 0, 0, 1.0f, 0));
                            W_2(base, 0x128) = 10;
                            W_2(base, 0x12c) = 0;
                            W_2(base, 0x130) = 0;
                            GAME_ASSERTMSG(answer, "FLY: Failed to trigger a flying car anim (%ld)", static_cast<int32_t>(10));
                        }
                        W_2(base, 0x114) = 0;
                        SOUND_SendEvent(0, gSoundBanks[2], 0xbb, ThirdOf(W_2(base, 0x60)), ThirdOf(W_2(base, 0x64)), ThirdOf(W_2(base, 0x68)));
                    }
                    if (W_2(base, 0xd8) != 0) {
                        const int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand()));
                        W_2(base, 0xd8) = W_2(base, 0x28) + (r - (r / 100) * 100);
                    }
                    if (W_2(base, 0x120) != 0) {
                        PAR_KillEmitter(W_2(base, 0x120));
                        W_2(base, 0x120) = 0;
                    }
                } else if (state > 1) {
                    W_2(base, 0) = W_2(base, 4);
                    if (W_2(base, 0x120) != 0) PAR_KillEmitter(W_2(base, 0x120));
                    W_2(base, 0x120) = static_cast<int32_t>(PAR_InitEmitter(0x27, W_2(base, 0x60), W_2(base, 0x64), W_2(base, 0x68)));
                }
            } else if (state < 5) {
                StartCrash(base);
                while (FLY_ForceExtractPeep(fly) != 0) {
                }
                W_2(base, 0) = 1;
            }
        }
    }
    return 0;
}

// 0x10078ee4: the original is void (r3 is whatever was left); the header says int32_t.
int32_t FLY_SetNumberOfCars(int32_t fly, int32_t a) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    W_3(b, 4) = a;
    if (W_3(b, 0x38) == 0) return 0;
    W_3(b, 0) = a;
    return 0;
}

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

int32_t FLY_GetCarInBase(int32_t fly) {
    if (fly > 0 && fly < 100) {
        void* base = gFlyBases[fly];
        if (base != nullptr) return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + 0x3c);
    }
    return 0;
}

// 0x10078fb0
int32_t FLY_GetBrokenStat(int32_t fly) {
    void* b = BaseOf(fly);
    if (b != nullptr && W_3(b, 0x114) != 0 && W_3(b, 0xdc) == 1) return 1;
    return 0;
}

// 0x10079024
int32_t FLY_LaunchCar(int32_t fly) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    if (W_3(b, 0x38) == 0 && W_3(b, 0x48) == 0 && W_3(b, 0xc) <= 1) return 0;
    if (W_3(b, 0x3c) != 0) {
        W_3(b, 0x3c) = 0;
        return 1;
    }
    return 0;
}

int32_t FLY_SetRunway(int32_t fly, int32_t a) {
    if (fly <= 0 || fly >= 100) return fly;
    void* base = gFlyBases[fly];
    if (base == nullptr) return fly;
    *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + 0x44) = a;
    return fly;   // the original leaves the base pointer in r3 here; nobody reads the answer
}

// 0x10079114
int32_t FLY_SetCarSize(int32_t fly, int32_t size) {
    if (fly <= 0 || fly >= 100) return 0;
    uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
    if (base == nullptr) return 0;
    std::memcpy(base + 0x20, &size, 4);
    return 0;
}
// 0x10079168 (the original leaves r3 as it was; the answer is not meaningful)
int32_t FLY_SetMotionScale(int32_t fly, int32_t scale) {
    if (fly <= 0 || fly >= 100) return 0;
    uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
    if (base == nullptr) return 0;
    std::memcpy(base + 0x24, &scale, 4);
    return 0;
}

// 0x100791c0
int32_t FLY_RideRunning(int32_t fly) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    if (W_3(b, 0x48) == 0 && W_3(b, 0x3c) != 0 && W_3(b, 0xc) == 1) return 0;
    return 1;
}

// 0x10079248 (1240 bytes): FLY_Save writes the fly file: the magic 'FLY_', the initialised flag, and when it is set the table of bases (400
// raw bytes: on the Mac 100 32-bit pointers; the original writes them as they are), the four zone limits, five 'DAP_' words, 0x122c, and for
// bases 1 to 99 that are set a swapped copy of the 0x122c-byte base. Every word goes through _USwapCopy32 and its own LbFile_Write.
// Always answers 0.

namespace {

constexpr uint32_t kBaseSize = 0x122c;

// _USwapCopy32 of one word, one LbFile_Write of 4 bytes, _USwapDelete.
void WriteWord(void* file, const uint32_t* word) {
    uint32_t* copy = SamsUtilities::_USwapCopy32(word, 1);
    uint32_t transferred;
    LbFile_Write(file, copy, 4, &transferred);
    SamsUtilities::_USwapDelete(copy);
}

}  // namespace

// Declared before its first use: the definition is further down.
template<> void tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>::Byteswap();

int32_t FLY_Save(void* file) {
    uint32_t word = 0x5f594c46;
    WriteWord(file, &word);
    WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyInitialised));
    if (gFlyInitialised != 0) {
        word = 0x190;
        WriteWord(file, &word);
        LbFile_Write(file, gFlyBases, word, nullptr);   // the table as it is in memory (the original's length is the word just written)
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneD));
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneB));
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneA));
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneC));
        word = 0x5f444150;
        for (int32_t k = 0; k < 5; ++k) WriteWord(file, &word);
        word = kBaseSize;
        WriteWord(file, &word);
        for (int32_t i = 1; i < 100; ++i) {
            if (gFlyBases[i] == nullptr) continue;
            alignas(8) uint8_t buffer[kBaseSize];
            std::memcpy(buffer, gFlyBases[i], kBaseSize);
            for (uint32_t offset = 0; offset <= 0x54; offset += 4)
                SamsUtilities::USwapInplace32<int32_t>(*reinterpret_cast<int32_t*>(buffer + offset));
            reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>*>(buffer + 0x58)->Byteswap();
            SamsUtilities::USwapBlockStruct(reinterpret_cast<FLYBOY*>(buffer + 0x5c), 0x14);
            LbFile_Write(file, buffer, kBaseSize, nullptr);
        }
    }
    return 0;
}

// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.

template<> void tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>::Byteswap() {
    mIndex = __builtin_bswap16(mIndex);   // 0x10079a0c, 12 bytes
}

// Unit v5209: FLY_Load 0x10079aa8, FLY_RestoreMeshes 0x10079f8c, OBJECT_Load 0x1007a0b8 (the HLSprite.cpp range).
//
// The Mac swaps every word it reads from a file (SamsUtilities::_USwapBlock32, USwapInplace32<long>, tag_IDX_TYPE::Byteswap,
// USwapBlockStruct<FLYBOY>): the files are little-endian, so on this little-endian host there is nothing to do and the swaps are
// left out (the same as the particle loader's helpers in rsse.h).

namespace {

// Reads one word of the file into `word`, as the original does (the byte count goes to a local).
void ReadWord(void* file, void* word) {
    uint32_t transferred;
    LbFile_Read(file, word, 4, &transferred);
}


}  // namespace

// 0x10079aa8: loads the fly file from the open `file`; 1 when the magic is wrong.
int32_t FLY_Load(void* file) {
    uint32_t word;
    if (gFlyInitialised != 0) FLY_Close();
    ReadWord(file, &word);
    if (word != 0x5f594c46u) {
        GAME_ASSERTMSG(0, "FLY: Load Fail ");
        return 1;
    }
    ReadWord(file, &word);
    if (word == 0) return 0;
    if (gFlyInitialised == 0) {
        gFlyInitialised = 1;
        gFlyActive = 1;
        for (int32_t i = 0; i < 100; ++i) gFlyBases[i] = nullptr;
        gFlySinTable = LbMem_Alloc(0x4000);
        for (int32_t i = 0; i < 0x1000; ++i) {
            double angle = 3.14159 * static_cast<double>(i * 2) / 4096.0;
            static_cast<int32_t*>(gFlySinTable)[i] = ppc_fctiwz(ppc_fmadd(1024.0, std::sin(angle), 0.5));
        }
    }
    ReadWord(file, &word);
    // The file's bytes go straight into the table of bases (the Mac's entries are 32-bit words; the host's are pointers, so the
    // words go through a copy and only the entries the file changed are replaced).
    uint32_t marks[100];
    for (int32_t i = 0; i < 100; ++i) marks[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gFlyBases[i]));
    uint32_t before[100];
    std::memcpy(before, marks, sizeof(marks));
    LbFile_Read(file, marks, word, nullptr);
    for (int32_t i = 0; i < 100; ++i)
        if (marks[i] != before[i]) gFlyBases[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(marks[i]));
    ReadWord(file, &gFlyZoneD);
    ReadWord(file, &gFlyZoneB);
    ReadWord(file, &gFlyZoneA);
    ReadWord(file, &gFlyZoneC);
    for (int32_t i = 0; i < 6; ++i) ReadWord(file, &word);   // the sixth is the size of a base
    for (int32_t i = 1; i < 100; ++i) {
        if (gFlyBases[i] != nullptr) {
            gFlyBases[i] = LbMem_Alloc(word);
            LbFile_Read(file, gFlyBases[i], word, nullptr);
        }
    }
    return 0;
}

// 0x10079f8c: gives every active slot of every fly base a new ride instance.
void FLY_RestoreMeshes() {
    for (int32_t i = 1; i < 100; ++i) {
        sFlyBase* base = static_cast<sFlyBase*>(gFlyBases[i]);
        if (base == nullptr) continue;
        for (int32_t j = 0; j < kFlySlotCount; ++j) {
            sFlySlot* slot = base->Slot(j);
            if (slot->mActive == 0) continue;
            uint16_t thingId = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(base) + 0x58);
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0,
                           "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            uint16_t type = *reinterpret_cast<uint16_t*>(static_cast<char*>(gThingArray[thingId].mThing) + 0xe);
            RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{type});
            slot->mRide = RestoreRideInstance(slot->mRide, info->mSubRideTypes[0], 0x101, nullptr, 0, nullptr, 0, 0, 0);
        }
    }
}
