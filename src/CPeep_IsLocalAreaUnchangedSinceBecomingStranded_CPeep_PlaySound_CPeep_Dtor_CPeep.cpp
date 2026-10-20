// CPeep (Peep.cpp, Person.cpp): the destructor, the sprite members and FinalDestroy. The peep is reached by Mac offset.
#include <cstddef>
#include <cstdint>

#include "ThingReceivers.h"
#include "World.h"
#include "TPCamera.h"
#include "analyser.h"

namespace {
uint8_t* PBytes(void* p) { return static_cast<uint8_t*>(p); }
uint32_t& PWord(void* p, std::size_t off) { return *reinterpret_cast<uint32_t*>(PBytes(p) + off); }
CDelayedScript* ScriptOf(void* p) { return reinterpret_cast<CDelayedScript*>(PBytes(p) + 0xc); }
CThoughts* ThoughtsOf(void* p) { return reinterpret_cast<CThoughts*>(PBytes(p) + 0x30); }
}  // namespace

// 0x10116d18: destroys the vehicle at +0xd4. The Mac's delete-storage flag (r4) is not modelled.
CPeep::~CPeep() {
    reinterpret_cast<Vehicle*>(PBytes(this) + 0xd4)->~Vehicle();
}

// 0x10114758
void CPeep::HideMySprite() {
    ScriptOf(this)->KillSpriteScript();
    ThoughtsOf(this)->RemoveThoughtBubble();
    if (PBytes(this)[2] == 1 && PWord(this, 0x210) != 0) reinterpret_cast<CPerson*>(this)->MakeBalloonPop();
}

// 0x10114624
void CPeep::DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script) {
    if (PWord(this, 0xc) == 0)
        ScriptOf(this)->SpawnSpriteScript(script, static_cast<int32_t>(PWord(this, 0x24)), static_cast<int32_t>(PWord(this, 0x20)));
    float height = DetermineHFieldHeight(x, z, 1);
    float lifted = y - height;
    SCR_SetScriptPosition(static_cast<int32_t>(PWord(this, 0xc)), x, lifted, z, frame);
    if (!ScriptOf(this)->IsRunningThisScript(script)) ScriptOf(this)->SetSpriteScript(script);
}

// 0x101130ac
void CPeep::FinalDestroy() {
    reinterpret_cast<CThing*>(this)->FinalDestroy();
    if (PWord(this, 0xc) != 0) ScriptOf(this)->KillSpriteScript();
    ReportAllEvents();
    ThoughtsOf(this)->FinalDestroy();
}
