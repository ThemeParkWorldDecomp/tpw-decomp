// CLBGlobeCityFlag (lobby_globe.cpp): a city's flag on the online globe. 0x100ac5c8 constructor, 0x100ac86c destructor.
#include <cmath>
#include <cstdint>

#include "FS_Online.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "ppc_math.h"

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
