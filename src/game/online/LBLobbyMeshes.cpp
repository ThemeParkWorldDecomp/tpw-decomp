// LBLobbyMeshes.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of LBLobbyMeshes code, named after it.

#include "Debug.h"
#include "FS_System.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"
#include "TPEngine.h"
#include "ui_ErrorDisplay.h"
#include <cmath>

// The lobby's meshes (main app of SimTheme Park 2.04b3, 0x100a7794..0x100a8aa8 and the list
// instances at 0x100a9c24..0x100aa4ec; the inventory files the range under lobby_globe.cpp):
// CLBMeshInstance, CLBMeshType and the lobby system that owns them, CLBLobbyMeshes.
//
// The lobby draws with the park's own machinery. Its ground is the height field of the level
// "lobby"; everything on it is an instance of a "ride type" loaded from a mesh file. An
// instance's place and attitude are the matrix of its mesh's node at +0x78 of the header.

template class TbTList<CLBMeshType*, CLBMeshType*>;
template class TbTList<CLBMeshInstance*, CLBMeshInstance*>;

namespace {

const float kUp[3] = {0.0f, 1.0f, 0.0f};   // 0x1029e4e4

// Takes a set's mesh out of the drawing, or puts it back: over its range of the map when
// the set has one (flag 0x4000).
void DisableSetDraw(tag_sRideAnimSet* set) {
    if ((set->mFlags & 0x4000) != 0) DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else DisableMeshDraw(set->mHeads);
}

void EnableSetDraw(tag_sRideAnimSet* set) {
    if ((set->mFlags & 0x4000) != 0) EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else EnableMeshDraw(set->mHeads);
}

tag_sNode* NodeOf(uint32_t instance) { return gRideAnimSets[instance]->mHeads->mHeader->mF78; }

}  // namespace

// ---- CLBMeshType (inline in the original)
CLBMeshType::CLBMeshType(const char* folder, const char* name, int32_t overMapRange, uint32_t a, const char16_t* sign1,
                         const char16_t* sign2, uint32_t index) {
    mFolder = folder;
    mName = name;
    uint32_t flags = 0xc0;
    if (overMapRange != 0) flags |= 0x4000;
    mRideType = LoadRideType(mFolder.mData, mName.mData, nullptr, nullptr, flags, GetUniqueID(kIDClassLobbyMeshType, index), a,
                             sign1, sign2, 0, 0);
    gLobbyMeshes->Register(this);
}

CLBMeshType::~CLBMeshType() { gLobbyMeshes->UnRegister(this); }

// ---- CLBMeshInstance
CLBMeshInstance::CLBMeshInstance(CLBMeshType* type, const char* folder, const char* name) {
    mInstance = InstanceRide(type->mRideType, 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffff, 0);
    DisableSetDraw(gRideAnimSets[mInstance]);
    mShown = 0;
    mFolder = folder;
    mName = name;
    gLobbyMeshes->Register(this);
}

CLBMeshInstance::~CLBMeshInstance() {
    gLobbyMeshes->UnRegister(this);
    DestroyRideInstance(mInstance);
}

void CLBMeshInstance::Show() {
    if (mShown != 0) return;
    EnableSetDraw(gRideAnimSets[mInstance]);
    mShown = 1;
}

void CLBMeshInstance::SetPosition(const s3DPoint& position) {
    tag_sNode* node = NodeOf(mInstance);
    node->mMatrix.m[3][0] = position.x;
    node->mMatrix.m[3][1] = position.y;
    node->mMatrix.m[3][2] = position.z;
}

// The matrix's rows become: across (up x direction, unit length), the new up (direction x
// across, unit length), and `direction` as it is given.
void CLBMeshInstance::SetDirection(const s3DPoint& direction) {
    tag_sRideAnimSet* set = gRideAnimSets[mInstance];
    SetMatrixIdentity(&set->mHeads->mHeader->mF78->mMatrix);
    const float x = direction.x, y = direction.y, z = direction.z;

    float acrossY = ppc_fmsubs(kUp[2], x, kUp[0] * z);
    float acrossX = ppc_fmsubs(kUp[1], z, kUp[2] * y);
    float acrossZ = ppc_fmsubs(kUp[0], y, kUp[1] * x);
    float length = static_cast<float>(
        std::sqrt(static_cast<double>(acrossZ * acrossZ) + ppc_fmadd(acrossX, acrossX, static_cast<double>(acrossY * acrossY))));
    acrossY /= length;
    acrossZ /= length;
    acrossX /= length;

    float upX = ppc_fmsubs(y, acrossZ, z * acrossY);
    float upY = ppc_fmsubs(z, acrossX, x * acrossZ);
    float upZ = ppc_fmsubs(x, acrossY, y * acrossX);
    length = static_cast<float>(
        std::sqrt(static_cast<double>(upZ * upZ) + ppc_fmadd(upX, upX, static_cast<double>(upY * upY))));
    upX /= length;
    upY /= length;
    upZ /= length;

    sMatrix& matrix = set->mHeads->mHeader->mF78->mMatrix;
    matrix.m[0][0] = acrossX;
    matrix.m[0][1] = acrossY;
    matrix.m[0][2] = acrossZ;
    matrix.m[1][0] = upX;
    matrix.m[1][1] = upY;
    matrix.m[1][2] = upZ;
    matrix.m[2][0] = x;
    matrix.m[2][1] = y;
    matrix.m[2][2] = z;
}

void CLBMeshInstance::RotateY(float angle) { RotateLMatrixXYZNoTrans(&NodeOf(mInstance)->mMatrix, kRotateY, &angle); }

void CLBMeshInstance::Scale(float x, float y, float z) {
    s3DPoint scale = {x, y, z};
    ScaleMesh(gRideAnimSets[mInstance]->mHeads, &scale);
}

void CLBMeshInstance::PlayAnim(int32_t anim, int32_t loop) {
    if (mInstance == 0) return;
    SetCurRideAnim(gRideAnimSets[mInstance], 5, static_cast<uint32_t>(anim), loop != 0 ? 1 : 0, 1.0f, 0);
}

uint32_t CLBMeshInstance::GetAnimTimeRemaining() {
    if (mInstance == 0) return 0;
    return GetCurrentAnimStats(gRideAnimSets[mInstance], nullptr, nullptr, nullptr, 0);
}

void CLBMeshInstance::SetRideSigns(const char16_t* sign1, const char16_t* sign2) {
    RegenerateRideSign(mInstance, mFolder.mData, mName.mData, sign1, sign2);
}

// ---- CLBLobbyMeshes
CLBLobbyMeshes::CLBLobbyMeshes() {
    {
        TbDynamicStringTemplate<char> level("lobby");
        gTPFileInfo.SetLevelPath(level);
    }
    LoadLevelHeightfield(1, 0);
    mHeightfieldShown = 1;
    HideHeightfield();
    mTypes = new TbPtrList<CLBMeshType>;
    mInstances = new TbPtrList<CLBMeshInstance>;
}

CLBLobbyMeshes::~CLBLobbyMeshes() {
    DeleteMeshInstanceListEntries();
    delete mInstances;
    mInstances = nullptr;
    DeleteMeshTypes();
    DestroyLevelHeightfield();
}

void CLBLobbyMeshes::Init() {}
void CLBLobbyMeshes::Tick() {}
void CLBLobbyMeshes::Render() {}
void CLBLobbyMeshes::Close() {}
void CLBLobbyMeshes::WindowMessagesFn(int32_t, int32_t, int32_t) {}

void CLBLobbyMeshes::ShowHeightfield() {
    if (mHeightfieldShown != 0) return;
    EnableSetDraw(gHeightfieldRideSet);
    mHeightfieldShown = 1;
}

void CLBLobbyMeshes::HideHeightfield() {
    if (mHeightfieldShown == 0) return;
    DisableSetDraw(gHeightfieldRideSet);
    mHeightfieldShown = 0;
}

CLBMeshInstance* CLBLobbyMeshes::InstanceMeshType(const char* folder, const char* name, int32_t overMapRange, uint32_t a,
                                                  const char16_t* sign1, const char16_t* sign2) {
    CLBMeshType* type = nullptr;
    for (auto* node = mTypes->mHead; node != nullptr && type == nullptr;) {
        CLBMeshType* candidate = node->mData;
        node = node->mNext;
        if (NS_MacDoze::_stricmp(candidate->mFolder.mData, folder) == 0 && NS_MacDoze::_stricmp(candidate->mName.mData, name) == 0) {
            type = candidate;
        }
    }
    if (type == nullptr) {
        const uint32_t index = static_cast<uint32_t>(mTypes->mCount);
        type = new CLBMeshType(folder, name, overMapRange, a, sign1, sign2, index);
    }
    return new CLBMeshInstance(type, folder, name);
}

void CLBLobbyMeshes::DeleteMeshTypes() {
    while (mTypes->mCount != 0) delete mTypes->RemoveHead();
    delete mTypes;
    mTypes = nullptr;
    DestroyAllRideTypes();
}

void CLBLobbyMeshes::DeleteMeshInstanceListEntries() {
    while (mInstances->mCount != 0) delete mInstances->RemoveHead();
}

void CLBLobbyMeshes::Register(CLBMeshType* type) { mTypes->AddTail(type); }

void CLBLobbyMeshes::Register(CLBMeshInstance* instance) { mInstances->AddTail(instance); }

void CLBLobbyMeshes::UnRegister(CLBMeshType* type) {
    POSITION position = mTypes->Find(type);
    if (position != nullptr) mTypes->RemoveAt(position);
}

void CLBLobbyMeshes::UnRegister(CLBMeshInstance* instance) {
    POSITION position = mInstances->Find(instance);
    if (position != nullptr) mInstances->RemoveAt(position);
}
