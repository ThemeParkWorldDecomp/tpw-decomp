// keyboard.cpp: actions referenced by the seven static key tables (unit w53).
#pragma once
#include <cstdint>

int32_t AllowFullCameraZoomout();   // 0x10032c08, .AllowFullCameraZoomout__Fv
int32_t ResetCameraPosition();   // 0x10032bc4, .ResetCameraPosition__Fv
int32_t ToggleDrawUpSpline();   // 0x10056258, .ToggleDrawUpSpline__Fv
int32_t OutputCoasterValues();   // 0x100545d4, .OutputCoasterValues__Fv
int32_t IncCoasterConst2();   // 0x100526d4, .IncCoasterConst2__Fv
int32_t DecCoasterConst2();   // 0x10052628, .DecCoasterConst2__Fv
int32_t IncCoasterConst1();   // 0x10052564, .IncCoasterConst1__Fv
int32_t DecCoasterConst1();   // 0x100524a0, .DecCoasterConst1__Fv
int32_t ToggleProceduralTexturing();   // 0x100634f8, .ToggleProceduralTexturing__Fv
int32_t RealTimeLightingMasterToggle();   // 0x1014d8bc, .RealTimeLightingMasterToggle__Fv
int32_t FPSDisplayToggle();   // 0x1014d950, .FPSDisplayToggle__Fv
int32_t place_litter();   // 0x1014dac8, .place_litter__Fv
int32_t place_vomit();   // 0x1014ce2c, .place_vomit__Fv
int32_t cheat_cleanup_park();   // 0x1014c250, .cheat_cleanup_park__Fv
int32_t complete_all_research();   // 0x1014d994, .complete_all_research__Fv
int32_t uncomplete_all_research();   // 0x1014d9f8, .uncomplete_all_research__Fv
int32_t spawn_person();   // 0x1014dbf4, .spawn_person__Fv
int32_t spawn_five_persons();   // 0x1014dca0, .spawn_five_persons__Fv
int32_t populate_park();   // 0x1014dd7c, .populate_park__Fv
int32_t start_winning_sequence();   // 0x1014ddd0, .start_winning_sequence__Fv
int32_t start_bankruptcy_sequence();   // 0x1014de5c, .start_bankruptcy_sequence__Fv
int32_t spawn_guard();   // 0x1014e0bc, .spawn_guard__Fv
int32_t spawn_researcher();   // 0x1014e03c, .spawn_researcher__Fv
int32_t spawn_mechanic();   // 0x1014dfbc, .spawn_mechanic__Fv
int32_t spawn_handyman();   // 0x1014df3c, .spawn_handyman__Fv
int32_t spawn_entertainer();   // 0x1014debc, .spawn_entertainer__Fv
int32_t spawn_bag_o_staff();   // 0x1014e138, .spawn_bag_o_staff__Fv
int32_t apply_for_loan_1();   // 0x1014e248, .apply_for_loan_1__Fv
int32_t apply_for_loan_2();   // 0x1014e2e4, .apply_for_loan_2__Fv
int32_t apply_for_loan_3();   // 0x1014e380, .apply_for_loan_3__Fv
int32_t apply_for_loan_4();   // 0x1014e41c, .apply_for_loan_4__Fv
int32_t get_doshed_up();   // 0x1014e4b8, .get_doshed_up__Fv
int32_t force_ride_breakdown();   // 0x1014f7e0, .force_ride_breakdown__Fv
int32_t force_ride_worn();   // 0x1014f7b0, .force_ride_worn__Fv
int32_t start_strike();   // 0x1014ceac, .start_strike__Fv
int32_t show_script();   // 0x1014c4e0, .show_script__Fv
int32_t LoadPtclLib();   // 0x1014c50c, .LoadPtclLib__Fv
int32_t force_ride_condemned();   // 0x1014f8bc, .force_ride_condemned__Fv
int32_t force_ride_fix();   // 0x1014f8f0, .force_ride_fix__Fv
int32_t place_bomb();   // 0x1014db74, .place_bomb__Fv
int32_t park_attraction_value();   // 0x1014fa2c, .park_attraction_value__Fv
int32_t debug_cell_dirs();   // 0x1014f954, .debug_cell_dirs__Fv
int32_t debug_track_cell_dirs();   // 0x1014f9f8, .debug_track_cell_dirs__Fv
int32_t toggle_cell_flash();   // 0x1014f62c, .toggle_cell_flash__Fv
int32_t debug_cell_neighbs();   // 0x1014f920, .debug_cell_neighbs__Fv
int32_t debug_track_cell_neighbs();   // 0x1014f984, .debug_track_cell_neighbs__Fv
int32_t debug_hoarding_cell_neighbs();   // 0x1014f9bc, .debug_hoarding_cell_neighbs__Fv
int32_t select_debug_person();   // 0x1014f588, .select_debug_person__Fv
int32_t set_allow_loops_no_research();   // 0x1014f5bc, .set_allow_loops_no_research__Fv
int32_t save_hoardings();   // 0x1014f3ec, .save_hoardings__Fv
int32_t save_complete_hoardings();   // 0x1014f43c, .save_complete_hoardings__Fv
int32_t enter_hoarding_mode();   // 0x1014f494, .enter_hoarding_mode__Fv
int32_t show_person_history_info();   // 0x1014f65c, .show_person_history_info__Fv
int32_t wheres_my_script();   // 0x1014d114, .wheres_my_script__Fv
int32_t dbgchan0();   // 0x1014fb54, .dbgchan0__Fv
int32_t dbgchan1();   // 0x1014fb7c, .dbgchan1__Fv
int32_t dbgchan2();   // 0x1014fba4, .dbgchan2__Fv
int32_t dbgchan3();   // 0x1014fbcc, .dbgchan3__Fv
int32_t dbgchan4();   // 0x1014fbf4, .dbgchan4__Fv
int32_t dbgchan5();   // 0x1014fc1c, .dbgchan5__Fv
int32_t dbgchan6();   // 0x1014fc44, .dbgchan6__Fv
int32_t dbgchan7();   // 0x1014fc6c, .dbgchan7__Fv
int32_t dbgchan8();   // 0x1014fc94, .dbgchan8__Fv
int32_t dbgchan9();   // 0x1014fcbc, .dbgchan9__Fv
int32_t ChatterToggle();   // 0x1014d51c, .ChatterToggle__Fv
int32_t delete_ride();   // 0x1014bf74, .delete_ride__Fv
int32_t togglehud();   // 0x1014e61c, .togglehud__Fv
int32_t TestAdvisor();   // 0x1014c454, .TestAdvisor__Fv
int32_t TestAdvisor2();   // 0x1014c2b0, .TestAdvisor2__Fv
int32_t TestAdvisor3();   // 0x1014c33c, .TestAdvisor3__Fv
int32_t TestAdvisor4();   // 0x1014c3c8, .TestAdvisor4__Fv
int32_t start_cheating();   // 0x1014d470, .start_cheating__Fv
int32_t recreate_recording();   // 0x1014c538, .recreate_recording__Fv
int32_t recreate_recording_real_time();   // 0x1014c594, .recreate_recording_real_time__Fv
int32_t recreate_recording_medium_speed();   // 0x1014c5f8, .recreate_recording_medium_speed__Fv
int32_t save_recording();   // 0x1014c660, .save_recording__Fv
int32_t create_arrival_vehicle();   // 0x1014da60, .create_arrival_vehicle__Fv
int32_t clear_patrol_area();   // 0x1014cfe4, .clear_patrol_area__Fv
int32_t next_debug_text_mode();   // 0x1014f5f8, .next_debug_text_mode__Fv
int32_t win_ticket_0();   // 0x1014ed40, .win_ticket_0__Fv
int32_t win_ticket_1();   // 0x1014edd8, .win_ticket_1__Fv
int32_t win_ticket_2();   // 0x1014ee70, .win_ticket_2__Fv
int32_t win_ticket_3();   // 0x1014ef08, .win_ticket_3__Fv
int32_t win_ticket_4();   // 0x1014efa0, .win_ticket_4__Fv
int32_t win_ticket_5();   // 0x1014f038, .win_ticket_5__Fv
int32_t win_ticket_6();   // 0x1014f0d0, .win_ticket_6__Fv
int32_t win_ticket_7();   // 0x1014f16c, .win_ticket_7__Fv
int32_t win_ticket_8();   // 0x1014f208, .win_ticket_8__Fv
int32_t win_ticket_9();   // 0x1014f2a4, .win_ticket_9__Fv
int32_t quickload_layout();   // 0x1014c97c, .quickload_layout__Fv
int32_t quicksave_layout();   // 0x1014c89c, .quicksave_layout__Fv
int32_t mainmenu();   // 0x1014e6c8, .mainmenu__Fv
int32_t Screen_BuyObject();   // 0x1014e714, .Screen_BuyObject__Fv
int32_t Screen_BuyStaff();   // 0x1014e768, .Screen_BuyStaff__Fv
int32_t Screen_ParkStatus();   // 0x1014e7bc, .Screen_ParkStatus__Fv
int32_t Screen_AllItems();   // 0x1014e810, .Screen_AllItems__Fv
int32_t Screen_AllStaff();   // 0x1014e864, .Screen_AllStaff__Fv
int32_t Screen_AllPeeps();   // 0x1014e8b8, .Screen_AllPeeps__Fv
int32_t Screen_Finance();   // 0x1014e90c, .Screen_Finance__Fv
int32_t Screen_Loans();   // 0x1014e960, .Screen_Loans__Fv
int32_t Screen_StaffCosts();   // 0x1014e9b0, .Screen_StaffCosts__Fv
int32_t Screen_EntryPrice();   // 0x1014ea04, .Screen_EntryPrice__Fv
int32_t Screen_Research();   // 0x1014eb50, .Screen_Research__Fv
int32_t Screen_Map();   // 0x1014ea58, .Screen_Map__Fv
int32_t Screen_StaffLoc();   // 0x1014eaa8, .Screen_StaffLoc__Fv
int32_t Screen_PeepLoc();   // 0x1014eafc, .Screen_PeepLoc__Fv
int32_t TakePostCard();   // 0x1014e678, .TakePostCard__Fv
int32_t CamcorderMode();   // 0x1014eba4, .CamcorderMode__Fv
int32_t abort_coaster_edit();   // 0x1014cdd4, .abort_coaster_edit__Fv
int32_t AnaglyphToggle();   // 0x1014fce4, .AnaglyphToggle__Fv
int32_t abort_all_requesters();   // 0x1014cb8c, .abort_all_requesters__Fv
int32_t turn_object_clockwise();   // 0x1014ca74, .turn_object_clockwise__Fv
int32_t turn_object_counterclockwise();   // 0x1014cafc, .turn_object_counterclockwise__Fv
int32_t enter_area_delete_mode();   // 0x1014ec48, .enter_area_delete_mode__Fv
int32_t togglegamewindow();   // 0x1014e5c8, .togglegamewindow__Fv
int32_t backtrack();   // 0x1014c010, .backtrack__Fv
int32_t slow_down_time();   // 0x1014d2c0, .slow_down_time__Fv
int32_t speed_up_time();   // 0x1014d314, .speed_up_time__Fv
int32_t normal_speed_time();   // 0x1014d364, .normal_speed_time__Fv
int32_t close_park();   // 0x1014d414, .close_park__Fv
int32_t open_park();   // 0x1014d3bc, .open_park__Fv
int32_t quickload();   // 0x1014c7bc, .quickload__Fv
int32_t quicksave();   // 0x1014c6dc, .quicksave__Fv
int32_t context_help();   // 0x1014f340, .context_help__Fv
int32_t show_version();   // 0x1014f39c, .show_version__Fv
int32_t pause();   // 0x1014c690, .pause__Fv
int32_t ToggleHelpText();   // 0x1014ebf4, .ToggleHelpText__Fv
int32_t snapshot();   // 0x1014e518, .snapshot__Fv
int32_t continuous_snapshot();   // 0x1014e560, .continuous_snapshot__Fv
int32_t DeactivateWeather();   // 0x1014d83c, .DeactivateWeather__Fv
int32_t WeatherReset();   // 0x1014d7e0, .WeatherReset__Fv
int32_t WeatherDownLots();   // 0x1014d74c, .WeatherDownLots__Fv
int32_t WeatherUpLots();   // 0x1014d6bc, .WeatherUpLots__Fv
int32_t WeatherDown();   // 0x1014d62c, .WeatherDown__Fv
int32_t WeatherUp();   // 0x1014d5a0, .WeatherUp__Fv
void ToggleFullScreen();   // engine 0x1003281c (import), ToggleFullScreen__Fv

#include "TbDynamicStringTemplate.h"
extern int32_t gTestAdvisorIndex;             // 0x102a4e54, original name unknown (starts 0x103): the advisor response TestAdvisor calls
extern int32_t gKeyHudToggle;     // 0x1037f238 (name invented): togglehud flips it and passes it to GAMESYSTEM_SetHudState
extern int32_t gKeyMovieToggle;   // 0x1037f23c (name invented): continuous_snapshot flips it and passes it to GAMESYSTEM_ScreenshotMovie
// Unit v51108: the runtime's operator new (0x102384b8) for the staff spawns; a weak definition in the src file.
namespace keyaction_callee {
void* OperatorNew(uint32_t size);
}  // namespace keyaction_callee
extern TbStringBase<char> gQuickSaveSuffix;   // 0x1037f248, original name unknown: appended to the theme name by the layout quick save and load
