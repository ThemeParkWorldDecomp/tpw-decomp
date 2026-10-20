// The advisor message (unit w180, src/MsgAdvisor.cpp): CMsgAdvisor (type 5) carries one CAdvisorRequest to the advisor through the message
// centre; CGoldenTicketControl::TellIfFirst makes two of them. Like CMsgWeather (include/ThingMessages.h) it has the constructor, GetType and
// destructor of CMsgDeath: the Mac's destructor only stores the vtable and, for a positive (signed 16 bit) delete flag, calls operator delete. The
// Mac vtables (base 0x102a10ec, CMsgAdvisor 0x102a09a4) are never used by the host. The receiver, CAdvisor::ReceiveMessage (0x1000c874), is untranslated.
#pragma once
#include <cstdint>

#include "AdvisorQueue.h"
#include "CMsg.h"

class CMsgAdvisor : public CMsg {
public:
    explicit CMsgAdvisor(const CAdvisorRequest& request);   // 0x1015190c (80 bytes): flags 0 and a copy of the request
    int32_t GetType() const override;                       // 0x100fd9cc (8 bytes), answers 5; native vtable key function
    ~CMsgAdvisor() override;                                // 0x100fcea8 (76 bytes)
    CAdvisorRequest mRequest;                               // 0x08 (0x18 bytes)
};
