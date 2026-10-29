// host_old_text_tables.cpp: checks include/platform/old_text_tables.h (an older build's UITEXT and CHAT_COMMANDS made
// the later build's, and its fonts found by file name) on its own, with no game and no files. Run by ctest
// ("old_text_tables"); exits nonzero on the first failure.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "platform/old_text_tables.h"

static int gChecks = 0;

#define CHECK(condition)                                                                \
    do {                                                                                \
        ++gChecks;                                                                      \
        if (!(condition)) {                                                             \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            std::exit(1);                                                               \
        }                                                                               \
    } while (0)

// `count` strings named "<prefix><index>".
static std::vector<std::string> Strings(const char* prefix, uint32_t count) {
    std::vector<std::string> strings;
    for (uint32_t i = 0; i < count; ++i) strings.push_back(prefix + std::to_string(i));
    return strings;
}

// A record as TbIResourceIndex's resourceIndexItem has it.
struct Item {
    uint32_t mId;
    uint16_t mType;
    uint16_t mPad;
    uint32_t mFilenameOffset;
};

int main() {
    const auto spanish = Strings("es", 473);
    const auto english = Strings("en", 474);
    std::vector<std::string> out;

    // UITEXT: the older build's 473 strings with English's 207 inserted. The game's date (the later build's 208) and the
    // entry price find the language's strings again.
    CHECK(IsOldTextTable(kOldTextUITextTable, 473));
    CHECK(!IsOldTextTable(kOldTextUITextTable, 474));
    CHECK(MergeOldTextTable(kOldTextUITextTable, spanish.data(), 473, english.data(), 474, out));
    CHECK(out.size() == 474);
    CHECK(out[0] == "es0" && out[206] == "es206");
    CHECK(out[207] == "en207");
    CHECK(out[208] == "es207" && out[473] == "es472");

    // CHAT_COMMANDS: the older build's 176 with English's 176 to 181 appended.
    const auto chat = Strings("es", 176);
    const auto englishChat = Strings("en", 182);
    CHECK(IsOldTextTable(kOldTextChatCommandsTable, 176));
    CHECK(!IsOldTextTable(kOldTextChatCommandsTable, 182));
    CHECK(MergeOldTextTable(kOldTextChatCommandsTable, chat.data(), 176, englishChat.data(), 182, out));
    CHECK(out.size() == 182);
    CHECK(out[0] == "es0" && out[175] == "es175");
    CHECK(out[176] == "en176" && out[181] == "en181");

    // Anything else is left alone: the later build's own tables, another table of 473 strings, a base that is not the
    // later build.
    out.assign(1, "kept");
    CHECK(!MergeOldTextTable(kOldTextUITextTable, english.data(), 474, english.data(), 474, out));
    CHECK(!MergeOldTextTable(kOldTextChatCommandsTable, englishChat.data(), 182, englishChat.data(), 182, out));
    CHECK(!IsOldTextTable(1007, 473));
    CHECK(!MergeOldTextTable(1007, spanish.data(), 473, english.data(), 474, out));
    CHECK(!MergeOldTextTable(kOldTextUITextTable, spanish.data(), 473, spanish.data(), 473, out));
    CHECK(!MergeOldTextTable(kOldTextChatCommandsTable, chat.data(), 176, chat.data(), 176, out));
    CHECK(out.size() == 1 && out[0] == "kept");

    // Pointer tables, as CText::Init has them.
    const unsigned char* a[473];
    const unsigned char* b[474];
    for (auto& p : a) p = reinterpret_cast<const unsigned char*>("a");
    for (auto& p : b) p = reinterpret_cast<const unsigned char*>("b");
    std::vector<const unsigned char*> pointers;
    CHECK(MergeOldTextTable(kOldTextUITextTable, a, 473, b, 474, pointers));
    CHECK(pointers.size() == 474 && pointers[207] == b[207] && pointers[208] == a[207]);

    // Fonts by file name, the case ignored: the older index (no DATE fonts, POSTCARD as 1048) and a later one's names.
    CHECK(SameOldTextFileName("DateBig.fnt", "DATEBIG.FNT"));
    CHECK(!SameOldTextFileName("DateBig.fnt", "DateBig.fn"));
    CHECK(!SameOldTextFileName("DateBig", "DateBig.fnt"));
    CHECK(SameOldTextFileName("", ""));
    const char names[] = "MBToUnicode.mbu\0Postcard.fnt\0Big.fnt\0Big.str\0";
    Item items[] = {{1, 2, 0, 0}, {1048, 4, 0, 16}, {1047, 4, 0, 29}, {1006, 3, 0, 37}};
    const auto* bytes = reinterpret_cast<const uint8_t*>(names);
    CHECK(FindOldTextResource(items, items + 3, bytes, "POSTCARD.FNT", 4) == &items[1]);
    CHECK(FindOldTextResource(items, items + 3, bytes, "big.fnt", 4) == &items[2]);
    CHECK(FindOldTextResource(items, items + 3, bytes, "Big.str", 4) == nullptr);   // a table, not a font
    CHECK(FindOldTextResource(items, items + 3, bytes, "Big.str", 3) == &items[3]);
    CHECK(FindOldTextResource(items, items + 3, bytes, "DateBig.fnt", 4) == nullptr);
    CHECK(FindOldTextResource<Item>(nullptr, nullptr, bytes, "Big.fnt", 4) == nullptr);

    std::printf("old_text_tables: %d checks passed\n", gChecks);
    return 0;
}
