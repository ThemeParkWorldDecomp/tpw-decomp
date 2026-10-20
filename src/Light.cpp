#include "Light.h"

CLight::CLight() : mFlags(0) {}
CLight::~CLight() = default;
int32_t CLight::GetLinkType() { return 1; }
void* CLight::GetData() { return (mFlags & 0xf) == 2 ? mData : nullptr; }
int32_t CLight::Prepare() {
    if ((mFlags & 0x10000000) == 0) {
        mNext = gev_LightChain;
        gev_LightChain = this;
    }
    return 1;
}
