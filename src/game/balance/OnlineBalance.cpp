// OnlineBalance.cpp: name proven by __sinit_OnlineBalance_cpp in the main application.

#include "OnlineBalance.h"

// OnlineBalance.cpp (SimTheme Park 2.04b3).
// Static initializer 0x100213fc; emitted members 0x10021480, 0x100214cc, 0x10021510.

// Typed description rows at 0x10298cbc, ending with type 12.
const BalanceDescription gOnlineBalanceDescriptions[16] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "MailPort", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "ChatPort", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "LoginPort", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CityPort", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "NewsPort", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "VotePort", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "MailServer", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "ChatServer", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "LoginServer", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "CityServer", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "NewsServer", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "VoteServer", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "ComplainEmail", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "OnlineInfo", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* COnlineBalance::GetBalanceDescription(int32_t row) {
    return &gOnlineBalanceDescriptions[row];
}
BalanceValue* COnlineBalance::GetBalanceValuesStart() { return mValues; }
COnlineBalance::~COnlineBalance() { UnInit(); }

COnlineBalance gOnlineBalance; // __sinit_OnlineBalance_cpp at 0x100213fc, object at 0x102b4d5c
