// RideDB.cpp, main app: RideInfo's text, custom effects and destruction.
#include "RideDB.h"
#include "Debug.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "TPString.h"
#include "bfl_FileSystem.h"
#include "rsse.h"
#include "platform/samsutils.h"
#include <cstdlib>
#include <cstring>

// The original text parser helpers (untranslated).
int32_t find_string_id(const char16_t* text); // 0x10155604
const char16_t* find_line(const char16_t* text); // 0x101556a0
void copy_line(const char16_t** destination, const char16_t* text); // 0x10155170

// 0x101559f8, 452 bytes. Swap remains a platform call, as in the other file readers.
void RideInfo::LoadStrings(bfl::FileSystem::CFile& file) {
    for (int32_t i = 0; i < 4; ++i) mStrings[i] = gRideMissingName;
    const int32_t size = file.GetSize();
    if (size % 2 != 0) {
        GAME_ASSERTMSG(0, "Object text file's size is not divis. by 2 - so I don't believe you that it's UNICODE!");
        return;
    }
    auto* buffer = static_cast<char16_t*>(LbMem_Alloc(static_cast<uint32_t>(size) + 2));
    const int32_t read = file.Read(buffer, static_cast<uint32_t>(size));
    if (buffer == nullptr || read == 0) return;
    const int32_t count = size / 2;
    buffer[count] = 0;
    const char16_t* cursor = buffer;
    bool swap = true;
    if (buffer[0] == 0xfffe || buffer[0] == 0xfeff) {
        cursor += 1;
        swap = buffer[0] != 0xfeff;
    }
    if (swap) SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(buffer), count);
    while (*cursor != 0) {
        const int32_t id = find_string_id(cursor);
        if (id == -1) {
            LbMem_Free(buffer);
            return;
        }
        cursor = find_line(cursor);
        copy_line(&mStrings[id], cursor);
        CTRACE(1, "Successfully copied field: %d", id);
        cursor = find_line(cursor);
    }
    LbMem_Free(buffer);
}

// 0x10156b28, 80 bytes.
void RideInfo::LoadSounds(bfl::FileSystem::CFileStorage& storage) {
    mSoundCategory = SOUND_RegisterCatagory(storage.GetRootPath(), "cat_ride");
}

// 0x10156bc4, 412 bytes. The final character selects a word without a range check.
void RideInfo::LoadParticles(bfl::FileSystem::CFileStorage& storage) {
    bfl::FileSystem::CFileInfo info;
    if (!storage.FindFirst(gTPGlobals.mF45B0.mData, info).Succeeded()) return;
    bfl::FileSystem::CFile file;
    TbDynamicStringTemplate<char> name;
    do {
        if (storage.OpenFile(file, info.mName, 0x80000001, nullptr).Succeeded()) {
            name = info.mName;
            name = RemoveExtension(gTPGlobals.mF43A8, name);
            name = name.Right(1);
            char digit[4];
            std::strcpy(digit, name.mData);
            const int32_t index = static_cast<int32_t>(std::atol(digit));
            const uint32_t emitter = static_cast<uint32_t>(PAR_LoadLibEmitter(&file));
            // Digits 6 to 9 overwrite the four string pointers on the Mac. Native
            // pointers need a whole pointer assignment rather than a four-byte stride.
            if (index < 6) mEmitterWords[index] = emitter;
            else mStrings[index - 6] = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(emitter));
        }
        file.Close();
    } while (storage.FindNext(info).Succeeded());
}

void RideInfo::SetIsCustomRide() { mFlags |= 1; } // 0x10156f40, 16 bytes

// 0x101550bc, 132 bytes. The placeholder is retained and owned blocks are cleared after Free.
void RideInfo::DeallocStrings() {
    for (int32_t i = 0; i < 4; ++i) {
        if (mStrings[i] != nullptr && mStrings[i] != gRideMissingName) {
            LbMem_Free(const_cast<char16_t*>(mStrings[i]));
            mStrings[i] = nullptr;
        }
    }
}

// 0x1015457c, 192 bytes. The caller's delete supplies the Mac's positive destruction flag.
RideInfo::~RideInfo() {
    for (int32_t i = 0; i < 4; ++i) {
        if (mEmitters[i] != 0) PAR_KillLibEmitter(mEmitters[i]);
    }
    if (mSoundCategory != 0) SOUND_FreeCatagory(mSoundCategory);
    mBalance.UnInit();
}
