// Test library for src/lobby_advisor.cpp (libtpw_unit_LobbyAdvisor.dylib; see
// tests/test_LobbyAdvisor.py): the lobby's advisor screen, driven by scripts over two such
// screens (the timeout line goes to the screen system's advisor, which the test can make
// the other one).
//
// Real: CLBScreenComponentAdvisor and its list. Fakes: the advisor (ADV_Call and ADV_Slap
// record), the clock, the screen base class (no registering here; its destructor records).
#include <cstdlib>
#include <cstring>
#include <new>

#include "LbTime.h"
#include "calllog.h"
#include "lobby.h"

using namespace calllog;

namespace {
alignas(16) unsigned char gStores[2][sizeof(CLBScreenComponentAdvisor)];
CLBScreenComponentAdvisor* gAdvisors[2];
bool gDestroyed[2];
alignas(16) unsigned char gSystemStore[sizeof(CLBScreenSystem)];
}  // namespace

SAdvisorState gAdvisorState;
CLBScreenSystem* gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(gSystemStore);
CLBScreenComponent::CLBScreenComponent() : mVisible(0) {}
CLBScreenComponent::~CLBScreenComponent() { ev("~CLBScreenComponent"); }
void CLBScreenComponent::Show() { mVisible = 1; }
void CLBScreenComponent::Hide() { mVisible = 0; }
int32_t CLBScreenComponent::IsVisible() { return mVisible; }
int32_t ADV_Call(AdvisorResponse response) {
    ev("ADV_Call", {response.mId});
    return 0;
}
void ADV_Slap() { ev("ADV_Slap"); }
uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock");
    return static_cast<uint32_t>(ret("Clock", 0));
}

extern "C" {
void tpw_la_begin() {
    for (int i = 0; i < 2; ++i) {
        std::memset(gStores[i], 0xa5, sizeof gStores[i]);
        gAdvisors[i] = new (gStores[i]) CLBScreenComponentAdvisor;
        gDestroyed[i] = false;
    }
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
    gAdvisorState.mSpeaking = 0;
    gLobbyScreens->mAdvisor = gAdvisors[0];
}

int64_t tpw_la_op(int32_t op, const int32_t* a) {
    CLBScreenComponentAdvisor* advisor = gAdvisors[a[0] & 1];
    if (op != 11 && op != 12 && gDestroyed[a[0] & 1]) return -98;
    switch (op) {
    case 1: advisor->Init(); return 0;
    case 2: advisor->Tick(); return 0;
    case 3: advisor->Add(AdvisorResponse{a[1]}, a[2]); return 0;
    case 4: advisor->ClearAll(); return 0;
    case 5: advisor->SetTimeoutResponse(AdvisorResponse{a[1]}, static_cast<uint32_t>(a[2])); return 0;
    case 6: static_cast<CLBScreenComponent*>(advisor)->Show(); return 0;
    case 7: static_cast<CLBScreenComponent*>(advisor)->Hide(); return 0;
    case 8: return static_cast<CLBScreenComponent*>(advisor)->IsVisible();
    case 9: static_cast<CLBScreenComponent*>(advisor)->Close(); return 0;
    case 10: {
        CLBScreenComponent* screen = advisor;
        screen->Render();
        screen->WindowMessagesFn(a[1], a[2], a[3]);
        return screen->ProcessCancel();
    }
    case 11: gAdvisorState.mSpeaking = a[1]; return 0;
    case 12: gLobbyScreens->mAdvisor = gAdvisors[a[1] & 1]; return 0;
    case 13: advisor->mVisible = a[1]; return 0;
    case 14:
        advisor->~CLBScreenComponentAdvisor();
        gDestroyed[a[0] & 1] = true;
        return 0;
    }
    return -99;
}

// Both screens (a destroyed one is left out): visible, the waiting lines from the head, the
// timeout fields. Then the blocks operator new and delete have handled since the start.
int32_t tpw_la_state(int64_t* out) {
    int64_t* p = out;
    for (int i = 0; i < 2; ++i) {
        const CLBScreenComponentAdvisor* advisor = gAdvisors[i];
        if (gDestroyed[i]) continue;
        *p++ = advisor->mVisible;
        *p++ = advisor->mResponses.mCount;
        int32_t n = 0;
        for (const auto* node = advisor->mResponses.mHead; node != nullptr && n < 40; node = node->mNext, ++n) *p++ = node->mData->mId;
        *p++ = advisor->mHasTimeout;
        *p++ = advisor->mTimeout;
        *p++ = advisor->mLastTime;
        *p++ = advisor->mTimeoutResponse.mId;
        *p++ = advisor->mSpoke;
    }
    *p++ = tpw_cl_heap[0];
    *p++ = tpw_cl_heap[2];
    return static_cast<int32_t>(p - out);
}
}
