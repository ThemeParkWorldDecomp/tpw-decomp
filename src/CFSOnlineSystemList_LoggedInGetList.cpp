// CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>: LoggedInGetList 0x10178414 (412) and CompleteGetList 0x10173f98 (468).
#include <cstdint>

#include "FS_Online.h"
#include "FS_OnlineList.h"

namespace {

void CallListCallbackDefault(int32_t callback, int32_t error, int32_t context, void* list) {
    typedef void (*Fn)(int32_t, const void*, void*);
    Fn fn = reinterpret_cast<Fn>(static_cast<uintptr_t>(static_cast<uint32_t>(callback)));
    fn(error, reinterpret_cast<const void*>(static_cast<uintptr_t>(static_cast<uint32_t>(context))), list);
}

void DeleteCityRecordDefault(CFSOnlineCity* city) { delete city; }

typedef CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList> CityBase;

}  // namespace

void (*gOnlineListCallbackCaller)(int32_t, int32_t, int32_t, void*) = CallListCallbackDefault;
void (*gCityRecordDeleter)(CFSOnlineCity*) = DeleteCityRecordDefault;

template<> void CityBase::LoggedInGetList() {
    if (ChildLocked() == 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        City* city = gOnlineSystem->ReturnConnectionToCityServer();
        if (city != nullptr) {
            mF0C = 1;
            mRequestID = RequestListFromServer(city, static_cast<uint32_t>(mF24));
            return;
        }
        mF18 = 0x4f;
    } else {
        mF18 = 0x4e;
    }
    // The failure: the same tail twice in the original.
    if (mF18 != 0) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (mF08 != 0) CFSOnlineSystem::Instance()->Logout();
    if (callback != 0) gOnlineListCallbackCaller(callback, 1, context, mList);
}

template<> void CityBase::CompleteGetList(int32_t error) {
    bool hadError = mF18 != 0;
    if (hadError) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (error == 0 && !hadError && mF14 != 0) {
        while (mList->GetCount() != 0) {
            CFSOnlineCity* city = mList->RemoveHead();
            if (city != nullptr) gCityRecordDeleter(city);
        }
        mF24 = 0;
        mF28 = 0;
        mF28 = 1;
        if (mReceived != nullptr) {
            StoreReceivedList(mReceived);
            mF24 = mReceived->mF08;
            ccCityList* received = mReceived;
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
