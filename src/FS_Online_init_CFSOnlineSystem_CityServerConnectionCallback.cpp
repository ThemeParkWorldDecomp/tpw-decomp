// CFSOnlineSystem::CityServerConnectionCallback 0x101727f8 (2252): the city server's answer to one of the nine list requests.
#include <cstdint>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

namespace {

// The release of one reference to a received answer (inlined on the Mac): the count drops, and at zero the infos are destroyed (last to
// first) and freed and the answer itself freed with operator delete only.
template <class R>
void DropReceived(R* received) {
    --received->mRefCount;
    if (received->mRefCount == 0) {
        if (received->mInfos != nullptr) delete[] received->mInfos;
        received->mInfos = nullptr;
        ::operator delete(static_cast<void*>(received));
    }
}

// The list takes the answer: the old one is released first (the pointer cleared), then the answer stored, the answer flags set.
template <class LIST, class R>
void Store(LIST* list, R* received) {
    R* old = list->mReceived;
    if (old != nullptr) {
        DropReceived(old);
        list->mReceived = nullptr;
    }
    list->mReceived = received;
    list->mF14 = 1;
    list->mF10 = 1;
}

// The failure: the list whose request id is the one answered gets the error code.
template <class LIST>
bool Fail(LIST* list, int32_t requestID, int32_t code) {
    if (requestID != list->mRequestID) return false;
    list->mF18 = code;
    list->mF14 = 0;
    list->mF10 = 1;
    return true;
}

}  // namespace

// a is the answer code (-10 to 8), b the request id, d the received answer (a counted block).
void CFSOnlineSystem::CityServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    (void)c;
    int32_t failed = 0;
    int32_t code = 0;
    uint32_t index = static_cast<uint32_t>(a + 10);
    if (index <= 0x12) {
        switch (a) {
        case 2: {   // the city list
            ccReceivedList<ccCityInfo>* received = static_cast<ccReceivedList<ccCityInfo>*>(d);
            if (b == static_cast<int32_t>(mCityList->mRequestID)) {
                SamsUtilities::UDebug("ONLINE CITY: City data downloaded ok\n");
                Store(mCityList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown city list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 3: {   // the park lists
            ccReceivedList<ccParkInfo>* received = static_cast<ccReceivedList<ccParkInfo>*>(d);
            if (b == mParksTop10->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Top 10 parks data downloaded ok\n");
                Store(mParksTop10, received);
            } else if (b == mParksInCity->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: All parks data downloaded ok\n");
                Store(mParksInCity, received);
            } else if (b == mParksByPlayer->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Player parks data downloaded ok\n");
                Store(mParksByPlayer, received);
            } else if (b == mParksByID->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: ParkInfo parks data downloaded ok\n");
                Store(mParksByID, received);
            } else if (b == mParksInCity2->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Chat Satellites parks data downloaded ok\n");
                Store(mParksInCity2, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown park data downloaded - ignoring\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 5: {   // the theme list
            ccReceivedList<ccThemeInfo>* received = static_cast<ccReceivedList<ccThemeInfo>*>(d);
            if (b == mThemeList->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Theme list downloaded OK\n");
                Store(mThemeList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown theme list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 4: {   // the server list
            ccReceivedList<ccServerInfo>* received = static_cast<ccReceivedList<ccServerInfo>*>(d);
            if (b == mServerList->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Server list downloaded OK\n");
                Store(mServerList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown server list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 6: {   // the ride list
            ccReceivedList<ccRideInfo>* received = static_cast<ccReceivedList<ccRideInfo>*>(d);
            if (b == mRideList->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Ride list downloaded OK\n");
                Store(mRideList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown ride list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case -10: code = 0x56; failed = 1; break;
        case -6: code = 0x4e; failed = 1; break;
        case -5: code = 0x4d; failed = 1; break;
        case -4: code = 0x4c; failed = 1; break;
        case -2: code = 0x4b; failed = 1; break;
        case -1: code = 0x37; failed = 1; break;
        case 7:
            SamsUtilities::UDebug("ONLINE CITY: CRC match\n");
            failed = 1;
            break;
        case 8:
            SamsUtilities::UDebug("ONLINE CITY: No CRC match\n");
            break;
        default:
            break;
        }
    }
    if (failed != 0) {
        if (Fail(mCityList, b, code)) return;
        if (Fail(mParksTop10, b, code)) return;
        if (Fail(mParksInCity, b, code)) return;
        if (Fail(mParksByPlayer, b, code)) return;
        if (Fail(mParksByID, b, code)) return;
        if (Fail(mParksInCity2, b, code)) return;
        if (Fail(mThemeList, b, code)) return;
        if (Fail(mServerList, b, code)) return;
        Fail(mRideList, b, code);
    }
}
