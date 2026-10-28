// lobby_globe.cpp: name proven by __sinit_lobby_globe_cpp in the main application.

#define TPW_GLOBE_INIT_REAL 1
#define TPW_GLOBE_SHOWHIDE_REAL 1
#include "AdvisorScoring.h"
#include "Bitmap.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "email.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "Light.h"
#include "lobby.h"
#include "m3d2.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// The globe's destructor, its key function: the vtable is emitted here (lead 2026-10-20, v51031, out of
// src/MapCell_kinds_LLCC_DrawLine_2.cpp and in place of the empty one src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Show_2.cpp had).

// 0x100ac534 (112 bytes): the Mac stores the vtable, then calls its screen's slot 0x3c with the globe (no NULL test on the screen);
// the delete storage flag (r4 > 0 as a short: operator delete) is the compiler's job here.
CLBGlobe::~CLBGlobe() { mScreen->UnRegisterGlobe(this); }

// CLBGlobeCityFlag (lobby_globe.cpp): a city's flag on the online globe. 0x100ac5c8 constructor, 0x100ac86c destructor.

// 0x1023e91e: the wide string the city's name is compared with (its text is not known here; the Mac's starts "No").
char16_t gGlobeNoCityName[16] = u"No";

CLBGlobeCityFlag::CLBGlobeCityFlag(const CLBScreenComponentOnlineGlobe* globe, const CFSOnlineCity* city) {
    mCity = city;
    if (NS_MacDoze::_wcsicmp(city->mName, gGlobeNoCityName) == 0)
        mMesh = gLobbyMeshes->InstanceMeshType("data:lobby:globe", "bullfrog", 0, 1, nullptr, nullptr);
    else
        mMesh = gLobbyMeshes->InstanceMeshType("data:lobby:globe", "hpin", 0, 1, nullptr, nullptr);
    mMesh->Show();

    const CFSOnlineCity* c = mCity;
    s3DPoint pos;
    pos.x = c->mPosX;
    pos.y = c->mPosY;
    pos.z = c->mPosZ;
    s3DPoint dir = pos;
    float y2 = dir.y * dir.y;
    float z2 = dir.z * dir.z;
    double x2 = static_cast<double>(dir.x) * static_cast<double>(dir.x);
    double sum = static_cast<double>(z2) + (x2 + static_cast<double>(y2));
    float len = static_cast<float>(std::sqrt(sum));
    dir.x = dir.x / len;
    dir.y = dir.y / len;
    dir.z = dir.z / len;
    mMesh->SetDirection(dir);
    mMesh->SetPosition(pos);

    float v = static_cast<float>(mCity->mValue);
    float t = ppc_fmadds(0.0005f, v, 0.02f);
    float lo = 0.02f;
    float k;
    if (t >= lo) {
        k = 0.1f;
        if (!(t > 0.1f)) k = t;
    } else {
        k = lo;
    }
    float s = ppc_fmadds(0.2f, k, 0.035f);
    mMesh->Scale(s, s, k);

    mGlobe = globe;
    reinterpret_cast<CLBGlobeFlagOwner*>(const_cast<CLBScreenComponentOnlineGlobe*>(mGlobe))->AddFlag(this);
}

// Complete-object destructor; the Mac's delete flag and the NULL check belong to the caller here.
CLBGlobeCityFlag::~CLBGlobeCityFlag() {
    reinterpret_cast<CLBGlobeFlagOwner*>(const_cast<CLBScreenComponentOnlineGlobe*>(mGlobe))->RemoveFlag(this);
}

// The lobby's online globe screens (main app of SimTheme Park 2.04b3): the base class's constructor (0x100ac90c) and the
// download and publish globes' Init (0x100ae2d0, 0x100aeac8). The rest of the three classes is untranslated.

// Two lists on the heap (the Mac tests each allocation for NULL before running the list's constructor, which is what a
// nothrow new does), then nine floats. The words 0x34, 0x44..0x6b and 0x70 are not written. A failed allocation leaves
// its field NULL and the other is still tried.
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {
    mGlobes = new (std::nothrow) TbTList<CLBGlobe*, CLBGlobe*>;
    mCityFlags = new (std::nothrow) TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>;
    mF1C = 0.4f;
    mF20 = 0.15f;
    mF24 = 0.5f;
    mF38 = 300.0f;
    mF28 = 0.2f;
    mF2C = 0.075f;
    mF30 = 0.25f;
    mF3C = 150.0f;
    mF40 = 1.5f;
    mF78 = 0;
    mF74 = 0;
}

// The online globe's end (main app, lobby_globe.cpp): the destructor and the two helpers that empty its lists (0x100aca4c,
// 0x100adf48, 0x100ae104) and Close (0x100ad50c), which does nothing. The vtable (0x1029e830) is emitted here, where the
// destructor is.

// The entries of both lists are destroyed one by one, then the lists, through their virtual destructors; the pointers are cleared
// after each. The base class's destructor follows. The lists are not tested for NULL by the helpers (a failed allocation in the
// constructor would crash them).
//
// HOST (unit v5L04, 2026-10-20): a derived destructor translated as the Mac laid it out carries this body inlined and then
// clears both pointers (CLBScreenComponentDownloadGlobe's, 0x100af330, unit v5553; the publish globe's, 0x100af1c8, will be
// the same); the Mac's code goes on to CLBScreenComponent's destructor directly, but C++ runs this one next, and reading
// mGlobes->mCount through the NULL pointer crashed the launcher at the lobby's close (the island gate). A NULL list is
// therefore left alone here. The non-NULL path is unchanged (LobbyExit's difftest; its NULL cases are in the untested list).
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {
    if (mGlobes != nullptr) {
        DeleteGlobeListEntries();
        delete mGlobes;
        mGlobes = nullptr;
    }
    if (mCityFlags != nullptr) {
        DeleteCityListEntries();
        delete mCityFlags;
        mCityFlags = nullptr;
    }
}

// Unit v51119: the world globe's camera steps and list registration (CLBScreenComponentOnlineGlobe), and the small
// destructors and byte swaps the neighbouring addresses hold.

namespace {
inline float LoadF(const uint8_t* p) {
    float f;
    std::memcpy(&f, p, 4);
    return f;
}
inline void StoreF(uint8_t* p, float f) { std::memcpy(p, &f, 4); }
}  // namespace

// ---- CLBScreenComponentOnlineGlobe
// 0x100acc98
void CLBScreenComponentOnlineGlobe::Init() { mF70 = 0; }

s3DPoint gGlobeCentre;       // 0x1029e71c
s3DPoint gGlobeViewTarget;   // 0x1029e728

namespace {

// The unit step of a clamp the original writes with explicit branches: below 0 gives 0, above 1 gives 1, else the value.
float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// A 4-byte word of the globe's byte-array fields (mF0C, mF34, mF44 in lobby.h), read and written with memcpy, so the body
// reads like the Mac's field accesses.
template <class T>
struct Word {
    uint8_t* p;
    operator T() const {
        T v;
        std::memcpy(&v, p, 4);
        return v;
    }
    Word& operator=(T v) {
        std::memcpy(p, &v, 4);
        return *this;
    }
    Word& operator=(const Word& other) { return *this = static_cast<T>(other); }
};

}  // namespace

// 0x100acce0: picks the city flag under the cursor (the ray from the eye through the cursor against the globe's
// sphere), scrolls the globe by the cursor's position or the right button, then moves the view.
void CLBScreenComponentOnlineGlobe::Tick() {
    Word<float> mTickScale{mF0C + 4};       // Mac 0x10
    Word<float> mDamping{mF0C + 12};        // Mac 0x18
    Word<float> mSizeNow{mF34};             // Mac 0x34
    Word<float> mSpinX{mF44 + 0x00};        // Mac 0x44
    Word<float> mSpinXSpeed{mF44 + 0x04};   // Mac 0x48
    Word<float> mSpinY{mF44 + 0x08};        // Mac 0x4c
    Word<float> mSpinYSpeed{mF44 + 0x0c};   // Mac 0x50
    Word<float> mRadius{mF44 + 0x10};       // Mac 0x54
    Word<int32_t> mZoomedIn{mF44 + 0x14};   // Mac 0x58
    Word<float> mHitX{mF44 + 0x18};         // Mac 0x5c
    Word<float> mHitY{mF44 + 0x1c};         // Mac 0x60
    Word<float> mHitZ{mF44 + 0x20};         // Mac 0x64
    Word<int32_t> mPicking{mF44 + 0x24};    // Mac 0x68
    CLBLobby* lobby = gLobby;
    if (mPicking != 0) {
        float in[3];
        in[0] = 0.3862f * ((2.0f * static_cast<float>(lobby->mWidth)) / 2048.0f - 1.0f);
        in[1] = 0.3862f * (gei_Display.mF04 * ((2.0f * static_cast<float>(0x600 - lobby->mHeight)) / 1536.0f - 1.0f));
        in[2] = 1.0f;
        float out[3];
        gei_ViewMatrix->TransformVertex(out, in, 2);

        const float ex = gLobbyView->mPosition.x;
        const float ey = gLobbyView->mPosition.y;
        const float ez = gLobbyView->mPosition.z;
        const float cx = gGlobeCentre.x;
        const float cy = gGlobeCentre.y;
        const float cz = gGlobeCentre.z;

        float dx = out[0] - ex;
        float dy = out[1] - ey;
        float dz = out[2] - ez;
        float sqy = dy * dy;
        float sqz = dz * dz;
        double sum = static_cast<double>(sqz) + ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(sqy));
        float len = static_cast<float>(std::sqrt(sum));
        dy = dy / len;
        dx = dx / len;
        dz = dz / len;
        float ty = cy - ey;
        float tx = cx - ex;
        float tz = cz - ez;
        float proj = ppc_fmadds(dz, tz, ppc_fmadds(dx, tx, dy * ty));
        float py = ty - dy * proj;
        float px = tx - dx * proj;
        float pz = tz - dz * proj;
        float perp = ppc_fmadds(pz, pz, ppc_fmadds(px, px, py * py));
        float dist = 0.0f;
        bool hit = false;
        if (perp < 13456.0f) {
            float half = static_cast<float>(std::sqrt(static_cast<double>(13456.0f - perp)));
            dist = proj - half;
            if (!(dist >= 0.0010000000474974513f)) dist = proj + half;
            if (dist > 0.0010000000474974513f) hit = true;
        }
        CLBGlobeCityFlag* picked = nullptr;   // mF70 (Mac 0x70): stored as an arena offset below
        if (hit) {
            mHitX = ppc_fmadds(dist, dx, ex);
            mHitY = ppc_fmadds(dist, dy, ey);
            mHitZ = ppc_fmadds(dist, dz, ez);
            float best = 99999.0f;
            auto* node = mCityFlags->mHead;
            while (node != nullptr) {
                CLBGlobeCityFlag* flag = node->mData;
                node = node->mNext;
                const CFSOnlineCity* city = flag->mCity;
                float qy = city->mPosY - mHitY;
                float qx = city->mPosX - mHitX;
                float qz = city->mPosZ - mHitZ;
                float d = ppc_fmadds(qz, qz, ppc_fmadds(qx, qx, qy * qy));
                if (d < 100.0f && d < best) {
                    best = d;
                    picked = flag;
                }
            }
        }
        mF70 = picked != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(picked)) : 0;
        if (picked == nullptr)
            UpdateOnlineCityInfoOverlay(nullptr);
        else
            UpdateOnlineCityInfoOverlay(picked->mCity);
    }

    if (!gPlayerSettings.mCurrent.mRMBScrollOn) {
        float fx = Clamp01(static_cast<float>(lobby->mWidth) / 2048.0f);
        float fy = Clamp01(static_cast<float>(lobby->mHeight) / 1536.0f);
        float step = 50.0f * mTickScale;
        if (fx < 0.009999999776482582f) {
            mSpinXSpeed = mSpinXSpeed - step * (0.009999999776482582f - fx);
        } else if (fx > 0.9900000095367432f) {
            mSpinXSpeed = mSpinXSpeed + step * (fx - 0.9900000095367432f);
        }
        if (fy < 0.009999999776482582f) {
            mSpinYSpeed = mSpinYSpeed + step * (0.009999999776482582f - fy);
        } else if (fy > 0.9900000095367432f) {
            mSpinYSpeed = mSpinYSpeed - step * (fy - 0.9900000095367432f);
        }
    }

    if (gPlayerSettings.mCurrent.mRMBScrollOn && lobby->IsKeyDown(2) != 0) {
        mSpinXSpeed = mSpinXSpeed + mTickScale * (static_cast<float>(lobby->mWidth - 0x400) * 0.0009765625f);
        mSpinYSpeed = mSpinYSpeed - mTickScale * (static_cast<float>(lobby->mHeight - 0x300) / 768.0f);
    }

    mSpinXSpeed = mSpinXSpeed * mDamping;
    mSpinYSpeed = mSpinYSpeed * mDamping;
    mSpinX = ppc_fmadds(mSpinXSpeed, lobby->mTickUnits, mSpinX);
    mSpinY = ppc_fmadds(mSpinYSpeed, lobby->mTickUnits, mSpinY);
    if (mSpinY > mF40) {
        mSpinYSpeed = 0.0f;
        mSpinY = mF40;
    }
    if (mSpinY < -mF40) {
        mSpinYSpeed = 0.0f;
        mSpinY = -mF40;
    }

    float sizeTarget;
    float radiusTarget;
    if (mZoomedIn != 0) {
        sizeTarget = mF3C;
        radiusTarget = lobby->mGlobals->mGlobeRadiusIn;
    } else {
        sizeTarget = mF38;
        radiusTarget = lobby->mGlobals->mGlobeRadiusOut;
    }
    float radius = mRadius;
    mRadius = ppc_fmadds(lobby->mTickUnits, 0.30000001192092896f * (radiusTarget - radius), radius);
    float size = mSizeNow;
    mSizeNow = ppc_fmadds(lobby->mTickUnits, 0.30000001192092896f * (sizeTarget - size), size);
    size = mSizeNow;
    gLobbyView->mLightUpOffset = size;
    gLobbyView->mLightRightOffset = -size;
    gLobbyView->mLightDownward = 0;

    float cosX = static_cast<float>(std::cos(static_cast<double>(mSpinX)));
    float cosY = static_cast<float>(std::cos(static_cast<double>(mSpinY)));
    float sinX = static_cast<float>(std::sin(static_cast<double>(mSpinX)));
    float sinY = static_cast<float>(std::sin(static_cast<double>(mSpinY)));
    float r = mRadius;
    float rc = r * cosY;
    float ry = r * sinY;
    gLobbyView->mPosition.x = cosX * rc;
    gLobbyView->mPosition.y = ry;
    gLobbyView->mPosition.z = sinX * rc;
    gLobbyView->mTarget = gGlobeViewTarget;
}

// 0x100ad4cc: nothing
void CLBScreenComponentOnlineGlobe::Render() {}

void CLBScreenComponentOnlineGlobe::Close() {}

// 0x100ad54c: the arrow keys (message 0x1000a, 0x2500 left, 0x2600 up, 0x2700 right, 0x2800 down) and the zoom button
// (message 0x10006, argument 2).
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    (void)b;
    Word<int32_t> mZoomedIn{mF44 + 0x14};   // Mac 0x58
    if (message == 0x1000a) {
        if (a == 0x2700) {
            GlobeRight();
        } else if (a == 0x2600) {
            GlobeUp();
        } else if (a == 0x2500) {
            GlobeLeft();
        } else if (a == 0x2800) {
            GlobeDown();
        }
    } else if (message == 0x10006 && a == 2) {
        if (mZoomedIn != 0)
            GlobeZoomOut();
        else
            GlobeZoomIn();
        WorldLobbySetZoomButton(mZoomedIn);
    }
}

// 0x100ad690
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }

// 0x100ad6dc PostGetCityListCallbackFunction (the online globe's city list callback)

void PostGetCityListCallbackFunction(int error, const void* context, TbPtrList<CFSOnlineCity>* cities) {
    CLBScreenComponentOnlineGlobe* globe = static_cast<CLBScreenComponentOnlineGlobe*>(const_cast<void*>(context));
    if (error == 0)
        globe->CreateCities(cities);
    else
        globe->HandleOnlineBacktrack();
}

// The online globe's window: Show, Hide, zoom, city flags (0x100ad788 .. 0x100ae228).

// The globe's vtable is emitted with its key function, the destructor: src/lobby_globe_CLBGlobe_dtor.cpp (lead 2026-10-20, v51031).

namespace {
float ReadF(const uint8_t* p) {
    float f;
    std::memcpy(&f, p, 4);
    return f;
}
void WriteF(uint8_t* p, float f) { std::memcpy(p, &f, 4); }
void WriteI(uint8_t* p, int32_t v) { std::memcpy(p, &v, 4); }
}  // namespace

// mF0C + 4, + 8, + 12 are the Mac's 0x10, 0x14, 0x18; mF44 + 0x14 is 0x58 (zoomed in), mF44 + 0x24 is 0x68.
void CLBScreenComponentOnlineGlobe::Show() {
    if (IsVisible() != 0) return;
    mVisible = 1;
    WriteF(mF44 + 0x00, 0.0f);
    WriteF(mF44 + 0x04, 0.0f);
    WriteF(mF44 + 0x08, 0.0f);
    WriteF(mF44 + 0x0c, 0.0f);
    WriteF(mF44 + 0x10, gLobby->mGlobals->mGlobeRadiusOut);
    gLobbyView->mFov = 60.0f;
    gei_Rain->SetRaindrops(0);
    gLobbyView->SetFogOff();
    WriteI(mF44 + 0x14, 0);
    SortOutZoomConstants();
    gei_Display.mFlags |= 0x2000000;
    gLobbyView->SetGlobalAmbientLight(1, 0x20202000);
    uint32_t color = 0xffffff00;
    gLobbyView->SetGlobalDirectionalLight(1, &color, nullptr, 1);
    std::memcpy(mF34, &mF38, 4);
    float f = ReadF(mF34);
    gLobbyView->mLightUpOffset = f;
    gLobbyView->mLightRightOffset = -f;
    gLobbyView->mLightDownward = 0;
    gLobbyScreens->mAdvisor->mHasTimeout = 0;
    gLobbyView->mSkyTarget[0] = 0xf30000;
    gLobbyView->mSkyTarget[1] = 0xcb0000;
    gLobbyView->mSkyTarget[2] = 0xbf0000;
    mF74 = SOUND_SendEvent(0, gLobbyThunderBank, 2, 0, 0, 0);
    mF78 = SOUND_SendEvent(0, gLobbyThunderBank, 3, 0, 0, 0);
    CLBGlobe* globe = new CLBGlobe;
    globe->mMesh = gLobbyMeshes->InstanceMeshType("data:lobby:globe", "globe", 1, 1, nullptr, nullptr);
    globe->mMesh->Show();
    globe->mScreen = this;
    globe->mScreen->RegisterGlobe(globe);
    mGlobeObj = globe;
    WriteI(mF44 + 0x24, 0);
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->StopProcessing();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->mCityList->GetList(PostGetCityListCallbackFunction, this);
}

void CLBScreenComponentOnlineGlobe::Hide() {
    if (IsVisible() == 0) return;
    mVisible = 0;
    RemoveWorldLobbyController();
    DeleteGlobeListEntries();
    DeleteCityListEntries();
    gLobbyMeshes->DeleteMeshInstanceListEntries();
    gei_Display.mFlags &= 0xFDFFFFFFu;
    SOUND_FadeSoundEffect(mF74);
    SOUND_FadeSoundEffect(mF78);
    mF78 = 0;
    mF74 = 0;
}

// 0x100adb54: the offset at 0x48 less the step at 0x14 (single precision)
void CLBScreenComponentOnlineGlobe::GlobeLeft() { StoreF(&mF44[4], LoadF(&mF44[4]) - LoadF(&mF0C[8])); }
// 0x100adba8
void CLBScreenComponentOnlineGlobe::GlobeRight() { StoreF(&mF44[4], LoadF(&mF44[4]) + LoadF(&mF0C[8])); }
// 0x100adbfc: the offset at 0x50
void CLBScreenComponentOnlineGlobe::GlobeUp() { StoreF(&mF44[12], LoadF(&mF44[12]) + LoadF(&mF0C[8])); }
// 0x100adc50
void CLBScreenComponentOnlineGlobe::GlobeDown() { StoreF(&mF44[12], LoadF(&mF44[12]) - LoadF(&mF0C[8])); }

void CLBScreenComponentOnlineGlobe::GlobeZoomIn() {
    WriteI(mF44 + 0x14, 1);
    HideWorldLobbySats();
    SortOutZoomConstants();
}

void CLBScreenComponentOnlineGlobe::GlobeZoomOut() {
    WriteI(mF44 + 0x14, 0);
    ShowWorldLobbySats();
    SortOutZoomConstants();
}

void CLBScreenComponentOnlineGlobe::SortOutZoomConstants() {
    int32_t zoomed;
    std::memcpy(&zoomed, mF44 + 0x14, 4);
    if (zoomed != 0) {
        WriteF(mF0C + 4, mF28);
        WriteF(mF0C + 8, mF2C);
        WriteF(mF0C + 12, mF30);
    } else {
        WriteF(mF0C + 4, mF1C);
        WriteF(mF0C + 8, mF20);
        WriteF(mF0C + 12, mF24);
    }
}

void CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>* cities) {
    TbPtrList<CFSOnlineCity>::CNode* node = cities->mHead;
    while (node != nullptr) {
        CFSOnlineCity* city = node->mData;
        node = node->mNext;
        if (IsCityWanted(city) != 0) {
            CLBGlobeCityFlag* flag = new CLBGlobeCityFlag(this, city);
            (void)flag;
        }
    }
    WriteI(mF44 + 0x24, 1);
}

// Takes the globes off the list from the head, deleting each one through its virtual destructor.
void CLBScreenComponentOnlineGlobe::DeleteGlobeListEntries() {
    while (mGlobes->mCount != 0) {
        CLBGlobe* globe = mGlobes->RemoveHead();
        if (globe != nullptr) delete globe;
    }
}

// 0x100adff4
void CLBScreenComponentOnlineGlobe::Register(CLBGlobe* globe) { mGlobes->AddTail(globe); }

// 0x100ae064
void CLBScreenComponentOnlineGlobe::UnRegister(CLBGlobe* globe) {
    POSITION position = mGlobes->Find(globe);
    if (position != nullptr) mGlobes->RemoveAt(position);
}

// The same for the city flags.
void CLBScreenComponentOnlineGlobe::DeleteCityListEntries() {
    while (mCityFlags->mCount != 0) {
        CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
        if (flag != nullptr) delete flag;
    }
}

// Unit v51106: the online islands' small members and the globes' panel members (lobby screens).

void ShowOnlineCityInfoOverlay();       // 0x101e2380
void HideOnlineCityInfoOverlay();       // 0x101e23e8

// ---- the globes

// 0x100ae1b0
void CLBScreenComponentOnlineGlobe::Register(CLBGlobeCityFlag* flag) { mCityFlags->AddTail(flag); }

void CLBScreenComponentOnlineGlobe::UnRegister(CLBGlobeCityFlag* flag) {
    POSITION position = mCityFlags->Find(flag);
    if (position != nullptr) mCityFlags->RemoveAt(position);
}

// The download and publish globes each clear the word at +0x70 and do nothing else (not the base class's Init).
void CLBScreenComponentDownloadGlobe::Init() { mF70 = 0; }

// 0x100ae318
void CLBScreenComponentDownloadGlobe::Tick() { TickGlobe(); }

// CLBGlobe::~CLBGlobe (0x100ac534) is in src/lobby_globe_CLBGlobe_dtor.cpp: the globe's vtable comes with it, and unit v5923 links it too.

// 0x100ae374 (172 bytes): the online globe's handling, then message 0x10005 with a == 0 and a picked city flag (the word at 0x70, an
// arena offset): the selection becomes the flag's city, the globe hides and the online islands show.
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    CLBScreenComponentOnlineGlobe_WindowMessagesFn(this, message, a, b);
    if (message == 0x10005 && a == 0 && mF70 != 0) {
        CLBGlobeCityFlag* flag = reinterpret_cast<CLBGlobeCityFlag*>(tpw::gArenaBase + static_cast<uint32_t>(mF70));
        gOnlineParkSelection = reinterpret_cast<OnlineParkSelection*>(const_cast<CFSOnlineCity*>(flag->mCity));
        Hide();
        gLobbyScreens->mOnlineIslands->Show();
    }
}

// Unit v51046: nine small functions of the lobby, the interface and the ESP sprites (main app).
//   OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
//   CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
//   CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
//   CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.

namespace mainmenu_callee {
CFSOnlineSystem* NewOnlineSystem();        // operator new(0x404), then CFSOnlineSystem::CFSOnlineSystem 0x1016f7d0
}

// ---- the download globe

uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() {
    uint32_t answer = 0;
    // The Mac's "if (gOnlineSystem == 0) make it" is inlined twice (the second test can never be true).
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    if (gOnlineSystem->mF384 != 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
        gOnlineSystem->StopProcessing();
        ShowPanels();   // virtual slot 0x4c of the globe (0x100ae7b0 here: the download globe's vtable 0x1029e7dc)
        answer = 1;
    }
    return answer;
}

void CLBScreenComponentDownloadGlobe::Show() {
    if (IsVisible() == 0) {
        CreateWorldLobbyController(false);
        CreateOnlineCityInfoOverlay();
        CLBScreenComponentOnlineGlobe::Show();
        gLobbyScreens->mAdvisor->ClearAll();
    }
}

// 0x100ae63c: a visible screen (asked twice through slot 0x2c) is hidden: both lists emptied (the globes and flags deleted), the
// meshes' instances deleted, display flag 0x2000000 cleared, both sounds faded; the city info overlay goes either way.
void CLBScreenComponentDownloadGlobe::Hide() {
    if (IsVisible() != 0) {
        if (IsVisible() != 0) {
            mVisible = 0;
            RemoveWorldLobbyController();
            while (mGlobes->mCount != 0) {
                CLBGlobe* globe = mGlobes->RemoveHead();
                if (globe != nullptr) delete globe;
            }
            while (mCityFlags->mCount != 0) {
                CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
                if (flag != nullptr) delete flag;
            }
            gLobbyMeshes->DeleteMeshInstanceListEntries();
            gei_Display.mFlags &= ~0x02000000u;
            SOUND_FadeSoundEffect(mF74);
            SOUND_FadeSoundEffect(mF78);
            mF78 = 0;
            mF74 = 0;
        }
        RemoveOnlineCityInfoOverlay();
    }
}

// 0x100ae7b0
void CLBScreenComponentDownloadGlobe::ShowPanels() {
    ShowWorldLobbyController();
    ShowOnlineCityInfoOverlay();
    ShowWorldLobbySats();
}

// 0x100ae828
void CLBScreenComponentDownloadGlobe::HidePanels() {
    HideWorldLobbyController();
    HideOnlineCityInfoOverlay();
    HideWorldLobbySats();
}

// The publish globe's window messages, Show, Hide, CreateCities and HandleOnlineBacktrack, and the download globe's
// CreateCities and HandleOnlineBacktrack (unit v5957).

// 0x100ae8a0
void CLBScreenComponentDownloadGlobe::CreateCities(TbPtrList<CFSOnlineCity>* cities) {
    TbPtrList<CFSOnlineCity>::CNode* node = cities->mHead;
    while (node != nullptr) {
        CFSOnlineCity* city = node->mData;
        node = node->mNext;
        if (IsCityWanted(city) != 0) {
            CLBGlobeCityFlag* flag = new CLBGlobeCityFlag(this, city);
            (void)flag;
        }
    }
    WriteI(mF44 + 0x24, 1);
    AdvisorResponse response;
    response.mId = 0x256;
    gLobbyScreens->mAdvisor->Add(response, 1);
}
// 0x100ae9c0
void CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack() {
    Hide();
    gLobbyScreens->mThemeIslands->Show();
}

// 0x100aea58
int32_t CLBScreenComponentDownloadGlobe::IsCityValid(const CFSOnlineCity* city) { return city->mValue > 0 ? 1 : 0; }

void CLBScreenComponentPublishGlobe::Init() { mF70 = 0; }

// 0x100aeb10
void CLBScreenComponentPublishGlobe::Tick() { TickGlobe(); }

// 0x100aeb6c: the base class's messages first; then message 0x10005 with 0 chooses the city of the flag the word at 0x70 holds
// (a Mac pointer to a city flag: its word at 0x0c is the city) and opens the publish panel.
void CLBScreenComponentPublishGlobe::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    CLBScreenComponentOnlineGlobe_WindowMessagesFn(this, message, a, b);
    if (message == 0x10005 && a == 0 && mF70 != 0) {
        CLBGlobeCityFlag* flag =
            reinterpret_cast<CLBGlobeCityFlag*>(tpw::gArenaBase + static_cast<uint32_t>(mF70));   // a 32-bit field: an arena offset (Ptr32.h)
        gOnlineParkSelection = reinterpret_cast<OnlineParkSelection*>(const_cast<CFSOnlineCity*>(flag->mCity));
        CreatePublishParkPanel();
    }
}

// 0x100aec3c
void CLBScreenComponentPublishGlobe::Show() {
    if (IsVisible() == 0) {
        CreateWorldLobbyController(true);
        CreateOnlineCityInfoOverlay();
        CLBScreenComponentOnlineGlobe::Show();
        gLobbyScreens->mAdvisor->ClearAll();
    }
}

// 0x100aece8
void CLBScreenComponentPublishGlobe::Hide() {
    if (IsVisible() == 0) return;
    if (IsVisible() != 0) {
        mVisible = 0;
        RemoveWorldLobbyController();
        while (mGlobes->mCount != 0) {
            CLBGlobe* globe = mGlobes->RemoveHead();
            if (globe != nullptr) delete globe;
        }
        while (mCityFlags->mCount != 0) {
            CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
            if (flag != nullptr) delete flag;
        }
        gLobbyMeshes->DeleteMeshInstanceListEntries();
        gei_Display.mFlags &= 0xfdffffffu;
        SOUND_FadeSoundEffect(mF74);
        SOUND_FadeSoundEffect(mF78);
        mF78 = 0;
        mF74 = 0;
    }
    RemoveOnlineCityInfoOverlay();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->SetPublishing(0, nullptr, nullptr);
}

// 0x100aeeb0
void CLBScreenComponentPublishGlobe::ShowPanels() {
    ShowWorldLobbyController();
    ShowOnlineCityInfoOverlay();
}

// 0x100aef20
void CLBScreenComponentPublishGlobe::HidePanels() {
    HideWorldLobbyController();
    HideOnlineCityInfoOverlay();
}

// 0x100aef90
void CLBScreenComponentPublishGlobe::CreateCities(TbPtrList<CFSOnlineCity>* cities) {
    TbPtrList<CFSOnlineCity>::CNode* node = cities->mHead;
    while (node != nullptr) {
        CFSOnlineCity* city = node->mData;
        node = node->mNext;
        if (IsCityWanted(city) != 0) {
            CLBGlobeCityFlag* flag = new CLBGlobeCityFlag(this, city);
            (void)flag;
        }
    }
    WriteI(mF44 + 0x24, 1);
}

// 0x100af094
void CLBScreenComponentPublishGlobe::HandleOnlineBacktrack() {
    Hide();
    gLobbyScreens->mThemeIslands->Show();
    gLobbyScreens->mThemeIslands->EnterCurrentIsland();
}

// 0x100af148: a city that is not full (its flag is 0 and its value is below the limit at 0x64)
int32_t CLBScreenComponentPublishGlobe::IsCityValid(const CFSOnlineCity* city) {
    if (city->mF60 != 0) return 0;
    return city->mValue < city->mF64 ? 1 : 0;
}

// 0x100af1c8: the base destructor's text is inlined (both lists emptied, destroyed and cleared); CLBScreenComponent's destructor
// follows (the compiler's, after this body).
CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe() {
    while (mGlobes != nullptr && mGlobes->mCount != 0) {   // Lead 2026-10-20: NULL lists are left alone, as src/lobby_globe_close.cpp
        CLBGlobe* globe = mGlobes->RemoveHead();
        if (globe != nullptr) delete globe;
    }
    if (mGlobes != nullptr) delete mGlobes;
    mGlobes = nullptr;
    while (mCityFlags != nullptr && mCityFlags->mCount != 0) {
        CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
        if (flag != nullptr) delete flag;
    }
    if (mCityFlags != nullptr) delete mCityFlags;
    mCityFlags = nullptr;
}

// 0x100af330: the destructor takes the online globe's lists apart itself (the Mac's compiler inlined the base destructor's body):
// every globe and every city flag is removed and deleted, then each list is deleted and cleared. QUIRK: a NULL list is read
// unconditionally by the loops. The base destructor of CLBScreenComponent follows (0x100b6c2c, the C++ chain), the delete flag's
// operator delete is the deleting form.
CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe() {
    while (mGlobes->mCount != 0) {
        CLBGlobe* globe = mGlobes->RemoveHead();
        if (globe != nullptr) delete globe;
    }
    delete mGlobes;
    mGlobes = nullptr;
    while (mCityFlags->mCount != 0) {
        CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
        if (flag != nullptr) delete flag;
    }
    delete mCityFlags;
    mCityFlags = nullptr;
}

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

// ---- the static initialisers: the wide strings the files hold (Mac 0x1023e9f0 and so on), the objects and the nodes

namespace {
using W = TbDynamicStringTemplate<char16_t>;
}  // namespace

alignas(16) unsigned char gV51077GlobeW48d8[sizeof(W)];    // 0x102e48d8
unsigned char gV51077GlobeNode48ec[16];                     // 0x102e48ec
void __sinit_lobby_globe_cpp() {
    void* object = new (static_cast<void*>(gV51077GlobeW48d8)) W(gGlobeNoCityName + 11);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077GlobeNode48ec);
}
