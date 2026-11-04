// The online world behind the Mac's online library (Auth, News, City, Upload: include/FS_Online.h).
//
// The port has no online library; platform/online_library.cpp defines its four classes. With an online server set by the
// launcher (include/platform/online_boss.h) every call goes to the online server client. Otherwise each request goes to the
// backend set here, in plain records; the launcher sets none, so with neither every Connect answers 0 and the game shows its
// own "cannot connect", as before. notes/online-screens.md maps the screens and the formats.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tpw::online {

// The account the login answers (the library's 0x6c-byte block; FS_Online.cpp LoginConnectionCallback).
struct Account {
    int32_t playerID = 0;
    int32_t customerID = 0;
    std::u16string email;
    uint32_t childLock = 0x3c;   // the bits that allow: 0x4 and 0x8 visits, 0x10 publishing and the lobby, 0x20 e-mail
    uint32_t avatar = 0;
};

// A city of the online globe (ccCityInfo).
struct City {
    int32_t id = 0;
    std::u16string name;
    std::u16string text08;
    std::u16string text1C;
    float x = 0, y = 0, z = 0;   // the flag's place on the globe (a point at the globe's radius, about 100)
    int32_t parkCount = 0;       // the flag's size; the visit globe shows a city with parks
    int32_t f20 = 0, f24 = 0;
    int32_t closed = 0;          // nonzero: the publish globe leaves it out
    int32_t room = 0;            // the publish globe shows a city whose park count is below this
};

// A published park (ccParkInfo).
struct Park {
    int32_t id = 0;
    std::u16string name;
    std::u16string description;  // the park info panel's text box
    std::u16string owner;
    int32_t city = 0;
    int32_t theme = 0;           // a theme list id
    int32_t visits = 0;          // shown as the park's number of visits
    int32_t votes = 0;
    int32_t rank = 0;            // 1 for the most voted park of the world; 0: no rank
    int32_t flagged = 0;
    int32_t year = 0, month = 0, day = 0;   // the day it was published
    std::string fileName;        // the first 19 characters name the park's folder in the player's online folder
    std::u16string email;        // the author's e-mail address, the panel's "Email" line (never kept: shown as not given)
    std::filesystem::path file;  // the park's .LAYS file, which a visit loads
};

// A theme (ccThemeInfo): the id parks carry and the theme's folder name (jungle, hallow, fantasy, space).
struct Theme {
    int32_t id = 0;
    std::u16string name;
};

// A server of the logical server list (ccServerInfo): type 1 is the park download server, 0x64 the upload server.
struct Server {
    int32_t id = 0;
    int32_t type = 0;
    std::u16string host;
    uint16_t port = 0;
};

// What a publish sends (Upload::UploadParkRequest's ccParkFileInfo and the file).
struct Publish {
    std::filesystem::path file;   // the saved park
    int32_t city = 0;
    int32_t theme = 0;
    int32_t flag = 0;
    std::u16string name;
    std::u16string description;
    std::u16string owner;
};

// What a sent e-mail carries (a postcard: SendPostcard 0x101b0f9c, or the outbox): the card is the attachment, a JPEG.
struct Mail {
    std::string subject;
    std::vector<std::string> to;
    std::filesystem::path attachment;
};

class Backend {
public:
    virtual ~Backend() = default;
    // False refuses the login.
    virtual bool Login(const std::u16string& name, const std::u16string& password, Account* account) = 0;
    // The server details (online.sam's format, written to onlineoverride.sam and loaded over online.sam).
    virtual std::string ServerDetails() = 0;
    // The news panel's two texts, in the game's language (a Windows language id) where the backend has it.
    virtual std::u16string GameNews(uint32_t language) = 0;
    virtual std::u16string ServerNews(uint32_t language) = 0;
    virtual std::vector<City> Cities() = 0;
    virtual std::vector<Theme> Themes() = 0;
    virtual std::vector<Server> Servers() = 0;
    virtual std::vector<Park> ParksInCity(int32_t city) = 0;
    virtual std::vector<Park> TopTen(int32_t city) = 0;
    virtual std::vector<Park> ParksByOwner(const std::u16string& owner) = 0;
    virtual std::vector<Park> ParkByID(int32_t park) = 0;
    // Keeps a published park; false (with the reason logged) when it cannot.
    virtual bool PublishPark(const Publish& publish) = 0;
    virtual void MarkVisited(int32_t park) = 0;
    virtual void UnpublishPark(int32_t park, int32_t player) = 0;
    // Takes an e-mail the game sends while logged in; false (the game shows its own error) when it cannot.
    virtual bool SendMail(const Mail& mail) = 0;
};

// The backend the library classes use; nullptr (the default) for none.
void SetBackend(std::unique_ptr<Backend> backend);
Backend* CurrentBackend();

}  // namespace tpw::online
