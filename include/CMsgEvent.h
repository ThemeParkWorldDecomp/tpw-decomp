// CMsgEvent: event message (constructor 0x10151df8; the inventory files it under RideDB.cpp,
// the out-of-line destructor 0x100f39cc and GetType 0x100f4470 under BlockSave.cpp). The
// destructor is inline where the type is known (GAMESYSTEM_Tick only resets the vtable
// pointer). All three are translated in src/MessageCentre_post.cpp (unit w107); its vtable
// holds only GetType and the destructor.
#pragma once
#include <cstdint>

#include "CMsg.h"

enum EventType : int32_t {};

class CMsgEvent : public CMsg {
public:
    explicit CMsgEvent(EventType type);
    ~CMsgEvent() override;
    int32_t GetType() const override;

    int32_t mEvent;   // 0x08 on the Mac (native vtable changes host offsets)
};
