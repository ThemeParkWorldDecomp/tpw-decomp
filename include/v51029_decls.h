// Unit v51029: declarations of the functions and globals of src/SkyObjects_destroy_CSkyObjects_InitSkyObjects_2.cpp
// (CText::~CText, __sinit_RideDB_cpp, __sinit_keyboard_cpp; the others are members declared in their class headers).
#pragma once
#include <cstdint>

class CText;

// 0x1015ee20: CText::~CText with the CodeWarrior hidden "delete storage" short. Answers `self`. (Text.h keeps `= default`
// for the inline compiler-made one; this is the Mac function by hand: params, 52 wrappers through __destroy_arr, manager,
// then the delete when the flag is positive.)
CText* CText_Destroy(CText* self, int16_t deleteFlag);
// The TVector at 0x102673c8: destroys one InterfaceFontWrapper (its destructor, 0x10168b68).
void FontWrapperVectorDestroy(void* object);
// operator delete as the destructor calls it (weak here; the test library replaces it with a recording fake).
void CText_OperatorDelete(void* object);

void __sinit_RideDB_cpp();        // 0x10157010
void __sinit_keyboard_cpp();      // 0x101512fc

// std::ios_base::Init::Init (glue 0x1023b9b0) and the glue that destroys one (import 0x103bcff4); weak here.
void IosBaseInitConstruct(void* object);
void IosBaseInitDestroy(void* object);
// The TVector at 0x102672a0: CRideDatabase::~CRideDatabase (0x10154660).
void RideDatabaseDestroy(void* object);
// The string destructor glue of the two string types (0x103bcbb8 wide, 0x103bcd38 narrow).
void UIMain_DestroyWString(void* object);
void UIMain_DestroyString(void* object);

// RideDB.cpp globals (Mac sizes: the registration nodes are 12 bytes on the Mac; the host keeps room).
extern uint8_t gRideDBIosInit[16];     // 0x1037fe0c: the std::ios_base::Init object
extern uint8_t gRideDBIosNode[32];     // 0x1037fe10
extern uint8_t gRideDBNode[32];        // 0x1037fe00
// keyboard.cpp globals.
extern int32_t gKeyboardF28C;          // 0x1037f28c, set to 0
extern int32_t gKeyboardF288;          // 0x1037f288, set to 0x10000
extern uint8_t gKeyboardWideString[48];   // 0x1037f268: a TbDynamicStringTemplate<char16_t>, built by the initialiser
extern uint8_t gKeyboardWideNode[32];  // 0x1037f27c
extern uint8_t gKeyboardQuickNode[32]; // 0x1037f25c
