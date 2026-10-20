// The lobby's island screen, part 1: its set-up, the list of islands, show and hide (main app of
// SimTheme Park 2.04b3: CLBIsland's constructor 0x100afe88; the screen's members 0x100b023c..0x100b08e4
// (constructor, destructor), 0x100b0a54 (Init), 0x100b1d94 (Render), 0x100b1dd0 (Close), 0x100b1fe0..0x100b2174
// (Show, Hide), 0x100b2374..0x100b25d0 (the list); the theme islands' Show, Hide, ShowPanels and HidePanels
// 0x100b2cc4..0x100b31b0; the list instances 0x100b4df8..0x100b5424. The inventory files the range under
// lobby_island.cpp.) Tick and CalcCamera (part 2) are untranslated.
//
// The screen shows the lobby's islands from the air. Each island is a CLBIsland made from the theme's
// description file (ParseThemeDescriptor, untranslated) when the theme islands are shown, and destroyed when
// they are hidden. The camera either orbits the current island or, with none, flies over the map on a random
// path: the constructor picks the first two random paths.
#include "lobby.h"

#include <cmath>

#include "FS_Online.h"
#include "Game.h"
#include "TPSound.h"
#include "TPString.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_rain.h"

template class TbTList<CLBIsland*, CLBIsland*>;

namespace {

// One coordinate of a random point of the box around `centre`: centre + (extent * f - extent / 2), where f
// is a number of the lobby's generator cut to 30 bits and divided by 2^30, so f is in [0, 1).
float RandomInBox(float centre, float extent) {
    const float half = 0.5f * extent;
    const float fraction = static_cast<float>(gLobby->GetRand() & 0x3fffffffu) / 1073741824.0f;
    return centre + ppc_fmsubs(extent, fraction, half);
}

// The vector scaled to unit length. The x square is a double product, the other two single products, and
// the sum and the square root are double, as the original does it.
void Normalise(s3DPoint& v) {
    const double xx = static_cast<double>(v.x) * static_cast<double>(v.x);
    const float yy = v.y * v.y;
    const float zz = v.z * v.z;
    const float length = static_cast<float>(std::sqrt(static_cast<double>(zz) + (xx + static_cast<double>(yy))));
    v.x /= length;
    v.y /= length;
    v.z /= length;
}

// The unit vector from one point to another; (1, 0, 0) when they are the same point.
s3DPoint UnitDirection(const s3DPoint& from, const s3DPoint& to) {
    s3DPoint direction = {to.x - from.x, to.y - from.y, to.z - from.z};
    if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) direction = {1.0f, 0.0f, 0.0f};
    Normalise(direction);
    return direction;
}

}  // namespace

// ---- CLBIsland
// The meshes stand at the island's place with y = 0 and are turned by its angle (degrees in, radians kept).
// The sky colour starts as three random numbers from 0 to 255, in 16.16.
CLBIsland::CLBIsland(const CLBScreenComponentIslands* owner, int32_t order, const char* name, const char* folder,
                     const char* meshA, const char* meshB, float angle, const s3DPoint& position, const char16_t* title,
                     const CFSOnlinePark* park, const char16_t* sign1, const char16_t* sign2) {
    mMeshA = gLobbyMeshes->InstanceMeshType(folder, meshA, 0, 2, sign1, sign2);
    mMeshB = gLobbyMeshes->InstanceMeshType(folder, meshB, 0, 1, sign1, sign2);
    mShown = 0;
    mOrder = order;
    mName = name;
    mPosition = position;
    mTitle = title;
    mLightning = 0;
    mRain = 0;
    const s3DPoint place = {position.x, 0.0f, position.z};
    mSky[0] = (static_cast<uint32_t>(msl_rand()) & 0xff) << 16;
    mSky[1] = (static_cast<uint32_t>(msl_rand()) & 0xff) << 16;
    mSky[2] = (static_cast<uint32_t>(msl_rand()) & 0xff) << 16;
    mPark = park;
    mAngle = 0.017453292f * angle;
    mMeshA->RotateY(mAngle);
    mMeshB->RotateY(mAngle);
    mMeshA->SetPosition(place);
    mMeshB->SetPosition(place);
    mSoundSet = SND_RegisterLocalLobbySounds(mName.mData);
    mSound1 = 0;
    mSound2 = 0;
    mOwner = const_cast<CLBScreenComponentIslands*>(owner);
    mOwner->Register(this);
}

// ---- CLBScreenComponentIslands
// The camera starts where the view is, with the two random flights (the camera's and the look-at's) already
// begun: two random points in the box, and the unit direction from the first to the second.
CLBScreenComponentIslands::CLBScreenComponentIslands() {
    mIslands = new TbPtrList<CLBIsland>;
    mCurrent = nullptr;
    mLast = nullptr;
    mCameraState = 0;
    mAngle = 0.0f;
    mSpinSpeed = 0.0f;
    mRadius = 0.0f;
    mHeight = 0.0f;
    mCameraGoal = gLobbyView->mPosition;
    mCameraPos = mCameraGoal;
    mLookAtGoal = gLobbyView->mTarget;
    mLookAt = mLookAtGoal;
    mRoamCentre = {500.0f, 75.0f, 500.0f};
    mRoamExtent = {400.0f, 50.0f, 400.0f};
    mRoamSpeed = 1.0f;
    mRoamArriveDistSq = 100.0f;
    mRoamPos.x = RandomInBox(mRoamCentre.x, mRoamExtent.x);
    mRoamPos.y = RandomInBox(mRoamCentre.y, mRoamExtent.y);
    mRoamPos.z = RandomInBox(mRoamCentre.z, mRoamExtent.z);
    mRoamGoal.x = RandomInBox(mRoamCentre.x, mRoamExtent.x);
    mRoamGoal.y = RandomInBox(mRoamCentre.y, mRoamExtent.y);
    mRoamGoal.z = RandomInBox(mRoamCentre.z, mRoamExtent.z);
    mRoamDir = UnitDirection(mRoamPos, mRoamGoal);
    mLookMaxSpeed = 2.0f;
    mLookSpeed = 2.0f;
    mLookSlowDistSq = 50.0f;
    mLookPos.x = RandomInBox(mRoamCentre.x, mRoamExtent.x);
    mLookPos.y = RandomInBox(mRoamCentre.y, mRoamExtent.y);
    mLookPos.z = RandomInBox(mRoamCentre.z, mRoamExtent.z);
    mLookGoal.x = RandomInBox(mRoamCentre.x, mRoamExtent.x);
    mLookGoal.y = RandomInBox(mRoamCentre.y, mRoamExtent.y);
    mLookGoal.z = RandomInBox(mRoamCentre.z, mRoamExtent.z);
    mLookDir = UnitDirection(mLookPos, mLookGoal);
}

// Lead 2026-10-20: a NULL list is left alone. The online islands' destructor (0x100b4be0) inlines this text and frees the
// list; on the Mac it then skips this destructor, C++ chains to it (as src/lobby_globe_close.cpp).
CLBScreenComponentIslands::~CLBScreenComponentIslands() {
    if (mIslands == nullptr) return;
    DeleteIslandListEntries();
    delete mIslands;
    mIslands = nullptr;
}

void CLBScreenComponentIslands::Init() { gei_Rain->GameLoad(); }

void CLBScreenComponentIslands::Render() {}

void CLBScreenComponentIslands::Close() {
    SND_FreeLocalLobbySounds();
    gei_Rain->Close();
}

// The base class's version: the screen's own lighting, fog and field of view, no current island, and the
// online system (made if need be) told to stop. The theme islands' Show has this inlined.
void CLBScreenComponentIslands::Show() {
    if (IsVisible() != 0) return;
    mVisible = 1;
    gLobbyMeshes->ShowHeightfield();
    gLobbyView->SetGlobalAmbientLight(1, 0x40404000);
    const uint32_t sun = 0xeeeeee00;
    gLobbyView->SetGlobalDirectionalLight(1, &sun, nullptr, 1);
    gLobbyView->mLightUpOffset = 2.0f;
    gLobbyView->mLightRightOffset = 8.0f;
    gLobbyView->mLightDownward = 1;
    gLobbyView->mFov = gLobby->mGlobals->mIslandFov;
    gLobbyView->SetFogOn();
    gLobbyView->SetFogSettings(0x44ddffff, 50.0f, 250.0f);
    mCameraState = 0;
    mCurrent = nullptr;
    mLast = nullptr;
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    gOnlineSystem->StopProcessing();
}

// Fades the current island's two looping sounds (and forgets them). The theme islands' Hide has this inlined.
void CLBScreenComponentIslands::Hide() {
    if (IsVisible() == 0) return;
    mVisible = 0;
    CLBIsland* island = mCurrent;
    if (island == nullptr) return;
    SOUND_FadeSoundEffect(island->mSound1);
    SOUND_FadeSoundEffect(island->mSound2);
    island->mSound2 = 0;
    island->mSound1 = 0;
}

// (The theme islands' Hide has this inlined too.)
void CLBScreenComponentIslands::DeleteIslandListEntries() {
    while (mIslands->mCount != 0) {
        CLBIsland* island = mIslands->RemoveHead();
        if (island != nullptr) delete island;
    }
}

// Kept in the order of the islands' mOrder: in front of the first island whose order is not below the new
// one's (signed), else at the tail. The new island is the last one registered.
void CLBScreenComponentIslands::Register(CLBIsland* island) {
    if (mIslands->mCount == 0) {
        mIslands->AddHead(island);
    } else {
        POSITION position = mIslands->GetHeadPosition();
        while (position != nullptr) {
            auto* node = static_cast<TbTList<CLBIsland*, CLBIsland*>::CNode*>(position);
            if (!(node->mData->mOrder < island->mOrder)) break;
            position = node->mNext;
        }
        if (position != nullptr) mIslands->InsertBefore(island, position);
        else mIslands->AddTail(island);
    }
    mLast = island;
}

// (The current island and the last one registered are not cleared, whatever leaves the list.)
void CLBScreenComponentIslands::UnRegister(CLBIsland* island) {
    POSITION position = mIslands->Find(island);
    if (position != nullptr) mIslands->RemoveAt(position);
}

void CLBScreenComponentIslands::SetCurrentIsland(const CLBIsland* island) {
    if (island == nullptr) return;
    mCurrent = const_cast<CLBIsland*>(island);
    mCameraState = 0;
}

// ---- CLBScreenComponentThemeIslands
// Shown: the base class's Show, the island lobby's panels, every theme's description read (its islands are
// made and registered), the first island made current and the UI, the advisor's timeout and the signs brought
// up to date, and the current island's two looping sounds started.
void CLBScreenComponentThemeIslands::Show() {
    if (IsVisible() != 0) return;
    CLBScreenComponentIslands::Show();
    CreateIslandLobbyController();
    ShowPanels();
    for (POSITION position = gGame.mThemes.GetHeadPosition(); position != nullptr;) {
        TbDynamicStringTemplate<char>* theme = gGame.mThemes.GetNext(position);
        TbDynamicStringTemplate<char16_t> unicodeName;
        StringToUniString(unicodeName, *theme);
        TbDynamicStringTemplate<char16_t> gate1;
        TbDynamicStringTemplate<char16_t> gate2;
        gGame.GetLocalisedThemeGateNames(unicodeName, gate1, gate2);
        ParseThemeDescriptor(theme->mData, nullptr, -1, gate1.mData, gate2.mData);
    }
    ResetCurrentIsland(0);
    UpdateIslandLobbyUI();
    ResetAdvisorTimeout();
    UpdateSigns();
    CLBIsland* island = mCurrent;
    if (island != nullptr) {
        island->mSound1 = SOUND_SendEvent(island->mSound1, gLobbyIslandSounds[island->mSoundSet * 2 + 1], 1, 0, 0, 0);
        island->mSound2 = SOUND_SendEvent(island->mSound2, gLobbyIslandSounds[island->mSoundSet * 2], 2, 0, 0, 0);
        SOUND_Tick();
    }
}

// Hidden: the base class's Hide, every island destroyed, the height field, the flying meshes and the mesh
// instances taken out of the lobby, the panels hidden and the island lobby's controller removed.
void CLBScreenComponentThemeIslands::Hide() {
    if (IsVisible() == 0) return;
    CLBScreenComponentIslands::Hide();
    DeleteIslandListEntries();
    gLobbyMeshes->HideHeightfield();
    gLobbyFlyingMeshes->DeleteFlyingMeshGroupListEntries();
    gLobbyMeshes->DeleteMeshInstanceListEntries();
    HidePanels();
    RemoveIslandLobbyController();
}

void CLBScreenComponentThemeIslands::ShowPanels() { ShowIslandLobbyController(); }

void CLBScreenComponentThemeIslands::HidePanels() { HideIslandLobbyController(); }
