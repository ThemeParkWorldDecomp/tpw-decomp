// Test library for one challenge (unit w174; libtpw_unit_Challenge.dylib, tests/test_Challenge.py), main app:
//   src/game/park/ChallengeHQ.cpp        CChallenge's constructor, Serialise, DidThisWinIt, the four message id getters and the ChallengeType / StaffType
//                            LOADVAR / SAVEVAR instances
//   src/game/ControlManager.cpp, src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/rides/FlyingRide.cpp, src/game/save/FS_MasterSave.cpp  the basic LOADVAR / SAVEVAR instances (REAL: the file is the fake)
//
// The fake file: LbFile_Read hands out the test's bytes in order (a cursor), LbFile_Write records the bytes as hex; the queues "LbFile_Read",
// "LbFile_Read:xfer", "LbFile_Write" and "LbFile_Write:xfer" replace the result and the transferred count, as in unit_Serialise.cpp. Every
// other callee is a fake that records the arguments the real one reads and answers from its queue: the world's GetParkAnalyser (answers the
// analyser block), the eight park analyser readers (a pointer out-id is written from the queue "<name>:id"), CTRACE, GAME_ASSERTMSG and
// the serialise log.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "ThingParts2.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;

namespace {

std::vector<uint8_t> gFileData;   // the bytes the reads hand out, in order
std::size_t gCursor = 0;
char gFileHandle;                 // the fake file's address (a label for the log)
alignas(16) unsigned char gWorldBlock[0x40];      // gGameWorld points here (never dereferenced: GetParkAnalyser is a fake)
alignas(16) unsigned char gAnalyserBlock[0x40];   // the analyser GetParkAnalyser answers (never dereferenced either)

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

// ---- globals of other files
CSaveLoad gSaveLoad;
CWorld* gGameWorld = reinterpret_cast<CWorld*>(gWorldBlock);
CWorld*& gGameWorldRef = gGameWorld;

// ---- the fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The only trace the unit makes has no conversion in its text.
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

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

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserBlock);
}

// The readers write their out id (when they have one) before they answer, as the translated ones do.
uint32_t CParkAnalyser::GetCoasterWithMostLoops(ThingId& id, ObjectId type) const {
    ev("CParkAnalyser::GetCoasterWithMostLoops", {lab(this), type.mIndex});
    id.mIndex = static_cast<uint16_t>(ret("CParkAnalyser::GetCoasterWithMostLoops:id"));
    return static_cast<uint32_t>(ret("CParkAnalyser::GetCoasterWithMostLoops"));
}
int32_t CParkAnalyser::GetGokartWithMostCrossRoads(ThingId& id) const {
    ev("CParkAnalyser::GetGokartWithMostCrossRoads", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("CParkAnalyser::GetGokartWithMostCrossRoads:id"));
    return static_cast<int32_t>(ret("CParkAnalyser::GetGokartWithMostCrossRoads"));
}
int32_t CParkAnalyser::GetLongestTrackRide(ThingId& id, ObjectId type) const {
    ev("CParkAnalyser::GetLongestTrackRide", {lab(this), type.mIndex});
    id.mIndex = static_cast<uint16_t>(ret("CParkAnalyser::GetLongestTrackRide:id"));
    return static_cast<int32_t>(ret("CParkAnalyser::GetLongestTrackRide"));
}
int32_t CParkAnalyser::GetHighestUpgradeLevelForObject(ObjectId id) const {
    ev("CParkAnalyser::GetHighestUpgradeLevelForObject", {lab(this), id.mIndex});
    return static_cast<int32_t>(ret("CParkAnalyser::GetHighestUpgradeLevelForObject"));
}
int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("CParkAnalyser::NumberOfVisitorsInPark", {lab(this)});
    return static_cast<int32_t>(ret("CParkAnalyser::NumberOfVisitorsInPark"));
}
int32_t CParkAnalyser::NumberOfVisitorsWithBalloons() const {
    ev("CParkAnalyser::NumberOfVisitorsWithBalloons", {lab(this)});
    return static_cast<int32_t>(ret("CParkAnalyser::NumberOfVisitorsWithBalloons"));
}
int32_t CParkAnalyser::NumberOfVisitorsWithCostumes() const {
    ev("CParkAnalyser::NumberOfVisitorsWithCostumes", {lab(this)});
    return static_cast<int32_t>(ret("CParkAnalyser::NumberOfVisitorsWithCostumes"));
}
int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel model) const {
    ev("CParkAnalyser::AverageQualityOfStaffType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("CParkAnalyser::AverageQualityOfStaffType"));
}
int32_t CParkAnalyser::NumberOfObjectInPark(ObjectId id) const {
    ev("CParkAnalyser::NumberOfObjectInPark", {lab(this), id.mIndex});
    return static_cast<int32_t>(ret("CParkAnalyser::NumberOfObjectInPark"));
}

extern "C" {

// The host address of a global by the module's name for it (the log's labels).
uint64_t tpw_chl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"file", &gFileHandle},
        {"gSaveLoad", &gSaveLoad},
        {"world", gWorldBlock},
        {"analyser", gAnalyserBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable like every Ptr32 the headers hold).
uint64_t tpw_chl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The bytes the next reads hand out; the cursor goes back to the start.
void tpw_chl_set_file(const char* data, int32_t size) {
    gFileData.assign(data, data + size);
    gCursor = 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_chl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CChallenge));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mType));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mTargetTime));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mTargetVal));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mTargetObj));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mTargetObj2));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mTargetStaffType));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mPrize));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mDoubleOrNothingFollowup));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mCheckAtEndOnly));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mIndependent));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mChallengeOffered));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mChallengeLost));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mChallengeDeclined));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mHaveWon));
    out[n++] = static_cast<int64_t>(offsetof(CChallenge, mThingIdForFollowup));
    return n;
}

// ---- the functions, one wrapper each with C types only
// The constructor over `record` (prefilled storage: placement new, as the hall's Init does).
void tpw_chl_ctor(uint64_t record, int32_t type, int32_t targetTime, int32_t prize, int32_t followup, int32_t checkAtEndOnly, int32_t targetVal,
                  uint32_t targetObj, uint32_t targetObj2, int32_t staffType, int32_t independent) {
    new (reinterpret_cast<void*>(record)) CChallenge(static_cast<ChallengeType>(type), targetTime, prize, static_cast<ChallengeType>(followup),
                                                    checkAtEndOnly != 0, targetVal, ObjectId{static_cast<uint16_t>(targetObj)},
                                                    ObjectId{static_cast<uint16_t>(targetObj2)}, static_cast<StaffType>(staffType), independent != 0);
}

int32_t tpw_chl_serialise(uint64_t record, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CChallenge*>(record)->Serialise(&gFileHandle, save, version, unused);
}

uint32_t tpw_chl_did(uint64_t record, uint64_t value, int32_t atEnd) {
    return reinterpret_cast<CChallenge*>(record)->DidThisWinIt(*reinterpret_cast<int32_t*>(value), atEnd != 0);
}

// 0 GetOfferId, 1 GetActiveId, 2 GetWinId, 3 GetLoseId
int32_t tpw_chl_get(int32_t which, uint64_t record) {
    const CChallenge* challenge = reinterpret_cast<const CChallenge*>(record);
    switch (which) {
    case 0: return challenge->GetOfferId();
    case 1: return challenge->GetActiveId();
    case 2: return challenge->GetWinId();
    default: return challenge->GetLoseId();
    }
}

// 0 LOADVAR<ChallengeType>, 1 LOADVAR<StaffType>, 2 SAVEVAR<ChallengeType>, 3 SAVEVAR<StaffType>
int32_t tpw_chl_inst(int32_t which, uint64_t value, const char* name, int32_t noLog) {
    void* file = &gFileHandle;
    void* v = reinterpret_cast<void*>(value);
    switch (which) {
    case 0: return LOADVAR(file, static_cast<ChallengeType*>(v), name, noLog);
    case 1: return LOADVAR(file, static_cast<StaffType*>(v), name, noLog);
    case 2: return SAVEVAR(file, static_cast<ChallengeType*>(v), name, noLog);
    default: return SAVEVAR(file, static_cast<StaffType*>(v), name, noLog);
    }
}

}  // extern "C"
