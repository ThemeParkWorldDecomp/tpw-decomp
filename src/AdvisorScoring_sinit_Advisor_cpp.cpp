// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"

#include <new>

#include "AdvisorQueue.h"
#include "BMP_InsideArea.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Serialise.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "keytable_actions.h"
#include "rsse.h"
#include "ui_main.h"

namespace {

// A Ptr32 field of a structure read by Mac offset.
template <class T>
T* PtrAt(const void* base, size_t offset) {
    return reinterpret_cast<const Ptr32<T>*>(static_cast<const uint8_t*>(base) + offset)->get();
}

}  // namespace

// ---- Advisor.cpp
const char16_t gAdvisorMsgText[] = u"gm";   // 0x1023c316 (only the first two characters are known)
unsigned char gAdvisorMsgStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x102b3aa8
unsigned char gAdvisorMsgStringNode[8];                                              // 0x102b3abc

// 0x1000fff0: the global wide string is made and registered with the wide string destructor glue.
void __sinit_Advisor_cpp() {
    auto* text = new (gAdvisorMsgStringStorage) TbDynamicStringTemplate<char16_t>(gAdvisorMsgText);
    __register_global_object(text, reinterpret_cast<void*>(&UIMain_DestroyWString), gAdvisorMsgStringNode);
}

// SAVEVAR<AdvisorMessage> 0x1000f820 and LOADVAR<AdvisorMessage> 0x1000f250 are lead unit v5L05's
// (src/Advisor_AdvisorMessage_LOADVAR_SAVEVAR.cpp, test_AdvisorMessageVar).

// ---- the bump rides
// 0x10025960: the index of the ride's buoy at (x, y), 0 when there is none.
int32_t FindBuoy(tagBUMPRIDE* ride, int32_t x, int32_t y) {
    for (tabBUOY* buoy = PtrAt<tabBUOY>(ride, 0xa8); buoy != nullptr; buoy = buoy->next.get()) {
        if (buoy->x == x && buoy->y == y) return buoy->index;
    }
    return 0;
}

// 0x10027424: the ride's collision objects are freed (flags cleared, the list's count lowered).
void FreeColObjectsForRide(tagBUMPRIDE* ride) {
    tabCOLOBJECT* object = PtrAt<tabCOLOBJECT>(ride, 0xc0);
    if (object == nullptr) return;
    do {
        gColList.count = gColList.count - 1;
        object->flags = 0;
        object = PtrAt<tabCOLOBJECT>(object, 0x14);
    } while (object != nullptr);
}

// 0x1002d340: the first buoy of the ride with flag 4 (no end test: the original would run off the list).
tabBUOY* GetFinishLine(tagBUMPRIDE* ride) {
    tabBUOY* buoy = PtrAt<tabBUOY>(ride, 0xa8);
    while ((buoy->flags & 4) == 0) buoy = buoy->next.get();
    return buoy;
}

// 0x1002d390: the ride's word 0xa4 becomes the first buoy after the finish line (wrapping to the first) without flag 2.
void SetBuoySequence(tagBUMPRIDE* ride) {
    tabBUOY* first = PtrAt<tabBUOY>(ride, 0xa8);
    tabBUOY* buoy = first;
    while ((buoy->flags & 4) == 0) buoy = buoy->next.get();
    do {
        buoy = buoy->next.get();
        if (buoy == nullptr) buoy = first;
    } while ((buoy->flags & 2) != 0);
    reinterpret_cast<Ptr32<tabBUOY>*>(reinterpret_cast<uint8_t*>(ride) + 0xa4)->set(buoy);
}

// 0x1002ffc4: LbFile_Read.
TbError BMPFile_Read(void* file, void* buffer, uint32_t size, uint32_t* transferred) {
    return LbFile_Read(file, buffer, size, transferred);
}

// ---- the lobby leaves
// 0x1002addc: the matrix of the data node `index` of the mesh (entries of 0x14 bytes on the Mac).
float* GetDataNodeWorldMatrix(tag_sMesh*& mesh, uint32_t& index) {
    tagDataNodeTable* table = static_cast<tagDataNodeTable*>(mesh->mF28);
    return table->mEntries[index].mMatrix;
}

// 0x1002ae38: the slot of the ride in gRideAnimSets.
tag_sRideAnimSet** GetRideFromID(uint32_t& id) {
    return gRideAnimSets + id;
}

// 0x10032300: the array destructor of gAdvisorStore.
void __arraydtor_3112() {
    __destroy_arr(gAdvisorStore, gAdvisorStoreDtorVector, 0xbc, 4);
}
unsigned char gAdvisorStoreDtorVector[16];   // stand-in for 0x10266010

// ---- the camera
// 0x10032b64: the ridden ride's instance while the first person view rides one, else 0.
uint32_t GetFirstPersonRideID() {
    if ((gFirstPersonFlags & 4) != 0 && gFirstPersonRideSet != nullptr) return gFirstPersonRideInstance;
    return 0;
}

// 0x10032bc4
int32_t ResetCameraPosition() {
    gCameraModeA |= 0x100;
    return 1;
}

// ---- the coasters
// 0x100458e8: the n-th instance's first track control (no end test on the list); `control` gets the instance's object.
tag_sBezierControl* GetNthCoasterTrackStart(uint32_t index, void** control) {
    tag_sCoasterInstance* instance = gCoasterInstances;
    for (uint32_t i = 0; i < index; i++) instance = instance->mNext;
    if (control != nullptr) *control = instance->mCObject;
    return instance->mLastBrace->mF90->mControlsA;
}

uint32_t gTPCoasterWord7314;   // 0x102b7314
uint32_t gTPCoasterWord7318;   // 0x102b7318

// 0x100459f4
void __sinit_TPCoaster_cpp() {
    gTPCoasterWord7318 = 0;
    gTPCoasterWord7314 = 0x10000;
}

// 0x10049120: the object number of the instance with that script id, 0 when none.
int32_t GetCoasterInstanceID(int32_t handle) {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if (instance->mScriptID == handle) return static_cast<int32_t>(instance->mObjectInstanceID);
    }
    return 0;
}

// 0x10049228 (the same machine code as CLBFlyingMeshSystem::Register).
void DestroyQueue(sWraparoundQueue* queue) {
    MReleaseFreeF(queue->mBlock);
}

// 0x1004b050: position >= the track's length (false for a NaN).
int32_t IsEndOfSampleBuffer(tag_sCoasterInstance* instance, float position) {
    return position < instance->mLength ? 0 : 1;   // the original's compare falls through to 1 for a NaN
}

// 0x1004d2ec (the same machine code as ThemeParkWorld::HandleUpdateWindow): returns at once.
void DoImmediatePhysicsFeedback(tag_sCoasterInstance*) {}

// 0x1004d920
void GetCoasterPeepStats(uint32_t id, float* ratingA, float* ratingB) {
    tag_sCoasterInstance* instance = static_cast<tag_sCoasterInstance*>(gObjectTable[id]);
    *ratingA = instance->mRatingA;
    *ratingB = instance->mRatingB;
}
