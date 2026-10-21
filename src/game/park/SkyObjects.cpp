// SkyObjects.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of SkyObjects code, named after it.

#include "CBalanceTable.h"
#include "Clock.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "InterfaceFontWrapper.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Main.h"
#include "MainBalance.h"
#include "MsgCheatMode.h"
#include "MslSubscriptionTree.h"
#include "OptionsBalance.h"
#include "platform/samsutils.h"
#include "RideDB.h"
#include "SaveLegalText.inc"
#include "SaveLoad.h"
#include "Serialise.h"
#include "SkyObjects.h"
#include "tb_sky.h"
#include "TbDynamicStringTemplate.h"
#include "TbResource.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPOptions.h"
#include "v51029_decls.h"
#include "World.h"
#include <cstdint>
#include <cstring>
#include <ios>
#include <new>

// CSkyObjects::CSkyObjects (main app, SimTheme Park 2.04b3, 0x1015b5f0, 4 bytes; unit w117): a bare blr. It stores nothing: the object stays as
// operator new left it (CGame::InitLevel_Load allocates 0x10 bytes for it and then InitSkyObjects fills the pointers).

CSkyObjects::CSkyObjects() {}

// Unit v51118: small stand-ins of the main app (CTimeSig's three members are inline in include/TimeSig.h and are tested through it;
// CSkyObjects, CSaveLoad, InterfaceFontWrapper, the ride database and balance destructors, two messages, and the std containers'
// destructors, constructors and allocator functions of the master save).

namespace v51118 {

// Runtime glue the destructors call; weak defaults here, recording fakes in the test library.
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }   // 0x10238218
// 0x1015b61c: nothing to destroy; the hidden flag decides the delete. Answers self.
CSkyObjects* SkyObjectsDtor(CSkyObjects* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

}  // namespace v51118

// 0x1015b61c (the delete is the hidden flag's: SkyObjectsDtor)
CSkyObjects::~CSkyObjects() {}

// Unit v51029: seven small functions of the main app.
//   CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010,
//   CSubscription::Save 0x10152b08, __sinit_keyboard_cpp 0x101512fc,
//   InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38.

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

// CSkyObjects::DestroySkyObjects (main app, SimTheme Park 2.04b3, Text.cpp's range in the inventory; unit w81).

// 0x1015b804 (116 bytes). One RemoveAdd(void*) on the engine's sky (gei_Sky, an import) for each of the first NumSkyObjects pointers.
// The count (gMainBalance value slot 0x6e4, a signed compare) and the pointer are read again on every iteration, as on the Mac.
void CSkyObjects::DestroySkyObjects() {
    for (int32_t i = 0; i < gMainBalance.mValues[kMainBalanceNumSkyObjects].mInteger; ++i) {
        gei_Sky.RemoveAdd(mObjects[i]);
    }
}

// SaveLoad.cpp (main app 0x10159e50 and 0x1015b484) and the header's Serialise (0x1015b8b0, the inventory files it under Text.cpp): the
// park file's header (unit w114). LoadHeader reads what follows the version word, the header's Serialise reads or writes its fields, and
// the static initialisation of SaveLoad.cpp builds the table of the seven legal texts LoadHeader checks.

// ---- the static initialisation of SaveLoad.cpp (0x1015b484, 324 bytes): the object's words are set (mF00, mError, mF14 = 0x10000 and
// mF18 = 0, in the member initialisers of CSaveLoad), then the seven texts are constructed one after the other and their destructors
// registered for the end of the program (the host's static destructors).
CSaveLoad gSaveLoad;   // 0x1037fefc, original name unknown

namespace {

// One run of equal values of the header: `count` values of a type, one name for all of them (the original names them "xxx[i]"). The
// first call that fails ends the run. The loaders and the savers are the same function with a different callee.
template<class Value> int32_t SerialiseRun(bool load, void* file, Value* values, int32_t count, const char* name) {
    for (int32_t i = 0; i < count; ++i) {
        const int32_t failed = load ? LOADVAR<Value>(file, values + i, name, 1) : SAVEVAR<Value>(file, values + i, name, 1);
        if (failed != 0) return 1;
    }
    return 0;
}

}  // namespace

// Reads (save == 0) or writes the header's 282 values in the order of the members, each with noLog = 1, and answers 1 at the first one
// that fails. A version outside 0 and 1 (signed compares, both directions) does nothing and answers 0; `unused` is ignored.
int32_t CSaveLoadHeader::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version >= 2 || version < 0) return 0;
    const bool load = save == 0;
    if (SerialiseRun(load, file, mCityName, 16, "strCityName[i]") != 0) return 1;
    if (SerialiseRun(load, file, mCityDescription, 128, "strCityDescription[i]") != 0) return 1;
    if (SerialiseRun(load, file, &mCityX, 1, "fCityX") != 0) return 1;
    if (SerialiseRun(load, file, &mCityY, 1, "fCityY") != 0) return 1;
    if (SerialiseRun(load, file, &mCityZ, 1, "fCityZ") != 0) return 1;
    if (SerialiseRun(load, file, mParkName, 16, "strParkName[i]") != 0) return 1;
    if (SerialiseRun(load, file, mAuthorPlayerName, 16, "strAuthorPlayerName[i]") != 0) return 1;
    if (SerialiseRun(load, file, mAuthorEmail, 50, "strAuthorEmail[i]") != 0) return 1;
    if (SerialiseRun(load, file, mParkDescription, 50, "strParkDescription[i]") != 0) return 1;
    if (SerialiseRun(load, file, &mParkDatePublishedDay, 1, "wParkDatePublished_d") != 0) return 1;
    if (SerialiseRun(load, file, &mParkDatePublishedMonth, 1, "wParkDatePublished_m") != 0) return 1;
    if (SerialiseRun(load, file, &mParkDatePublishedYear, 1, "wParkDatePublished_y") != 0) return 1;
    return 0;
}
