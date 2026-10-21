// tb_switch.h: CSwitch (engine library, 0x100326f0..0x10032b00; the inventory files it
// under Wavelet.cpp at low confidence). PARTIAL: construction only; Do, ModeSwitch and
// RezSwitch are untranslated.
#pragma once
#include <cstdint>

class CSwitch {
public:
    CSwitch() : mF00(-1) {}   // 0x1003287c

    int32_t mF00;             // 0x00  -1
    int32_t mF04;             // 0x04  not set by the constructor
    int32_t mF08;             // 0x08
    int32_t Do(int32_t toggle);   // 0x100328ac (src/engine/Switch.cpp)
    // Unit v51066 (src/engine/Switch.cpp)
    void ModeSwitch();                      // 0x10032aa4: size 0 0, Do(1)
    void RezSwitch(int32_t width, int32_t height);   // 0x10032b00: Do(0)
};
extern CSwitch* gei_Switch;   // engine data 0x10173180 (not exported; name invented)

int32_t Switch_Init();    // 0x100326f0
int32_t Switch_Close();   // 0x100327a8

// Unit v51066 (src/engine/Switch.cpp, src/engine/EngineUtil.cpp, src/engine/tb_init.cpp, src/engine/tb_hfield.cpp)
// ToggleFullScreen 0x1003281c answers 1 (keytable_actions.h declares it void; this file's src declares it int32_t locally).
bool IsAlignedTo64Bit(void*& pointer);   // 0x100391c8: the pointer's low three bits are clear
extern uint32_t gei_ParameterSettings[4];   // engine data 0x100af128 (name from the Ghidra draft), zeroed by __sinit_tb_init_cpp
extern float gei_HFieldOne;                 // engine data 0x100af120 (name invented), 1.0f after __sinit_tb_hfield_cpp
void __sinit_tb_init_cpp();     // 0x100141bc
void __sinit_tb_hfield_cpp();   // 0x1001271c
namespace SamsUtilities {
void USwapInplace16UShort_10006218(uint16_t& value);   // 0x10006218 USwapInplace16<unsigned short>
void USwapInplace32UInt_100061d0(uint32_t& value);     // 0x100061d0 USwapInplace32<unsigned int>
}
