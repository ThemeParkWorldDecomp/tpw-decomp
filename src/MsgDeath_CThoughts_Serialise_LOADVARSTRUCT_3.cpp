#include "CMsgDeath.h"
#include "ThingReceivers.h"
#include "Serialise.h"
#include "World.h"
#include "FS_Online.h"
#include "ESP_sprites.h"
#include "analyser.h"
#include "Debug.h"
#include "TPCamera.h"
#include "LbSCBack.h"
#include "SaveLoad.h"
#include "LbFile.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "ThingMessages.h"
#include "platform/samsutils.h"

TagTableElement gTagTable[0x17e];   // 0x102a12c0
int32_t gTbErrorZero;               // 0x102a4400

namespace {

const char kNotFound[] = "CTagTable - not found %d\n";                    // 0x10246616
const char kTextA[] = "TAG SYSTEM says: Postcard sent\n";                                     // 0x10246451
const char kTextB[] = "TAG SYSTEM says: Postcard to outbox\n";                                      // 0x10246471
const char kTextC[] = "TAG SYSTEM says: Postcard failed\n";                                      // 0x10246496
const char kBadMessage[] = "CTagSystem::ReceiveMessage has encountered an unknown (to it) message type (%d).\n";                                  // 0x102464b8

int16_t ElementShort(const TagTableElement* e) { return *reinterpret_cast<const int16_t*>(&e->mF00[2]); }

}  // namespace

// 0x1012f5a8
template<>
int32_t LOADVARSTRUCT<CThoughts::EventHistType>(void* file, CThoughts::EventHistType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    TbError result = LbFile_Read(file, value, 4, &read);
    // The comparison against the TbError constant: (result >= 0) == (constant == 0).
    if (result.Succeeded() != (gTbErrorZero == 0) || read != 4) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should be a struct");
    // The two halves from the file's order (little-endian), as SAVEVARSTRUCT<EventHistType> writes them: the Mac swaps each, the
    // host reads them as they are (an unconditional swap here read a host-written save's history back byte-swapped; v5L15).
    value->mFirst = serialise::UnswapWord(value->mFirst);
    value->mSecond = serialise::UnswapWord(value->mSecond);
    return 0;
}

// 0x1012dd94
const TagTableElement* CTagTable::GetElement(int32_t id) const {
    if (static_cast<uint32_t>(id) - 0x17e < 2) return nullptr;
    const TagTableElement* element = reinterpret_cast<const TagTableElement*>(reinterpret_cast<const uint8_t*>(gTagTable) + static_cast<int64_t>(id) * 0x20);
    if (id != static_cast<int32_t>(element->mF00[0])) {
        for (int32_t i = 0; i < 0x17e; i++) {
            element = &gTagTable[i];
            if (id == static_cast<int32_t>(element->mF00[0])) return element;
        }
        CTRACE(3, kNotFound, id);
        return nullptr;
    }
    return element;
}

// 0x1012c90c
void CTagSystem::ReceiveMessage(const CMsg& message) {
    CTagParams params;
    params.mId = 0x17f;
    params.mF4 = 0x17f;
    params.mText = nullptr;
    params.mTime = 0;
    params.mFrame = 0;
    params.mF14 = 0;
    params.mF18 = 0;
    params.mF1C = 0;
    params.mF20 = 0;
    params.mShow = 0;
    char16_t* text = reinterpret_cast<char16_t*>(mF0C);
    const CTagTable* table = reinterpret_cast<const CTagTable*>(mF0C + 0x800);   // this + 0x80c

    switch (message.GetType()) {
    case 0x18: {
        const CMsgTagShow& m = static_cast<const CMsgTagShow&>(message);
        int32_t id = m.mTag;
        const TagTableElement* element = table->GetElement(id);
        if (element == nullptr) return;
        uint16_t thing = m.mThing;
        int32_t time = m.mTime;
        if (time < 0) time = static_cast<int32_t>(element->mF10[2]);
        params.mId = static_cast<uint32_t>(id);
        params.mF4 = static_cast<int32_t>(element->mF00[1]);
        params.mFrame = ElementShort(element);
        FormatTagText(element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{thing}, m.mValue, nullptr);
        params.mText = text;
        params.mTime = time;
        params.mF1C = thing;
        params.mF14 = static_cast<int32_t>(element->mF10[0]);
        params.mF18 = static_cast<int32_t>(element->mF10[1]);
        params.mF20 = static_cast<int32_t>(m.mValue);
        gUIInterface.MessageTag(params);
        return;
    }
    case 0x19: {
        const CMsgChallenge& m = static_cast<const CMsgChallenge&>(message);
        int32_t id = m.mTagId;
        const TagTableElement* element = table->GetElement(id);
        if (element == nullptr) return;
        params.mTime = static_cast<int32_t>(element->mF10[2]);
        params.mId = static_cast<uint32_t>(id);
        params.mF4 = static_cast<int32_t>(element->mF00[1]);
        params.mFrame = ElementShort(element);
        FormatTagText(element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, &m);
        params.mText = text;
        params.mF14 = static_cast<int32_t>(element->mF10[0]);
        params.mF18 = static_cast<int32_t>(element->mF10[1]);
        gUIInterface.MessageTag(params);
        return;
    }
    case 0x13: {
        int32_t code = static_cast<const CMsgTagSimple&>(message).mCode;
        int32_t id = 0x17f;
        if (code == 8) {
            SamsUtilities::UDebug(kTextB);
            id = 0x17c;
        } else if (code < 8) {
            if (code >= 7) {
                SamsUtilities::UDebug(kTextA);
                id = 0x17b;
            }
        } else if (code < 10) {
            SamsUtilities::UDebug(kTextC);
            id = 0x17d;
        }
        if (id == 0x17f) return;
        const TagTableElement* element = table->GetElement(id);
        if (element == nullptr) return;
        params.mTime = static_cast<int32_t>(element->mF10[2]);
        params.mId = static_cast<uint32_t>(id);
        params.mF4 = static_cast<int32_t>(element->mF00[1]);
        params.mFrame = ElementShort(element);
        FormatTagText(element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, nullptr);
        params.mText = text;
        params.mF14 = static_cast<int32_t>(element->mF10[0]);
        params.mF18 = static_cast<int32_t>(element->mF10[1]);
        gUIInterface.MessageTag(params);
        return;
    }
    default:
        TRACE(kBadMessage, message.GetType());
        return;
    }
}
