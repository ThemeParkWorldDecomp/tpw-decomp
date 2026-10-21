// TPCoaster.cpp: name proven by __sinit_TPCoaster_cpp in the main application.

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "CoasterBalance.h"
#include "Debug.h"
#include "FS_Online.h"
#include "helpers/TPCoasterHelpers.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MapWho.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "tb_matrix.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "World.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Coaster trains and cars (TPCoaster.cpp on the Mac, unit w192): InstanceCoasterTrains 0x10042084, DestroyCoasterTrains 0x10042344,
// GetCoaster1stPersonCar 0x100433f4, SaveTrainsAndCars 0x10043a18, LoadTrainsAndCars 0x10043e8c and TPCoaster.cpp's three swap instances
// 0x10043db8, 0x10043e00 and 0x10043e44, plus 0x10021bb4: the main app's one copy of USwapInplaceFloat (the inventory guesses TPAdvisor.cpp;
// TPCoaster's save and load and the later SaveCoasters / SaveRides call it). Every callee outside the unit is untranslated or translated
// elsewhere.
//
// The Mac keeps a train as 0x80 bytes (sTrainInstance) and a car as 0x60 (sCarInstance); the host structs hold native pointers. A train's type
// word on the Mac is the coaster type plus 0x2f0 (its embedded train type); the host's points at the coaster type (include/TPCoaster.h).
//
// The save file is little-endian. The Mac swaps every word (its swap templates swap unconditionally); a little-endian host writes the same bytes
// by not swapping, so the swaps here are real only on a big-endian host or with TPW_SWAP_FILE_WORDS (the tests' second library), as in
// src/rsse_save.cpp. The car records' inline byte swaps (no call on the Mac) use the same guard.

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define TRAINS_SWAP_FILE_WORDS 1
#endif

namespace {

uint16_t Swap16(uint16_t v) {
#ifdef TRAINS_SWAP_FILE_WORDS
    return __builtin_bswap16(v);
#else
    return v;
#endif
}

constexpr uint32_t kTrainRecord = 0x1b;   // the packed train record of the save
constexpr uint32_t kCarRecord = 0xb;      // the packed car record of the save

// The train record's fields (offsets in the packed buffer): flags, cars in use (a byte), position, the 0x78 float, the 0x38 word, the two sound
// words and the 0x10 halfword.
constexpr int kRecFlags = 0x00, kRecCarsInUse = 0x04, kRecPosition = 0x05, kRecF78 = 0x09, kRecF38 = 0x0d, kRecF64 = 0x11, kRecF68 = 0x15,
              kRecF10 = 0x19;

// The seven swaps of a train record, in the original's order (the same sequence turns it back).
void SwapTrainRecord(uint8_t* record) {
    uint32_t word;
    float value;
    int32_t number;
    uint16_t half;
    std::memcpy(&word, record + kRecFlags, sizeof word);
    TPCoasterSwap::USwapInplace32ULong(word);
    std::memcpy(record + kRecFlags, &word, sizeof word);
    std::memcpy(&value, record + kRecPosition, sizeof value);
    TPCoasterSwap::USwapInplaceFloat(value);
    std::memcpy(record + kRecPosition, &value, sizeof value);
    std::memcpy(&value, record + kRecF78, sizeof value);
    TPCoasterSwap::USwapInplaceFloat(value);
    std::memcpy(record + kRecF78, &value, sizeof value);
    std::memcpy(&word, record + kRecF38, sizeof word);
    TPCoasterSwap::USwapInplace32ULong(word);
    std::memcpy(record + kRecF38, &word, sizeof word);
    std::memcpy(&number, record + kRecF64, sizeof number);
    TPCoasterSwap::USwapInplace32Int(number);
    std::memcpy(record + kRecF64, &number, sizeof number);
    std::memcpy(&number, record + kRecF68, sizeof number);
    TPCoasterSwap::USwapInplace32Int(number);
    std::memcpy(record + kRecF68, &number, sizeof number);
    std::memcpy(&half, record + kRecF10, sizeof half);
    TPCoasterSwap::USwapInplace16UShort(half);
    std::memcpy(record + kRecF10, &half, sizeof half);
}

// The car record's two inline word swaps (flags at 0, the ride number at 7).
void SwapCarRecord(uint8_t* record) {
    uint32_t word;
    std::memcpy(&word, record, sizeof word);
    word = Swap32(word);
    std::memcpy(record, &word, sizeof word);
    std::memcpy(&word, record + 7, sizeof word);
    word = Swap32(word);
    std::memcpy(record + 7, &word, sizeof word);
}

// The ride set of a car whose mesh is hidden: the 0x4000 form draws over a range of the map (the src/RideAnimTick.cpp form).
void HideCarMesh(tag_sRideAnimSet* set) {
    if ((set->mFlags & 0x4000) != 0) DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else DisableMeshDraw(set->mHeads);
}

}  // namespace

namespace TPCoasterSwap {

// 0x10043db8 (12 bytes)
void USwapInplace16UShort(uint16_t& value) { value = Swap16(value); }
// 0x10043e00 (12 bytes)
void USwapInplace32Int(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }
// 0x10043e44 (12 bytes)
void USwapInplace32ULong(uint32_t& word) { word = Swap32(word); }
void USwapInplaceFloat(float& value);

}  // namespace TPCoasterSwap

// 0x10042084 (620 bytes). The type's train and car counts are read once for the sizes; the loops read the train count again each pass.
// MReleaseAllocF's flag 1 zeroes the blocks (the cars' mRide stays 0 when no ride is made).
void InstanceCoasterTrains(tag_sCoasterType* type, tag_sCoasterInstance* instance, int32_t flag) {
    const uint32_t trainCount = static_cast<uint32_t>(type->mF2E8);
    const uint32_t cars = static_cast<uint32_t>(type->mF2F4);
    instance->mTrains = static_cast<sTrainInstance*>(MReleaseAllocF(trainCount * static_cast<uint32_t>(sizeof(sTrainInstance)), 1));
    instance->mTrainWords = static_cast<uint32_t*>(MReleaseAllocF(static_cast<uint32_t>(type->mF2E8) * 4, 1));
    instance->mCars = static_cast<sCarInstance*>(MReleaseAllocF(trainCount * cars * static_cast<uint32_t>(sizeof(sCarInstance)), 1));
    sCarInstance* car = instance->mCars;
    instance->mHeads = static_cast<uint32_t*>(MReleaseAllocF(static_cast<uint32_t>(type->mF318) << 3, 1));
    tag_sRideAnimSet* set = nullptr;
    uint32_t* heads = instance->mHeads;
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(type->mF2E8); ++i) {
        train->mFlags = 0;
        train->mType = type;
        train->mCarsInUse = 0;
        train->mCars = car;
        train->mCentreCar = 0;
        train->mF30 = 0.0f;
        train->mPosition = 0.0f;
        train->mF78 = 0.0f;
        for (uint32_t j = 0; j < cars; ++j) {
            sCarType* carType;
            if (j == 0) carType = type->mCarTypeA;
            else if (j == cars - 1) carType = type->mCarTypeC;
            else carType = type->mCarTypeB;
            car->mFlags = 0;
            car->mIndex = j;
            car->mType = carType;
            std::memset(car->mF10, 0, 12);
            car->mF1C = 0;
            car->mHeadsA = heads;
            heads += car->mType->mHeadCount;
            car->mHeadsB = heads;
            heads += car->mType->mHeadCount;
            car->mCurrentA = car->mHeadsA;
            car->mCurrentB = car->mHeadsB;
            if (flag == 0) {
                if (carType->mRideType != 0) {
                    car->mRide = InstanceRide(carType->mRideType, 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffff, 0);
                    HideCarMesh(gRideAnimSets[car->mRide]);
                    set = gRideAnimSets[car->mRide];
                } else {
                    set = nullptr;
                }
            }
            if (set != nullptr) OverrideActiveChannels(set);
            ++car;
        }
        if (type->mCarTypeA == type->mCarTypeB) train->mFirstPersonCar = train->mCars;
        else train->mFirstPersonCar = train->mCars + 1;
        ++train;
    }
}

// 0x10042344 (240 bytes). The type and the train base are read first; the train and car counts are read again each pass; the three blocks are freed
// but not cleared (only mHeads is).
void DestroyCoasterTrains(tag_sCoasterInstance* instance) {
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    sTrainInstance* train = instance->mTrains;
    MReleaseFreeF(instance->mHeads);
    instance->mHeads = nullptr;
    if (instance->mTrains != nullptr) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(type->mF2E8); ++i) {
            if ((train->mFlags & 2) != 0) DeactivateCoasterTrain(instance, train);
            sCarInstance* car = train->mCars;
            const tag_sCoasterType* trainType = train->mType;
            for (uint32_t j = 0; j < static_cast<uint32_t>(trainType->mF2F4); ++j) {
                if (car->mRide != 0) {
                    DisableCoasterCarDrawSafe(car->mRide);
                    DestroyRideInstance(car->mRide);
                }
                ++car;
            }
            ++train;
        }
    }
    MReleaseFreeF(instance->mCars);
    MReleaseFreeF(instance->mTrains);
    MReleaseFreeF(instance->mTrainWords);
}

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

uint32_t gExtrudeFaceCountA;
uint32_t gExtrudeFaceCountB;

// 0x10042478 (72 bytes, TPCoaster.cpp).
void InitialiseCoasterTypes() {
    CreateExtrudeFaceStorage(gExtrudeFaceCountA, gExtrudeFaceCountB);
    SetupObjectInstanceTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest);
}

// PlaceEndPylons 0x100424f0 (508 bytes) and CoasterUVAnimTick 0x100428d4 (676 bytes), TPCoaster.cpp.

// 0x100424f0 (508 bytes). The height argument is never read. The end pylons (one with the type's flag 0x20, else two) take the position of a
// head block of a ride's embedded data; when the entry is not found the point keeps what it had (the Mac leaves the stack slot as it was:
// uninitialised on the first round, the previous round's value on the second).
void PlaceEndPylons(tag_sCoasterInstance* instance, float height) {
    (void)height;
    sPylonRecord* first = reinterpret_cast<sPylonRecord*>(instance->mPylons);
    const uint32_t* type = instance->mType;
    FloatToCell(first->mX);
    FloatToCell(first->mZ);
    const uint32_t count = (type[0] & 0x20) != 0 ? 1 : 2;
    s3DPoint point = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < count; i++) {
        tag_sRideAnimSet* set = nullptr;
        uint32_t entry;
        if ((type[0] & 0x20) != 0) {
            tag_sTrackBrace* brace = instance->mLastBrace;
            const uint8_t* block = reinterpret_cast<const uint8_t*>(brace->mData->mF18Table);
            std::memcpy(&entry, block + 0x2c, 4);
            set = gRideAnimSets[brace->mWord70];
        } else {
            set = gRideAnimSets[instance->mEntranceID];
            entry = FindDataNodeIndexByID(set, 0x400000, i + 1);
        }
        if (entry != 0xffffffffu) {
            sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(set->mHeads->mF28);
            const uint8_t* block = embedded->mRecords[entry].mBlock;
            if (block != nullptr) {
                std::memcpy(&point.x, block + 0x30, 4);
                std::memcpy(&point.y, block + 0x34, 4);
                std::memcpy(&point.z, block + 0x38, 4);
            } else {
                point.x = point.y = point.z = 0.0f;
            }
        }
        const float zero = 0.0f;
        sPylonRecord* pylon = NewPylon(0, 0, 0x13, 0, nullptr, &point);
        const uint32_t flags = (i == 0 && (type[0] & 0x20) != 0) ? 0x144 : 4;
        NewTrackBrace(pylon, pylon->mX, pylon->mZ, zero + pylon->mY, flags, nullptr, 0, 0, 0);
    }
}

// Coaster instances (TPCoaster.cpp on the Mac, unit w186): SetCurrentEditCoaster 0x1003aca0, the pylon interval three 0x10040e10, 0x10040e64 and
// 0x10040eb8, FlagGenerateTrack 0x10040f08, FlagCollisionUpdateTrackAroundBrace 0x10040fa4, XZAngleBetweenPylons 0x10041018,
// GetPotentialPylonPositionArea 0x10041170, DestroyEndPylons 0x10042728, EndTrack 0x100427d0, SetCoasterState 0x10042bb8, IsCoasterType 0x100432dc,
// IsCoaster 0x10043330, FindCoasterInstanceFromEntranceID 0x10043380, SetCoasterCObject 0x10043480, InstanceCoaster 0x100434ec,
// SetCoasterScriptID 0x1004373c and ui_GetNumCoasterTracks 0x100458ac. The globals' places are in include/TPCoaster.h (names invented); every
// callee outside the unit is untranslated.

tag_sCoasterInstance* gCoasterInstances = nullptr;   // 0x102b68b4
int32_t gCoasterInstanceCount = 0;                  // 0x102b68ac
namespace {

// The ride anim set's mesh node: its y place (the chain gRideAnimSets[i] -> mesh -> header -> node mF78 -> matrix row 3, entry 1).
float EntranceHeight(uint32_t entrance) { return gRideAnimSets[entrance]->mHeads->mHeader->mF78->mMatrix.m[3][1]; }

// SetCoasterState's unloading: both unloads of each train, then the queue's peeps are kicked out. The train base is read once.
void UnloadTrains(tag_sCoasterInstance* instance) {
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i) {
        UnloadTrain(instance, train, 0);
        UnloadTrain(instance, train, 1);
        ++train;
    }
    KickPeepsFromEntranceQueue(instance);
}

}  // namespace

// 0x10042728 (104 bytes): the end pylon braces lead the list; each is deleted (the next is read first). There is no NULL test: the list always
// holds a plain brace after the end pylons.
void DestroyEndPylons(tag_sCoasterInstance* instance) {
    tag_sTrackBrace* brace = instance->mBraces;
    while ((brace->mData->mFlags & 1) != 0) {
        tag_sTrackBrace* next = brace->mNext;
        DeleteTrackBrace(instance, brace);
        brace = next;
    }
    instance->mBlock3C &= ~1u;
}

// 0x100427d0 (228 bytes): the edit coaster is read from its global each time (the callees may change it).
void EndTrack(int32_t flag) {
    gCoasterEditInstance->mBlock3C |= 1;
    tag_sCoasterInstance* edit = gCoasterEditInstance;
    PlaceEndPylons(edit, EntranceHeight(edit->mEntranceID));
    edit = gCoasterEditInstance;
    RecalcTrackParams(edit);
    edit->mFlags |= 1;
    edit->mFlags |= 0x1000;
    if (gActionRecorder.mRecording == 0) SetCoasterEditState(0x10);
    if (flag == 0) CoasterPanelEndTrack();
    SetCoasterState(gCoasterEditInstance, 2);
}

// 0x100428d4 (676 bytes). Scrolls every dynamic texture's offset by the elapsed time (the difference of two unsigned times, in thousandths
// of a second, times the coaster's speed word) and then moves the V coordinates of the vertices that the track mesh sets' flagged face sets
// use back by the same step. The step is reduced by every whole number an offset gives back (kept).
void CoasterUVAnimTick(tag_sCoasterInstance* instance) {
    const uint32_t now = GetRideAnimTime(0);
    uint32_t last;
    std::memcpy(&last, instance->mF138, 4);
    float step = instance->mF2C * (static_cast<float>(static_cast<double>(now - last)) / 1000.0f);
    tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType));
    float* offset = instance->mUVOffsets;
    for (uint32_t n = type->mTextureCount; n != 0; n--) {
        *offset = *offset + step;
        if (*offset > 1.0f) {
            double whole;
            std::modf(static_cast<double>(*offset), &whole);
            const float cut = static_cast<float>(whole);
            *offset = *offset - cut;
            step = step - cut;
        }
        offset++;
    }
    sDTextureSet* textures = type->mTextures;
    for (tag_sDMeshSet* set = instance->mF38; set != nullptr; set = set->mF38) {
        int32_t used[256];
        std::memset(used, 0, 0x400);
        uint32_t maxIndex = 0;
        tag_sNode* node = set->mMesh->mHeader->mF78.get();
        const sFaceSet* faceSet = node->mFaceSets.get();
        for (uint32_t i = 0; i < node->mFaceSetCount; i++, faceSet++) {
            const int32_t bytes = static_cast<int32_t>(reinterpret_cast<intptr_t>(faceSet->mTexture.get()) -
                                                       reinterpret_cast<intptr_t>(textures->mTextures));
            const int32_t slot = bytes / 8;
            if ((textures->mEntries[slot].mFlags & 0x20) == 0) continue;
            const sFace* face = node->mFaces.get() + faceSet->mFirstFace;
            for (uint32_t k = faceSet->mFaceCount; k != 0; k--, face++) {
                used[face->mF02[0]] = 1;
                used[face->mF02[1]] = 1;
                used[face->mF02[2]] = 1;
                if (face->mF02[0] > maxIndex) maxIndex = face->mF02[0];
                if (face->mF02[1] > maxIndex) maxIndex = face->mF02[1];
                if (face->mF02[2] > maxIndex) maxIndex = face->mF02[2];
            }
        }
        sUVSet* uvs = node->mUVs.get();
        for (uint32_t k = 0; k <= maxIndex; k++) {
            if (used[k] == 0) continue;
            sUVSet& block = uvs[k >> 2];
            const float u = block.mF00[k & 3];
            const float v = block.mF00[4 + (k & 3)];
            block.mF00[k & 3] = u;
            block.mF00[4 + (k & 3)] = v - step;
        }
    }
    std::memcpy(instance->mF138, &now, 4);
}

// 0x10042bb8 (1,072 bytes): the low nibble of the state is the running kind (1 building, 2 stopped, 4 and 8 the loading kinds), the bits 0x70
// the mode (0x10 and 0x20 the editor's, 0x40 the test run). Every compare is unsigned.
void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) {
    const uint32_t low = state & 0xf;
    if (low != 0) {
        const uint32_t current = instance->mState & 0xf;
        if (current == 4) {
            if (low == 2) {
                UnloadTrains(instance);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        } else if (current == 2 && low != 4) {
            if (low == 1) {
                for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i)
                    DeactivateCoasterTrain(instance, instance->mTrains + i);
                instance->mFlags &= ~0x800u;
                FlashEndPylons(instance, 2);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        } else if (current == 1 && low != 4) {
            if (low == 2) {
                if ((instance->mState & 0x40) == 0) instance->mF2C = 1.0f;
                sTrainInstance* train = instance->mTrains;
                for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i) {
                    ActivateCoasterTrain(instance, train);
                    ++train;
                }
                FlashEndPylons(instance, 0);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        } else if (current == 8 && low != 4) {
            if (low == 2) {
                UnloadTrains(instance);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        }
        instance->mState &= ~0xfu;
        instance->mState |= low;
    }
    const uint32_t high = state & 0x70;
    if (high != 0) {
        const uint32_t current = instance->mState & 0x70;
        if ((current == 0x20 || current == 0x10) && high == 0x40) {
            instance->mF2C = 0.0f;
        } else if (current == 0x40 && (high == 0x20 || high == 0x10)) {
            instance->mF2C = 1.0f;
            sTrainInstance* train = instance->mTrains;
            for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i) {
                train->mFlags |= 0x80;
                train->mFlags &= ~0x40u;
                ++train;
            }
        }
        instance->mState = (instance->mState & ~0x70u) | high;
    }
}

// The coasters' per-frame ticks and the track editor's pointer input (main app TPCoaster.cpp and the TPConfig.cpp range, unit w137):
// CoasterTickPreRender 0x10043028 (428 bytes), CoasterTickPostRender 0x10043200 (176) and CoasterUserInputTick 0x100554b4 (2,348).
// The globals' places are in include/TPCoaster.h (names invented); every callee outside the unit is untranslated.

tag_sCoasterInstance* gCoasterCurrent;
void* gCoasterCurrentData;
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterWord7310;
namespace {

constexpr uint32_t kTrainRunning = 2;   // the train's flags word

}  // namespace

// 0x10043028. The first walk generates the flagged coasters' tracks (the preview brace is linked into the editor's coaster; the action
// 0x43 and the coaster's id are recorded the first time), the second ticks the running ones. GenerateTrack gets the global.
void CoasterTickPreRender() {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if ((instance->mFlags & 1) == 0) continue;
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        if (instance == gCoasterEditInstance && (instance->mFlags & 4) != 0) LinkPreviewBrace(instance);
        if ((instance->mFlags & 0x2000) == 0) gActionRecorder.RecordAction(0x43, static_cast<uint16_t>(instance->mObjectInstanceID));
        GenerateTrack(gCoasterCurrent);
        instance->mFlags = instance->mFlags | 0x2000;
    }
    gCoasterCurrent = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if ((instance->mState & 0xe) == 0) continue;
        CoasterCarRideTrackTick(instance);
        if ((*instance->mType & 4) != 0) CoasterUVAnimTick(instance);
        sTrainInstance* train = instance->mTrains;
        const uint32_t state = instance->mState;
        if ((state & 0xc) != 0 && (state & 0x40) == 0) {
            for (uint32_t count = static_cast<uint32_t>(instance->mTrainCount); count != 0; --count) {
                if ((train->mFlags & kTrainRunning) != 0) {
                    HandleTrainLoading(instance, train);
                    TickTrainSounds(instance, train);
                }
                ++train;
            }
        } else {
            for (uint32_t count = static_cast<uint32_t>(instance->mTrainCount); count != 0; --count) {
                if ((train->mFlags & kTrainRunning) != 0) TickTrainSounds(instance, train);
                ++train;
            }
        }
    }
    gCoasterWord7310 = 0;
}

// 0x10043200. The flagged coasters lose flag 1 (through the global); the editor's one unlinks its preview brace first.
void CoasterTickPostRender() {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if ((instance->mFlags & 1) == 0) continue;
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        if (instance == gCoasterEditInstance && (instance->mFlags & 4) != 0) UnLinkPreviewBrace(instance);
        gCoasterCurrent->mFlags = gCoasterCurrent->mFlags & ~1u;
    }
    gCoasterCurrent = nullptr;
}

// 0x100432dc (44 bytes).
int32_t IsCoasterType(uint32_t type) { return (gRideTypes[type]->mFlags & 0x40000) != 0 ? 1 : 0; }

// 0x10043330 (44 bytes).
int32_t IsCoaster(uint32_t instance) { return (gRideAnimSets[instance]->mFlags & 0x40000) != 0 ? 1 : 0; }

// 0x10043380 (56 bytes).
tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance) {
    tag_sCoasterInstance* instance = gCoasterInstances;
    while (instance != nullptr) {
        if (instance->mEntranceID == entrance) return instance;
        instance = instance->mNext;
    }
    return nullptr;
}

// 0x100433f4 (92 bytes): the list is walked here (FindCoasterInstanceFromEntranceID is not called); the answer is the ride of the lead train's
// first-person car, 0 when the entrance has no coaster or no train is active.
uint32_t GetCoaster1stPersonCar(uint32_t entrance) {
    tag_sCoasterInstance* found = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if (instance->mEntranceID == entrance) {
            found = instance;
            break;
        }
    }
    if (found != nullptr && (found->mFlags & 0x800) != 0) return found->mLeadTrain->mFirstPersonCar->mRide;
    return 0;
}

// 0x10043480 (64 bytes).
void SetCoasterCObject(uint32_t entrance, void* object) {
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(entrance);
    if (instance != nullptr) instance->mCObject = object;
}

// 0x100434ec (544 bytes). Without the flag the instance gets an object number (none: the instance is freed and 0 answered). The type is found by
// its ride type number (NULL when there is none: the Mac would crash on the first read). The Mac answers its register 25 when the flag is set
// (undefined); the host answers 0.
uint32_t InstanceCoaster(uint32_t typeNumber, uint32_t entrance, uint32_t x, uint32_t z, int32_t flag, void* object) {
    // The Mac asks for 0x14c bytes; the host's record holds native pointers and is bigger, so it asks for its own size.
    auto* instance = static_cast<tag_sCoasterInstance*>(MReleaseAllocF(sizeof(tag_sCoasterInstance), 1));
    uint32_t id = 0;
    if (flag == 0) {
        id = AllocateObjectInstanceToTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest, 0xff, gObjectTable, instance, 0xffffffffu);
        if (id == 0) {
            MReleaseFreeF(instance);
            return 0;
        }
        instance->mObjectInstanceID = id;
    }
    instance->mNext = gCoasterInstances;
    gCoasterInstances = instance;
    tag_sCoasterType* type = nullptr;
    for (uint32_t i = 0; i < gCoasterTypeCount; ++i) {
        if (gCoasterTypes[i]->mRideTypeNumber == typeNumber) {
            type = gCoasterTypes[i];
            break;
        }
    }
    instance->mType = reinterpret_cast<const uint32_t*>(type);
    instance->mTypeID = typeNumber;
    instance->mEntranceID = entrance;
    instance->mX = x;
    instance->mZ = z;
    instance->mF118 = type->mFA4;
    instance->mF11C = 0.0f;
    instance->mFAC = 3.0f;
    AllocateQueue(&instance->mQueueB0, static_cast<uint32_t>(type->mF2F8));
    AllocateQueue(&instance->mQueueD0, static_cast<uint32_t>(type->mF318) << 1);
    if ((type->mFlags & 8) != 0) instance->mBlock3C |= 8;
    if ((type->mFlags & 4) != 0)
        instance->mUVOffsets = static_cast<float*>(MReleaseAllocF(type->mTextureCount << 2, 1));
    instance->mCObject = object;
    gCoasterEditInstance = instance;
    SetCoasterState(instance, 0x11);
    CoasterSetSpeed(instance, 0x3c, flag);
    if (flag == 0) {
        InstanceCoasterTrains(type, instance, 0);
        SetTrainCapacity(instance, 0, static_cast<uint32_t>(type->mF2F4), 1);
        PlaceEntrancePylons(instance, x, z, EntranceHeight(entrance), 0);
    }
    ++gCoasterInstanceCount;
    gCoasterEditInstance = nullptr;
    return id;
}
// 0x1004373c (100 bytes): the sounds are restored even when no instance was found (the Mac passes the NULL on).
void SetCoasterScriptID(uint32_t entrance, int32_t id) {
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(entrance);
    if (instance != nullptr) instance->mScriptID = id;
    RestoreCoasterSounds(instance);
}

// Coaster instance teardown (main app TPCoaster.cpp, unit w105): DestroyCoasterInstance 0x100437cc (236 bytes),
// DestroyCoasterInstanceFromEntranceID 0x100438fc (92) and DestroyCoasterInstances 0x10043994 (84).

// 0x100437cc. The object table loses the instance's number and the count goes down before the parts are destroyed. The list is
// unlinked from the head, or by a walk of the whole list (it does not stop at the match; the predecessor is always set by then because
// the head is not the instance), then the queues and the instance itself are freed.
void DestroyCoasterInstance(tag_sCoasterInstance* instance) {
    RemoveObjectInstanceFromTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest, gObjectTable,
                                  instance->mObjectInstanceID);
    --gCoasterInstanceCount;
    DestroyCoasterTrains(instance);
    DestroyCoasterTrack(instance);
    DestroyCoasterPylons(instance);
    tag_sCoasterInstance* head = gCoasterInstances;
    if (instance == head) {
        gCoasterInstances = instance->mNext;
    } else {
        tag_sCoasterInstance* previous = nullptr;
        for (tag_sCoasterInstance* node = head; node != nullptr; node = node->mNext) {
            if (node == instance) previous->mNext = node->mNext;
            previous = node;
        }
    }
    DestroyQueue(&instance->mQueueB0);
    DestroyQueue(&instance->mQueueD0);
    MReleaseFreeF(instance);
}

// 0x100438fc. The first instance of the list with that entrance id, if any.
void DestroyCoasterInstanceFromEntranceID(uint32_t entrance) {
    tag_sCoasterInstance* found = nullptr;
    for (tag_sCoasterInstance* node = gCoasterInstances; node != nullptr; node = node->mNext) {
        if (node->mEntranceID == entrance) {
            found = node;
            break;
        }
    }
    if (found != nullptr) DestroyCoasterInstance(found);
}

// 0x10043994. The next instance is read before each is destroyed; the head is cleared at the end.
void DestroyCoasterInstances() {
    tag_sCoasterInstance* node = gCoasterInstances;
    while (node != nullptr) {
        tag_sCoasterInstance* next = node->mNext;
        DestroyCoasterInstance(node);
        node = next;
    }
    gCoasterInstances = nullptr;
}

// 0x10043a18 (864 bytes). A train: the packed 0x1b byte record (swapped for the write, swapped back after it, then the count of records written
// is checked); each car: an 11 byte record and the two arrays of head words (a swapped copy each). A failed array write leaves the copy unfreed.
int32_t SaveTrainsAndCars(void* file, tag_sCoasterInstance* instance) {
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF2E8); ++i) {
        uint8_t record[kTrainRecord];
        uint32_t transferred = 0;
        const uint32_t flags = train->mFlags;
        std::memcpy(record + kRecFlags, &flags, sizeof flags);
        record[kRecCarsInUse] = static_cast<uint8_t>(train->mCarsInUse);
        std::memcpy(record + kRecPosition, &train->mPosition, sizeof(float));
        const uint32_t f38 = train->mF38;
        std::memcpy(record + kRecF38, &f38, sizeof f38);
        const int32_t f64 = train->mF64;
        std::memcpy(record + kRecF64, &f64, sizeof f64);
        const int32_t f68 = train->mF68;
        std::memcpy(record + kRecF68, &f68, sizeof f68);
        const uint16_t f10 = static_cast<uint16_t>(train->mF10);
        std::memcpy(record + kRecF10, &f10, sizeof f10);
        std::memcpy(record + kRecF78, &train->mF78, sizeof(float));
        SwapTrainRecord(record);
        LbFile_Write(file, record, kTrainRecord, &transferred);
        const uint32_t written = transferred / kTrainRecord;
        SwapTrainRecord(record);
        if (written != 1) {
            SamsUtilities::UDebug("write failed");
            return 0;
        }
        sCarInstance* car = train->mCars;
        for (uint32_t j = 0; j < static_cast<uint32_t>(train->mType->mF2F4); ++j) {
            uint8_t carRecord[kCarRecord];
            uint32_t carTransferred = 0;
            std::memcpy(carRecord, &car->mFlags, sizeof(uint32_t));
            carRecord[4] = static_cast<uint8_t>(car->mCountA);
            carRecord[5] = static_cast<uint8_t>(car->mCountB);
            carRecord[6] = static_cast<uint8_t>(car->mF44);
            std::memcpy(carRecord + 7, &car->mRide, sizeof(uint32_t));
            SwapCarRecord(carRecord);
            LbFile_Write(file, carRecord, kCarRecord, &carTransferred);
            SwapCarRecord(carRecord);
            if (carTransferred / kCarRecord != 1) {
                SamsUtilities::UDebug("write failed");
                return 0;
            }
            if (car->mCountA != 0) {
                uint32_t* copy = SamsUtilities::_USwapCopy32(car->mCurrentA, static_cast<int32_t>(car->mCountA));
                TbError result = LbFile_Write(file, copy, car->mCountA * 4, nullptr);
                if (!result.Succeeded()) return 0;
                SamsUtilities::_USwapDelete(copy);
            }
            if (car->mCountB != 0) {
                uint32_t* copy = SamsUtilities::_USwapCopy32(car->mCurrentB, static_cast<int32_t>(car->mCountB));
                TbError result = LbFile_Write(file, copy, car->mCountB * 4, nullptr);
                if (!result.Succeeded()) return 0;
                SamsUtilities::_USwapDelete(copy);
            }
            ++car;
        }
        ++train;
    }
    return 1;
}

// 0x10043e8c (948 bytes): the mirror of the save. A train's record is swapped, then the count read is checked, then the fields are stored and flag 4
// ("restored from a save") is set; each car's record is swapped and checked, its ride restored and its mesh hidden unless the record's flag 1 is set,
// then the head words are read when both current pointers are set.
int32_t LoadTrainsAndCars(void* file, tag_sCoasterInstance* instance) {
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF2E8); ++i) {
        uint8_t record[kTrainRecord];
        uint32_t transferred = 0;
        LbFile_Read(file, record, kTrainRecord, &transferred);
        const uint32_t read = transferred / kTrainRecord;
        SwapTrainRecord(record);
        if (read != 1) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }
        uint32_t flags;
        std::memcpy(&flags, record + kRecFlags, sizeof flags);
        train->mFlags = flags;
        train->mCarsInUse = record[kRecCarsInUse];
        std::memcpy(&train->mPosition, record + kRecPosition, sizeof(float));
        std::memcpy(&train->mF38, record + kRecF38, sizeof(uint32_t));
        std::memcpy(&train->mF64, record + kRecF64, sizeof(int32_t));
        std::memcpy(&train->mF68, record + kRecF68, sizeof(int32_t));
        uint16_t f10;
        std::memcpy(&f10, record + kRecF10, sizeof f10);
        train->mF10 = f10;
        std::memcpy(&train->mF78, record + kRecF78, sizeof(float));
        train->mFlags |= 4;
        sCarInstance* car = train->mCars;
        for (uint32_t j = 0; j < static_cast<uint32_t>(train->mType->mF2F4); ++j) {
            uint8_t carRecord[kCarRecord];
            uint32_t carTransferred = 0;
            LbFile_Read(file, carRecord, kCarRecord, &carTransferred);
            SwapCarRecord(carRecord);
            if (carTransferred / kCarRecord != 1) {
                SamsUtilities::UDebug("read failed");
                return 0;
            }
            uint32_t carFlags;
            std::memcpy(&carFlags, carRecord, sizeof carFlags);
            car->mFlags = carFlags;
            car->mCountA = carRecord[4];
            car->mCountB = carRecord[5];
            car->mF44 = carRecord[6];
            std::memcpy(&car->mRide, carRecord + 7, sizeof(uint32_t));
            if (car->mType->mRideType != 0) {
                RestoreRideInstance(car->mRide, car->mType->mRideType, 0x101, nullptr, 0, nullptr, 0, 0, 0);
                if ((carFlags & 1) == 0) HideCarMesh(gRideAnimSets[car->mRide]);
                tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
                if (set != nullptr) OverrideActiveChannels(set);
            }
            if (car->mCurrentA != nullptr && car->mCurrentB != nullptr) {
                TbError result = LbFile_Read(file, car->mCurrentA, car->mCountA * 4, nullptr);
                if (!result.Succeeded()) return 0;
                result = LbFile_Read(file, car->mCurrentB, car->mCountB * 4, nullptr);
                if (!result.Succeeded()) return 0;
                SamsUtilities::_USwapBlock32(car->mCurrentA, static_cast<int32_t>(car->mCountA));
                SamsUtilities::_USwapBlock32(car->mCurrentB, static_cast<int32_t>(car->mCountB));
            }
            ++car;
        }
        ++train;
    }
    return 1;
}

// SaveCoasters 0x10044280 (2156 bytes): the coasters' part of a save. The count of instances and the object table's three counters, then for each
// instance a 0x20 byte record, for each of its braces a count and the brace's heights (kind 2 braces) and a 0x22 byte record, then the trains
// (SaveTrainsAndCars). Every write is checked against the count of items written; a short write logs "write failed" and answers 0.
// The file's words are little-endian: the swaps are the real ones on a big-endian host and nothing on this one (TPCoasterSwap).

namespace {

template <class T>
T Get(const uint8_t* p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void Swap32_2(uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof v);
    TPCoasterSwap::USwapInplace32ULong(v);
    std::memcpy(p, &v, sizeof v);
}

void Swap16_2(uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof v);
    TPCoasterSwap::USwapInplace16UShort(v);
    std::memcpy(p, &v, sizeof v);
}

void SwapFloat(uint8_t* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    TPCoasterSwap::USwapInplaceFloat(v);
    std::memcpy(p, &v, sizeof v);
}

void SwapInstanceRecord(uint8_t* r) {
    Swap32_2(r + 0x00);
    Swap32_2(r + 0x04);
    Swap16_2(r + 0x08);
    Swap16_2(r + 0x0a);
    Swap16_2(r + 0x0c);
    Swap16_2(r + 0x0e);
    Swap16_2(r + 0x10);
    SwapFloat(r + 0x12);
    Swap16_2(r + 0x16);
    Swap16_2(r + 0x18);
    Swap16_2(r + 0x1a);
    Swap32_2(r + 0x1c);
}

void SwapBraceRecord(uint8_t* r) {
    Swap32_2(r + 0x00);
    Swap32_2(r + 0x04);
    Swap16_2(r + 0x08);
    Swap16_2(r + 0x0a);
    SwapFloat(r + 0x0d);
    SwapFloat(r + 0x11);
    SwapFloat(r + 0x15);
    SwapFloat(r + 0x19);
    SwapFloat(r + 0x1d);
}

// A 32-bit word swapped where it is, written, swapped back; answers whether one item went out.
bool WriteWord(void* file, uint32_t& word, uint32_t shift) {
    uint32_t transferred;
    TPCoasterSwap::USwapInplace32ULong(word);
    LbFile_Write(file, &word, 4, &transferred);
    TPCoasterSwap::USwapInplace32ULong(word);
    return (transferred >> shift) == 1;
}

bool WriteHeights(void* file, const tag_sTrackBrace* brace) {
    // The collision list (mCollisions on the host): each brace's first word, its number.
    for (uint32_t i = 0; i < brace->mFB0; ++i) {
        uint16_t value = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(brace->mCollisions[i]));
        uint32_t transferred;
        TPCoasterSwap::USwapInplace16UShort(value);
        LbFile_Write(file, &value, 2, &transferred);
        TPCoasterSwap::USwapInplace16UShort(value);
        if ((transferred >> 1) != 1) {
            SamsUtilities::UDebug("write failed");
            return false;
        }
    }
    return true;
}

}  // namespace

int32_t SaveCoasters(void* file) {
    uint32_t count = 0;
    for (tag_sCoasterInstance* it = gCoasterInstances; it != nullptr; it = it->mNext) ++count;
    if (!WriteWord(file, count, 2) || !WriteWord(file, gObjectTableCount, 2) || !WriteWord(file, gObjectTableFirstFree, 2) ||
        !WriteWord(file, gObjectTableLowest, 2)) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    for (tag_sCoasterInstance* inst = gCoasterInstances; inst != nullptr; inst = inst->mNext) {
        uint16_t visible = 0;
        for (tag_sTrackBrace* b = inst->mLastBrace; b != nullptr; b = b->mPrev)
            if ((b->mData->mFlags & 0x10) == 0) ++visible;

        uint32_t flags = 0;
        if (inst->mBlock3C & 1) flags |= 1;
        if (inst->mBlock3C & 2) flags |= 2;
        if (inst->mFlags & 0x20) flags |= 4;
        if (inst->mFlags & 0x40) flags |= 8;
        if (inst->mFlags & 0x80) flags |= 0x10;
        if (inst->mFlags & 0x100) flags |= 0x20;
        if (inst->mFlags & 0x200) flags |= 0x40;
        if (inst->mFlags & 0x400) flags |= 0x80;
        if (inst->mFlags & 0x800) flags |= 0x100;

        uint8_t rec[0x20];
        std::memcpy(rec + 0x00, &flags, 4);
        std::memcpy(rec + 0x04, &inst->mState, 4);
        const uint16_t h08 = static_cast<uint16_t>(inst->mTypeID);
        const uint16_t h0a = static_cast<uint16_t>(inst->mEntranceID);
        const uint16_t h0c = static_cast<uint16_t>(inst->mObjectInstanceID);
        const uint16_t h0e = static_cast<uint16_t>(inst->mX);
        const uint16_t h10 = static_cast<uint16_t>(inst->mZ);
        std::memcpy(rec + 0x08, &h08, 2);
        std::memcpy(rec + 0x0a, &h0a, 2);
        std::memcpy(rec + 0x0c, &h0c, 2);
        std::memcpy(rec + 0x0e, &h0e, 2);
        std::memcpy(rec + 0x10, &h10, 2);
        std::memcpy(rec + 0x12, &inst->mF2C, 4);
        std::memcpy(rec + 0x16, &visible, 2);
        const uint16_t h18 = static_cast<uint16_t>(inst->mTrainCount);
        const uint16_t h1a = static_cast<uint16_t>(Get<uint32_t>(inst->mFF0));
        std::memcpy(rec + 0x18, &h18, 2);
        std::memcpy(rec + 0x1a, &h1a, 2);
        std::memcpy(rec + 0x1c, inst->mF140, 4);
        SwapInstanceRecord(rec);
        uint32_t transferred;
        LbFile_Write(file, rec, 0x20, &transferred);
        const uint32_t written = transferred >> 5;
        SwapInstanceRecord(rec);
        if (written != 1) {
            SamsUtilities::UDebug("write failed");
            return 0;
        }

        for (tag_sTrackBrace* b = inst->mLastBrace; b != nullptr; b = b->mPrev) {
            if (b->mNumber == 2) {
                if (!WriteWord(file, b->mFB0, 2)) {
                    SamsUtilities::UDebug("write failed");
                    return 0;
                }
                if (!WriteHeights(file, b)) return 0;
            }
            if ((b->mData->mFlags & 0x10) != 0) continue;

            uint32_t bf = 0;
            if (b->mFlags & 0x1) bf |= 1;
            if (b->mFlags & 0x2) bf |= 2;
            if (b->mFlags & 0x4) bf |= 4;
            if (b->mFlags & 0x80) bf |= 8;
            if (b->mFlags & 0x40) bf |= 0x10;
            if (b->mFlags & 0x100) bf |= 0x20;
            if (b->mFlags & 0x10000) bf |= 0x40;
            if (b->mFlags & 0x20000) bf |= 0x80;
            if (b->mFlags & 0x40000) bf |= 0x100;
            if (b->mFlags & 0x80000) bf |= 0x200;
            if (b->mFlags & 0x200000) bf |= 0x400;
            if (b->mFlags & 0x400000) bf |= 0x800;

            uint8_t br[0x22];
            std::memcpy(br + 0x00, &bf, 4);
            std::memcpy(br + 0x04, &b->mWord70, 4);
            const uint16_t c08 = static_cast<uint16_t>(b->mData->mCellX);
            const uint16_t c0a = static_cast<uint16_t>(b->mData->mCellZ);
            std::memcpy(br + 0x08, &c08, 2);
            std::memcpy(br + 0x0a, &c0a, 2);
            br[0x0c] = static_cast<uint8_t>(Get<uint32_t>(b->mF00 + 4));
            std::memcpy(br + 0x0d, &b->mFE8w, 4);
            std::memcpy(br + 0x11, &b->mFD8, 4);
            std::memcpy(br + 0x15, &b->mFDC, 4);
            std::memcpy(br + 0x19, &b->mAngle, 4);
            std::memcpy(br + 0x1d, &b->mFF8, 4);
            br[0x21] = static_cast<uint8_t>(b->mFB0);
            SwapBraceRecord(br);
            LbFile_Write(file, br, 0x22, &transferred);
            const uint32_t bwritten = transferred / 0x22;
            SwapBraceRecord(br);
            if (bwritten != 1) {
                SamsUtilities::UDebug("write failed");
                return 0;
            }
            if (!WriteHeights(file, b)) return 0;
        }
        SaveTrainsAndCars(file, inst);
    }
    return 1;
}

namespace {
// A person (reached by Mac offset, as the other sources do).
uint16_t PersonRide(const CThing* person) { return *reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(person) + 0x1dc); }
int32_t PersonState(const CThing* person) { return *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(person) + 0x220); }
}  // namespace

// 0x10044b10 (552 bytes). r28/r29 stay the caller's when the coaster's script is NULL; here they start 0.
void HACKRestoreCoasterQueues(uint32_t instance) {
    tag_sCoasterInstance* found = nullptr;
    for (tag_sCoasterInstance* it = gCoasterInstances; it != nullptr; it = it->mNext) {
        if (it->mEntranceID == instance) {
            found = it;
            break;
        }
    }
    if (found == nullptr) return;
    uint16_t coasterId = static_cast<const CThing*>(found->mCObject)->GetThingId().mIndex;
    uint32_t var0 = 0;
    uint32_t var1 = 0;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(found->mScriptID);
    if (script != nullptr) {
        var0 = static_cast<uint32_t>(RSSE_Getvar(script, 0));
        var1 = static_cast<uint32_t>(RSSE_Getvar(script, 1));
    }
    for (CThing* person = UI_IterateThingNext(kUIIterVisitors, nullptr); person != nullptr;
         person = UI_IterateThingNext(kUIIterVisitors, person)) {
        if (PersonRide(person) != coasterId || PersonState(person) != 0x10) continue;
        uint16_t peep = person->GetThingId().mIndex;
        if (var0 == peep || var1 == peep) continue;
        bool present = false;
        sTrainInstance* train = found->mTrains;
        for (uint32_t t = 0; t < static_cast<uint32_t>(found->mTrainCount) && !present; ++t, ++train) {
            sCarInstance* car = train->mCars;
            for (uint32_t c = 0; c < train->mCarsInUse && !present; ++c, ++car) {
                for (uint32_t k = 0; k < car->mCountA; ++k) {
                    if (static_cast<uint16_t>(car->mCurrentA[k]) == peep) {
                        present = true;
                        break;
                    }
                }
                if (present) break;
                for (uint32_t k = 0; k < car->mCountB; ++k) {
                    if (static_cast<uint16_t>(car->mCurrentB[k]) == peep) {
                        present = true;
                        break;
                    }
                }
            }
        }
        if (!present) HACKCoasterReturnLostPeep(found, peep);
    }
}

// LoadCoasters (0x10044d68, 2848 bytes, TPCoaster.cpp on the Mac): reads the coasters of a park file back (the counterpart of SaveCoasters).
// The file is little-endian; the swaps are TPCoasterSwap's (real on a big-endian host, no-ops here).

namespace {

// The unaligned floats of the file's records go through a local (the Mac swaps them in place).
float SwapFloatAt(const uint8_t* p) {
    float value;
    std::memcpy(&value, p, 4);
    TPCoasterSwap::USwapInplaceFloat(value);
    return value;
}
uint32_t SwapWordAt(const uint8_t* p) {
    uint32_t value;
    std::memcpy(&value, p, 4);
    TPCoasterSwap::USwapInplace32ULong(value);
    return value;
}
uint16_t SwapHalfAt(const uint8_t* p) {
    uint16_t value;
    std::memcpy(&value, p, 2);
    TPCoasterSwap::USwapInplace16UShort(value);
    return value;
}

// The height of an entrance's ride: set -> mesh -> header -> the node at header + 0x78, its float at +0x44.
float EntranceHeight_2(uint32_t entrance) {
    const tag_sRideAnimSet* set = gRideAnimSets[entrance];
    const tag_sM3D2Header* header = set->mHeads->mHeader;
    const uint8_t* node = reinterpret_cast<const uint8_t*>(header->mF78.get());
    float value;
    std::memcpy(&value, node + 0x44, 4);
    return value;
}

// Reads a brace's mFB0 collision entries (halfwords of the file: the braces' numbers) into mCollisions, the numbers held as
// pointers until they are resolved; false on a short read. A count above 8 fills slots the Mac shares with mFD4..mFDC.

bool ReadBraceList(void* file, tag_sTrackBrace* brace) {
    for (uint32_t k = 0; k < brace->mFB0; k++) {
        uint16_t half = 0;
        uint32_t transferred = 0;
        LbFile_Read(file, &half, 2, &transferred);
        if (transferred >> 1 != 1) {
            SamsUtilities::UDebug("read failed");
            return false;
        }
        TPCoasterSwap::USwapInplace16UShort(half);
        brace->mCollisions[k] = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(half));
    }
    return true;
}

}  // namespace

int32_t LoadCoasters(void* file) {
    uint32_t count = 0;
    uint32_t transferred = 0;

    LbFile_Read(file, &count, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(count);

    LbFile_Read(file, &gObjectTableCount, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(gObjectTableCount);

    LbFile_Read(file, &gObjectTableFirstFree, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(gObjectTableFirstFree);

    LbFile_Read(file, &gObjectTableLowest, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    TPCoasterSwap::USwapInplace32ULong(gObjectTableLowest);

    tag_sTrackBrace* previewBrace = nullptr;   // r17: the brace of flag 0x40 (or the start of the list)
    tag_sTrackBrace* flagBrace = nullptr;      // r15: the brace of flag 0x80

    for (uint32_t index = 0; index < count; index++) {
        gPylonStackSkip = 1;

        // The coaster's record: 0x20 bytes at (offsets) 0 flags, 4 state, 8 half, 10 entrance, 12 extra, 14 x, 16 z, 18 float, 22 pylon count,
        // 24, 26 halves, 28 word.
        uint8_t rec[0x20];
        LbFile_Read(file, rec, 0x20, &transferred);
        uint32_t recordsRead = transferred >> 5;
        uint32_t recFlags = SwapWordAt(rec + 0);
        uint32_t recState = SwapWordAt(rec + 4);
        (void)SwapHalfAt(rec + 8);
        uint32_t entrance = SwapHalfAt(rec + 10);
        uint32_t extra = SwapHalfAt(rec + 12);
        uint32_t cellX = SwapHalfAt(rec + 14);
        uint32_t cellZ = SwapHalfAt(rec + 16);
        float recFloat = SwapFloatAt(rec + 18);
        uint32_t pylonCount = SwapHalfAt(rec + 22);
        uint32_t recTrains = SwapHalfAt(rec + 24);
        uint32_t recWord = SwapHalfAt(rec + 26);
        uint32_t recLast = SwapWordAt(rec + 28);
        if (recordsRead != 1) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }

        tag_sCoasterInstance* instance = gCoasterInstances;
        for (; instance != nullptr; instance = instance->mNext) {
            if (instance->mEntranceID == entrance) {
                gCoasterEditInstance = instance;
                gCoasterCurrentData = &instance->mBlock3C;
                InstanceCoasterTrains(reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType)), instance, 1);
                PlaceEntrancePylons(instance, cellX, cellZ, EntranceHeight_2(entrance), 1);
                instance->mObjectInstanceID =
                    AllocateObjectInstanceToTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest, 0xff, gObjectTable, instance, extra);
                instance->mFlags |= 2;
                break;
            }
        }
        // No coaster with that entrance: the original goes on with a NULL instance (it reads address 0).
        instance->mFlags |= 0x4000;

        if (recFlags & 2) {
            flagBrace = instance->mBraces;
            previewBrace = instance->mLastBrace->mPrev->mPrev;
        }
        tag_sTrackBrace* brace = instance->mLastBrace->mPrev->mPrev;

        LbFile_Read(file, &brace->mFB0, 4, &transferred);
        if (transferred >> 2 != 1) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }
        TPCoasterSwap::USwapInplace32ULong(brace->mFB0);
        if (!ReadBraceList(file, brace)) return 0;

        for (uint32_t p = 0; p < pylonCount; p++) {
            // The pylon's record: 0x22 bytes at 0 flags, 4 extra, 8 x, 10 z, 12 level, 13 loft, 17 wobble x, 21 wobble z, 25 rotate, 29 wobble
            // (flag 0x10), 33 brace count.
            uint8_t prec[0x22];
            LbFile_Read(file, prec, 0x22, &transferred);
            uint32_t pRead = transferred / 0x22;
            uint32_t pFlags = SwapWordAt(prec + 0);
            uint32_t pExtra = SwapWordAt(prec + 4);
            uint32_t pX = SwapHalfAt(prec + 8);
            uint32_t pZ = SwapHalfAt(prec + 10);
            uint8_t pLevel = prec[12];
            float loft = SwapFloatAt(prec + 13);
            float wobbleX = SwapFloatAt(prec + 17);
            float wobbleZ = SwapFloatAt(prec + 21);
            float rotate = SwapFloatAt(prec + 25);
            float wobbleAlt = SwapFloatAt(prec + 29);
            uint8_t pBraces = prec[33];
            if (pRead != 1) {
                SamsUtilities::UDebug("read failed");
                return 0;
            }
            if (pFlags & 8) SetPlaceLoop();

            tag_sPylon* pylon = nullptr;
            PlacePylon(pX, pZ, 1, &pylon, &brace, pLevel, pExtra);
            if (pylon == nullptr) continue;

            SetLoftPylon(loft, brace->mNumber, 0, 1);
            SetRotatePylon(rotate, brace->mNumber, 0, 1);
            if (pFlags & 0x10)
                SetWobblePylon(wobbleAlt, 0.0f, brace->mNumber, 0, 1);
            else
                SetWobblePylon(wobbleX, wobbleZ, brace->mNumber, 0, 1);
            if (pFlags & 1) brace->mFlags |= 1;
            if (pFlags & 2) brace->mFlags |= 2;
            if (pFlags & 4) brace->mFlags |= 4;
            if (pFlags & 0x100) brace->mFlags |= 0x40000;
            if (pFlags & 0x200) brace->mFlags |= 0x80000;
            if (pFlags & 0x40) {
                brace->mFlags |= 0x10000;
                previewBrace = brace;
            }
            if (pFlags & 0x80) {
                brace->mFlags |= 0x20000;
                flagBrace = brace;
            }
            if (pFlags & 0x400)
                brace->mFlags |= 0x200000;
            else
                brace->mFlags &= 0xffdfffffu;
            if (pFlags & 0x800) brace->mFlags |= 0x400000;
            brace->mFB0 = pBraces;
            if (!ReadBraceList(file, brace)) return 0;
        }

        gPylonStackSkip = 0;
        for (sCoasterPylon* pylon = instance->mPylons; pylon != nullptr; pylon = pylon->mNext) {
            if (pylon->mFlags & 1) continue;
            if (gPylonStackSkip == 0) {
                for (uint32_t k = 0; k < pylon->mCount; k++) {
                    tag_sTrackBrace* b = pylon->mBraces[k];
                    uint32_t n = pylon->mCount;
                    if (n > 1 && k < n - 1)
                        b->mFlags |= 0x100;
                    else
                        b->mFlags &= 0xfffffeffu;
                    uint8_t* raw = reinterpret_cast<uint8_t*>(b->mF70);
                    const uint8_t* table = reinterpret_cast<const uint8_t*>(b->mData->mF18Table);
                    uint32_t bflags = b->mFlags;
                    if ((bflags & 0x2000) == 0 && (bflags & 0x100) != 0 && (*reinterpret_cast<const uint32_t*>(table) & 8) != 0) {
                        b->mF90 = reinterpret_cast<sBraceSpline*>(raw + (0x84 - 0x70));
                        b->mF74 = reinterpret_cast<const uint32_t*>(table + 0x44);
                    } else {
                        b->mF90 = reinterpret_cast<sBraceSpline*>(raw + (0x78 - 0x70));
                        b->mF74 = reinterpret_cast<const uint32_t*>(table + 0x30);
                    }
                }
            }
            RecalcTrackParams(instance);
            instance->mFlags |= 1;
            instance->mFlags |= 0x1000;
            UpdatePylonStackMeshes(pylon->mControl, reinterpret_cast<tag_sPylon*>(pylon), 0, 1);
        }

        if (recFlags & 1) {
            gCoasterEditInstance->mBlock3C |= 1;
            PlaceEndPylons(gCoasterEditInstance, EntranceHeight_2(gCoasterEditInstance->mEntranceID));
            tag_sCoasterInstance* edit = gCoasterEditInstance;
            RecalcTrackParams(edit);
            edit->mFlags |= 1;
            edit->mFlags |= 0x1000;
            if (gActionRecorder.mRecording == 0) SetCoasterEditState(0x10);
            SetCoasterState(gCoasterEditInstance, 2);
        }

        for (tag_sTrackBrace* b = instance->mLastBrace; b != nullptr; b = b->mPrev) {
            for (uint32_t k = 0; k < b->mFB0; k++) {
                const uint32_t number = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(b->mCollisions[k]));
                b->mCollisions[k] = GetTrackBrace(reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C), number);
            }
        }

        if (recFlags & 2) {
            instance->mBlock3C |= 2;
            instance->mPreviewBrace = previewBrace;
            instance->mBrace64 = flagBrace;
            if (previewBrace->mNumber == 2) previewBrace->mFlags |= 0x50000;
        }
        if (recFlags & 4) instance->mFlags |= 0x20;
        if (recFlags & 8) instance->mFlags |= 0x40;
        if (recFlags & 0x10) instance->mFlags |= 0x80;
        if (recFlags & 0x20) instance->mFlags |= 0x100;
        if (recFlags & 0x40) instance->mFlags |= 0x200;
        if (recFlags & 0x80) instance->mFlags |= 0x400;
        if (recFlags & 0x100) instance->mFlags |= 0x800;

        instance->mState = recState;
        instance->mF2C = recFloat;
        instance->mTrainCount = static_cast<int32_t>(recTrains);
        std::memcpy(instance->mFF0, &recWord, 4);
        std::memcpy(instance->mF140, &recLast, 4);

        LoadTrainsAndCars(file, instance);
        SetTrainCapacity(instance, 0, reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF2F4, 0);
        ClearCoasterButtons();
        if ((instance->mBlock3C & 2) != 0 || (instance->mBlock3C & 1) == 0) FlashEndPylons(instance, 1);
        gCoasterEditInstance = nullptr;
    }
    return 1;
}

// 0x100458ac (12 bytes).
int32_t ui_GetNumCoasterTracks() { return gCoasterInstanceCount; }

// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).

// ---- the coasters
// 0x100458e8: the n-th instance's first track control (no end test on the list); `control` gets the instance's object.
tag_sBezierControl* GetNthCoasterTrackStart(uint32_t index, void** control) {
    tag_sCoasterInstance* instance = gCoasterInstances;
    for (uint32_t i = 0; i < index; i++) instance = instance->mNext;
    if (control != nullptr) *control = instance->mCObject;
    return instance->mLastBrace->mF90->mControlsA;
}

// CCoasterBalance: main app balance descriptions at 0x1028ec14.

const BalanceDescription gCoasterBalanceDescriptions[384] = {
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "pcTextureFilename", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "bIsSelfIlluminating", 0, 2, 0.0f, 0.0f, 0, 0u},
    {7, "fScrollRate", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asTextureData", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints1", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints2", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints3", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints4", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints5", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints6", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints7", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints8", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints9", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints10", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints11", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints12", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints13", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints14", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints15", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints16", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints17", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints18", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints19", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fU", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNx", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNy", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionPoints20", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges1", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges2", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges3", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges4", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges5", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges6", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges7", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges8", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges9", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges10", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges11", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges12", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges13", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges14", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges15", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges16", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges17", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges18", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges19", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usTexture", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint1", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPoint2", 0, 20, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionEdges20", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usCSPointBlockIndex", 0, 21, 0.0f, 0.0f, 0, 0u},
    {6, "usCSEdgeBlockIndex", 0, 21, 0.0f, 0.0f, 0, 0u},
    {6, "usTrackEdge", 0, 20, 0.0f, 0.0f, 0, 0u},
    {6, "usBlendToCSPointBlockIndex", 0, 21, 0.0f, 0.0f, 0, 0u},
    {7, "fBlendLength", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSections", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fSpeedRangeMin", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fSpeedRangeMax", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fDirectionRangeMin", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fDirectionRangeMax", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fPitchRangeMin", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fPitchRangeMax", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankRangeMin", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankRangeMax", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "usCrossSectionIndex", 0, 21, 0.0f, 0.0f, 0, 0u},
    {6, "usBlendFromCrossSectionIndex", 0, 21, 0.0f, 0.0f, 0, 0u},
    {6, "usBlendToCrossSectionIndex", 0, 21, 0.0f, 0.0f, 0, 0u},
    {3, "asCrossSectionSelects", 0, 0, 0.0f, 0.0f, 20, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "pcMeshFilename", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "uiPylonTypeUniqueID", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "bDontDisableBaseOnStack", 0, 2, 0.0f, 0.0f, 0, 0u},
    {8, "fCollisionPossibleRadius", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fPylonStraightDotThreshold", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "uiPylonWidthCells", 1, 9, 0.0f, 0.0f, 0, 0u},
    {6, "uiPylonLengthCells", 1, 9, 0.0f, 0.0f, 0, 0u},
    {6, "uiMaxStackedPylons", 0, 11, 0.0f, 0.0f, 0, 0u},
    {7, "fMinHeight", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxHeight", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxHeightDiffPerGSquareDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fEntranceMinXZAngleAtMinDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fEntranceMaxXZAngleAtMinDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fEntranceMinXZAngleAtMaxDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fEntranceMaxXZAngleAtMaxDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMinXZAngleAtMinDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxXZAngleAtMinDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMinXZAngleAtMaxDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxXZAngleAtMaxDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxRotnAngleDeviance", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxLoopRotnAngleDeviance", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "uiMinPlaceDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "uiMaxPlaceDist", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxLoopHeightDifference", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fCostPerUnit", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fCostPer2DLength", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fCostPerHeight", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoftAnimStartCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoftAnimEndCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoftAnimMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoftAnimAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fRotateAnimStartCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fRotateAnimEndCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fRotateAnimMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fRotateAnimAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInclineAnimStartCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInclineAnimEndCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInclineAnimMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInclineAnimAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankAnimStartCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankAnimEndCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankAnimMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankAnimAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoopRotateAnimStartCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoopRotateAnimEndCap", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoopRotateAnimMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fLoopRotateAnimAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asPylonControls", 0, 0, 0.0f, 0.0f, 2, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "bBendCar", 0, 2, 0.0f, 0.0f, 0, 0u},
    {5, "uiCarTypeUniqueID", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "pcMeshFilename", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fFrontDistanceAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fRearDistanceAdd", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fYOffset", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fAnimSpeedMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asCarTypes", 0, 0, 0.0f, 0.0f, 3, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "uiMaxCars", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "uiFrontCarTypeIndex", 0, 3, 0.0f, 0.0f, 0, 0u},
    {6, "uiCentreCarTypeIndex", 0, 3, 0.0f, 0.0f, 0, 0u},
    {6, "uiRearCarTypeIndex", 0, 3, 0.0f, 0.0f, 0, 0u},
    {7, "fFrontCarSpacing", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fStdCarSpacing", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fEndCarSpacing", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "sTrainType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fCollisionX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fCollisionY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "asTrackCollisionPoints", 0, 0, 0.0f, 0.0f, 6, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {8, "fCurviness", 0, 0, 0.0f, 0.0f, 0, 0u},
    {8, "fStraightDivisor", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fAutoBankDivisor", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fAccelerationPerHeight", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fUphillAccelModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fDownhillAccelModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInitialPylonLoftHeight", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "uiMaxTrains", 0, 51, 0.0f, 0.0f, 0, 0u},
    {5, "uiTrainReleaseRate", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "bSpecialIsTrough", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "bIsSuspendedCoaster", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "bNoAutoBanking", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "bVisibleEndPylon", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "uiInvalidCrossSection", 0, 21, 0.0f, 0.0f, 0, 0u},
    {6, "uiWinchCrossSection", 0, 21, 0.0f, 0.0f, 0, 0u},
    {7, "fTrackHeightAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fWinchSpeed", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMinSpeed", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxSpeedAtMinSetting", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fMaxSpeedAtMaxSetting", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fFrictionMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBendElevationLow", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBendElevationHigh", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fGravityX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fGravityY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fGravityZ", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fForceMultiplier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInitialSickness", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInitialExcitement", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInitialUncomfortableness", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fGForceSicknessModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fUpsideDownSicknessModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fSpeedExcitementModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fGForceExcitementModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fUpsideDownExcitementModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fBankExcitementModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fXGForceUncomfortableModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fInvertedUncomfortableModifier", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNormalScreamStartExciteDelta", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fTerrorScreamStartExciteDelta", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fNormalScreamSustainExcitement", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fTerrorScreamSustainExcitement", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "fEntranceSizeInterval", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "iTrackSectionMeshVerts", 0, 1001, 0.0f, 0.0f, 0, 0u},
    {6, "iTrackSectionMeshFaces", 0, 1001, 0.0f, 0.0f, 0, 0u},
    {1, "sCoasterType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CCoasterBalance::GetBalanceDescription(int32_t row) {
    return &gCoasterBalanceDescriptions[row];
}
BalanceValue* CCoasterBalance::GetBalanceValuesStart() { return mValues; }

uint32_t gTPCoasterWord7314;   // 0x102b7314
uint32_t gTPCoasterWord7318;   // 0x102b7318

// 0x100459f4
void __sinit_TPCoaster_cpp() {
    gTPCoasterWord7318 = 0;
    gTPCoasterWord7314 = 0x10000;
}
