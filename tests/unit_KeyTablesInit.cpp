// Unit w53: recording allocator, strncpy, formatter and action fakes.
#include "keyboard.h"
#include "keytable_actions.h"
#include "Text.h"
#include "TbITable.h"
#include "calllog.cpp"
#include <csignal>
#include <csetjmp>
#include <cstddef>

CText gText;
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
CKeyTable* gKeyTable;
CKeyTable* gKeyTableGame;
CKeyTable* gKeyTableCamera;
CKeyTable* gKeyTableCheat;
CKeyTable* gKeyTableCoaster;
CKeyTable* gKeyTableShortcuts;
CKeyTable* gKeyTableDebug;

namespace {
using namespace calllog;
CKeyTable** const globals[] = {&gKeyTable, &gKeyTableGame, &gKeyTableCamera,
    &gKeyTableCheat, &gKeyTableCoaster, &gKeyTableShortcuts, &gKeyTableDebug};
CKeyTableEntry* const arrays[] = {gKeyEntriesSystem, gKeyEntriesGame, gKeyEntriesCamera,
    gKeyEntriesCheat, gKeyEntriesCoaster, gKeyEntriesShortcuts, gKeyEntriesDebug};
constexpr int counts[] = {12, 15, 25, 42, 4, 17, 71};
CKeyTableEntry baseline[7][71];
CKeyTable* blocks[7];
int slot;
int nullMask;
int fillByte;
int overrideCounts[7];
int afterFormat;
bool observeString;
sigjmp_buf faultJump;

// Retain the addresses even after delete so the dangling globals can be compared.
int TableLabel(const void* pointer) {
    if (pointer == nullptr) return 0;
    for (int i = 0; i < 7; ++i) {
        if (pointer == blocks[i]) return 100 + i;
    }
    return -1;
}

// Only the translation's faulting null-allocation paths run under this handler.
void Fault(int) { siglongjmp(faultJump, 1); }
int ActionLabel(void* pointer);
}

// The original's allocation is nullable; each call gets a distinct prefilled block.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const int index = slot++;
    observeString = false;
    calllog::ev("new", {index, size == sizeof(CKeyTable) ? 0x18 : static_cast<int64_t>(size)});
    observeString = true;
    if ((nullMask & (1 << index)) != 0) return nullptr;
    void* block = std::malloc(size);
    std::memset(block, fillByte, size);
    blocks[index] = static_cast<CKeyTable*>(block);
    tpw_cl_label(100 + index, block);
    return block;
}

// Record destination, all arguments, and copy exactly n bytes, including padding.
extern "C" char* strncpy(char* out, const char* text, std::size_t count) {
    if (observeString) {
        observeString = false;
        const void* table = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(out) - offsetof(CKeyTable, mName));
        calllog::ev("strncpy", {calllog::lab(table), 8, static_cast<int64_t>(count)}, {text});
        observeString = true;
        // Triage 2026-10-13a: 0x10000 (not a short) leaves the count the constructor stored.
        if (overrideCounts[slot - 1] != 0x10000) blocks[slot - 1]->mCount = static_cast<int16_t>(overrideCounts[slot - 1]);
    }
    std::size_t i = 0;
    while (i < count && text[i] != 0) {
        out[i] = text[i];
        ++i;
    }
    while (i < count) out[i++] = 0;
    return out;
}

// The formatter writes both UTF-16 characters and can change the current count.
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    observeString = false;
    calllog::ev("FormatString", {calllog::lab(this), calllog::lab(out), size, id});
    observeString = true;
    out[0] = static_cast<char16_t>(calllog::ret("chars", id, 0));
    out[1] = static_cast<char16_t>(calllog::ret("tails", id, 0));
    if (afterFormat != -999) {
        CKeyTable* table = id <= 14 ? gKeyTableShortcuts : id == 15 || id == 20 ? gKeyTable : gKeyTableGame;
        table->mCount = static_cast<int16_t>(afterFormat);
    }
    return -7;
}

int32_t AllowFullCameraZoomout() { calllog::ev("Action", {1}); return 0; }
int32_t ResetCameraPosition() { calllog::ev("Action", {2}); return 0; }
int32_t ToggleDrawUpSpline() { calllog::ev("Action", {3}); return 0; }
int32_t OutputCoasterValues() { calllog::ev("Action", {4}); return 0; }
int32_t IncCoasterConst2() { calllog::ev("Action", {5}); return 0; }
int32_t DecCoasterConst2() { calllog::ev("Action", {6}); return 0; }
int32_t IncCoasterConst1() { calllog::ev("Action", {7}); return 0; }
int32_t DecCoasterConst1() { calllog::ev("Action", {8}); return 0; }
int32_t ToggleProceduralTexturing() { calllog::ev("Action", {9}); return 0; }
int32_t RealTimeLightingMasterToggle() { calllog::ev("Action", {10}); return 0; }
int32_t FPSDisplayToggle() { calllog::ev("Action", {11}); return 0; }
int32_t place_litter() { calllog::ev("Action", {12}); return 0; }
int32_t place_vomit() { calllog::ev("Action", {13}); return 0; }
int32_t cheat_cleanup_park() { calllog::ev("Action", {14}); return 0; }
int32_t complete_all_research() { calllog::ev("Action", {15}); return 0; }
int32_t uncomplete_all_research() { calllog::ev("Action", {16}); return 0; }
int32_t spawn_person() { calllog::ev("Action", {17}); return 0; }
int32_t spawn_five_persons() { calllog::ev("Action", {18}); return 0; }
int32_t populate_park() { calllog::ev("Action", {19}); return 0; }
int32_t start_winning_sequence() { calllog::ev("Action", {20}); return 0; }
int32_t start_bankruptcy_sequence() { calllog::ev("Action", {21}); return 0; }
int32_t spawn_guard() { calllog::ev("Action", {22}); return 0; }
int32_t spawn_researcher() { calllog::ev("Action", {23}); return 0; }
int32_t spawn_mechanic() { calllog::ev("Action", {24}); return 0; }
int32_t spawn_handyman() { calllog::ev("Action", {25}); return 0; }
int32_t spawn_entertainer() { calllog::ev("Action", {26}); return 0; }
int32_t spawn_bag_o_staff() { calllog::ev("Action", {27}); return 0; }
int32_t apply_for_loan_1() { calllog::ev("Action", {28}); return 0; }
int32_t apply_for_loan_2() { calllog::ev("Action", {29}); return 0; }
int32_t apply_for_loan_3() { calllog::ev("Action", {30}); return 0; }
int32_t apply_for_loan_4() { calllog::ev("Action", {31}); return 0; }
int32_t get_doshed_up() { calllog::ev("Action", {32}); return 0; }
int32_t force_ride_breakdown() { calllog::ev("Action", {33}); return 0; }
int32_t force_ride_worn() { calllog::ev("Action", {34}); return 0; }
int32_t start_strike() { calllog::ev("Action", {35}); return 0; }
int32_t show_script() { calllog::ev("Action", {36}); return 0; }
int32_t LoadPtclLib() { calllog::ev("Action", {37}); return 0; }
int32_t force_ride_condemned() { calllog::ev("Action", {38}); return 0; }
int32_t force_ride_fix() { calllog::ev("Action", {39}); return 0; }
int32_t place_bomb() { calllog::ev("Action", {40}); return 0; }
int32_t park_attraction_value() { calllog::ev("Action", {41}); return 0; }
int32_t debug_cell_dirs() { calllog::ev("Action", {42}); return 0; }
int32_t debug_track_cell_dirs() { calllog::ev("Action", {43}); return 0; }
int32_t toggle_cell_flash() { calllog::ev("Action", {44}); return 0; }
int32_t debug_cell_neighbs() { calllog::ev("Action", {45}); return 0; }
int32_t debug_track_cell_neighbs() { calllog::ev("Action", {46}); return 0; }
int32_t debug_hoarding_cell_neighbs() { calllog::ev("Action", {47}); return 0; }
int32_t select_debug_person() { calllog::ev("Action", {48}); return 0; }
int32_t set_allow_loops_no_research() { calllog::ev("Action", {49}); return 0; }
int32_t save_hoardings() { calllog::ev("Action", {50}); return 0; }
int32_t save_complete_hoardings() { calllog::ev("Action", {51}); return 0; }
int32_t enter_hoarding_mode() { calllog::ev("Action", {52}); return 0; }
int32_t show_person_history_info() { calllog::ev("Action", {53}); return 0; }
int32_t wheres_my_script() { calllog::ev("Action", {54}); return 0; }
int32_t dbgchan0() { calllog::ev("Action", {55}); return 0; }
int32_t dbgchan1() { calllog::ev("Action", {56}); return 0; }
int32_t dbgchan2() { calllog::ev("Action", {57}); return 0; }
int32_t dbgchan3() { calllog::ev("Action", {58}); return 0; }
int32_t dbgchan4() { calllog::ev("Action", {59}); return 0; }
int32_t dbgchan5() { calllog::ev("Action", {60}); return 0; }
int32_t dbgchan6() { calllog::ev("Action", {61}); return 0; }
int32_t dbgchan7() { calllog::ev("Action", {62}); return 0; }
int32_t dbgchan8() { calllog::ev("Action", {63}); return 0; }
int32_t dbgchan9() { calllog::ev("Action", {64}); return 0; }
int32_t ChatterToggle() { calllog::ev("Action", {65}); return 0; }
int32_t delete_ride() { calllog::ev("Action", {66}); return 0; }
int32_t togglehud() { calllog::ev("Action", {67}); return 0; }
int32_t TestAdvisor() { calllog::ev("Action", {68}); return 0; }
int32_t TestAdvisor2() { calllog::ev("Action", {69}); return 0; }
int32_t TestAdvisor3() { calllog::ev("Action", {70}); return 0; }
int32_t TestAdvisor4() { calllog::ev("Action", {71}); return 0; }
int32_t start_cheating() { calllog::ev("Action", {72}); return 0; }
int32_t recreate_recording() { calllog::ev("Action", {73}); return 0; }
int32_t recreate_recording_real_time() { calllog::ev("Action", {74}); return 0; }
int32_t recreate_recording_medium_speed() { calllog::ev("Action", {75}); return 0; }
int32_t save_recording() { calllog::ev("Action", {76}); return 0; }
int32_t create_arrival_vehicle() { calllog::ev("Action", {77}); return 0; }
int32_t clear_patrol_area() { calllog::ev("Action", {78}); return 0; }
int32_t next_debug_text_mode() { calllog::ev("Action", {79}); return 0; }
int32_t win_ticket_0() { calllog::ev("Action", {80}); return 0; }
int32_t win_ticket_1() { calllog::ev("Action", {81}); return 0; }
int32_t win_ticket_2() { calllog::ev("Action", {82}); return 0; }
int32_t win_ticket_3() { calllog::ev("Action", {83}); return 0; }
int32_t win_ticket_4() { calllog::ev("Action", {84}); return 0; }
int32_t win_ticket_5() { calllog::ev("Action", {85}); return 0; }
int32_t win_ticket_6() { calllog::ev("Action", {86}); return 0; }
int32_t win_ticket_7() { calllog::ev("Action", {87}); return 0; }
int32_t win_ticket_8() { calllog::ev("Action", {88}); return 0; }
int32_t win_ticket_9() { calllog::ev("Action", {89}); return 0; }
int32_t quickload_layout() { calllog::ev("Action", {90}); return 0; }
int32_t quicksave_layout() { calllog::ev("Action", {91}); return 0; }
int32_t mainmenu() { calllog::ev("Action", {92}); return 0; }
int32_t Screen_BuyObject() { calllog::ev("Action", {93}); return 0; }
int32_t Screen_BuyStaff() { calllog::ev("Action", {94}); return 0; }
int32_t Screen_ParkStatus() { calllog::ev("Action", {95}); return 0; }
int32_t Screen_AllItems() { calllog::ev("Action", {96}); return 0; }
int32_t Screen_AllStaff() { calllog::ev("Action", {97}); return 0; }
int32_t Screen_AllPeeps() { calllog::ev("Action", {98}); return 0; }
int32_t Screen_Finance() { calllog::ev("Action", {99}); return 0; }
int32_t Screen_Loans() { calllog::ev("Action", {100}); return 0; }
int32_t Screen_StaffCosts() { calllog::ev("Action", {101}); return 0; }
int32_t Screen_EntryPrice() { calllog::ev("Action", {102}); return 0; }
int32_t Screen_Research() { calllog::ev("Action", {103}); return 0; }
int32_t Screen_Map() { calllog::ev("Action", {104}); return 0; }
int32_t Screen_StaffLoc() { calllog::ev("Action", {105}); return 0; }
int32_t Screen_PeepLoc() { calllog::ev("Action", {106}); return 0; }
int32_t TakePostCard() { calllog::ev("Action", {107}); return 0; }
int32_t CamcorderMode() { calllog::ev("Action", {108}); return 0; }
int32_t abort_coaster_edit() { calllog::ev("Action", {109}); return 0; }
int32_t AnaglyphToggle() { calllog::ev("Action", {110}); return 0; }
int32_t abort_all_requesters() { calllog::ev("Action", {111}); return 0; }
int32_t turn_object_clockwise() { calllog::ev("Action", {112}); return 0; }
int32_t turn_object_counterclockwise() { calllog::ev("Action", {113}); return 0; }
int32_t enter_area_delete_mode() { calllog::ev("Action", {114}); return 0; }
int32_t togglegamewindow() { calllog::ev("Action", {115}); return 0; }
int32_t backtrack() { calllog::ev("Action", {116}); return 0; }
int32_t slow_down_time() { calllog::ev("Action", {117}); return 0; }
int32_t speed_up_time() { calllog::ev("Action", {118}); return 0; }
int32_t normal_speed_time() { calllog::ev("Action", {119}); return 0; }
int32_t close_park() { calllog::ev("Action", {120}); return 0; }
int32_t open_park() { calllog::ev("Action", {121}); return 0; }
int32_t quickload() { calllog::ev("Action", {122}); return 0; }
int32_t quicksave() { calllog::ev("Action", {123}); return 0; }
int32_t context_help() { calllog::ev("Action", {124}); return 0; }
int32_t show_version() { calllog::ev("Action", {125}); return 0; }
int32_t pause() { calllog::ev("Action", {126}); return 0; }
int32_t ToggleHelpText() { calllog::ev("Action", {127}); return 0; }
int32_t snapshot() { calllog::ev("Action", {128}); return 0; }
int32_t continuous_snapshot() { calllog::ev("Action", {129}); return 0; }
int32_t DeactivateWeather() { calllog::ev("Action", {130}); return 0; }
int32_t WeatherReset() { calllog::ev("Action", {131}); return 0; }
int32_t WeatherDownLots() { calllog::ev("Action", {132}); return 0; }
int32_t WeatherUpLots() { calllog::ev("Action", {133}); return 0; }
int32_t WeatherDown() { calllog::ev("Action", {134}); return 0; }
int32_t WeatherUp() { calllog::ev("Action", {135}); return 0; }
void ToggleFullScreen() { calllog::ev("Action", {136}); }


namespace {
int ActionLabel(void* pointer) {
    if (pointer == nullptr) return 0;
    if (pointer == reinterpret_cast<void*>(AllowFullCameraZoomout)) return 1;
    if (pointer == reinterpret_cast<void*>(ResetCameraPosition)) return 2;
    if (pointer == reinterpret_cast<void*>(ToggleDrawUpSpline)) return 3;
    if (pointer == reinterpret_cast<void*>(OutputCoasterValues)) return 4;
    if (pointer == reinterpret_cast<void*>(IncCoasterConst2)) return 5;
    if (pointer == reinterpret_cast<void*>(DecCoasterConst2)) return 6;
    if (pointer == reinterpret_cast<void*>(IncCoasterConst1)) return 7;
    if (pointer == reinterpret_cast<void*>(DecCoasterConst1)) return 8;
    if (pointer == reinterpret_cast<void*>(ToggleProceduralTexturing)) return 9;
    if (pointer == reinterpret_cast<void*>(RealTimeLightingMasterToggle)) return 10;
    if (pointer == reinterpret_cast<void*>(FPSDisplayToggle)) return 11;
    if (pointer == reinterpret_cast<void*>(place_litter)) return 12;
    if (pointer == reinterpret_cast<void*>(place_vomit)) return 13;
    if (pointer == reinterpret_cast<void*>(cheat_cleanup_park)) return 14;
    if (pointer == reinterpret_cast<void*>(complete_all_research)) return 15;
    if (pointer == reinterpret_cast<void*>(uncomplete_all_research)) return 16;
    if (pointer == reinterpret_cast<void*>(spawn_person)) return 17;
    if (pointer == reinterpret_cast<void*>(spawn_five_persons)) return 18;
    if (pointer == reinterpret_cast<void*>(populate_park)) return 19;
    if (pointer == reinterpret_cast<void*>(start_winning_sequence)) return 20;
    if (pointer == reinterpret_cast<void*>(start_bankruptcy_sequence)) return 21;
    if (pointer == reinterpret_cast<void*>(spawn_guard)) return 22;
    if (pointer == reinterpret_cast<void*>(spawn_researcher)) return 23;
    if (pointer == reinterpret_cast<void*>(spawn_mechanic)) return 24;
    if (pointer == reinterpret_cast<void*>(spawn_handyman)) return 25;
    if (pointer == reinterpret_cast<void*>(spawn_entertainer)) return 26;
    if (pointer == reinterpret_cast<void*>(spawn_bag_o_staff)) return 27;
    if (pointer == reinterpret_cast<void*>(apply_for_loan_1)) return 28;
    if (pointer == reinterpret_cast<void*>(apply_for_loan_2)) return 29;
    if (pointer == reinterpret_cast<void*>(apply_for_loan_3)) return 30;
    if (pointer == reinterpret_cast<void*>(apply_for_loan_4)) return 31;
    if (pointer == reinterpret_cast<void*>(get_doshed_up)) return 32;
    if (pointer == reinterpret_cast<void*>(force_ride_breakdown)) return 33;
    if (pointer == reinterpret_cast<void*>(force_ride_worn)) return 34;
    if (pointer == reinterpret_cast<void*>(start_strike)) return 35;
    if (pointer == reinterpret_cast<void*>(show_script)) return 36;
    if (pointer == reinterpret_cast<void*>(LoadPtclLib)) return 37;
    if (pointer == reinterpret_cast<void*>(force_ride_condemned)) return 38;
    if (pointer == reinterpret_cast<void*>(force_ride_fix)) return 39;
    if (pointer == reinterpret_cast<void*>(place_bomb)) return 40;
    if (pointer == reinterpret_cast<void*>(park_attraction_value)) return 41;
    if (pointer == reinterpret_cast<void*>(debug_cell_dirs)) return 42;
    if (pointer == reinterpret_cast<void*>(debug_track_cell_dirs)) return 43;
    if (pointer == reinterpret_cast<void*>(toggle_cell_flash)) return 44;
    if (pointer == reinterpret_cast<void*>(debug_cell_neighbs)) return 45;
    if (pointer == reinterpret_cast<void*>(debug_track_cell_neighbs)) return 46;
    if (pointer == reinterpret_cast<void*>(debug_hoarding_cell_neighbs)) return 47;
    if (pointer == reinterpret_cast<void*>(select_debug_person)) return 48;
    if (pointer == reinterpret_cast<void*>(set_allow_loops_no_research)) return 49;
    if (pointer == reinterpret_cast<void*>(save_hoardings)) return 50;
    if (pointer == reinterpret_cast<void*>(save_complete_hoardings)) return 51;
    if (pointer == reinterpret_cast<void*>(enter_hoarding_mode)) return 52;
    if (pointer == reinterpret_cast<void*>(show_person_history_info)) return 53;
    if (pointer == reinterpret_cast<void*>(wheres_my_script)) return 54;
    if (pointer == reinterpret_cast<void*>(dbgchan0)) return 55;
    if (pointer == reinterpret_cast<void*>(dbgchan1)) return 56;
    if (pointer == reinterpret_cast<void*>(dbgchan2)) return 57;
    if (pointer == reinterpret_cast<void*>(dbgchan3)) return 58;
    if (pointer == reinterpret_cast<void*>(dbgchan4)) return 59;
    if (pointer == reinterpret_cast<void*>(dbgchan5)) return 60;
    if (pointer == reinterpret_cast<void*>(dbgchan6)) return 61;
    if (pointer == reinterpret_cast<void*>(dbgchan7)) return 62;
    if (pointer == reinterpret_cast<void*>(dbgchan8)) return 63;
    if (pointer == reinterpret_cast<void*>(dbgchan9)) return 64;
    if (pointer == reinterpret_cast<void*>(ChatterToggle)) return 65;
    if (pointer == reinterpret_cast<void*>(delete_ride)) return 66;
    if (pointer == reinterpret_cast<void*>(togglehud)) return 67;
    if (pointer == reinterpret_cast<void*>(TestAdvisor)) return 68;
    if (pointer == reinterpret_cast<void*>(TestAdvisor2)) return 69;
    if (pointer == reinterpret_cast<void*>(TestAdvisor3)) return 70;
    if (pointer == reinterpret_cast<void*>(TestAdvisor4)) return 71;
    if (pointer == reinterpret_cast<void*>(start_cheating)) return 72;
    if (pointer == reinterpret_cast<void*>(recreate_recording)) return 73;
    if (pointer == reinterpret_cast<void*>(recreate_recording_real_time)) return 74;
    if (pointer == reinterpret_cast<void*>(recreate_recording_medium_speed)) return 75;
    if (pointer == reinterpret_cast<void*>(save_recording)) return 76;
    if (pointer == reinterpret_cast<void*>(create_arrival_vehicle)) return 77;
    if (pointer == reinterpret_cast<void*>(clear_patrol_area)) return 78;
    if (pointer == reinterpret_cast<void*>(next_debug_text_mode)) return 79;
    if (pointer == reinterpret_cast<void*>(win_ticket_0)) return 80;
    if (pointer == reinterpret_cast<void*>(win_ticket_1)) return 81;
    if (pointer == reinterpret_cast<void*>(win_ticket_2)) return 82;
    if (pointer == reinterpret_cast<void*>(win_ticket_3)) return 83;
    if (pointer == reinterpret_cast<void*>(win_ticket_4)) return 84;
    if (pointer == reinterpret_cast<void*>(win_ticket_5)) return 85;
    if (pointer == reinterpret_cast<void*>(win_ticket_6)) return 86;
    if (pointer == reinterpret_cast<void*>(win_ticket_7)) return 87;
    if (pointer == reinterpret_cast<void*>(win_ticket_8)) return 88;
    if (pointer == reinterpret_cast<void*>(win_ticket_9)) return 89;
    if (pointer == reinterpret_cast<void*>(quickload_layout)) return 90;
    if (pointer == reinterpret_cast<void*>(quicksave_layout)) return 91;
    if (pointer == reinterpret_cast<void*>(mainmenu)) return 92;
    if (pointer == reinterpret_cast<void*>(Screen_BuyObject)) return 93;
    if (pointer == reinterpret_cast<void*>(Screen_BuyStaff)) return 94;
    if (pointer == reinterpret_cast<void*>(Screen_ParkStatus)) return 95;
    if (pointer == reinterpret_cast<void*>(Screen_AllItems)) return 96;
    if (pointer == reinterpret_cast<void*>(Screen_AllStaff)) return 97;
    if (pointer == reinterpret_cast<void*>(Screen_AllPeeps)) return 98;
    if (pointer == reinterpret_cast<void*>(Screen_Finance)) return 99;
    if (pointer == reinterpret_cast<void*>(Screen_Loans)) return 100;
    if (pointer == reinterpret_cast<void*>(Screen_StaffCosts)) return 101;
    if (pointer == reinterpret_cast<void*>(Screen_EntryPrice)) return 102;
    if (pointer == reinterpret_cast<void*>(Screen_Research)) return 103;
    if (pointer == reinterpret_cast<void*>(Screen_Map)) return 104;
    if (pointer == reinterpret_cast<void*>(Screen_StaffLoc)) return 105;
    if (pointer == reinterpret_cast<void*>(Screen_PeepLoc)) return 106;
    if (pointer == reinterpret_cast<void*>(TakePostCard)) return 107;
    if (pointer == reinterpret_cast<void*>(CamcorderMode)) return 108;
    if (pointer == reinterpret_cast<void*>(abort_coaster_edit)) return 109;
    if (pointer == reinterpret_cast<void*>(AnaglyphToggle)) return 110;
    if (pointer == reinterpret_cast<void*>(abort_all_requesters)) return 111;
    if (pointer == reinterpret_cast<void*>(turn_object_clockwise)) return 112;
    if (pointer == reinterpret_cast<void*>(turn_object_counterclockwise)) return 113;
    if (pointer == reinterpret_cast<void*>(enter_area_delete_mode)) return 114;
    if (pointer == reinterpret_cast<void*>(togglegamewindow)) return 115;
    if (pointer == reinterpret_cast<void*>(backtrack)) return 116;
    if (pointer == reinterpret_cast<void*>(slow_down_time)) return 117;
    if (pointer == reinterpret_cast<void*>(speed_up_time)) return 118;
    if (pointer == reinterpret_cast<void*>(normal_speed_time)) return 119;
    if (pointer == reinterpret_cast<void*>(close_park)) return 120;
    if (pointer == reinterpret_cast<void*>(open_park)) return 121;
    if (pointer == reinterpret_cast<void*>(quickload)) return 122;
    if (pointer == reinterpret_cast<void*>(quicksave)) return 123;
    if (pointer == reinterpret_cast<void*>(context_help)) return 124;
    if (pointer == reinterpret_cast<void*>(show_version)) return 125;
    if (pointer == reinterpret_cast<void*>(pause)) return 126;
    if (pointer == reinterpret_cast<void*>(ToggleHelpText)) return 127;
    if (pointer == reinterpret_cast<void*>(snapshot)) return 128;
    if (pointer == reinterpret_cast<void*>(continuous_snapshot)) return 129;
    if (pointer == reinterpret_cast<void*>(DeactivateWeather)) return 130;
    if (pointer == reinterpret_cast<void*>(WeatherReset)) return 131;
    if (pointer == reinterpret_cast<void*>(WeatherDownLots)) return 132;
    if (pointer == reinterpret_cast<void*>(WeatherUpLots)) return 133;
    if (pointer == reinterpret_cast<void*>(WeatherDown)) return 134;
    if (pointer == reinterpret_cast<void*>(WeatherUp)) return 135;
    if (pointer == reinterpret_cast<void*>(ToggleFullScreen)) return 136;
    return -1;
}
}

// Restore static data between cases and apply id edits before initialisation.
extern "C" void tpw_ki_prepare(int mask, int byte, const int32_t* tableCounts,
                                const int32_t* edits, int editCount, int after) {
    static bool first = true;
    if (first) {
        for (int t = 0; t < 7; ++t) std::memcpy(baseline[t], arrays[t], counts[t] * sizeof(CKeyTableEntry));
        first = false;
    }
    nullMask = mask;
    fillByte = byte;
    slot = 0;
    afterFormat = after;
    for (int t = 0; t < 7; ++t) {
        std::memcpy(arrays[t], baseline[t], counts[t] * sizeof(CKeyTableEntry));
        overrideCounts[t] = tableCounts[t];
        blocks[t] = nullptr;
        *globals[t] = nullptr;
    }
    for (int i = 0; i < editCount; ++i) arrays[edits[3 * i]][edits[3 * i + 1]].mId = static_cast<int16_t>(edits[3 * i + 2]);
    gText.mResourcePointers[2] = reinterpret_cast<void*>(0x1234);
    tpw_cl_label(10, gText.mResourcePointers[2]);
}

// Capture the original's null dereference as a fault result without ending difftest.
extern "C" int tpw_ki_init() {
    auto oldSegv = std::signal(SIGSEGV, Fault);
    auto oldBus = std::signal(SIGBUS, Fault);
    int fault = 0;
    if (sigsetjmp(faultJump, 1) == 0) {
        observeString = true;
        InitialiseKeytables();
    } else fault = 1;
    observeString = false;
    std::signal(SIGSEGV, oldSegv);
    std::signal(SIGBUS, oldBus);
    return fault;
}

// Dump labels, every visible table byte and all seven fields of every entry.
extern "C" int tpw_ki_state(int32_t* out) {
    int n = 0;
    for (int t = 0; t < 7; ++t) {
        out[n++] = TableLabel(*globals[t]);
        const CKeyTable* table = blocks[t];
        out[n++] = table != nullptr;
        if (table != nullptr) {
            int entryLabel = -1;
            for (int i = 0; i < 7; ++i) {
                if (table->mEntries == arrays[i]) entryLabel = i;
            }
            out[n++] = entryLabel;
            out[n++] = table->mCount;
            out[n++] = table->mEnabled;
            for (unsigned char byte : table->mName) out[n++] = byte;
        }
        for (int i = 0; i < counts[t]; ++i) {
            const CKeyTableEntry& e = arrays[t][i];
            out[n++] = e.mId;
            out[n++] = e.mKey;
            out[n++] = e.mModifiers;
            out[n++] = e.mState;
            const char* text = e.mHelpText;
            out[n++] = text != nullptr;
            const int length = text != nullptr ? std::strlen(text) : 0;
            out[n++] = length;
            for (int k = 0; k < length; ++k) out[n++] = static_cast<unsigned char>(text[k]);
            out[n++] = ActionLabel(e.mUpAction);
            out[n++] = ActionLabel(e.mAction);
        }
    }
    return n;
}

// Free completed initialisations outside observation; fault cases leave partial blocks.
extern "C" void tpw_ki_cleanup() {
    for (auto* block : blocks) std::free(block);
}

// Every null-global pattern is tested. Ordinary calllog delete records and frees.
extern "C" void tpw_ki_deinit(int mask, int32_t* out) {
    for (int i = 0; i < 7; ++i) {
        blocks[i] = (mask & (1 << i)) != 0 ? static_cast<CKeyTable*>(std::malloc(sizeof(CKeyTable))) : nullptr;
        *globals[i] = blocks[i];
        tpw_cl_label(100 + i, blocks[i]);
    }
    DeInitialiseKeyTables();
    for (int i = 0; i < 7; ++i) out[i] = TableLabel(*globals[i]);
}

// Search observes every entry and both table words before and after its read-only call.
extern "C" int tpw_ki_search(int count, int enabled, int key, int mods, int total,
                              const int32_t* words, int32_t* out) {
    CKeyTableEntry entries[7]{};
    for (int i = 0; i < total; ++i) {
        entries[i].mId = static_cast<int16_t>(words[4 * i]);
        entries[i].mKey = static_cast<int16_t>(words[4 * i + 1]);
        entries[i].mModifiers = static_cast<int16_t>(words[4 * i + 2]);
        entries[i].mState = static_cast<int16_t>(words[4 * i + 3]);
    }
    CKeyTable table{entries, static_cast<int16_t>(count), static_cast<int16_t>(enabled), {}};
    const int result = table.Search(static_cast<int16_t>(key), static_cast<int16_t>(mods));
    out[0] = table.mCount;
    out[1] = table.mEnabled;
    for (int i = 0; i < total; ++i) {
        out[2 + 4 * i] = entries[i].mId;
        out[3 + 4 * i] = entries[i].mKey;
        out[4 + 4 * i] = entries[i].mModifiers;
        out[5 + 4 * i] = entries[i].mState;
    }
    return result;
}
