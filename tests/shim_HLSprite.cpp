#include "HLSprite.h"
extern "C" {
void* tpw_HLSprite_element(int i) { return &gHLSpriteLists[i]; }
void tpw_HLSprite_seed(const int32_t* values) {
    for (int i = 0; i < 16; ++i) gHLSpriteLists[i].mCount = values[i];
}
void tpw_HLSprite_read(int32_t* values) {
    for (int i = 0; i < 16; ++i) values[i] = gHLSpriteLists[i].mCount;
}
void tpw_HLSprite_arraydtor() {
    for (int i = 15; i >= 0; --i) gHLSpriteLists[i].~CSpriteList();
}
}
