// Advisor.cpp (main app): the advisor's part of the save file. ADV_Save 0x10009788 (808 bytes) and ADV_Load 0x10009cb8
// (708 bytes). The file image of the state is 0x168 bytes with the Mac's layout (a 32-bit pointer at +0x140); the host's
// SAdvisorState is larger (a native pointer), so the image is packed from and unpacked into the fields. The file's words are
// little-endian: the Mac swaps every field one by one (USwapInplace32 and friends); on this little-endian host those swaps are
// no-ops, so only the calls that go through the platform (_USwapBlock32, _USwapCopy32, _USwapDelete) are kept.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Advisor.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "platform/samsutils.h"

namespace {

constexpr uint32_t kAdvisorFileTag = 0x5f564441;   // "ADV_" (little-endian bytes 41 44 56 5f)
constexpr uint32_t kStateImageSize = 0x168;
constexpr uint32_t kPointerOffset = 0x140;         // the data pointer in the image
constexpr uint32_t kTailSize = 0x24;               // mAccessoriesOn, mAccessoriesOff and mPausedSound

void PackState(uint8_t* image) {
    SAdvisorState& state = gAdvisorState;
    std::memcpy(image, &state, kPointerOffset);
    const uint32_t pointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(state.mSyncData));
    std::memcpy(image + kPointerOffset, &pointer, 4);
    std::memcpy(image + kPointerOffset + 4, &state.mAccessoriesOn, kTailSize);
}

void UnpackState(const uint8_t* image) {
    SAdvisorState& state = gAdvisorState;
    std::memcpy(&state, image, kPointerOffset);
    uint32_t pointer;
    std::memcpy(&pointer, image + kPointerOffset, 4);
    state.mSyncData = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(pointer));
    std::memcpy(&state.mAccessoriesOn, image + kPointerOffset + 4, kTailSize);
}

// A word written through _USwapCopy32 (the original's four-byte writes).
void WriteWord(void* file, uint32_t value) {
    uint32_t transferred;
    uint32_t* copy = SamsUtilities::_USwapCopy32(&value, 1);
    LbFile_Write(file, copy, 4, &transferred);
    SamsUtilities::_USwapDelete(copy);
}

}  // namespace

// 0x10009788
void ADV_Save(void* file) {
    SAdvisorState& state = gAdvisorState;
    WriteWord(file, kAdvisorFileTag);
    WriteWord(file, kStateImageSize);
    uint32_t image[kStateImageSize / 4];
    PackState(reinterpret_cast<uint8_t*>(image));
    LbFile_Write(file, image, kStateImageSize, nullptr);
    if (state.mSyncData == nullptr) {
        WriteWord(file, 0);
    } else {
        uint32_t size;
        std::memcpy(&size, &state.mSyncSize, 4);
        WriteWord(file, size);
        uint32_t* copy = SamsUtilities::_USwapCopy32(reinterpret_cast<const uint32_t*>(state.mSyncData), static_cast<int32_t>(size >> 2));
        LbFile_Write(file, copy, size, nullptr);
        SamsUtilities::_USwapDelete(copy);
    }
}

// 0x10009cb8
void ADV_Load(void* file) {
    SAdvisorState& state = gAdvisorState;
    uint32_t transferred;
    uint32_t tag = 0;
    LbFile_Read(file, &tag, 4, &transferred);
    SamsUtilities::_USwapBlock32(&tag, 1);
    if (tag != kAdvisorFileTag) return;

    if (state.mSyncData != nullptr) {
        if (state.mSyncData == gAdvisorStaticBuffer)
            gAdvisorStaticInUse = 0;
        else
            std::free(state.mSyncData);
        state.mSyncData = nullptr;
    }
    uint32_t length = 0;
    LbFile_Read(file, &length, 4, &transferred);
    SamsUtilities::_USwapBlock32(&length, 1);
    // The Mac reads `length` bytes straight over the state (0x168 in every file the game writes) and swaps its fields.
    std::vector<uint8_t> image(length > kStateImageSize ? length : kStateImageSize, 0);
    PackState(image.data());
    LbFile_Read(file, image.data(), length, nullptr);
    UnpackState(image.data());

    LbFile_Read(file, &state.mSyncSize, 4, &transferred);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&state.mSyncSize), 1);
    if (state.mSyncSize == 0) return;
    if (state.mSyncSize < 1000 && gAdvisorStaticInUse == 0) {
        gAdvisorStaticInUse = 1;
        state.mSyncData = gAdvisorStaticBuffer;
    } else {
        state.mSyncData = static_cast<int32_t*>(std::malloc(static_cast<uint32_t>(state.mSyncSize)));
    }
    LbFile_Read(file, state.mSyncData, static_cast<uint32_t>(state.mSyncSize), nullptr);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(state.mSyncData), static_cast<int32_t>(static_cast<uint32_t>(state.mSyncSize) >> 2));
}
