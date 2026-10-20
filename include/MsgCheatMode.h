// CMsgCheatMode: PARTIAL. Destructor at 0x10147050, GetType at 0x101472bc.
// Constructor at 0x10151ec8 belongs to untranslated message code.
#pragma once
#include "CMsg.h"

class CMsgCheatMode : public CMsg {
public:
    explicit CMsgCheatMode(bool cheating);
    ~CMsgCheatMode() override;
    int32_t GetType() const override;

    uint8_t mCheating;   // 0x08  byte payload (native vtable changes host offsets)
};
