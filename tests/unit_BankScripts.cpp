// Test library for unit w177 (libtpw_unit_BankScripts.dylib, tests/test_BankScripts.py), main app: src/BankAccount_loans.cpp (eleven CBankAccount members),
// src/ChallengeBalance.cpp (the destructor and two virtuals of CChallengeBalance) and src/DelayedScript.cpp (CDelayedScript and its ScriptIdx LOADVAR /
// SAVEVAR instances). gMainBalance is the real storage (src/MainBalance.cpp). Every other callee is a fake that records the same event as the PowerPC
// side's stub.
//
// The worlds are not blocks of their Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and a world pointer is that block's
// address minus 0x1da6f0 on both sides. The account (or the delayed script) lives in gObj over 0x40 guard bytes on each side; the two analysers are
// blocks that reach their words +0x1fc90 and +0x1fee0 over guards. The destructor test deletes a heap block of the host's CChallengeBalance (calllog.cpp
// logs the delete of a labelled block). The file for the ScriptIdx instances is the fake of unit_Challenge.cpp (a cursor over the test's bytes).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "Serialise.h"

#include "CMsg.h"
#include "CMsgEvent.h"
#include "ChallengeBalance.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "SaveLoad.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"

using namespace calllog;

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kObjSize = 0x128;
constexpr int32_t kObjBlock = kGuard + kObjSize + kGuard;
constexpr int32_t kAnalyserBlock = kGuard + 0x1fee4 + kGuard;
constexpr int32_t kOutBlock = 0x60;
constexpr int32_t kCodeWords = 883;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gObj[kObjBlock];
alignas(16) unsigned char gAnalyser[2][kAnalyserBlock];
alignas(16) unsigned char gOut[kOutBlock];
alignas(16) unsigned char gBalanceStorage[sizeof(CChallengeBalance)];
uint32_t gCodeStore[kCodeWords];
const char* gBase;               // the object under test (offsets in the VAR logs are from it)
std::vector<uint8_t> gFileData;  // the bytes the reads hand out, in order
std::size_t gCursor = 0;
char gFileHandle;

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }
CParkAnalyser* AnalyserPtr(int32_t k) { return reinterpret_cast<CParkAnalyser*>(gAnalyser[k] + kGuard); }

// The Mac code address of a host pointer into the code array (0 for nullptr or any other pointer): the arithmetic of SCR_CodeAddress.
int64_t MacCode(const void* code) {
    const uint8_t* byte = static_cast<const uint8_t*>(code);
    const uint8_t* first = reinterpret_cast<const uint8_t*>(gCodeStore);
    if (byte >= first && byte < first + sizeof gCodeStore) return kScriptCodeAddress + static_cast<int64_t>(byte - first);
    return code == nullptr ? 0 : -1;
}

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

// ---- The unit's globals and the script code array (883 words, defined here: src/ScriptCheckTable.cpp is not linked)
CWorld* gWorld;
CWorld* gGameWorld;
CSaveLoad gSaveLoad;
const uint32_t* gScriptCodeWords = gCodeStore;
const uint32_t* gScriptCodeWordsEnd = gCodeStore + kCodeWords;

// ---- The MacLong instances (fakes) and the base thing
template <> int32_t LOADVAR<MacLong>(void* f, MacLong* v, const char* n, int32_t l) {
    ev("LOADVAR<long>", {lab(f), static_cast<int64_t>(reinterpret_cast<const char*>(v) - gBase), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVAR<MacLong>(void* f, MacLong* v, const char* n, int32_t l) {
    ev("SAVEVAR<long>", {lab(f), static_cast<int64_t>(reinterpret_cast<const char*>(v) - gBase), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
void CThing::FinalDestroy() { ev("CThing::FinalDestroy", {lab(this)}); }
void CBankAccount::MonthlyUpdate() { ev("MonthlyUpdate", {lab(this)}); }

// ---- The asserts, traces, serialise log and file
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}
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

// ---- The world, the analyser, the time, the balance table
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return AnalyserPtr(this == WorldPtr(0) ? 0 : 1);
}
bool CWorld::IsPlayingWinningAnim() {
    ev("IsPlayingWinningAnim", {lab(this)});
    return ret("win", 0) != 0;
}
void CWorld::StartBankruptcySequence() { ev("StartBankruptcySequence", {lab(this)}); }
void CParkAnalyser::PersonPaidAdmission(int32_t price) { ev("PersonPaidAdmission", {lab(this), price}); }
TbTimeDiff CParkTime::GetFunnyTimeSinceGameTurn(uint32_t turn) const {
    ev("GetFunnyTimeSinceGameTurn", {lab(this), turn});
    TbTimeDiff span;
    span.mTicks = ret("span", 0);
    return span;
}
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() { ev("CBalanceTable::UnInit", {lab(this)}); }

// ---- The scripts
int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {MacCode(block), static_cast<int32_t>(sprite), a, static_cast<int32_t>(acts), fbits(x), fbits(y), fbits(z)});
    return static_cast<int32_t>(ret("spawn", 0));
}
void ESP_ForceGoto(int32_t handle, void* code) { ev("ESP_ForceGoto", {handle, MacCode(code)}); }
int32_t ESP_RURunningThis(int32_t handle, void* code) {
    ev("ESP_RURunningThis", {handle, MacCode(code)});
    return static_cast<int32_t>(ret("running", 0));
}
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
void SCR_SetServiceInterval(int32_t handle, int32_t period) { ev("SCR_SetServiceInterval", {handle, period}); }

// ---- The message: a fake class with the message layout whose GetType answers the case's type
namespace {
struct EventShape : CMsg {
    int32_t mEvent = 0;
    int32_t mWantedType = 0;
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return mWantedType;
    }
};
alignas(16) unsigned char gMessageStorage[sizeof(EventShape)];
}  // namespace

extern "C" {

// The blocks the test fills and reads: 0 the account or delayed script (over guards), 1 and 2 the world tails, 3 and 4 the analysers, 6 the out block.
void* tpw_bks_addr(int32_t which) {
    switch (which) {
    case 0: return gObj;
    case 1: return gTail[0];
    case 2: return gTail[1];
    case 3: return gAnalyser[0];
    case 4: return gAnalyser[1];
    case 6: return gOut;
    case 7: return &gSaveLoad;
    default: return nullptr;
    }
}
void* tpw_bks_world(int32_t k) { return WorldPtr(k); }

// The globals before a case: the worlds named by index, the opening cash slot of gMainBalance, the account as the base of the offsets.
void tpw_bks_prepare(int32_t world, int32_t game, int32_t cash) {
    gWorld = WorldPtr(world);
    gGameWorld = WorldPtr(game);
    gMainBalance.mValues[kMainBalanceInitialCash].mInteger = cash;
    gBase = reinterpret_cast<const char*>(gObj) + kGuard;
}
// The bytes the next reads hand out; the cursor goes back to the start.
void tpw_bks_set_file(const char* data, int32_t size) {
    gFileData.assign(data, data + size);
    gCursor = 0;
}
int32_t tpw_bks_cash() { return gMainBalance.mValues[kMainBalanceInitialCash].mInteger; }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_bks_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CBankAccount));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mLoans));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mBalance));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mBatchBalance));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mAdmissionFee));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mLastBalance));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mTurnEnteredRed));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mProfitThisYear));
    out[n++] = static_cast<int64_t>(sizeof(CLoan));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100) + 5 * sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100) + 6 * sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA738));
    out[n++] = static_cast<int64_t>(sizeof(CDelayedScript));
    out[n++] = static_cast<int64_t>(offsetof(CDelayedScript, mSpriteScript));
    out[n++] = static_cast<int64_t>(offsetof(CDelayedScript, mNextAnim));
    out[n++] = static_cast<int64_t>(offsetof(CDelayedScript, mNextServiceInterval));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceDescription));
    return n;
}

static CBankAccount* Account() { return reinterpret_cast<CBankAccount*>(gObj + kGuard); }
static CDelayedScript* Script() { return reinterpret_cast<CDelayedScript*>(gObj + kGuard); }
static CMsg* Message(int32_t type, int32_t event) {
    auto* m = new (gMessageStorage) EventShape();
    m->mFlags = 0;
    m->mEvent = event;
    m->mWantedType = type;
    return m;
}

// ---- CBankAccount (the file label 6 is the message block; the account is label 8)
void tpw_bks_finaldestroy() { Account()->FinalDestroy(); }
void tpw_bks_makedeposit(uint32_t amount) { Account()->MakeDeposit(amount); }
// The span comes back by value; it is copied to the middle of the out block.
void tpw_bks_redfor() {
    const TbTimeDiff span = Account()->BeenInTheRedFor();
    std::memcpy(gOut + 0x20, &span, sizeof span);
}
void* tpw_bks_message() { return Message(0, 0); }
void tpw_bks_receive(int32_t type, int32_t event, uint32_t label) {
    CMsg* m = Message(type, event);
    tpw_cl_label(static_cast<int32_t>(label), m);
    Account()->ReceiveMessage(*m);
}
void tpw_bks_payadmission() { Account()->PayAdmission(); }
int32_t tpw_bks_loansbought() { return Account()->GetNumberOfLoansBought(); }
int32_t tpw_bks_apply(uint32_t loan) { return Account()->ApplyForLoan(loan) ? 1 : 0; }
uint32_t tpw_bks_outstanding(uint32_t loan) { return Account()->OutstandingOnLoan(loan); }
uint32_t tpw_bks_cheapest() { return Account()->GetCheapestLoanTaken(); }
int32_t tpw_bks_profit() { return Account()->GetProfitThisYear(); }
uint32_t tpw_bks_total() { return Account()->TotalOutstandingOnLoans(); }

// ---- CChallengeBalance: a heap block (label 7) deleted by the destructor call when the flag's low half is positive
void* tpw_bks_balance_new() {
    void* p = ::operator new(sizeof(CChallengeBalance));
    std::memset(p, 0xa5, sizeof(CChallengeBalance));
    tpw_cl_label(7, p);
    return p;
}
// The answer is the object's label (0 for NULL, 7 for the block), taken before the delete forgets it.
int32_t tpw_bks_balance_dtor(void* object, int32_t flag) {
    auto* o = static_cast<CChallengeBalance*>(object);
    const int32_t label = lab(o);
    if (o != nullptr) {
        if (static_cast<int16_t>(flag) > 0) delete o;
        else o->~CChallengeBalance();
    }
    return label;
}
// The offset of the answer from the description table / from the object.
int64_t tpw_bks_balance_desc(int32_t row) {
    auto* o = reinterpret_cast<CChallengeBalance*>(gBalanceStorage);
    return reinterpret_cast<const char*>(o->CChallengeBalance::GetBalanceDescription(row)) - reinterpret_cast<const char*>(gChallengeBalanceDescriptions);
}
int64_t tpw_bks_balance_values() {
    auto* o = reinterpret_cast<CChallengeBalance*>(gBalanceStorage);
    return reinterpret_cast<const char*>(o->CChallengeBalance::GetBalanceValuesStart()) - reinterpret_cast<const char*>(o);
}

// ---- CDelayedScript
void tpw_bks_script_ctor() { new (gObj + kGuard) CDelayedScript(); }
int32_t tpw_bks_script_serialise(int32_t save, int32_t version, uint32_t unused) {
    return Script()->Serialise(reinterpret_cast<void*>(0x1000), save, version, unused);
}
void tpw_bks_script_set(int32_t script) { Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(script)); }
int32_t tpw_bks_script_isrunning(int32_t script) { return Script()->IsRunningThisScript(static_cast<CDelayedScript::ScriptIdx>(script)) ? 1 : 0; }
void tpw_bks_script_spawn(int32_t script, int32_t set, int32_t index) { Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(script), set, index); }
void tpw_bks_script_kill() { Script()->KillSpriteScript(); }
void tpw_bks_script_process() { Script()->ProcessSpriteScripts(); }
void tpw_bks_script_interval(float rate) { Script()->SetServiceInterval(rate); }
// The ScriptIdx instances over the value at gObj + kGuard + 4 (the file label is the address 0x1000).
int32_t tpw_bks_script_load(const char* name, int32_t noLog) {
    return LOADVAR(reinterpret_cast<void*>(0x1000), reinterpret_cast<CDelayedScript::ScriptIdx*>(gObj + kGuard + 4), name, noLog);
}
int32_t tpw_bks_script_save(const char* name, int32_t noLog) {
    return SAVEVAR(reinterpret_cast<void*>(0x1000), reinterpret_cast<CDelayedScript::ScriptIdx*>(gObj + kGuard + 4), name, noLog);
}

}  // extern "C"
