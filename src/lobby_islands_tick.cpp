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
#include "lobby.h"

#include <cmath>

#include "FS_UserInfo.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_rain.h"

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

// ---- CLBFlyingMeshSystem

void CLBFlyingMeshSystem::Init() {}

void CLBFlyingMeshSystem::Tick() {
    for (auto* node = mGroups->mHead; node != nullptr;) {
        CLBFlyingMeshGroup* group = node->mData;
        node = node->mNext;
        group->Tick();
    }
}

void CLBFlyingMeshSystem::Render() {}

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
