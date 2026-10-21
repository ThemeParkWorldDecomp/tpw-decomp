// Test library for CWeather and CWeatherForecast (unit w181; libtpw_unit_WeatherParts.dylib, tests/test_WeatherParts.py), main app:
//   src/game/park/Weather.cpp          the fifteen functions
//   src/game/ControlManager.cpp, src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/rides/FlyingRide.cpp, src/game/save/FS_MasterSave.cpp   the basic LOADVAR / SAVEVAR instances (REAL: the file is the fake)
//   src/game/park/ChallengeHQ.cpp, src/game/park/Weather.cpp, src/game/ui/MessageCentre.cpp    the message classes (the test's message is a subclass whose GetType answers the case's type)
//   src/game/balance/MainBalance.cpp      gMainBalance (CBalanceTable::Init and UnInit are fakes, as in the things' turns unit)
//
// The fake file: LbFile_Read hands out the test's bytes in order, LbFile_Write records them as hex; the queues "LbFile_Read", "LbFile_Read:xfer",
// "LbFile_Write" and "LbFile_Write:xfer" replace the result and the transferred count. Every other callee records what the real one reads and
// answers from its queue: CThing::Serialise / FinalDestroy, CWorld::GetRand, CParkTime::GetFunnyTime (queue "GetFunnyTime", the 64-bit stamp),
// TbTimeDiff::Set (the span is days * 0x100000001) and TbTimeStamp::operator+ (the wrapping sum), CRain::Close / SetVelocity /
// SetWindParameters, CWeather::SetPrecipitationType, CSaveLoad::WriteSerialiseLog and GAME_ASSERTMSG (with the double of its "%f").
// The world, the rain and the file are never dereferenced: they are addresses.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#define TPW_CALLLOG_EVENTS 20000
#define TPW_CALLLOG_LABELS 64
#define TPW_CALLLOG_QUEUE_VALUES 4000
#include "calllog.cpp"
#include "Debug.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"
#include "tb_rain.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CWorld* gGameWorld;
CRain* gei_Rain;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
CSaveLoad gSaveLoad;
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}

namespace {

constexpr uint32_t kGuard = 0x40;
alignas(16) uint8_t gBlock[kGuard + sizeof(CWeather) + kGuard];   // 0x40 guard bytes, the weather, 0x40 guard bytes
alignas(16) uint8_t gVec[kGuard + 0x10 + 0x10 + kGuard];          // guards, the start vector (12 bytes + 4 spare), the end vector (the same)
alignas(16) uint8_t gWorldBlock[0x400];
alignas(16) uint8_t gRainBlock[0x10];
alignas(16) uint8_t gMsgStorage[64];
char gFile;
std::vector<uint8_t> gFileData;
std::size_t gCursor = 0;

uint8_t* Weather() { return gBlock + kGuard; }

struct MsgShape : CMsgWeatherWarning {
    int32_t mWanted = 0;
    int32_t GetType() const override {
        ev("CMsg::GetType", {lab(this)});
        return mWanted;
    }
};

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    const auto* p = static_cast<const uint8_t*>(data);
    std::string out;
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

}  // namespace

// ---- the fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    if (std::strstr(format, "%f") != nullptr) {
        va_list args;
        va_start(args, format);
        const double value = va_arg(args, double);
        va_end(args);
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        ev("GAME_ASSERTMSG", {condition, static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xffffffffu)}, {format});
    } else {
        ev("GAME_ASSERTMSG", {condition}, {format});
    }
}

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CThing::Serialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CThing::Serialise"));
}

void CThing::FinalDestroy() { ev("CThing::FinalDestroy", {lab(this)}); }

uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("CWorld::GetRand"));
}

TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    const int32_t present = (a != nullptr) + (b != nullptr) + (c != nullptr) + (d != nullptr) + (e != nullptr) + (f != nullptr) + (g != nullptr) +
                            (h != nullptr);
    ev("CParkTime::GetFunnyTime", {lab(this), present});
    TbTimeStamp stamp;
    stamp.mTicks = ret("GetFunnyTime");
    return stamp;
}

TbError TbTimeDiff::Set(int32_t days, int32_t hours, int32_t minutes, int32_t seconds, int32_t milliseconds) {
    ev("TbTimeDiff::Set", {days, hours, minutes, seconds, milliseconds});
    mTicks = static_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(days)) * 0x100000001ull);
    return TbError(0);
}

TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    ev("TbTimeStamp::operator+", {mTicks, diff.mTicks});
    TbTimeStamp sum;
    sum.mTicks = static_cast<int64_t>(static_cast<uint64_t>(mTicks) + static_cast<uint64_t>(diff.mTicks));
    return sum;
}

int32_t CRain::Close() {
    ev("CRain::Close", {lab(this)});
    return 0;
}

void CRain::SetVelocity(float speed) { ev("CRain::SetVelocity", {lab(this), fbits(speed)}); }

void CRain::SetWindParameters(float direction, float speed) { ev("CRain::SetWindParameters", {lab(this), fbits(direction), fbits(speed)}); }

void CWeather::SetPrecipitationType(PrecipitationType type) { ev("CWeather::SetPrecipitationType", {lab(this), static_cast<int32_t>(type)}); }

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const std::size_t available = gFileData.size() > gCursor ? gFileData.size() - gCursor : 0;
    uint32_t count = available < length ? static_cast<uint32_t>(available) : length;
    const std::size_t taken = count;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        std::vector<uint8_t> bytes(length, 0);
        std::memcpy(bytes.data(), gFileData.data() + (gFileData.size() > gCursor ? gCursor : gFileData.size()), taken < length ? taken : length);
        std::memcpy(buffer, bytes.data(), length);
    }
    gCursor += taken;
    return TbError(result);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), length}, {Hex(buffer, length).c_str()});
    const int64_t forced = ret("LbFile_Write:xfer", -1);
    *transferred = forced >= 0 ? static_cast<uint32_t>(forced) : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

extern "C" {

// The host address of a block by the module's name for it.
uint64_t tpw_wq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gSaveLoad", &gSaveLoad}, {"file", &gFile}, {"block", gBlock}, {"vec", gVec}, {"world", gWorldBlock}, {"rain", gRainBlock}, {"msg", gMsgStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The globals the functions read: the heightfield size and the seven balance values (snow low and high, rain low and high, lightning low and
// high, the maximum of raindrops); gGameWorld and gei_Rain point at the blocks.
void tpw_wq_set_globals(int32_t width, int32_t height, const int32_t* balance) {
    gHeightfieldWidth = width;
    gHeightfieldHeight = height;
    gGameWorld = reinterpret_cast<CWorld*>(gWorldBlock);
    gei_Rain = reinterpret_cast<CRain*>(gRainBlock);
    static const int kSlots[7] = {303, 304, 305, 306, 307, 308, kMainBalanceMaxRaindrops};
    for (int i = 0; i < 7; ++i) gMainBalance.mValues[kSlots[i]].mInteger = balance[i];
}

void tpw_wq_set_file(const char* data, int32_t size) {
    gFileData.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size);
    gCursor = 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_wq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CWeather));
    out[n++] = static_cast<int64_t>(sizeof(CWeatherForecast));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mForecast));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mF48));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mF58));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mWindDirection));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mF64));
    out[n++] = static_cast<int64_t>(sizeof(D3DVECTOR_SWAPABLE));
    out[n++] = static_cast<int64_t>(sizeof(TbTimeStamp));
    out[n++] = static_cast<int64_t>(offsetof(CMsgWeatherWarning, mQuality));
    out[n++] = static_cast<int64_t>(offsetof(CMsgWeatherWarning, mDays));
    return n;
}

// One entry for the fifteen functions, by the module's index (OPS in the module). a, b, c are the integer arguments (the float of the wind
// setters as its bits in a), p the name of the instances, q the message's quality.
int64_t tpw_wq_call(uint32_t op, int64_t a, int64_t b, int64_t c, uint64_t p, int64_t q) {
    CWeather* const weather = reinterpret_cast<CWeather*>(Weather());
    CWeatherForecast* const forecast = &weather->mForecast;
    const char* const name = reinterpret_cast<const char*>(static_cast<uintptr_t>(p));
    auto asFloat = [](int64_t bits) {
        const uint32_t word = static_cast<uint32_t>(bits);
        float value;
        std::memcpy(&value, &word, sizeof(value));
        return value;
    };
    switch (op) {
        case 0: weather->FinalDestroy(); return 0;
        case 1: return weather->Serialise(&gFile, static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<uint32_t>(c));
        case 2: {
            MsgShape* message = new (gMsgStorage) MsgShape();
            message->mWanted = static_cast<int32_t>(a);
            message->mQuality = static_cast<int32_t>(b);
            message->mDays = static_cast<int32_t>(c);
            weather->ReceiveMessage(*message);
            return 0;
        }
        case 3: {
            D3DVECTOR* start = reinterpret_cast<D3DVECTOR*>(gVec + kGuard);
            D3DVECTOR* end = reinterpret_cast<D3DVECTOR*>(gVec + kGuard + 0x10);
            weather->FindLightningCoords(*start, *end, static_cast<int32_t>(a));
            return 0;
        }
        case 4: weather->SetQuality(static_cast<int32_t>(a)); return 0;
        case 5: weather->SetWindDirection(asFloat(a)); return 0;
        case 6: weather->SetWindForce(asFloat(a)); return 0;
        case 7: weather->UpdateWindInfoToEngine(); return 0;
        case 8: return forecast->Serialise(&gFile, static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<uint32_t>(c));
        case 9: forecast->Invalidate(); return 0;
        case 10: forecast->Set(static_cast<int32_t>(a), static_cast<int32_t>(b)); return 0;
        case 11: return SAVEVARSTRUCT<D3DVECTOR_SWAPABLE>(&gFile, weather->LastLightningLocation(), name, static_cast<int32_t>(a));
        case 12: return LOADVARSTRUCT<D3DVECTOR_SWAPABLE>(&gFile, weather->LastLightningLocation(), name, static_cast<int32_t>(a));
        case 13: return LOADVAR<PrecipitationType>(&gFile, reinterpret_cast<PrecipitationType*>(&weather->mF58), name, static_cast<int32_t>(a));
        case 14: return SAVEVAR<PrecipitationType>(&gFile, reinterpret_cast<PrecipitationType*>(&weather->mF58), name, static_cast<int32_t>(a));
        default: return 0;
    }
}

}  // extern "C"
