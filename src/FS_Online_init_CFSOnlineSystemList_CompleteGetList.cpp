// CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::CompleteGetList 0x10174410 (468): the ride instance of the city's 0x10173f98.
#include <new>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "LbTime.h"
#include "platform/samsutils.h"

typedef CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList> RideBase;

void (*gRideRecordDeleter)(CFSOnlineRide* ride) = [](CFSOnlineRide* ride) { delete ride; };

template<> void RideBase::CompleteGetList(int32_t error) {
    bool hadError = mF18 != 0;
    if (hadError) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (error == 0 && !hadError && mF14 != 0) {
        while (mList->GetCount() != 0) {
            CFSOnlineRide* ride = mList->RemoveHead();
            if (ride != nullptr) gRideRecordDeleter(ride);
        }
        mF24 = 0;
        mF28 = 0;
        mF28 = 1;
        if (mReceived != nullptr) {
            StoreReceivedList(mReceived);
            mF24 = mReceived->mF08;
            ccRideList* received = mReceived;
            if (received != nullptr) {
                --received->mRefCount;
                if (received->mRefCount == 0) {
                    if (received->mInfos != nullptr) delete[] received->mInfos;
                    received->mInfos = nullptr;
                    ::operator delete(static_cast<void*>(received));
                }
                mReceived = nullptr;
            }
        }
    }
    int32_t result = 0;
    if (error != 0 || hadError || mF28 == 0) {
        if (mF08 != 0) {
            if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
            gOnlineSystem->Logout();
        }
        result = 1;
    }
    if (callback != 0) gOnlineListCallbackCaller(callback, result, context, mList);
}
