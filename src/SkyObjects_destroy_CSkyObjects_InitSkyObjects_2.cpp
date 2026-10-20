// Unit v51029: seven small functions of the main app.
//   CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010,
//   CSubscription::Save 0x10152b08, __sinit_keyboard_cpp 0x101512fc,
//   InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38.
#include "SkyObjects.h"
#include "MainBalance.h"
#include "tb_sky.h"
#include "TPGlobals.h"
#include "Main.h"
#include "FS_System.h"
#include "Text.h"
#include "TPEngine.h"
#include "MslSubscriptionTree.h"
#include "InterfaceFontWrapper.h"
#include "TbDynamicStringTemplate.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Debug.h"
#include "v51029_decls.h"

#include <cstring>
#include <ios>
#include <new>

// ---------------------------------------------------------------------------------------------------------------- sky objects

// 0x1015b688 (328 bytes). For each of the first NumSkyObjects entries of the main balance table (six value slots each from slot
// 440: position x y z, size, a flag word, the texture file name) the path is the theme path, a separator, the "Sky" folder, a
// separator and the file name; CSky::LoadAdd(&texture, path, position, size, 0, flag ? 1 : 2) answers nonzero when the sprite was
// added, and the texture is then kept in mObjects[i]. The count is read again every iteration; `texture` is shared by all of them.
void CSkyObjects::InitSkyObjects() {
    TbDynamicStringTemplate<char> path;
    void* texture = nullptr;
    for (int32_t i = 0; i < gMainBalance.mValues[kMainBalanceNumSkyObjects].mInteger; ++i) {
        const int32_t slot = kMainBalanceNumSkyObjects + 1 + i * 6;
        D3DVECTOR position = {gMainBalance.mValues[slot].mFloat, gMainBalance.mValues[slot + 1].mFloat,
                              gMainBalance.mValues[slot + 2].mFloat};
        float size = gMainBalance.mValues[slot + 3].mFloat;
        int32_t kind = gMainBalance.mValues[slot + 4].mInteger == 0 ? 2 : 1;
        path = get_theme_path();
        FS_AddDirectorySeparator(path);
        path += gTPGlobals.mF4BA0;
        FS_AddDirectorySeparator(path);
        path += gMainBalance.mValues[slot + 5].mString;
        if (gei_Sky.LoadAdd(&texture, path.mData, position, size, 0, kind) != 0) mObjects[i] = texture;
    }
}

// ---------------------------------------------------------------------------------------------------------------- CText::~CText

__attribute__((weak)) void CText_OperatorDelete(void* object) { ::operator delete(object); }

// The TVector 0x102673c8 hands __destroy_arr this: one InterfaceFontWrapper's destructor.
void FontWrapperVectorDestroy(void* object) { static_cast<InterfaceFontWrapper*>(object)->~InterfaceFontWrapper(); }

// 0x1015ee20 (136 bytes). A NULL object does nothing. Else: the render parameters (+0x4fc), the 52 font wrappers (+0x1c, stride
// 0x18 on the Mac, 0x34 of them: the original's count), the text resource manager (+0xc), and the storage is given back when the
// short flag is above zero. The fonts' size argument is the Mac's 0x18 (the host wrappers are larger).
CText* CText_Destroy(CText* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mRenderParams.~TbFontRenderParams();
        __destroy_arr(self->mFonts, reinterpret_cast<void*>(&FontWrapperVectorDestroy), 0x18, 0x34);
        self->mResources.~TbTextResourceManager();
        if (deleteFlag > 0) CText_OperatorDelete(self);
    }
    return self;
}

// ---------------------------------------------------------------------------------------------------------------- RideDB.cpp initialiser

uint8_t gRideDBIosInit[16];
uint8_t gRideDBIosNode[32];
uint8_t gRideDBNode[32];

__attribute__((weak)) void IosBaseInitConstruct(void* object) { new (object) std::ios_base::Init(); }
__attribute__((weak)) void IosBaseInitDestroy(void* object) { static_cast<std::ios_base::Init*>(object)->~Init(); }
void RideDatabaseDestroy(void* object) { static_cast<CRideDatabase*>(object)->~CRideDatabase(); }

// 0x10157010 (140 bytes). The iostream initialiser object is built and registered; then the ride database global: the count is
// cleared, the ride table (0x528 bytes on the Mac: the 0x14a ride pointers) is zeroed, the cached id and ride cleared, and the
// database is registered for destruction. The other fields (the name builder) are left alone.
void __sinit_RideDB_cpp() {
    IosBaseInitConstruct(gRideDBIosInit);
    __register_global_object(gRideDBIosInit, reinterpret_cast<void*>(&IosBaseInitDestroy), gRideDBIosNode);
    gRideDatabase.mRideCount = 0;
    std::memset(&gRideDatabase.mRides, 0, sizeof(gRideDatabase.mRides));
    gRideDatabase.mCachedId = 0;
    gRideDatabase.mCachedRide = nullptr;
    __register_global_object(&gRideDatabase, reinterpret_cast<void*>(&RideDatabaseDestroy), gRideDBNode);
}

// ---------------------------------------------------------------------------------------------------------------- CSubscription::Save

// 0x10152b08 (124 bytes). The id is written byte-swapped (2 bytes) to the file the reference holds. A transfer count other than 2
// shows the "Warning! Something went hideously wrong ..." message and answers -1; else 0. The count is not set before the call.
int32_t CSubscription::SaveResult(void*& file) const {
    // The Mac swaps the big-endian id before the write, so the file holds it little-endian: on the host that is the value as it is.
    uint16_t swapped = mThingId;
    uint32_t written = 0;
    LbFile_Write(file, &swapped, 2, &written);
    if (written == 2) return 0;
    DEBUG_MESSAGE("Warning! Something went hideously wrong while saving the message subscriptions!", 0);
    return -1;
}

// The game calls the void form (CSubscriptionHeader::Save ignores the answer), as CSubscription::Load is defined over LoadResult.
// Without it the launcher's stand-in wrote nothing, so a save kept each header's count but none of its ids and the load read
// the following modules' bytes as subscriptions (v5L15).
void CSubscription::Save(void*& file) const { SaveResult(file); }

// ---------------------------------------------------------------------------------------------------------------- keyboard.cpp initialiser

int32_t gKeyboardF28C;
int32_t gKeyboardF288;
alignas(16) uint8_t gKeyboardWideString[48];
uint8_t gKeyboardWideNode[32];
uint8_t gKeyboardQuickNode[32];
static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof(gKeyboardWideString), "wide string storage");
static_assert(sizeof(TbDynamicStringTemplate<char>) <= sizeof(TbStringBase<char>) + 8, "narrow string storage");

// 0x101512fc (120 bytes). Two words are set (0 and 0x10000); a wide string (the text at 0x10248c7e) and the narrow quick-save
// suffix ("_online", gQuickSaveSuffix) are built and each registered for destruction.
void __sinit_keyboard_cpp() {
    gKeyboardF28C = 0;
    gKeyboardF288 = 0x10000;
    new (gKeyboardWideString) TbDynamicStringTemplate<char16_t>(u"gms.dat");
    __register_global_object(gKeyboardWideString, reinterpret_cast<void*>(&UIMain_DestroyWString), gKeyboardWideNode);
    new (&gQuickSaveSuffix) TbDynamicStringTemplate<char>("_online");
    __register_global_object(&gQuickSaveSuffix, reinterpret_cast<void*>(&UIMain_DestroyString), gKeyboardQuickNode);
}

// ---------------------------------------------------------------------------------------------------------------- InterfaceFontWrapper

// 0x10168c7c (132 bytes). Loops while the index is below last - first (signed 32-bit) and the character is not NUL; each adds the
// virtual width (a short: the call is slot +0x14 of the vptr at +0x10, which is InterfaceFont::GetCharWidth here).
int32_t InterfaceFontWrapper::GetWidth(const char16_t* text, int16_t first, int16_t last) {
    int32_t total = 0;
    int32_t count = static_cast<int32_t>(last) - static_cast<int32_t>(first);
    const char16_t* p = text + first;
    for (int32_t i = 0; i < count && *p != 0; ++i, ++p) total += static_cast<int16_t>(GetCharWidth(*p));
    return static_cast<int16_t>(total);
}

// 0x10168d38 (144 bytes). While the 16-bit remaining room is above zero and the character is not NUL: room -= width (the width is
// the full register), index + 1. Answers the index - 1.
int32_t InterfaceFontWrapper::LastCharIn(const char16_t* text, int16_t first, int16_t last) {
    int32_t index = first;
    int32_t room = last;
    const char16_t* p = text + first;
    while (static_cast<int16_t>(room) > 0 && *p != 0) {
        room = room - GetCharWidth(*p);
        ++index;
        ++p;
    }
    return index - 1;
}
