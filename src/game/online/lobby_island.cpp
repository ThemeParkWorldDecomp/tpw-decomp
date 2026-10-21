// lobby_island.cpp: name proven by __sinit_lobby_island_cpp in the main application.

#define TPW_GLOBE_SHOWHIDE_REAL   // the download globe's Show calls the online globe's real Show (0x100ad788), not the inline base
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbTime.h"
#include "lobby.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_printf_opengl.h"
#include "tb_rain.h"
#include "TbDynamicStringTemplate.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "TPString.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

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

// The lobby's island screen, part 3: the theme descriptor parser and the rest of the island and theme
// islands members (main app of SimTheme Park 2.04b3). CLBScreenComponentIslands::ParseThemeDescriptor
// 0x100b5d54, ResetIslandLoopingSounds 0x100b2228; CLBIsland::~CLBIsland 0x100b0108, Show 0x100b01c0;
// CLBLobby::GetRand 0x100aa98c; CLBScreenComponentThemeIslands: WindowMessagesFn 0x100b2af8, ProcessCancel
// 0x100b2c00, SelectCurrentIsland 0x100b3218, StartCurrentIslandZoom 0x100b33e8, EnterCurrentIsland
// 0x100b346c, SetCurrentIsland 0x100b3540, GotoNextIsland 0x100b35ac, GotoPreviousIsland 0x100b3684,
// ResetCurrentIsland 0x100b3760, ResetAdvisorTimeout 0x100b3854, UpdateSigns 0x100b38d4, destructor
// 0x100b4cec; and LB_ReadTbUniStringUntilNext 0x100b69f8, which the parser needs (the parser's other
// string reader is in src/lobby_parser.cpp).
//
// Each theme has a description file, "data:lobby:<theme>.txt": one island per ISLAND( line, optional
// FLYINGMESH( lines for the things that fly over the sea, and, after an island's line, the LIGHTNING(,
// RAINY( and SKYCOLOUR( lines that set that island's weather. The theme islands screen reads every
// theme's file when it is shown (src/lobby_islands.cpp).

TbDynamicStringTemplate<char> gLobbyIslandTheme("");   // 0x102e48f8

namespace {

// What the lobby's window sends the screen: message 0x1000b with a key code in the first argument (0x2500 and
// 0x2700 look like the left and right arrow keys' virtual codes in the high byte; names invented) and message
// 0x10005, which selects when its first argument is 0.
constexpr int32_t kMessageClick = 0x10005;
constexpr int32_t kMessageKey = 0x1000b;
constexpr int32_t kKeyLeft = 0x2500;
constexpr int32_t kKeyRight = 0x2700;
constexpr int32_t kKeyReturn = 0x0d;

}  // namespace

// The island leaves its screen's list (the vtable's slot 0x3c of the screen; a plain member here); the
// name and the title are destroyed, then the object freed when the destructor is the deleting one.
CLBIsland::~CLBIsland() { mOwner->UnRegister(this); }

// ---- CLBIsland
// Both meshes are turned on, once.
void CLBIsland::Show() {
    if (mShown != 0) return;
    mMeshA->Show();
    mMeshB->Show();
    mShown = 1;
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

// The lobby's island screen, part 2: the per-frame work (main app of SimTheme Park 2.04b3).
// CLBScreenComponentIslands::Tick 0x100b0ab8, CalcCamera 0x100b1bac, GotoNextIsland
// 0x100b2638, GotoPreviousIsland 0x100b2808, CLBScreenComponentThemeIslands::Tick
// 0x100b29dc, and the flying meshes' Init 0x100ab790, Tick 0x100ab7c4, Render 0x100ab83c.
//
// Every frame the islands screen moves the lobby's camera (the lobby view, gLobbyView) and
// sets the weather. The camera has two modes. While the player browses (a user is logged in
// and an island is current) it circles the current island, turns to the island's heading
// when asked to enter and then zooms in, fading the screen to white. Otherwise it wanders
// over the lobby, looking at the nearest island, which becomes the current one. Positions
// are never set directly: each frame they close a tenth (the look-at point a fifth) of the
// distance to their goal, scaled by the lobby's step.
//
// The rain falls on the islands that have it, harder the closer they are; the current
// island may strike lightning (its mask against the lobby's random numbers), and now and
// then it plays a sound. Those draws come from the lobby's generator, stepped inline here
// (CLBLobby::GetRand is the same step, used for the wandering camera's three); the choice
// of an island's animation and of the advisor's line is the C library's rand.

// TPSound.h declares SOUND_SendEvent as returning nothing, but the listing keeps the result
// (the sound's handle). Declared here with the result until TPSound.h is corrected.
int32_t SOUND_SendEvent(int32_t handle, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c);   // 0x100dd9fc
void SOUND_Tick();                                                                                     // 0x100dd668

namespace {
constexpr float kPi = 3.14159274f;       // 0x40490fdb
constexpr float kTwoPi = 6.28318548f;    // 0x40c90fdb

// Brings an angle into [0, 2 pi) by whole turns. NaN and infinity pass through.
float Wrap(float angle) {
    while (angle >= kTwoPi) angle -= kTwoPi;
    while (angle < 0.0f) angle += kTwoPi;
    return angle;
}

// One step of the lobby's random generator, in place (the same as CLBLobby::GetRand).
uint32_t Step(CLBLobby& lobby) {
    lobby.mStarted = lobby.mStarted * 1664525u + 1013904223u;
    return lobby.mStarted;
}

// A draw as a number in [0, 1): its low 30 bits over 2^30.
float Unit(uint32_t draw) { return static_cast<float>(draw & 0x3fffffffu) / 1073741824.0f; }

s3DPoint Subtract(const s3DPoint& a, const s3DPoint& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

// The listing's squared length: x*x fused with y*y, then z*z added.
float SquaredLength(const s3DPoint& v) {
    const float yy = v.y * v.y;
    const float zz = v.z * v.z;
    return zz + ppc_fmadds(v.x, v.x, yy);
}

// The listing's length: x*x as a double product, y*y and z*z single, all summed in double.
float Length(const s3DPoint& v) {
    const double xx = static_cast<double>(v.x) * static_cast<double>(v.x);
    const double yy = static_cast<double>(v.y * v.y);
    const double zz = static_cast<double>(v.z * v.z);
    return static_cast<float>(std::sqrt(zz + (xx + yy)));
}

void Normalize(s3DPoint& v) {
    const float length = Length(v);
    v.x = v.x / length;
    v.y = v.y / length;
    v.z = v.z / length;
}

// `direction` becomes the unit vector from `from` to `to`; the x axis when they are the same point.
void Aim(s3DPoint& direction, const s3DPoint& to, const s3DPoint& from) {
    direction = Subtract(to, from);
    if (0.0f == direction.x && 0.0f == direction.y && 0.0f == direction.z) direction = {1.0f, 0.0f, 0.0f};
    Normalize(direction);
}

// value += rate * (goal - value), fused.
void Approach(s3DPoint& value, const s3DPoint& goal, float rate) {
    value.x = ppc_fmadds(rate, goal.x - value.x, value.x);
    value.y = ppc_fmadds(rate, goal.y - value.y, value.y);
    value.z = ppc_fmadds(rate, goal.z - value.z, value.z);
}

// position += step * direction, fused.
void Advance(s3DPoint& position, const s3DPoint& direction, float step) {
    position.x = ppc_fmadds(step, direction.x, position.x);
    position.y = ppc_fmadds(step, direction.y, position.y);
    position.z = ppc_fmadds(step, direction.z, position.z);
}

// A coordinate of the wandering camera's next destination: somewhere in the box round its centre.
float Pick(CLBLobby& lobby, float centre, float size) {
    const float half = 0.5f * size;
    const float draw = Unit(lobby.GetRand());
    return centre + ppc_fmsubs(size, draw, half);
}

// GotoNextIsland and GotoPreviousIsland: when the screen is idle (state 0) and has islands, the
// island after (before) the current one, wrapping round, becomes the current one and its two
// sounds replace the old island's.
void GotoIsland(CLBScreenComponentIslands& screen, bool forwards) {
    if (screen.mCameraState != 0 || screen.mIslands->mCount == 0) return;
    if (CLBIsland* old = screen.mCurrent; old != nullptr) {
        SOUND_FadeSoundEffect(old->mSound1);
        SOUND_FadeSoundEffect(old->mSound2);
        old->mSound2 = 0;
        old->mSound1 = 0;
    }
    auto* list = screen.mIslands;
    CLBIsland* island = screen.mCurrent;
    if (island == nullptr) {
        screen.SetCurrentIsland(forwards ? list->mHead->mData : list->mTail->mData);
    } else if (island == (forwards ? list->mTail->mData : list->mHead->mData)) {
        screen.SetCurrentIsland(forwards ? list->mHead->mData : list->mTail->mData);
    } else {
        auto* node = static_cast<TbPtrList<CLBIsland>::CNode*>(list->Find(island));
        screen.SetCurrentIsland(forwards ? node->mNext->mData : node->mPrev->mData);
    }
    island = screen.mCurrent;
    if (island == nullptr) return;
    island->mSound1 = SOUND_SendEvent(island->mSound1, gLobbyIslandSounds[(island->mSoundSet) * 2 + 1], 1, 0, 0, 0);
    island->mSound2 = SOUND_SendEvent(island->mSound2, gLobbyIslandSounds[(island->mSoundSet) * 2], 2, 0, 0, 0);
    SOUND_Tick();
}
}  // namespace

// ---- CLBScreenComponentIslands

// The camera of the browsing player: the goal is a point round the current island at the
// screen's angle, radius and height; the look-at goal is the island. Both are approached,
// the look-at point faster, and the lobby view is given the result.
void CLBScreenComponentIslands::CalcCamera() {
    mLookAtGoal = mCurrent->mPosition;
    mCameraGoal = mLookAtGoal;
    const float sine = static_cast<float>(std::sin(static_cast<double>(mAngle)));
    mCameraGoal.x = ppc_fmadds(mRadius, sine, mCameraGoal.x);
    mCameraGoal.y = mCameraGoal.y + mHeight;
    const float cosine = static_cast<float>(std::cos(static_cast<double>(mAngle)));
    mCameraGoal.z = ppc_fmadds(mRadius, cosine, mCameraGoal.z);
    Approach(mCameraPos, mCameraGoal, 0.1f * gLobby->mTickUnits);
    Approach(mLookAt, mLookAtGoal, 0.2f * gLobby->mTickUnits);
    gLobbyView->mPosition = mCameraPos;
    gLobbyView->mTarget = mLookAt;
}

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

// ---- CLBScreenComponentIslands
// The current island's two looping sounds are faded and cleared; then the sounds of the FIRST island of the
// list (not the current one) are started again, from its set's row of the table.
void CLBScreenComponentIslands::ResetIslandLoopingSounds() {
    if (CLBIsland* current = mCurrent; current != nullptr) {
        SOUND_FadeSoundEffect(current->mSound1);
        SOUND_FadeSoundEffect(current->mSound2);
        current->mSound2 = 0;
        current->mSound1 = 0;
    }
    if (mIslands != nullptr && mIslands->mCount != 0) {
        CLBIsland* first = mIslands->mHead->mData;
        if (first != nullptr) {
            first->mSound1 = SOUND_SendEvent(first->mSound1, gLobbyIslandSounds[first->mSoundSet * 2 + 1], 1, 0, 0, 0);
            first->mSound2 = SOUND_SendEvent(first->mSound2, gLobbyIslandSounds[first->mSoundSet * 2], 2, 0, 0, 0);
            SOUND_Tick();
        }
    }
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

void CLBScreenComponentIslands::TickIslands() {
    CLBLobby& lobby = *gLobby;
    CLBLobbyView& view = *gLobbyView;
    const int32_t islandCount = mIslands->mCount;

    bool browsing = false;
    if (islandCount != 0) {
        if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
        browsing = gUserInfo->mGameUser != -1 && mCurrent != nullptr;
    }

    if (browsing) {
        const int32_t state = mCameraState;
        if (state == 1) {
            // Turn to the island's heading (plus pi), the short way round, then start zooming in.
            view.mFadeTarget = 0;
            mSpinSpeed = lobby.mGlobals->mSpinSpeed;
            mRadius = lobby.mGlobals->mSpinRadius;
            mHeight = lobby.mGlobals->mVerticalOffset;
            const float heading = Wrap(kPi + mCurrent->mAngle);
            float turn = Wrap(mAngle - heading);
            const float step = 0.05f * lobby.mTickUnits;
            const float margin = 0.5f * step;
            if (turn < margin || turn > kTwoPi - margin) {
                mCurrent->mMeshB->PlayAnim(0, 0);
                mCameraState = 2;
            } else {
                if (turn < kPi) turn = turn - step;
                else turn = turn + step;
                mAngle = turn + heading;
            }
        } else if (state < 1) {
            if (state >= 0) {
                // Circle the island.
                view.mFadeTarget = 0;
                mSpinSpeed = lobby.mGlobals->mSpinSpeed;
                mRadius = lobby.mGlobals->mSpinRadius;
                mHeight = lobby.mGlobals->mVerticalOffset;
                mAngle = Wrap(ppc_fmadds(mSpinSpeed, lobby.mTickUnits, mAngle));
            }
        } else if (state < 3) {
            // Close in on the island; once the radius is under 8 the screen is white and the
            // island is entered.
            mAngle = kPi + mCurrent->mAngle;
            mRadius = ppc_fnmsubs(0.07f * mRadius, lobby.mTickUnits, mRadius);
            mHeight = ppc_fnmsubs(0.6f * mHeight, lobby.mTickUnits, mHeight);
            if (mRadius < 8.0f) {
                view.mFadeTarget = 0x10000;
                if (view.mFadeCurrent == 0x10000) EnterCurrentIsland();
            } else {
                float span = lobby.mGlobals->mSpinRadius - 8.0f;
                if (0.0f == span) span = 1.0f;
                const float fade = (1.0f - (mRadius - 8.0f) / span) * 65536.0f;
                float clamped = 0.0f;
                if (!(fade < 0.0f)) {
                    clamped = 65535.0f;
                    if (!(fade > 65535.0f)) clamped = fade;
                }
                view.mFadeTarget = ppc_fctiwz(clamped);
            }
        }
        CalcCamera();
        mRoamPos = mCameraPos;
        mLookPos = mLookAt;
    } else {
        // Wander: fly towards the destination (a new one in the box when it is reached), and
        // look at the nearest island (at the centre, ten up, when there are none).
        view.mFadeTarget = 0;
        Aim(mRoamGoalDir, mRoamGoal, mRoamPos);
        Approach(mRoamDir, mRoamGoalDir, 0.1f * lobby.mTickUnits);
        Normalize(mRoamDir);
        Advance(mRoamPos, mRoamDir, mRoamSpeed * lobby.mTickUnits);
        if (SquaredLength(Subtract(mRoamGoal, mRoamPos)) < mRoamArriveDistSq) {
            mRoamGoal.x = Pick(lobby, mRoamCentre.x, mRoamExtent.x);
            mRoamGoal.y = Pick(lobby, mRoamCentre.y, mRoamExtent.y);
            mRoamGoal.z = Pick(lobby, mRoamCentre.z, mRoamExtent.z);
        }
        view.mPosition = mRoamPos;
        mCurrent = nullptr;
        if (islandCount <= 0) {
            mLookGoal = mRoamCentre;
            mLookGoal.y = 10.0f;
        } else {
            float nearest = 9999999.0f;
            for (auto* node = mIslands->mHead; node != nullptr;) {
                CLBIsland* island = node->mData;
                node = node->mNext;
                const float distance = SquaredLength(Subtract(island->mPosition, mCameraPos));
                if (distance < nearest) {
                    mLookGoal = island->mPosition;
                    mCurrent = island;
                    nearest = distance;
                }
            }
        }
        Aim(mLookGoalDir, mLookGoal, mLookPos);
        Approach(mLookDir, mLookGoalDir, 0.1f * lobby.mTickUnits);
        Normalize(mLookDir);
        Advance(mLookPos, mLookDir, mLookSpeed * lobby.mTickUnits);
        // The look-at point speeds up while it is far from its goal and stops when near.
        if (SquaredLength(Subtract(mLookGoal, mLookPos)) < mLookSlowDistSq) {
            if (mLookSpeed > 0.0f) mLookSpeed = ppc_fnmsubs(0.05f, mLookMaxSpeed, mLookSpeed);
            else mLookSpeed = 0.0f;
        } else {
            if (mLookSpeed < mLookMaxSpeed) mLookSpeed = ppc_fmadds(0.05f, mLookMaxSpeed, mLookSpeed);
            else mLookSpeed = mLookMaxSpeed;
        }
        view.mTarget = mLookPos;
        mCameraPos = mRoamPos;
        mLookAt = mLookPos;
    }

    // The weather. Rain falls harder the nearer the rainy islands are on average (the fourth
    // power of the mean squared distance on the ground plane).
    if (islandCount <= 0) return;
    gei_Rain->SetRainType(1);
    int32_t rainy = 0;
    float total = 0.0f;
    for (auto* node = mIslands->mHead; node != nullptr;) {
        CLBIsland* island = node->mData;
        node = node->mNext;
        if (island->mRain != 0) {
            ++rainy;
            const float dx = island->mPosition.x - view.mPosition.x;
            const float dz = island->mPosition.z - view.mPosition.z;
            total = total + SquaredLength({dx, 0.0f, dz});
        }
    }
    float drops = 0.0f;
    if (rainy > 0) {
        float mean = total / static_cast<float>(rainy);
        mean = mean * mean;
        mean = mean * mean;
        drops = 1.0e17f / mean;
        if (drops < 20.0f) drops = 0.0f;
        else if (drops > 400.0f) drops = 400.0f;
    }
    gei_Rain->SetRaindrops(ppc_fctiwz(drops));

    CLBIsland* current = mCurrent;
    if (current == nullptr) return;
    const int32_t mask = current->mLightning;
    if (mask > 0 && (Step(lobby) & static_cast<uint32_t>(mask)) == 1) {
        // A bolt from a point 500 up, within 50 of the island, to a point within 10 of it.
        const float skyX = ppc_fmadds(100.0f, Unit(Step(lobby)), current->mPosition.x) - 50.0f;
        const float skyZ = ppc_fmadds(100.0f, Unit(Step(lobby)), current->mPosition.z) - 50.0f;
        const float groundX = ppc_fmadds(20.0f, Unit(Step(lobby)), skyX) - 10.0f;
        const float groundZ = ppc_fmadds(20.0f, Unit(Step(lobby)), skyZ) - 10.0f;
        gei_Rain->AddLightning({groundX, 0.0f, groundZ}, {skyX, 500.0f, skyZ}, 500, 1);
        gei_Rain->LightningFlash();
        SOUND_SendEvent(0, gLobbyThunderBank, 1, 0, 0, 0);
    }
    // One frame in sixteen, a sound of the island: its event 3, 4 or 5.
    if ((Step(lobby) & 0xf) == 1) {
        const int32_t event = 3 + static_cast<int32_t>(Step(lobby) % 3);
        SOUND_SendEvent(0, gLobbyIslandSounds[(current->mSoundSet) * 2], event, 0, 0, 0);
    }
    view.mSkyTarget[0] = current->mSky[0];
    view.mSkyTarget[1] = current->mSky[1];
    view.mSkyTarget[2] = current->mSky[2];
    // An island whose animation has run out starts one of its two at random, once.
    for (auto* node = mIslands->mHead; node != nullptr;) {
        CLBIsland* island = node->mData;
        node = node->mNext;
        if (static_cast<int32_t>(island->mMeshA->GetAnimTimeRemaining()) <= 0) island->mMeshA->PlayAnim(msl_rand() & 1, 0);
    }
}

void CLBScreenComponentIslands::GotoNextIsland() { GotoIsland(*this, true); }

void CLBScreenComponentIslands::GotoPreviousIsland() { GotoIsland(*this, false); }

// ---- CLBScreenComponentThemeIslands

void CLBScreenComponentThemeIslands::Tick() {
    TickIslands();
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    if (gUserInfo->mGameUser != -1) {
        // The advisor says one of two lines after 90 seconds of silence.
        gLobbyScreens->mAdvisor->SetTimeoutResponse({(msl_rand() & 1) == 1 ? 0x18b : 0x18a}, 90000);
    }
}

// A key or a click on the lobby: left and right arrows go to the previous and next island, Return and a
// click with nothing in the first argument select the current one.
void CLBScreenComponentThemeIslands::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    if (message == kMessageKey) {
        if (a == kKeyLeft) GotoPreviousIsland();
        else if (a == kKeyRight) GotoNextIsland();
        else if (a == kKeyReturn) SelectCurrentIsland();
    } else if (message == kMessageClick) {
        if (a == 0) SelectCurrentIsland();
    }
}

// While zooming in (state 1 or 2) it backs out to the browsing state; from the state 2 the island's second
// mesh plays its animation 1 once. The answer is whether it did something. The island lobby's panels come
// back either way.
uint32_t CLBScreenComponentThemeIslands::ProcessCancel() {
    uint32_t handled = 0;
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);
        mCameraState = 0;
        handled = 1;
    }
    ShowIslandLobbyController();
    UpdateIslandLobbyUI();
    return handled;
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

// Browsing with a current island: the easy mode goes in free, anything else asks the master save whether
// the player holds the keys the island costs.
void CLBScreenComponentThemeIslands::SelectCurrentIsland() {
    if (mCameraState != 0 || mCurrent == nullptr) return;
    if (TheGameType().mType == kGameTypeEasy) {
        StartCurrentIslandZoom();
        return;
    }
    CFSGlobalParkData* park;
    {
        const TbDynamicStringTemplate<char> name(mCurrent->mName.mData);
        if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
        park = gMasterSave->GetInfoOn(name);
    }
    if (park == nullptr) return;
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    const int32_t keys = gMasterSave->HowManyKeys();
    const int32_t cost = park->KeyCostToEnter();
    if (cost <= keys) StartCurrentIslandZoom();
}

void CLBScreenComponentThemeIslands::StartCurrentIslandZoom() {
    mCameraState = 1;
    KeyParticlePOUFFE();
    HideIslandLobbyController();
}

// The zoom has finished: the theme of the island becomes the current theme and the lobby is done (state 2).
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {
    if (mCurrent == nullptr) return;
    gLobbyIslandTheme = mCurrent->mName.mData;
    gGame.SetCurrentTheme(TbDynamicStringTemplate<char>(mCurrent->mName.mData));
    gLobby->mState = 2;
}

void CLBScreenComponentThemeIslands::SetCurrentIsland(const CLBIsland* island) {
    if (island == nullptr) return;
    mCurrent = const_cast<CLBIsland*>(island);
    mCameraState = 0;
}

// Only while browsing (state 0), and not in the easy mode: the base class moves, then the panels follow.
void CLBScreenComponentThemeIslands::GotoNextIsland() {
    if (mCameraState != 0) return;
    if (TheGameType().mType == kGameTypeEasy) return;
    CLBScreenComponentIslands::GotoNextIsland();
    ShowIslandLobbyController();
    UpdateIslandLobbyUI();
}

void CLBScreenComponentThemeIslands::GotoPreviousIsland() {
    if (mCameraState != 0) return;
    if (TheGameType().mType == kGameTypeEasy) return;
    CLBScreenComponentIslands::GotoPreviousIsland();
    ShowIslandLobbyController();
    UpdateIslandLobbyUI();
}

// Theme-island ResetCurrentIsland 0x100b3760 (168 bytes) and UpdateSigns 0x100b38d4 (528); existing bodies, main, unit w43.

void CLBScreenComponentThemeIslands::ResetCurrentIsland(int32_t a) {
    mCurrent = mIslands->mHead->mData;
    if (a == 0) {
        for (auto* node = mIslands->mHead; node != nullptr;) {
            CLBIsland* island = node->mData;
            node = node->mNext;
            if (NS_MacDoze::_stricmp(island->mName.mData, gLobbyIslandTheme.mData) == 0) mCurrent = island;
        }
    } else {
        gLobbyIslandTheme = "";
    }
}

void CLBScreenComponentThemeIslands::ResetAdvisorTimeout() { mFE4 = LbTime_GetClock(); }

void CLBScreenComponentThemeIslands::UpdateSigns() {
    for (auto* node = mIslands->mHead; node != nullptr;) {
        CLBIsland* island = node->mData;
        node = node->mNext;
        TbDynamicStringTemplate<char16_t> sign1;
        TbDynamicStringTemplate<char16_t> sign2;
        if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
        if (gUserInfo->mGameUser != -1) {
            CFSGlobalParkData* park;
            {
                const TbDynamicStringTemplate<char> name(island->mName.mData);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
                park = gMasterSave->GetInfoOn(name);
            }
            char16_t textA[0x100];
            park->GetSignNameA(textA, 0x100);
            sign1 = textA;
            char16_t textB[0x100];
            park->GetSignNameB(textB, 0x100);
            sign2 = textB;
        } else {
            TbDynamicStringTemplate<char16_t> unicodeName;
            {
                const TbDynamicStringTemplate<char> name(island->mName.mData);
                StringToUniString(unicodeName, name);
            }
            gGame.GetLocalisedThemeGateNames(unicodeName, sign1, sign2);
        }
        island->mMeshA->SetRideSigns(sign1.mData, sign2.mData);
        island->mMeshB->SetRideSigns(sign1.mData, sign2.mData);
    }
}

// The online islands screen's small members (unit v5255): Tick 0x100b3b28, SelectCurrentIsland 0x100b44ac, EnterCurrentIsland
// 0x100b45d0, GotoNextIsland 0x100b46fc, DownloadFailFunction 0x100b4a98, HandleOnlineBacktrack 0x100b4b48.

namespace {

// The word at 0xe8 (mFE8): 1 while the city lobby controller is shown.
inline int32_t GetShown(const CLBScreenComponentOnlineIslands* s) {
    int32_t v;
    std::memcpy(&v, s->mFE8, 4);
    return v;
}
inline void SetShown(CLBScreenComponentOnlineIslands* s, int32_t v) { std::memcpy(s->mFE8, &v, 4); }

}  // namespace

void CLBScreenComponentOnlineIslands::Tick() {
    TickIslands();
    if (mFE4 != 0 && GetShown(this) != 0 && mCurrent != nullptr && mCurrent->mPark != nullptr) UpdateCityLobbyUI(mCurrent);
}

// Unit v51106: the online islands' small members and the globes' panel members (lobby screens).

void RemoveCityLobbyController();       // 0x101e07f0
namespace {
// The Mac word at 0xe8 (mFE8): 1 while the city lobby controller shows.
inline void SetShownOnline(CLBScreenComponentOnlineIslands* s, int32_t v) { std::memcpy(s->mFE8, &v, 4); }

constexpr int32_t kKeyReturn_2 = 0xd;
}  // namespace

// 0x100b3bc8
void CLBScreenComponentOnlineIslands::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    if (message == kMessageKey) {
        if (a == kKeyLeft) GotoPreviousIsland();
        else if (a == kKeyRight) GotoNextIsland();
        else if (a == kKeyReturn_2) SelectCurrentIsland();
    } else if (message == kMessageClick) {
        if (a == 0) SelectCurrentIsland();
    }
}

// CLBScreenComponentOnlineIslands::CreateParks (0x100b4880), Show (0x100b3ec8) and ProcessCancel (0x100b3cd0).

namespace {
// The Mac's "if (gOnlineSystem == 0) make it" that every function here repeats.
CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    return gOnlineSystem;
}
}  // namespace

// 0x100b3cd0: the camera state first (2: the zoom is undone), else the online system when it is busy; the city lobby controller shows
// in every case. Answer 1 when something was cancelled.
uint32_t CLBScreenComponentOnlineIslands::ProcessCancel() {
    bool handled = false;
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);
        handled = true;
        mCameraState = 0;
    }
    if (!handled) {
        if (OnlineSystem()->mF384 == 0) {
            ShowCityLobbyController();
            return 0;
        }
    }
    OnlineSystem()->StopProcessing();
    ShowCityLobbyController();
    return 1;
}

// Unit v51046: nine small functions of the lobby, the interface and the ESP sprites (main app).
//   OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
//   CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
//   CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
//   CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.

// 0x100b3e28 (TVector 0x10266778): error 0 makes the islands of the park list, any other error backs out.
void PostGetTop10ParksListCallbackFunction(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* parks) {
    CLBScreenComponentOnlineIslands* screen =
        const_cast<CLBScreenComponentOnlineIslands*>(static_cast<const CLBScreenComponentOnlineIslands*>(context));
    if (error == 0) {
        screen->CreateParks(parks);
    } else {
        screen->HandleOnlineBacktrack();
    }
}

// 0x100b3ec8
void CLBScreenComponentOnlineIslands::Show() {
    if (mainmenu_callee::ScreenIsVisible(this) != 0) return;
    gLobbyScreens->mAdvisor->ClearAll();
    CreateCityLobbyController();
    parkinfo_callee::OnlineIslandsSlot30(this);   // slot 0x30
    if (mainmenu_callee::ScreenIsVisible(this) == 0) {
        mVisible = 1;
        gLobbyMeshes->ShowHeightfield();
        gLobbyView->SetGlobalAmbientLight(1, 0x40404000u);
        uint32_t color = 0xEEEEEE00u;
        gLobbyView->SetGlobalDirectionalLight(1, &color, nullptr, 1);
        gLobbyView->mLightUpOffset = 2.0f;
        gLobbyView->mLightRightOffset = 8.0f;
        gLobbyView->mLightDownward = 1;
        gLobbyView->mFov = gLobby->mGlobals->mIslandFov;
        gLobbyView->SetFogOn();
        gLobbyView->SetFogSettings(0x44DDFFFFu, 50.0f, 250.0f);
        mLast = nullptr;
        mCurrent = nullptr;
        mCameraState = 0;
        OnlineSystem()->StopProcessing();
    }
    mFE4 = 0;
    OnlineSystem()->mParksTop10->GetList(gOnlineParkSelection->mID, PostGetTop10ParksListCallbackFunction, this);
    CLBIsland* island = mCurrent;
    if (island != nullptr) {
        island->mSound1 = SOUND_SendEvent(island->mSound1, gLobbyIslandSounds[island->mSoundSet * 2 + 1], 1, 0, 0, 0);
        island->mSound2 = SOUND_SendEvent(island->mSound2, gLobbyIslandSounds[island->mSoundSet * 2], 2, 0, 0, 0);
        SOUND_Tick();
    }
}

// 0x100b4170: the base class's Hide is inlined (the visible flag, the current island's sounds, then the islands are destroyed).
void CLBScreenComponentOnlineIslands::Hide() {
    if (IsVisible() == 0) return;
    if (IsVisible() != 0) {
        mVisible = 0;
        CLBIsland* current = mCurrent;
        if (current != nullptr) {
            SOUND_FadeSoundEffect(current->mSound1);
            SOUND_FadeSoundEffect(current->mSound2);
            current->mSound2 = 0;
            current->mSound1 = 0;
        }
    }
    while (mIslands->mCount != 0) {
        CLBIsland* island = mIslands->RemoveHead();
        if (island != nullptr) delete island;
    }
    gLobbyMeshes->HideHeightfield();
    gLobbyFlyingMeshes->DeleteFlyingMeshGroupListEntries();
    gLobbyMeshes->DeleteMeshInstanceListEntries();
    HidePanels();
    RemoveCityLobbyController();
}

// 0x100b42d4, slot 0x30
void CLBScreenComponentOnlineIslands::ShowPanels() {
    ShowCityLobbyController();
    ShowInfoOverlayPanel();
}

// 0x100b4354, slot 0x34
void CLBScreenComponentOnlineIslands::HidePanels() { HideCityLobbyController(); }

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

// ---- the static initialisers: the wide strings the files hold (Mac 0x1023e9f0 and so on), the objects and the nodes

namespace {
using W = TbDynamicStringTemplate<char16_t>;
using N = TbDynamicStringTemplate<char>;
const char16_t kIslandW[] = u"upload\0gms.dat";   // 0x1023e9f0 (the second string starts at 7)
}  // namespace

alignas(16) unsigned char gV51077IslandW4938[sizeof(W)];   // 0x102e4938
unsigned char gV51077IslandNode494c[16];                    // 0x102e494c
alignas(16) unsigned char gV51077IslandW4918[sizeof(W)];   // 0x102e4918
unsigned char gV51077IslandNode492c[16];                    // 0x102e492c
alignas(16) unsigned char gV51077IslandN48f8[sizeof(N)];   // 0x102e48f8
unsigned char gV51077IslandNode490c[16];                    // 0x102e490c
// ---- the lobby callback, the fly accessors, Debug.cpp, the ride pointers, the keyboard actions

void IslandOnlineParkInfoPanelCancelCallback() { ShowCityLobbyController(); }

// ---- the online islands screen's callbacks

// 0x100b4420 (TVector 0x10266748): error 0 runs the screen's slot 0x4c, any other error its DownloadFailFunction.
void OnlineIslandParkDownloadCompleteCallback(int32_t error, const void* context) {
    CLBScreenComponentOnlineIslands* screen =
        const_cast<CLBScreenComponentOnlineIslands*>(static_cast<const CLBScreenComponentOnlineIslands*>(context));
    if (error == 0) {
        OnlineIslandsSlot4c(screen);
    } else {
        screen->DownloadFailFunction();
    }
}

// ---- the islands screen

uint32_t CLBScreenComponentIslands::ProcessCancelIslands() {
    uint32_t answer = 0;
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);   // the original does not test mCurrent
        answer = 1;
        mCameraState = 0;
    }
    return answer;
}

void CLBScreenComponentOnlineIslands::SelectCurrentIsland() {
    if (mCameraState == 0 && mCurrent != nullptr && mCurrent->mPark != nullptr) {
        SetShown(this, 0);
        HideCityLobbyController();
        CreateOnlineParkInfoPanel(mCurrent->mPark, nullptr, IslandOnlineParkInfoPanelCancelCallback,
                                  OnlineIslandParkDownloadCompleteCallback, this);
    }
}

// ---- the online islands

// 0x100b4574, slot 0x4c
void CLBScreenComponentOnlineIslands::StartCurrentIslandZoom() { mCameraState = 1; }

// 0x100b4690, slot 0x54
void CLBScreenComponentOnlineIslands::SetCurrentIsland(const CLBIsland* island) {
    if (island == nullptr) return;
    mCurrent = const_cast<CLBIsland*>(island);
    mCameraState = 0;
}

void CLBScreenComponentOnlineIslands::EnterCurrentIslandOnline() {
    gLobby->mState = 2;
    TheGameType().SetGameType(kGameTypeOnline);
}

void CLBScreenComponentOnlineIslands::GotoNextIsland() {
    if (mCameraState == 0) {
        CLBScreenComponentIslands::GotoNextIsland();
        SetShown(this, 1);
        ShowCityLobbyController();
    }
}

// 0x100b4790, slot 0x44
void CLBScreenComponentOnlineIslands::GotoPreviousIsland() {
    if (mCameraState == 0) {
        CLBScreenComponentIslands::GotoPreviousIsland();
        SetShownOnline(this, 1);
        ShowCityLobbyController();
    }
}

// 0x100b4828
void CLBScreenComponentOnlineIslands::ShowInfoOverlayPanel() { SetShownOnline(this, 1); }

// 0x100b4880: one ParseThemeDescriptor per online park (its theme's name and gate names), then the camera goes to the first island.
// An empty island list would crash the Mac's last lines (the head is read unchecked); so does this.
void CLBScreenComponentOnlineIslands::CreateParks(TbPtrList<CFSOnlinePark>* parks) {
    int32_t order = 0;
    for (POSITION position = parks->GetHeadPosition(); position != nullptr;) {
        CFSOnlinePark* park = parks->GetNext(position);
        TbDynamicStringTemplate<char16_t> themeName;
        {
            TbDynamicStringTemplate<char> narrow(OnlineSystem()->mThemeList->ReturnOnlineThemeName(park->mF4C));
            StringToUniString(themeName, narrow);
        }
        TbDynamicStringTemplate<char16_t> gate1;
        TbDynamicStringTemplate<char16_t> gate2;
        gGame.GetLocalisedThemeGateNames(themeName, gate1, gate2);
        const char16_t* gateText2 = gate2.mData;
        const char16_t* gateText1 = gate1.mData;
        char* theme = OnlineSystem()->mThemeList->ReturnOnlineThemeName(park->mF4C);
        ParseThemeDescriptor(theme, park, order, gateText1, gateText2);
        order++;
    }
    mCurrent = mIslands->mHead->mData;
    mFE4 = 1;
}

void CLBScreenComponentOnlineIslands::DownloadFailFunction() {
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);
        mCameraState = 0;
    }
    ShowCityLobbyController();
}

void CLBScreenComponentOnlineIslands::HandleOnlineBacktrack() {
    Hide();
    gLobbyScreens->mDownloadGlobe->Show();
}

// 0x100b4be0: the base destructor's text is inlined (the islands are destroyed and the list freed); the program then calls
// CLBScreenComponent's destructor, which the compiler does after this body. The hidden delete flag is the caller's.
CLBScreenComponentOnlineIslands::~CLBScreenComponentOnlineIslands() {
    while (mIslands != nullptr && mIslands->mCount != 0) {   // Lead 2026-10-20: a NULL list is left alone (as src/lobby_globe_close.cpp)
        CLBIsland* island = mIslands->RemoveHead();
        if (island != nullptr) delete island;
    }
    if (mIslands != nullptr) delete mIslands;
    mIslands = nullptr;
}

// ---- the base islands

// 0x100b1e3c: the same text, through the base class's slots.
void CLBScreenComponentIslands::WindowMessagesFnIslands(int32_t message, int32_t a, int32_t b) {
    if (message == kMessageKey) {
        if (a == kKeyLeft) GotoPreviousIsland();
        else if (a == kKeyRight) GotoNextIsland();
        else if (a == kKeyReturn_2) Slot48();
    } else if (message == kMessageClick) {
        if (a == 0) Slot48();
    }
}

// ---- CLBScreenComponentThemeIslands
// (The base destructor's text, inlined in the original, runs as the base class's.)
CLBScreenComponentThemeIslands::~CLBScreenComponentThemeIslands() {}

void __sinit_lobby_island_cpp() {
    void* wide1 = new (static_cast<void*>(gV51077IslandW4938)) W(kIslandW);
    __register_global_object(wide1, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077IslandNode494c);
    void* wide2 = new (static_cast<void*>(gV51077IslandW4918)) W(kIslandW + 7);
    __register_global_object(wide2, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077IslandNode492c);
    void* narrow = new (static_cast<void*>(gV51077IslandN48f8)) N("");
    __register_global_object(narrow, reinterpret_cast<void*>(&UIMain_DestroyString), gV51077IslandNode490c);
}
