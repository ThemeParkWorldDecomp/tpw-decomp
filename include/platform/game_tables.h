// game_tables.h: the original program's data tables, read at run time from the player's own copy of the game
// (platform/game_tables.cpp). They are not in this repository and, by default, not in the build either: the launcher
// reads them from a full Mac "SimTheme Park" or "Theme Park World" application (its data fork, a PowerPC PEF file) before
// the static initialisers run. The tables, as devtools/gen_game_tables.py writes them for a build made with
// -DTPW_TABLES_AT_CONFIGURE=ON (which compiles them in and makes LoadGameTables a no-op):
//
//   gAdvisorLines                the advisor's response table, 611 rows of 0x20 bytes (Advisor.h)
//   gAdvisorMessageBlock         the advisor message table, 351 records of 0x30 bytes, with record -1 and a spare zero record
//   gAdvisorBalanceDescriptions  the description rows of the advisor balance table, 775 of 0x3c bytes (AdvisorScoring.h)
//   gAdvisorScorerSources        the 351 scorers, pointers to member of CAdvisorScoring (241 distinct)
//   gSaveLegalText               the save file header's seven legal texts (SaveLoad.h; GameTableLegalText below)
//   the 48 window scripts        the park, status and online panels' scripts, the int16_t arrays src/game/ui defines
//   gRideTemplates               BMP_CreateRide's 14 ride templates, 52 words each (BMP_InsideArea.h)
//
// A program is known by the SHA-1 of its data fork (the CD's 2.0.1, the 2.04b3 patch's, 2.1.1 and 2.1.4, with the
// updaters' copies: include/platform/game_table_lists.inc) and read where that build holds the tables; any other full Mac
// build is searched by content, the way gen_game_tables.py does. Either way every block is checked whole by its SHA-1.
#pragma once
#include <cstdint>
#include <string>

struct AdvisorLine;
struct AdvisorMessageRecord;
struct BalanceDescription;
class CAdvisorScoring;
typedef int32_t (CAdvisorScoring::*AdvisorScorer)();   // as AdvisorScoring.h declares it

// Reads the tables from programDataFork into the game's globals (the arrays above, and gSaveLegalText rebuilt from the
// loaded texts). False, with the reason in *why, when the file cannot be read or is not a full Mac build whose tables are
// the known ones; the globals are then left as they were. Called once by the launcher, before RunStaticInitialisers (the
// scorers' static initialiser copies gAdvisorScorerSources into the message table). A build with the tables compiled in
// (TPW_TABLES_AT_CONFIGURE) reads nothing and answers true.
bool LoadGameTables(const char* programDataFork, std::string* why);

namespace tpw {

// The save file header's legal text for a language (0 English, 1 Spanish, 2 Italian, 3 Swedish, 4 German, 5 French,
// 6 Japanese: one Shift-JIS code unit per element), as LoadGameTables read it; empty before that, for another language,
// and in a build with the tables compiled in (gSaveLegalText holds them there).
const std::u16string& GameTableLegalText(int language);

// The build LoadGameTables read the tables from (a known build's name, or "found by content"); "compiled in" in a build
// with the tables compiled in; empty before LoadGameTables.
const std::string& GameTableSource();

// The counts the tables have in every build.
constexpr int kGameTableAdvisorLines = 611;
constexpr int kGameTableMessageRecords = 351;          // gAdvisorMessageBlock holds two more (record -1 and the spare)
constexpr int kGameTableBalanceRows = 775;
constexpr int kGameTableScorers = 241;                 // distinct functions among the 351 scorer sources
constexpr int kGameTableLegalTexts = 7;
constexpr int kGameTableWindowScripts = 48;
constexpr int kGameTableRideTemplateWords = 14 * 0x34;

// Where ReadGameTables puts each table: arrays of the sizes above (messageBlock kGameTableMessageRecords + 2, scorers
// kGameTableMessageRecords, each window script GameTableWindowScriptWords(i) words), all of them non-null.
struct GameTableDestinations {
    AdvisorLine* lines;
    AdvisorMessageRecord* messageBlock;
    BalanceDescription* balance;
    AdvisorScorer* scorers;
    std::u16string* legalTexts;
    int16_t* windowScripts[kGameTableWindowScripts];
    uint32_t* rideTemplates;
};

// The window scripts in the order of game_table_lists.inc: the word count and the name of the array src/game/ui defines.
int GameTableWindowScriptWords(int index);
const char* GameTableWindowScriptName(int index);

// The reader under LoadGameTables, into the caller's arrays (a host test compares them with the configure-time tables).
// searchOnly: find the tables by content even when the program's SHA-1 is a known build's. *build (if not null) names the
// known build, or says that the content search found the tables. On failure nothing in `to` is defined.
bool ReadGameTables(const char* programDataFork, const GameTableDestinations& to, bool searchOnly, std::string* build,
                    std::string* why);

}  // namespace tpw
