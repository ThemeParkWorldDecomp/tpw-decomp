// Test library for src/game/online/LBLobbyMeshes.cpp and src/game/Debug.cpp (libtpw_unit_LobbyMeshes.dylib;
// see tests/test_LobbyMeshes.py): the lobby's mesh system, its types and instances, driven
// by scripts over a small world of ride sets the test builds.
//
// Real: CLBLobbyMeshes, CLBMeshType, CLBMeshInstance, their lists, GetUniqueID. Fakes that
// record: the ride and height field code, the draw switches, the matrix helpers, the string
// compare, the lobby system base class. The strings are functional.
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "FS_System.h"
#include "TPEngine.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "ui_ErrorDisplay.h"

#include "fake_strings.h"

using namespace calllog;

namespace {
enum { kSets = 6, kSlots = 64 };   // set 0 is the height field's
tag_sRideAnimSet gSets[kSets];
tag_sMesh gMeshObjects[kSets];
tag_sM3D2Header* gHeaders[kSets];
tag_sNode* gNodes[kSets];
alignas(16) unsigned char gSystemStore[sizeof(CLBLobbyMeshes)];
CLBLobbyMeshes* gSystem;
CLBMeshInstance* gSlots[kSlots];
CLBMeshType* gSpareType;
int32_t gSlotCount;

float FromBits(int32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
int64_t Hash(const char* text) {
    uint32_t h = 2166136261u;
    for (; text != nullptr && *text != 0; ++text) h = (h ^ static_cast<unsigned char>(*text)) * 16777619u;
    return h;
}
// Any NaN as one value (see the test's comment).
int64_t MatrixWord(float value) {
    const uint32_t bits = fbits(value);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0 ? 0x7fc00000u : bits;
}
int32_t Lower(int32_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int64_t Word(HFieldXZ value) { return (static_cast<int64_t>(value.mF00) << 16) | value.mF02; }
int64_t Number(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }
const char16_t* Sign(int32_t value) { return reinterpret_cast<const char16_t*>(static_cast<intptr_t>(value)); }
}  // namespace

// ---- globals and fakes
CLBLobbyMeshes* gLobbyMeshes;
CSYS_TPFileInfo gTPFileInfo;
tag_sRideAnimSet* gRideAnimSets[kSets + 2];
tag_sRideAnimSet* gHeightfieldRideSet;

CLBLobbySystem::CLBLobbySystem() { ev("CLBLobbySystem", {lab(this)}); }
CLBLobbySystem::~CLBLobbySystem() { ev("~CLBLobbySystem", {lab(this)}); }
void CSYS_TPFileInfo::SetLevelPath(const TbDynamicStringTemplate<char>& path) { ev("SetLevelPath", {}, {path.mData}); }
// (With operator new's count so far: the lists are made after the height field is loaded.)
int32_t LoadLevelHeightfield(int32_t a, int32_t b) {
    ev("LoadLevelHeightfield", {a, b, tpw_cl_heap[0]});
    return 1;
}
void DestroyLevelHeightfield() { ev("DestroyLevelHeightfield"); }
void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("EnableMeshDrawRange", {lab(mesh), Word(from), Word(to)}); }
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {lab(mesh)}); }
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("DisableMeshDrawRange", {lab(mesh), Word(from), Word(to)}); }
void ScaleMesh(tag_sMesh* mesh, s3DPoint* scale) { ev("ScaleMesh", {lab(mesh), fbits(scale->x), fbits(scale->y), fbits(scale->z)}); }
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c,
                      const char16_t* sign1, const char16_t* sign2, int32_t d, int32_t e) {
    ev("LoadRideType", {Number(a), Number(b), flags, id, c, Number(sign1), Number(sign2), d, e}, {folder, name});
    return static_cast<uint32_t>(ret("RideType", 7));
}
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d,
                      int32_t e, uint32_t f, int32_t g) {
    ev("InstanceRide", {type, flags, Number(params), a, b, Number(c), d, e, f, g});
    return static_cast<uint32_t>(ret("Instance", 1));
}
// (With the instance list's count: an instance leaves the list before its ride goes.)
void DestroyRideInstance(uint32_t instance) {
    ev("DestroyRideInstance", {instance, gSystem != nullptr && gSystem->mInstances != nullptr ? gSystem->mInstances->mCount : -1});
}
void DestroyAllRideTypes() { ev("DestroyAllRideTypes"); }
void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
}
uint32_t GetCurrentAnimStats(tag_sRideAnimSet* set, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t d) {
    ev("GetCurrentAnimStats", {lab(set), Number(a), Number(b), Number(c), d});
    return static_cast<uint32_t>(ret("AnimTime", 0));
}
void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {lab(matrix)});
    std::memset(matrix, 0, sizeof *matrix);
    for (int i = 0; i < 4; ++i) matrix->m[i][i] = 1.0f;
}
void RotateLMatrixXYZNoTrans(sMatrix* matrix, eRotateParams axis, float* angle) {
    ev("RotateLMatrixXYZNoTrans", {lab(matrix), axis, fbits(*angle)});
}
int32_t RegenerateRideSign(uint32_t instance, char* folder, char* name, const char16_t* sign1, const char16_t* sign2) {
    ev("RegenerateRideSign", {instance, Number(sign1), Number(sign2)}, {folder, name});
    return 1;
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int32_t ca = Lower(static_cast<unsigned char>(*a));
        const int32_t cb = Lower(static_cast<unsigned char>(*b));
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

extern "C" {
// The world: six ride sets (0 the height field's), each with a mesh whose node's matrix
// holds set * 100 + its index. world: per set its flags and the two map words. Labels: the
// meshes 10.., the sets 20.., the matrices 30.., the system's memory 5.
void tpw_lm_begin(const uint32_t* world) {
    for (int i = 0; i < kSets; ++i) {
        if (gHeaders[i] == nullptr) {
            gHeaders[i] = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
            gNodes[i] = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        }
        std::memset(gHeaders[i], 0xa5, sizeof(tag_sM3D2Header));
        std::memset(gNodes[i], 0xa5, sizeof(tag_sNode));
        gHeaders[i]->mF78 = gNodes[i];
        for (int k = 0; k < 16; ++k) gNodes[i]->mMatrix.m[k / 4][k % 4] = static_cast<float>(i * 100 + k);
        std::memset(&gMeshObjects[i], 0xa5, sizeof gMeshObjects[i]);
        gMeshObjects[i].mHeader = gHeaders[i];
        std::memset(&gSets[i], 0xa5, sizeof gSets[i]);
        gSets[i].mFlags = world[i * 3];
        gSets[i].mHeads = &gMeshObjects[i];
        gSets[i].mFC0 = {static_cast<uint16_t>(world[i * 3 + 1] >> 16), static_cast<uint16_t>(world[i * 3 + 1])};
        gSets[i].mFC4 = {static_cast<uint16_t>(world[i * 3 + 2] >> 16), static_cast<uint16_t>(world[i * 3 + 2])};
        gRideAnimSets[i] = i == 0 ? nullptr : &gSets[i];
        tpw_cl_label(10 + i, &gMeshObjects[i]);
        tpw_cl_label(20 + i, &gSets[i]);
        tpw_cl_label(30 + i, &gNodes[i]->mMatrix);
    }
    gHeightfieldRideSet = &gSets[0];
    gLobbyMeshes = nullptr;
    gSystem = nullptr;
    gSpareType = nullptr;
    gSlotCount = 0;
    std::memset(gSlots, 0, sizeof gSlots);
    tpw_cl_label(5, gSystemStore);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}

int64_t tpw_lm_op(int32_t op, const int32_t* a, const char* folder, const char* name) {
    CLBMeshInstance* instance = op >= 20 && op < 40 ? gSlots[a[0]] : nullptr;
    switch (op) {
    case 1:
        std::memset(gSystemStore, 0xa5, sizeof gSystemStore);
        gSystem = new (gSystemStore) CLBLobbyMeshes;
        gLobbyMeshes = gSystem;
        return 0;
    case 2: gSystem->~CLBLobbyMeshes(); return 0;
    case 3: gSystem->ShowHeightfield(); return 0;
    case 4: gSystem->HideHeightfield(); return 0;
    case 5: {
        CLBLobbySystem* system = gSystem;
        system->Init();
        system->Tick();
        system->Render();
        system->Close();
        system->WindowMessagesFn(a[0], a[1], a[2]);
        return 0;
    }
    case 6:
        gSlots[gSlotCount++] = gSystem->InstanceMeshType(folder, name, a[0], static_cast<uint32_t>(a[1]), Sign(a[2]), Sign(a[3]));
        return 0;
    case 7: gSystem->DeleteMeshInstanceListEntries(); return 0;
    case 8: gSystem->DeleteMeshTypes(); return 0;
    case 9: {   // the type at list position a[0] leaves the list and is kept aside
        auto* node = gSystem->mTypes->mHead;
        for (int32_t i = 0; i < a[0] && node != nullptr; ++i) node = node->mNext;
        if (node == nullptr) return -1;
        gSpareType = node->mData;
        gSystem->UnRegister(gSpareType);
        return 0;
    }
    case 10: gSystem->Register(gSpareType); return 0;
    case 11: gSystem->UnRegister(gSpareType); return 0;   // not in the list: nothing happens
    case 20: instance->Show(); return 0;
    case 21: {
        const s3DPoint point = {FromBits(a[1]), FromBits(a[2]), FromBits(a[3])};
        instance->SetPosition(point);
        return 0;
    }
    case 22: {
        const s3DPoint point = {FromBits(a[1]), FromBits(a[2]), FromBits(a[3])};
        instance->SetDirection(point);
        return 0;
    }
    case 23: instance->RotateY(FromBits(a[1])); return 0;
    case 24: instance->Scale(FromBits(a[1]), FromBits(a[2]), FromBits(a[3])); return 0;
    case 25: instance->PlayAnim(a[1], a[2]); return 0;
    case 26: return instance->GetAnimTimeRemaining();
    case 27: instance->SetRideSigns(Sign(a[1]), Sign(a[2])); return 0;
    case 28: delete instance; return 0;
    case 29: gSystem->UnRegister(instance); return 0;
    case 30: gSystem->Register(instance); return 0;
    case 31: instance->mInstance = static_cast<uint32_t>(a[1]); return 0;
    }
    return -99;
}

// alive: one flag per instance slot. Out: the height field flag and the two lists (0 when
// the system is gone), the instances alive, the six matrices, operator new's and delete's
// counts.
int32_t tpw_lm_state(int32_t systemAlive, const int32_t* alive, int64_t* out) {
    int64_t* p = out;
    if (systemAlive != 0) {
        *p++ = gSystem->mHeightfieldShown;
        if (gSystem->mTypes == nullptr) {
            *p++ = -1;
        } else {
            *p++ = gSystem->mTypes->mCount;
            for (const auto* node = gSystem->mTypes->mHead; node != nullptr; node = node->mNext) {
                *p++ = node->mData->mRideType;
                *p++ = Hash(node->mData->mFolder.mData);
                *p++ = Hash(node->mData->mName.mData);
            }
        }
        if (gSystem->mInstances == nullptr) {
            *p++ = -1;
        } else {
            *p++ = gSystem->mInstances->mCount;
            for (const auto* node = gSystem->mInstances->mHead; node != nullptr; node = node->mNext) {
                int32_t slot = -1;
                for (int32_t i = 0; i < gSlotCount; ++i) if (gSlots[i] == node->mData) slot = i;
                *p++ = slot;
            }
        }
    }
    for (int32_t i = 0; i < gSlotCount; ++i) {
        if (alive[i] == 0) continue;
        *p++ = gSlots[i]->mInstance;
        *p++ = gSlots[i]->mShown;
        *p++ = Hash(gSlots[i]->mFolder.mData);
        *p++ = Hash(gSlots[i]->mName.mData);
    }
    for (int i = 0; i < kSets; ++i) {
        for (int k = 0; k < 16; ++k) *p++ = MatrixWord(gNodes[i]->mMatrix.m[k / 4][k % 4]);
    }
    *p++ = tpw_cl_heap[0];
    *p++ = tpw_cl_heap[2];
    return static_cast<int32_t>(p - out);
}

uint32_t tpw_lm_unique(int32_t idClass, uint32_t index) { return GetUniqueID(static_cast<eIDClass>(idClass), index); }
}
