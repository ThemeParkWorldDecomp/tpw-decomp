// The advisor's screen in the lobby (main app of SimTheme Park 2.04b3, 0x100a6c98..0x100a76ac;
// the inventory files the range under lobby_globe.cpp): CLBScreenComponentAdvisor and its
// list of responses.
//
// Lines for the advisor are added at the head of a list and said from the tail, one a
// tick, and only while he is silent. When a timeout line is set and nothing has been said
// for its time, the SCREEN SYSTEM'S advisor screen is given that line (normally this one).
#include "lobby.h"

#include "LbTime.h"

template class TbTList<AdvisorResponse*, AdvisorResponse*>;

// The responses still in the list are not freed (the list frees its nodes only).
CLBScreenComponentAdvisor::~CLBScreenComponentAdvisor() {}

void CLBScreenComponentAdvisor::Init() {
    mHasTimeout = 0;
    mTimeout = 0;
    mLastTime = 0;
    mSpoke = 0;
}

void CLBScreenComponentAdvisor::Tick() {
    if (gAdvisorState.mSpeaking != 0) {
        mSpoke = 1;
        return;
    }
    if (mResponses.mCount == 0) {
        if (mHasTimeout != 0) {
            const uint32_t now = LbTime_GetClock();
            if (mSpoke == 0) {
                if (mLastTime + mTimeout < now) {
                    mLastTime = now;
                    gLobbyScreens->mAdvisor->Add(mTimeoutResponse, 0);
                }
            } else {
                mLastTime = now;
            }
        }
        mSpoke = 0;
    } else {
        AdvisorResponse* response = mResponses.RemoveTail();
        ADV_Call(*response);
        delete response;
        mSpoke = 1;
    }
}

void CLBScreenComponentAdvisor::Render() {}
void CLBScreenComponentAdvisor::Close() { ClearAll(); }
void CLBScreenComponentAdvisor::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentAdvisor::ProcessCancel() { return 0; }

void CLBScreenComponentAdvisor::Show() {
    if (IsVisible() == 0) mVisible = 1;
}

void CLBScreenComponentAdvisor::Hide() {
    if (IsVisible() != 0) mVisible = 0;
}

void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    if (clearFirst != 0) ClearAll();
    AdvisorResponse* copy = new AdvisorResponse(response);
    mResponses.AddHead(copy);
}

// Cuts him short when he is talking, and forgets every line still waiting.
void CLBScreenComponentAdvisor::ClearAll() {
    if (gAdvisorState.mSpeaking != 0) ADV_Slap();
    while (mResponses.mCount != 0) delete mResponses.RemoveHead();
}

void CLBScreenComponentAdvisor::SetTimeoutResponse(AdvisorResponse response, uint32_t milliseconds) {
    mHasTimeout = 1;
    mTimeout = milliseconds;
    mTimeoutResponse = response;
}
