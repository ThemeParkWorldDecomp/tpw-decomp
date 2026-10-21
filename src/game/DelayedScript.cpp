// DelayedScript.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of DelayedScript code, named after it.

#include "analyser.h"
#include "ESP_sprites.h"
#include "ppc_math.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include <cstdint>

// CDelayedScript (main app, BlockSave.cpp range of the inventory; unit w177): a handle to a sprite script plus the script and service interval to switch to
// at the next ProcessSpriteScripts. CDelayedScript 0x100fa898, Serialise 0x100fa8d8, SetSpriteScript 0x100faa74, IsRunningThisScript 0x100faad0,
// SpawnSpriteScript 0x100fab9c, KillSpriteScript 0x100fac4c, ProcessSpriteScripts 0x100facc0, SetServiceInterval 0x100fad70 and the LOADVAR / SAVEVAR
// instances for ScriptIdx 0x100fade4 / 0x100faf30.

// LOADVAR / SAVEVAR of the 32-bit ScriptIdx enum: the same bodies as the int instances. Declared before Serialise below uses them.
template<> int32_t LOADVAR<CDelayedScript::ScriptIdx>(void* file, CDelayedScript::ScriptIdx* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100fade4, 236 bytes
}
template<> int32_t SAVEVAR<CDelayedScript::ScriptIdx>(void* file, CDelayedScript::ScriptIdx* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100faf30, 260 bytes
}

namespace {
// The names at 0x102411d4 (TOC word -0x4e5c) plus the offset in the comment.
const char kSpriteScriptName[] = "mSpriteScript";               // +0x00
const char kNextAnimName[] = "mNextAnim";                       // +0x0e
const char kNextServiceIntervalName[] = "mNextServiceInterval"; // +0x18

constexpr int32_t kScriptTableSize = 13;
// The table at 0x102a08fc (TOC word -0x4e60): the Mac code address of each script's start in the script code array (gScriptCodeWords), 0 for the first.
// The ESP_ calls read a code argument of 0 to 3 as a custom data block number, so the 0 entry goes through as a null pointer, as on the Mac.
const uint32_t kScriptStarts[kScriptTableSize] = {
    0x00000000, 0x1029f2ac, 0x1029f308, 0x1029f364, 0x1029f388, 0x1029f424, 0x1029f460,
    0x1029f4d8, 0x1029f49c, 0x1029f210, 0x1029f514, 0x1029f570, 0x1029f68c,
};

// The code pointer the ESP_ calls take for a script index (no bound test, as on the Mac: the index times four into the table).
void* ScriptCode(CDelayedScript::ScriptIdx script) {
    const uint32_t address = kScriptStarts[static_cast<int32_t>(script)];
    if (address == 0) return nullptr;
    return const_cast<uint32_t*>(gScriptCodeWords) + (address - kScriptCodeAddress) / 4;
}

constexpr float kServiceTicksPerSecond = 62.0f;   // 0x10265118
constexpr uint32_t kMaxServiceInterval = 250;
}  // namespace

// 0x100fa898 (20 bytes).
CDelayedScript::CDelayedScript() : mSpriteScript(0), mNextAnim(static_cast<ScriptIdx>(0)), mNextServiceInterval(0) {}

// 0x100fa8d8 (356 bytes): no base call. Versions 0 and 1 only (signed); the three words by the save flag, in order, the first nonzero answer ends it
// with 1. Anything else answers 0.
int32_t CDelayedScript::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version >= 2 || version < 0) return 0;
    if (save != 0) {
        if (SAVEVAR<MacLong>(file, reinterpret_cast<MacLong*>(&mSpriteScript), kSpriteScriptName, 0) != 0) return 1;
        if (SAVEVAR<ScriptIdx>(file, &mNextAnim, kNextAnimName, 0) != 0) return 1;
        if (SAVEVAR<MacLong>(file, reinterpret_cast<MacLong*>(&mNextServiceInterval), kNextServiceIntervalName, 0) != 0) return 1;
        return 0;
    }
    if (LOADVAR<MacLong>(file, reinterpret_cast<MacLong*>(&mSpriteScript), kSpriteScriptName, 0) != 0) return 1;
    if (LOADVAR<ScriptIdx>(file, &mNextAnim, kNextAnimName, 0) != 0) return 1;
    if (LOADVAR<MacLong>(file, reinterpret_cast<MacLong*>(&mNextServiceInterval), kNextServiceIntervalName, 0) != 0) return 1;
    return 0;
}

// 0x100faa74 (8 bytes).
void CDelayedScript::SetSpriteScript(ScriptIdx script) { mNextAnim = script; }

// 0x100faad0 (116 bytes): the interpreter is always asked first; the pending script counts too.
bool CDelayedScript::IsRunningThisScript(ScriptIdx script) {
    if (ESP_RURunningThis(mSpriteScript, ScriptCode(script)) != 0) return true;
    return mNextAnim == script;
}

// 0x100fab9c (88 bytes): the sprite set and index are passed on; the action state and the position are 0.
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    mSpriteScript = ESP_SpawnScript(ScriptCode(script), static_cast<ESP_Sprites>(set), index, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
}

// 0x100fac4c (60 bytes).
void CDelayedScript::KillSpriteScript() {
    SCR_KillScript(mSpriteScript);
    mSpriteScript = 0;
}

// 0x100facc0 (116 bytes): the pending script is switched to, then the pending service interval is set; each is cleared once handed over.
void CDelayedScript::ProcessSpriteScripts() {
    if (mNextAnim != static_cast<ScriptIdx>(0)) {
        ESP_ForceGoto(mSpriteScript, ScriptCode(mNextAnim));
        mNextAnim = static_cast<ScriptIdx>(0);
    }
    if (mNextServiceInterval != 0) {
        SCR_SetServiceInterval(mSpriteScript, mNextServiceInterval);
        mNextServiceInterval = 0;
    }
}

// 0x100fad70 (56 bytes): 62 / rate in single precision, truncated (fctiwz: saturating, NaN to INT_MIN), below 0 gives 0, above 250 (unsigned) gives 250.
void CDelayedScript::SetServiceInterval(float rate) {
    const float quotient = kServiceTicksPerSecond / rate;
    int32_t interval = ppc_fctiwz(static_cast<double>(quotient));
    if (interval < 0) {
        interval = 0;
    } else if (static_cast<uint32_t>(interval) > kMaxServiceInterval) {
        interval = static_cast<int32_t>(kMaxServiceInterval);
    }
    mNextServiceInterval = interval;
}
