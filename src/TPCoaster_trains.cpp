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
#include <cstdint>
#include <cstring>

#include "LbFile.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "platform/samsutils.h"

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define TRAINS_SWAP_FILE_WORDS 1
#endif

namespace {

uint32_t Swap32(uint32_t v) {
#ifdef TRAINS_SWAP_FILE_WORDS
    return __builtin_bswap32(v);
#else
    return v;
#endif
}
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
// 0x10021bb4 (12 bytes): the bits of the float.
void USwapInplaceFloat(float& value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    bits = Swap32(bits);
    std::memcpy(&value, &bits, sizeof bits);
}

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
