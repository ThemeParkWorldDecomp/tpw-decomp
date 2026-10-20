// A ride script's ride-specific commands (main app, rsse.cpp range of the inventory; unit w172): ExecuteTourSpecificCommand 0x100d6fec (2,240 bytes),
// ExecuteBumperSpecificCommand 0x100d78ec (2,008), ExecuteCoasterSpecificCommand 0x100d8104 (1,192) and __sinit_rsse_cpp 0x100d85f0 (28).
// Each dispatcher reads the command word with ReadScript (the original inlines it), switches on it as an unsigned number (a jump table; 0 and
// every number above the bound go to the default, an assertion) and each case reads its operand and calls one ride function. The original inlines
// ExtractData too; here both are called. The answer of some commands goes into the script's word +0x48 (mCommandAnswer).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "HLSprite.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte; the first ends in a newline).
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x400
const char kNoNode[] = "RSSE: Cant create a tour ride (no node)";                                 // +0xa07
const char kUnknownTour[] = "RSSE: Unknown tour ride command";                                    // +0xa2f
const char kUnknownBumper[] = "RSSE: Unknown bumper ride command";                                // +0xa4f

// The inlined test "the word names a variable": the top byte is 0x40. (Not ExtractData: the index is the whole word with that byte cleared.)
bool NamesVariable(uint32_t word) { return (word & 0xff000000u) == 0x40000000u; }
uint32_t VariableIndex(uint32_t word) { return word ^ 0x40000000u; }

// The next word of the program, run through ExtractData (a variable's value, else the low halfword sign extended).
int32_t Operand(RSSE_SCRIPT* script) { return ExtractData(script, static_cast<int32_t>(ReadScript(script))); }

// The row 0 of the thing table must be free; the dispatchers of the bumper and the coaster check it first.
void CheckRowZero() { GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero); }

// The halfword at +0xe6 (mFE6) as a host-order int16_t (the bumper's track direction; the header keeps it as two bytes).
int16_t ReadTrackDirection(const RSSE_SCRIPT* script) {
    int16_t value;
    std::memcpy(&value, script->mFE6, sizeof(value));
    return value;
}
void WriteTrackDirection(RSSE_SCRIPT* script, int16_t value) { std::memcpy(script->mFE6, &value, sizeof(value)); }

// Tour command 1: creates the fly base of the ride at its node 0x63 (the first embedded entry with flag 0x800).
void CreateTour(RSSE_SCRIPT* script) {
    const int32_t a = Operand(script);
    tag_sRideAnimSet* set = gRideAnimSets[script->mRideAnim];
    const uint32_t node = FindDataNodeIndexByID(set, 0x800, 0x63);
    if (node == 0xffffffffu) {
        GAME_ASSERTMSG(0, kNoNode);
        return;
    }
    set = gRideAnimSets[script->mRideAnim];
    tag_sMesh* mesh = set->mHeads;
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(mesh->mF28);
    const uint8_t* block = embedded->mRecords[node].mBlock;
    float x = 0.0f;   // 0x10264fb4
    float y = 0.0f;
    float z = 0.0f;
    if (block != nullptr) {
        std::memcpy(&x, block + 0x30, sizeof(x));
        std::memcpy(&y, block + 0x34, sizeof(y));
        std::memcpy(&z, block + 0x38, sizeof(z));
    }
    // angle = (mF10 << 12) / 360: the shift wraps, then the compiler's multiply by 0xb60b60b7 (mulhw), add, srawi 8 and the sign bit.
    const int32_t shifted = static_cast<int32_t>(static_cast<uint32_t>(mesh->mF10) << 12);
    const int32_t high = static_cast<int32_t>((static_cast<int64_t>(static_cast<int32_t>(0xb60b60b7u)) * shifted) >> 32);
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(high) + static_cast<uint32_t>(shifted));
    const int32_t scaled = sum >> 8;
    const int32_t angle = static_cast<int32_t>(static_cast<uint32_t>(scaled) + (static_cast<uint32_t>(scaled) >> 31));
    // The thing id word at +0xac: the id in the high halfword, the halfword at +0xae in the low one (host order halfwords).
    uint16_t low;
    std::memcpy(&low, script->mFAE, sizeof(low));
    const uint32_t thingWord = (static_cast<uint32_t>(script->mThingId) << 16) | low;
    // fctiwz truncates (saturating), then mulli 300 wraps.
    const int32_t px = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(x)) * 300u);
    const int32_t py = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(y)) * 300u);
    const int32_t pz = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(z)) * 300u);
    script->mFlyHandle = FLY_CreateBase(thingWord, script->mHandle, px, py, pz, angle, a);
}

}  // namespace

// 0x100d6fec (2,240 bytes)
void ExecuteTourSpecificCommand(RSSE_SCRIPT* script) {
    const uint32_t command = ReadScript(script);
    switch (command) {
        case 1:
            CreateTour(script);
            break;
        case 2:
            FLY_RemoveBase(script->mFlyHandle);
            break;
        case 3: {
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) script->mCommandAnswer = FLY_AddPeep(script->mFlyHandle, script->mVars[VariableIndex(word)]);
            break;
        }
        case 4:
        case 16: {
            // 4 extracts a peep, 16 asks whether the ride runs: the answer goes into the variable, then into the command answer.
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) {
                const uint32_t index = VariableIndex(word);
                const int32_t answer = command == 4 ? FLY_ExtractPeep(script->mFlyHandle) : FLY_RideRunning(script->mFlyHandle);
                script->mVars[index] = answer;
                script->mCommandAnswer = script->mVars[index];
            }
            break;
        }
        case 5:
            FLY_SetNumberOfCars(script->mFlyHandle, Operand(script));
            break;
        case 8: {
            const int32_t a = Operand(script);
            FLY_SetRunTime(script->mFlyHandle, static_cast<int32_t>(static_cast<uint32_t>(a) * 1000u));
            break;
        }
        case 9:
            FLY_SetRepairState(script->mFlyHandle, Operand(script));
            break;
        case 10:   // the original tests the word for a variable and ignores the result
            ReadScript(script);
            script->mCommandAnswer = FLY_GetCarInBase(script->mFlyHandle);
            break;
        case 11:
            ReadScript(script);
            script->mCommandAnswer = FLY_LaunchCar(script->mFlyHandle);
            break;
        case 12:
            FLY_SetRunway(script->mFlyHandle, Operand(script));
            break;
        case 14:
            FLY_SetOpenClosed(script->mFlyHandle, Operand(script));
            break;
        case 15:
            ReadScript(script);
            script->mCommandAnswer = FLY_GetBrokenStat(script->mFlyHandle);
            break;
        case 17:
            FLY_SetCarSize(script->mFlyHandle, Operand(script));
            break;
        case 18:
            FLY_SetMotionScale(script->mFlyHandle, Operand(script));
            break;
        default:   // 0, 6, 7, 13 and everything above 18
            GAME_ASSERTMSG(0, kUnknownTour);
            break;
    }
}

// 0x100d78ec (2,008 bytes)
void ExecuteBumperSpecificCommand(RSSE_SCRIPT* script) {
    CheckRowZero();
    CObject* object = static_cast<CObject*>(gThingArray[script->mThingId].mThing);
    const uint32_t command = ReadScript(script);
    switch (command) {
        case 1: {
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) script->mCommandAnswer = BMP_AddPeepToRideJoiningList(object->mF24, static_cast<uint32_t>(script->mVars[VariableIndex(word)]));
            break;
        }
        case 2: {
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) {
                const uint32_t index = VariableIndex(word);
                script->mVars[index] = BMP_PollForPeeps(object->mF24);
                script->mCommandAnswer = script->mVars[index];
            }
            break;
        }
        case 3:
            ReadScript(script);
            BMP_BeginRide(object->mF24);
            break;
        case 4:
            ReadScript(script);
            script->mCommandAnswer = BMP_LAUNCH(object->mF24, 0);
            break;
        case 5:
            ReadScript(script);
            script->mCommandAnswer = object->mF28;
            break;
        case 6:
            ReadScript(script);
            BMP_CloseRide(object->mF24);
            break;
        case 7:
            ReadScript(script);
            BMP_OpenRide(object->mF24);
            break;
        case 8:
            if (Operand(script) != 0) {
                BMP_BreakRide(object->mF24);
            } else {
                BMP_FixRide(object->mF24);
            }
            break;
        case 9:
            if (Operand(script) != 0) {
                BMP_RideWorn(object->mF24);
            } else {
                BMP_FixRide(object->mF24);
            }
            break;
        case 10:
            ReadScript(script);
            BMP_HaltRide(object->mF24);
            break;
        case 11: {
            // The answer is stored first; a variable operand then gets it too.
            const uint32_t word = ReadScript(script);
            script->mCommandAnswer = BMP_GetCarsOnRide(object->mF24);
            if (NamesVariable(word)) script->mVars[VariableIndex(word)] = script->mCommandAnswer;
            break;
        }
        case 12:
            ReadScript(script);
            script->mCommandAnswer = BMP_AddJoiningListToBump(object->mF24);
            break;
        case 13: {
            const int32_t a = Operand(script);
            BMP_SetRideDuration(object->mF24, static_cast<int32_t>(static_cast<uint32_t>(a) * 30u));
            script->mCommandAnswer = a;
            break;
        }
        case 14: {
            const int32_t a = Operand(script);
            BMP_SetRideDuration(object->mF24, static_cast<int32_t>(0u - static_cast<uint32_t>(a)));
            script->mCommandAnswer = a;
            break;
        }
        case 16:
            ReadScript(script);
            script->mCommandAnswer = BMP_REMOVEBUMP(object->mF24, 1);
            break;
        case 17: {
            const int32_t a = Operand(script);
            if (static_cast<int32_t>(ReadTrackDirection(script)) != a) {
                WriteTrackDirection(script, static_cast<int16_t>(a));
                // The flag is inverted: a nonzero value passes false (the original's way round).
                const bool flag = a == 0;
                LLCC_SetTrackAnim(object, flag);
                LLCC_SetTrackAnimReverse(object, flag);
            }
            break;
        }
        default:   // 0, 15 and everything above 17
            GAME_ASSERTMSG(0, kUnknownBumper);
            break;
    }
}

// 0x100d8104 (1,192 bytes)
void ExecuteCoasterSpecificCommand(RSSE_SCRIPT* script) {
    CheckRowZero();
    const uint32_t command = ReadScript(script);
    switch (command) {
        case 1: {
            const int32_t a = Operand(script);
            CoasterAddPeep(script->mCoaster, static_cast<uint32_t>(a));
            break;
        }
        case 2:
        case 3: {
            // The answer is stored first; a variable operand then gets it too.
            const uint32_t word = ReadScript(script);
            script->mCommandAnswer = static_cast<int32_t>(command == 2 ? CoasterGetQueueSpace(script->mCoaster) : CoasterGetPeep(script->mCoaster));
            if (NamesVariable(word)) script->mVars[VariableIndex(word)] = script->mCommandAnswer;
            break;
        }
        case 4:
            CoasterSetRepairState(script->mCoaster, static_cast<uint32_t>(Operand(script)));
            break;
        case 5:
            CoasterSetOpenClosedState(script->mCoaster, Operand(script));
            break;
        case 6:
            CoasterSetCapacity(script->mCoaster, Operand(script));
            break;
        case 7:   // the original tests the word for a variable and does nothing with it
            ReadScript(script);
            break;
        case 8:
            ReadScript(script);
            script->mCoaster = static_cast<uint32_t>(GetCoasterInstanceID(script->mHandle));
            if (script->mCoaster != 0) CoasterSetSpeed(script->mCoaster, static_cast<uint32_t>(static_cast<int32_t>(script->mSpeed)));
            break;
        default:   // 0 and everything above 8; the text is the bumper's (the original's copy and paste)
            GAME_ASSERTMSG(0, kUnknownBumper);
            break;
    }
}

// __sinit_rsse_cpp 0x100d85f0 (28 bytes): two words of the file's BSS, set at start-up. Their use is not known; the names are mine (the addresses
// are their names).
uint32_t gRsseWord102F7C00 = 0x10000;   // 0x102f7c00
uint32_t gRsseWord102F7C04 = 0;         // 0x102f7c04
