// The effect manager (main app, the HLSprite.cpp range of the inventory, 0x1007a3d0..0x1007a6b8): up to three effects, each a
// group of particle emitters that orbit a point which moves from one place to another. The advisor uses it for its sparkles
// (src/Advisor.cpp declares the five functions too, identically).
#pragma once
#include <cstdint>

// One effect, 0x44 bytes on the Mac (all fields 32 bits). The centre and the radii are fractions of the screen the emitters are
// placed on: EM_Tick maps -1..1 to 0..100000 across and to 0..75000 down (the units PAR_SetEmitterPos takes).
struct EMEffect {
    float mX;                   // 0x00  where the effect's centre was when the move began
    float mY;                   // 0x04
    float mTargetX;             // 0x08  where it is going (EM_SetEffectTranslation)
    float mTargetY;             // 0x0c
    float mStart;               // 0x10  the real time (RealTimePauseAdjusted, as a float) the move began
    float mDuration;            // 0x14  how long the move takes, in the same unit (EM_Tick divides by it; it is not clamped)
    float mOrbitTurns;          // 0x18  turns the emitters make round the centre per unit of the move's progress (EM_SetEffectOrbit)
    float mOrbitX;              // 0x1c  the orbit's radii when the change began
    float mOrbitY;              // 0x20
    float mOrbitTargetX;        // 0x24  the radii it is changing to
    float mOrbitTargetY;        // 0x28
    int32_t mCount;             // 0x2c  how many emitters the effect has (EM_SetEmitersOn)
    uint32_t mEmitters[5];      // 0x30  their handles; the original takes any count, past 5 it writes into the next effect
};
static_assert(sizeof(EMEffect) == 0x44 && __builtin_offsetof(EMEffect, mStart) == 0x10 && __builtin_offsetof(EMEffect, mOrbitTurns) == 0x18 &&
                  __builtin_offsetof(EMEffect, mCount) == 0x2c && __builtin_offsetof(EMEffect, mEmitters) == 0x30,
              "an effect is 0x44 bytes");
constexpr int32_t kEffects = 3;
extern EMEffect gEffects[kEffects];   // 0x102dab10

// ---- src/effect_manager.cpp (unit w26c)
// 0x1007a3d0 (100 bytes): clears the three effects and makes the table of sines the effects' orbits read.
void EM_Init();
// 0x1007a454 (220 bytes): gives effect `effect` (no range check) `count` emitters of library type `emitter`: starts the missing ones
// (PAR_InitEmitter at 0, 0, 0) or ends the extra ones (PAR_KillEmitter). The answer is the number started less the number ended.
int32_t EM_SetEmitersOn(int32_t effect, int32_t count, int32_t emitter);
// 0x1007a558 (176 bytes): the effect's centre will move to (x, y); the move starts from the old target (`index` 0) or from (x, y)
// itself (else), takes z (when it is not 0; then it starts now) and EM_Tick follows it.
void EM_SetEffectTranslation(int32_t effect, int32_t index, float x, float y, float z);
// 0x1007a63c (80 bytes): the orbit's radii will change to (a, b) (from the old target when `index` is 0, else from (a, b) itself)
// and the emitters make c turns round it.
void EM_SetEffectOrbit(int32_t effect, int32_t index, float a, float b, float c);
// 0x1007a6b8 (604 bytes): puts the emitters of each effect that has some on their places for now: spread evenly round the centre
// (turns = progress * mOrbitTurns + their share), at the radii for the move's progress (stopping at 1), the centre at the move's
// progress without a stop (the move goes on past its end).
void EM_Tick();

// ---- what the effect manager calls of the rest of the main app (src/host_leaves.cpp, unit w38)
uint32_t RealTimePauseAdjusted();     // 0x10007c68  the game's real-time clock in milliseconds, held still while paused
int32_t HOST_Sin(int32_t angle);      // 0x100218f4  table of 512 sines (angle & 511) times 1024
int32_t HOST_Cos(int32_t angle);      // 0x10021928  the same a quarter turn on (angle + 128)
void BMP_GenerateSinTab();            // 0x100259d0  fills that table: 256 * sin(2 pi i / 512) shifted left by 2 on reading
extern int32_t gHostSineTable[512];       // main app data 0x102b4ec0 (not exported; name invented): 256 * sin(3.14159 * 2i / 512) cut toward zero
