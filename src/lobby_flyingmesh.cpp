// The lobby's flying meshes (main app of SimTheme Park 2.04b3, 0x100aa5d4..0x100aba0c; the
// inventory files them under lobby_globe.cpp): CLBFlyingMeshInstance 0x100aa5d4 (constructor),
// 0x100aa9dc (destructor), 0x100aaa80 (Tick); CLBFlyingMeshGroup 0x100aae1c, 0x100ab008, 0x100ab1a8,
// 0x100ab2fc, 0x100ab3b4, 0x100ab464, 0x100ab4d8; CLBFlyingMeshSystem 0x100ab578, 0x100ab620,
// 0x100ab874, 0x100ab8ac, 0x100ab8f0, 0x100ab99c, 0x100aba0c (Init, Tick and Render are in
// src/lobby_islands_tick.cpp); and two odds and ends: CLBLobby::WindowMessagesFn 0x100a6484 and
// PAR_GetEmitterParticleCount 0x100ba170.
//
// The theme description's FLYINGMESH( lines make groups (ParseThemeDescriptor). A group
// registers with the system and then makes its instances a few at a time on its ticks, up to the
// share of its count that the LOBBYOBJECTS option allows. An instance is a mesh of the lobby's
// mesh system that flies from one random point of the group's box to another: each tick it turns
// a tenth of the way towards the goal, moves, and picks a new goal when it is near enough.
#include "lobby.h"

#include <cmath>

#include "InterfaceWindow.h"
#include "OptionsBalance.h"
#include "ppc_math.h"
#include "rsse.h"

// The lists (include/TbTList.h): their out-of-line members are emitted here, as in the original
// (0x100abaac..0x100abe0c and 0x100ac444 for the groups, 0x100abee8..0x100ac348 for the instances).
template class TbTList<CLBFlyingMeshGroup*, CLBFlyingMeshGroup*>;
template class TbTList<CLBFlyingMeshInstance*, CLBFlyingMeshInstance*>;

namespace {

// A draw of the lobby's generator as a number in [0, 1): its low 30 bits over 2^30.
float Unit(uint32_t draw) { return static_cast<float>(draw & 0x3fffffffu) / 1073741824.0f; }

// A random coordinate of the box round `centre` that is `extent` big: centre + (extent x draw - extent / 2),
// the difference fused.
float Pick(float centre, float extent) {
    const float half = 0.5f * extent;
    const float draw = Unit(gLobby->GetRand());
    return centre + ppc_fmsubs(extent, draw, half);
}

// The listing's length: x * x as a double product, y * y and z * z single, all summed in double.
float Length(const s3DPoint& v) {
    const double xx = static_cast<double>(v.x) * static_cast<double>(v.x);
    const double yy = static_cast<double>(v.y * v.y);
    const double zz = static_cast<double>(v.z * v.z);
    return static_cast<float>(std::sqrt(zz + (xx + yy)));
}

// The vector divided by its length (no check for a vector of no length: that gives NaNs).
void Normalize(s3DPoint& v) {
    const float length = Length(v);
    v.x = v.x / length;
    v.y = v.y / length;
    v.z = v.z / length;
}

// The squared distance: x * x fused with y * y, plus z * z.
float SquaredLength(const s3DPoint& v) {
    const float yy = v.y * v.y;
    const float zz = v.z * v.z;
    return zz + ppc_fmadds(v.x, v.x, yy);
}

}  // namespace

// ---- CLBFlyingMeshInstance

// The mesh starts hidden (the group shows it) and plays its first animation, looping. The start and the first
// goal are random points of the box; the direction is the unit vector between them.
CLBFlyingMeshInstance::CLBFlyingMeshInstance(const CLBFlyingMeshGroup* group, const char* folder, const char* name,
                                             const s3DPoint& position, const s3DPoint& direction, float speed,
                                             float arrival)
    : mGroup(const_cast<CLBFlyingMeshGroup*>(group)) {
    mMesh = gLobbyMeshes->InstanceMeshType(folder, name, 0, 1, nullptr, nullptr);
    mMesh->PlayAnim(0, 1);
    mShown = 0;
    mCentre = position;
    mExtent = direction;
    mSpeed = speed;
    mArriveDistSq = arrival;
    mPosition.x = Pick(mCentre.x, mExtent.x);
    mPosition.y = Pick(mCentre.y, mExtent.y);
    mPosition.z = Pick(mCentre.z, mExtent.z);
    mGoal.x = Pick(mCentre.x, mExtent.x);
    mGoal.y = Pick(mCentre.y, mExtent.y);
    mGoal.z = Pick(mCentre.z, mExtent.z);
    mDirection = {mGoal.x - mPosition.x, mGoal.y - mPosition.y, mGoal.z - mPosition.z};
    Normalize(mDirection);
    mMesh->SetPosition(mPosition);
    mGroup->Register(this);
}

CLBFlyingMeshInstance::~CLBFlyingMeshInstance() { mGroup->UnRegister(this); }

void CLBFlyingMeshInstance::Tick() {
    mGoalDirection = {mGoal.x - mPosition.x, mGoal.y - mPosition.y, mGoal.z - mPosition.z};
    Normalize(mGoalDirection);
    // The direction closes a tenth of the way (times the lobby's step) to the goal's, then is made unit again.
    const float rate = 0.1f * gLobby->mTickUnits;
    mDirection.x = ppc_fmadds(rate, mGoalDirection.x - mDirection.x, mDirection.x);
    mDirection.y = ppc_fmadds(rate, mGoalDirection.y - mDirection.y, mDirection.y);
    mDirection.z = ppc_fmadds(rate, mGoalDirection.z - mDirection.z, mDirection.z);
    Normalize(mDirection);
    const float step = mSpeed * gLobby->mTickUnits;
    mPosition.x = ppc_fmadds(step, mDirection.x, mPosition.x);
    mPosition.y = ppc_fmadds(step, mDirection.y, mPosition.y);
    mPosition.z = ppc_fmadds(step, mDirection.z, mPosition.z);
    const s3DPoint left = {mGoal.x - mPosition.x, mGoal.y - mPosition.y, mGoal.z - mPosition.z};
    if (SquaredLength(left) < mArriveDistSq) {
        mGoal.x = Pick(mCentre.x, mExtent.x);
        mGoal.y = Pick(mCentre.y, mExtent.y);
        mGoal.z = Pick(mCentre.z, mExtent.z);
    }
    mMesh->SetDirection(mDirection);
    mMesh->SetPosition(mPosition);
}

// ---- CLBFlyingMeshGroup

// mMax is a share of `number`: number x LOBBYOBJECTS / 100 (the product wraps in 32 bits), at least 1, and no
// more than `number` (which, for a number below 1, leaves the 1).
CLBFlyingMeshGroup::CLBFlyingMeshGroup(int32_t number, const char* folder, const char* name, const s3DPoint& position,
                                       const s3DPoint& direction, float speed, float limit)
    : mInstances(new TbPtrList<CLBFlyingMeshInstance>) {
    mShown = 0;
    int32_t share = number * gOptionsBalance.mValues[27].mInteger;   // LOBBYOBJECTS, a per cent
    share /= 100;
    if (share < 1) share = 1;
    else if (share > number) share = number;
    mMax = share;
    mCount = 0;
    mFolder = folder;
    mName = name;
    mPosition = position;
    mExtent = direction;
    mSpeed = speed;
    mArriveDistSq = limit;
    gLobbyFlyingMeshes->Register(this);
}

CLBFlyingMeshGroup::~CLBFlyingMeshGroup() {
    gLobbyFlyingMeshes->UnRegister(this);
    DeleteFlyingMeshInstanceListEntries();
    delete mInstances;
    mInstances = nullptr;
}

// Makes instances until the maximum is reached: after each one, the lobby's generator is stepped (in place, the
// same step as GetRand) and the making goes on unless the draw's low three bits are 1 (about one time in eight),
// so a few come each tick. A new instance is shown at once when the group is shown. Then every instance ticks.
void CLBFlyingMeshGroup::Tick() {
    if (mCount < mMax) {
        do {
            CLBFlyingMeshInstance* instance =
                new CLBFlyingMeshInstance(this, mFolder.mData, mName.mData, mPosition, mExtent, mSpeed, mArriveDistSq);
            if (mShown != 0 && instance->mShown == 0) {
                instance->mMesh->Show();
                instance->mShown = 1;
            }
            ++mCount;
            if (mCount >= mMax) break;
            gLobby->mStarted = gLobby->mStarted * 0x19660du + 0x3c6ef35fu;
        } while ((gLobby->mStarted & 7) != 1);
    }
    for (auto* node = mInstances->mHead; node != nullptr;) {
        CLBFlyingMeshInstance* instance = node->mData;
        node = node->mNext;
        instance->Tick();
    }
}

void CLBFlyingMeshGroup::Show() {
    if (mShown != 0) return;
    for (auto* node = mInstances->mHead; node != nullptr;) {
        CLBFlyingMeshInstance* instance = node->mData;
        node = node->mNext;
        if (instance->mShown == 0) {
            instance->mMesh->Show();
            instance->mShown = 1;
        }
    }
    mShown = 1;
}

void CLBFlyingMeshGroup::DeleteFlyingMeshInstanceListEntries() {
    while (mInstances->mCount != 0) {
        CLBFlyingMeshInstance* instance = mInstances->RemoveHead();
        delete instance;
    }
}

void CLBFlyingMeshGroup::Register(CLBFlyingMeshInstance* instance) { mInstances->AddTail(instance); }

void CLBFlyingMeshGroup::UnRegister(CLBFlyingMeshInstance* instance) {
    POSITION position = mInstances->Find(instance);
    if (position != nullptr) mInstances->RemoveAt(position);
}

// ---- CLBFlyingMeshSystem

CLBFlyingMeshSystem::CLBFlyingMeshSystem() : mGroups(new TbPtrList<CLBFlyingMeshGroup>) {}

CLBFlyingMeshSystem::~CLBFlyingMeshSystem() {
    DeleteFlyingMeshGroupListEntries();
    delete mGroups;
    mGroups = nullptr;
}

void CLBFlyingMeshSystem::Close() {}

void CLBFlyingMeshSystem::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {}

void CLBFlyingMeshSystem::DeleteFlyingMeshGroupListEntries() {
    while (mGroups->mCount != 0) {
        CLBFlyingMeshGroup* group = mGroups->RemoveHead();
        delete group;
    }
}

void CLBFlyingMeshSystem::Register(CLBFlyingMeshGroup* group) { mGroups->AddTail(group); }

void CLBFlyingMeshSystem::UnRegister(CLBFlyingMeshGroup* group) {
    POSITION position = mGroups->Find(group);
    if (position != nullptr) mGroups->RemoveAt(position);
}

// ---- CLBLobby::WindowMessagesFn

// The lobby window's messages (the window calls LB_Lobby_WindowMessagesFn). The bits of the word at 0x1c are
// switched on by message 0x10005 and off by 0x10004 for the argument 0, 1 or 2 (bit 1, 2 or 4; any other argument
// does nothing): the window's keyboard modifiers or mouse buttons. After the message's own work every system is
// given the message, and the window's default handler answers.
int32_t CLBLobby::WindowMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x10003:
        mWidth = static_cast<int32_t>(static_cast<uint32_t>(a) >> 16);
        mHeight = static_cast<int32_t>(static_cast<uint32_t>(a) & 0xffff);
        break;
    case 0x14:
        if (gLobby != nullptr) mWindow = nullptr;
        break;
    case 0x11:
        if (a == 1) UpdateOutboxMailIcon();
        break;
    case 0x22:
        mF1C = 0;
        break;
    case 0x10005:
        if (a >= 0 && a <= 2) mF1C = static_cast<int32_t>(static_cast<uint32_t>(mF1C) | (1u << a));
        break;
    case 0x10004:
        if (a >= 0 && a <= 2) mF1C = static_cast<int32_t>(static_cast<uint32_t>(mF1C) & ~(1u << a));
        break;
    default:
        break;
    }
    if (gLobby != nullptr) {
        for (POSITION position = mSystems->GetHeadPosition(); position != nullptr;)
            mSystems->GetNext(position)->WindowMessagesFn(message, a, b);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// ---- PAR_GetEmitterParticleCount

int32_t PAR_GetEmitterParticleCount(uint32_t emitter) {
    if (gParSystemRunning != 0) {
        const PAREmitter& record = gParEmitters[emitter & 0xffff];
        if (record.mInUse != 0) return record.mParticles;
    }
    return 0;
}
