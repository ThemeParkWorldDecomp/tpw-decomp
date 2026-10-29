// old_text_tables.h: the older build of the language files, read by the program they were not made for. The languages
// that came only on the older discs (Spanish, Dutch, Italian and Polish, and the Chinese ones' string tables) were made
// for an earlier program than the Mac 2.04b3 the port is: their UITEXT table has 473 strings, not 474 (the later build
// inserted one at 207, so every later string moved up by one), CHAT_COMMANDS has 176, not 182 (the later build appended
// six), and their ResIdx.dat (1343 bytes, not 1442) names no DATETINY, DATESMALL, DATEMED or DATEBIG font (ids 1048 to
// 1051; POSTCARD, 1048 there, is 1052 in the later one). src/game/Text.cpp (CText::Init, the port's older-tables fix) fills
// the gaps from the edition's own language, English or American, keeping every file as the disc has it, through the
// four functions at the end (platform/old_text_tables.cpp). The rest is pure index arithmetic and name matching, so
// tests/host_old_text_tables.cpp checks it on its own.
#ifndef TPW_PLATFORM_OLD_TEXT_TABLES_H
#define TPW_PLATFORM_OLD_TEXT_TABLES_H

#include <cctype>
#include <cstdint>
#include <vector>

constexpr uint32_t kOldTextUITextTable = 1006;          // UITEXT::TableID
constexpr uint32_t kOldTextChatCommandsTable = 1008;    // CHAT_COMMANDS::TableID
constexpr uint32_t kOldTextUITextCount = 473;           // the older build's count; the later one has 474
constexpr uint32_t kOldTextUITextInserted = 207;        // the string the later build inserted
constexpr uint32_t kOldTextChatCommandsCount = 176;     // the older build's count; the later one has 182
constexpr uint32_t kOldTextChatCommandsNewCount = 182;
constexpr uint32_t kOldTextPostcardFont = 1052;         // POSTCARD in the later ResIdx.dat; the older one has no 1052

// Whether the table `id` with `count` strings is an older build's UITEXT or CHAT_COMMANDS.
inline bool IsOldTextTable(uint32_t id, uint32_t count) {
    return (id == kOldTextUITextTable && count == kOldTextUITextCount) ||
           (id == kOldTextChatCommandsTable && count == kOldTextChatCommandsCount);
}

// The table `id` as the later build has it, from the language's older table (`language`, `languageCount` strings) and
// the base language's later one (`base`, `baseCount`): UITEXT with the base's string 207 inserted, CHAT_COMMANDS with the
// base's 176 to 181 appended. False, leaving `out` alone, when the language's table is not an older one or the base's
// is not a later one.
template <class String>
bool MergeOldTextTable(uint32_t id, const String* language, uint32_t languageCount, const String* base,
                       uint32_t baseCount, std::vector<String>& out) {
    if (id == kOldTextUITextTable && languageCount == kOldTextUITextCount && baseCount == kOldTextUITextCount + 1) {
        out.assign(language, language + kOldTextUITextInserted);
        out.push_back(base[kOldTextUITextInserted]);
        out.insert(out.end(), language + kOldTextUITextInserted, language + languageCount);
        return true;
    }
    if (id == kOldTextChatCommandsTable && languageCount == kOldTextChatCommandsCount &&
        baseCount == kOldTextChatCommandsNewCount) {
        out.assign(language, language + languageCount);
        out.insert(out.end(), base + languageCount, base + baseCount);
        return true;
    }
    return false;
}

// Whether two of ResIdx.dat's file names are the same file, ignoring case as the Mac's file system does.
inline bool SameOldTextFileName(const char* a, const char* b) {
    for (; *a != '\0' && *b != '\0'; ++a, ++b)
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) return false;
    return *a == *b;
}

// The record of the index records `first` to `last` (inclusive; TbIResourceIndex's resourceIndexItem: mType and
// mFilenameOffset into `names`) for the file `name` of type `type`; nullptr if none.
template <class Item>
Item* FindOldTextResource(Item* first, Item* last, const uint8_t* names, const char* name, uint16_t type) {
    if (first == nullptr) return nullptr;
    for (Item* item = first; item <= last; ++item)
        if (item->mType == type && SameOldTextFileName(reinterpret_cast<const char*>(names) + item->mFilenameOffset, name))
            return item;
    return nullptr;
}

// CText::Init's fix (platform/old_text_tables.cpp; the Text unit test's stubs leave the original's path). The edition's
// own language is opened by the first call that needs it and closed by OldTextDone.
class TbTextResourceManager;
class TbIFont;

// The table `*table` that `language` gave for CText's table `id` (`name` in the log): an older build's UITEXT or
// CHAT_COMMANDS is replaced by the later build's, made with the edition's own language's strings. True when replaced.
bool OldTextTable(TbTextResourceManager& language, void** table, uint32_t id, const char* name);

// Whether `language`'s ResIdx.dat is the older build's (it names no font 1052) and the edition's own language is there
// to find its fonts by.
bool OldTextIndex(TbTextResourceManager& language);

// The font `id` of a language whose index is the older build's: the file the edition's own language's index names for
// it, from the language's folder when its index names that file too, else from the edition's own folder. False when
// that index has no such font (the original's load then runs, and fails as it did).
bool OldTextFont(TbTextResourceManager& language, TbIFont** out, uint32_t id);

// The edition's own language closed; the tables and fonts taken from it keep their own references.
void OldTextDone();

#endif
