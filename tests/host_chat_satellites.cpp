// The numbers on the online globe's five chat satellites, through the real code the launcher links: the lobby's
// ProcessGetChatSatellitePlayers and ChatServerCallbackFn (src/game/ui/ui_worldLobbyController.cpp), chat_satellite_callee::ChatConnect
// and ChatRoomInfo (platform/online_chat_glue.cpp), Argument::StrVal and the translated library's strings, over
// platform/online_boss.cpp's connection, with only the connection itself a fake. The program is linked as tpw_launcher is, with its
// own stand-ins from launcher/gen_stubs.py, so a body that is empty or missing answers 0 here just as it does in the game (a recording
// fake would prove the call and not the body, which is how these two stayed stand-ins with their tests green).
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "platform/arena.h"
#include "platform/online_boss.h"

namespace boss = tpw::online::boss;

static int gStandIns = 0;

// What the stand-ins report, launcher/main.cpp's in the game. Any of them reached on this path is a body that is not there.
void tpw_stub_called(const char* name, bool platform, bool first) {
    (void)platform;
    if (first) std::printf("a stand-in was called: %s\n", name);
    gStandIns++;
}

namespace {

using Bytes = std::vector<uint8_t>;

int gChecks = 0;
int gFailures = 0;

void Check(bool ok, const std::string& what) {
    gChecks++;
    if (ok) return;
    gFailures++;
    std::printf("FAILED: %s\n", what.c_str());
}

struct FakeLine {
    std::mutex lock;
    std::string host;
    uint16_t port = 0;
    Bytes fromClient;
    std::deque<uint8_t> toClient;
    bool closed = false;
    std::atomic<bool> clientClosed{false};
};

std::mutex gLinesLock;
std::vector<std::shared_ptr<FakeLine>> gLines;

class FakeTransport : public boss::Transport {
public:
    explicit FakeTransport(std::shared_ptr<FakeLine> line) : mLine(std::move(line)) {}
    bool Open(const std::string& host, uint16_t port, const std::atomic<bool>&) override {
        std::lock_guard<std::mutex> hold(mLine->lock);
        mLine->host = host;
        mLine->port = port;
        return true;
    }
    std::string Failure() const override { return {}; }
    int Wait(bool wantWrite, int milliseconds) override {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        do {
            {
                std::lock_guard<std::mutex> hold(mLine->lock);
                int ready = wantWrite ? 2 : 0;
                if (!mLine->toClient.empty() || mLine->closed) ready |= 1;
                if (ready != 0) return ready;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < end);
        return 0;
    }
    long Send(const uint8_t* bytes, size_t size) override {
        std::lock_guard<std::mutex> hold(mLine->lock);
        mLine->fromClient.insert(mLine->fromClient.end(), bytes, bytes + size);
        return static_cast<long>(size);
    }
    long Receive(uint8_t* bytes, size_t size) override {
        std::lock_guard<std::mutex> hold(mLine->lock);
        if (mLine->toClient.empty()) return mLine->closed ? 0 : kWouldBlock;
        size_t n = 0;
        for (; n < size && !mLine->toClient.empty(); ++n) {
            bytes[n] = mLine->toClient.front();
            mLine->toClient.pop_front();
        }
        return static_cast<long>(n);
    }
    void Close() override { mLine->clientClosed = true; }

private:
    std::shared_ptr<FakeLine> mLine;
};

std::unique_ptr<boss::Transport> MakeFake() {
    auto line = std::make_shared<FakeLine>();
    std::lock_guard<std::mutex> hold(gLinesLock);
    gLines.push_back(line);
    return std::make_unique<FakeTransport>(line);
}

std::shared_ptr<FakeLine> LastLine() {
    std::lock_guard<std::mutex> hold(gLinesLock);
    return gLines.empty() ? nullptr : gLines.back();
}

// Pumps as the game's frame loop does, until `done` or two seconds.
bool PumpUntil(const std::function<bool()>& done) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < end) {
        boss::Pump();
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    boss::Pump();
    return done();
}

bool TakeRequest(FakeLine& line, boss::Header& header, Bytes& payload) {
    bool whole = false;
    PumpUntil([&] {
        std::lock_guard<std::mutex> hold(line.lock);
        if (line.fromClient.size() < boss::kHeader) return false;
        whole = line.fromClient.size() >= boss::kHeader + boss::GetHeader(line.fromClient.data()).length;
        return whole;
    });
    if (!whole) return false;
    std::lock_guard<std::mutex> hold(line.lock);
    header = boss::GetHeader(line.fromClient.data());
    const size_t size = boss::kHeader + header.length;
    payload.assign(line.fromClient.begin() + boss::kHeader, line.fromClient.begin() + static_cast<long>(size));
    line.fromClient.erase(line.fromClient.begin(), line.fromClient.begin() + static_cast<long>(size));
    return true;
}

void AddText(Bytes& bytes, const std::string& text) {
    bytes.push_back(0x01);
    bytes.insert(bytes.end(), text.begin(), text.end());
    bytes.push_back(0xff);
}

// The reply the server's chat.go sends: the event word, its narrow arguments, the end byte.
void Answer(FakeLine& line, uint32_t seq, uint8_t event, const std::vector<std::string>& arguments) {
    Bytes body = {0, 0, 0, event};
    for (const std::string& a : arguments) AddText(body, a);
    body.push_back(0);
    boss::Header h;
    h.from = 's';
    h.type = 301;
    h.status = 0;
    h.length = static_cast<uint32_t>(body.size());
    h.context = seq;
    h.seq = seq;
    Bytes framed(boss::kHeader);
    boss::PutHeader(framed.data(), h);
    framed.insert(framed.end(), body.begin(), body.end());
    std::lock_guard<std::mutex> hold(line.lock);
    line.toClient.insert(line.toClient.end(), framed.begin(), framed.end());
}

// The parks the lobby's GetList(1) would have filled mParksInCity2 with.
void SetRooms(const std::vector<int32_t>& ids) {
    CFSOnlineSystemParkList_InCity* list = CFSOnlineSystem::Instance()->mParksInCity2;
    while (list->mList->GetHeadPosition() != nullptr) delete list->mList->RemoveHead();
    for (int32_t id : ids) {
        CFSOnlinePark* park = new CFSOnlinePark();
        park->mParkID = id;
        list->mList->AddTail(park);
    }
    list->mF28 = 1;
}

// The requests of one opening of the globe: one per room, type 300 with command 7 and the id as text, no end byte.
std::vector<uint32_t> Opened(const std::vector<int32_t>& ids, std::shared_ptr<FakeLine>& line) {
    const size_t before = gLines.size();
    gChatSatelliteWanted = 1;
    ProcessGetChatSatellitePlayers();
    Check(gChatSatelliteWanted == 0, "the lobby took the request");
    Check(gLines.size() == before + 1, "a chat connection was opened (the real ChatConnect answered 1)");
    std::vector<uint32_t> seqs;
    if (gLines.size() != before + 1) return seqs;
    line = LastLine();
    for (int32_t id : ids) {
        boss::Header h;
        Bytes payload;
        const bool got = TakeRequest(*line, h, payload);
        Check(got, "RoomInfo of park " + std::to_string(id) + " was sent (the real ChatRoomInfo)");
        if (!got) continue;
        Bytes want = {0, 0, 0, 7};
        AddText(want, std::to_string(id));
        Check(h.type == 300 && h.param == 0, "RoomInfo is a type 300 request");
        Check(payload == want, "RoomInfo of park " + std::to_string(id) + ": the command word, the id as narrow text, no end byte");
        seqs.push_back(h.seq);
    }
    Check(line->port == 6002 && line->host == "127.0.0.1", "to the server the launcher set, on the chat port");
    return seqs;
}

void TestFiveRooms() {
    const std::vector<int32_t> ids = {41, 7, 1234, 99, 5};
    SetRooms(ids);
    std::shared_ptr<FakeLine> line;
    const std::vector<uint32_t> seqs = Opened(ids, line);
    if (seqs.size() != ids.size()) return;
    for (size_t i = 0; i < ids.size(); i++) Check(gChatSatelliteParkIDs[i] == ids[i], "the satellite's park, in list order");
    // The fourth room is empty; the others have players, in the server's order of requests.
    Answer(*line, seqs[0], 7, {"", "", "41", "3"});
    Answer(*line, seqs[1], 7, {"", "", "7", "12"});
    Answer(*line, seqs[2], 7, {"", "", "1234", "250"});
    Check(PumpUntil([] { return gChatSatelliteAnswers == 3; }), "three answers counted");
    Check(gChatSatelliteFinished == 0, "not finished before the fifth answer");
    Answer(*line, seqs[3], 0x2c, {"99"});
    Answer(*line, seqs[4], 7, {"", "", "5", "1"});
    Check(PumpUntil([] { return gChatSatelliteFinished == 1; }), "finished at the fifth answer");
    const int32_t want[5] = {3, 12, 250, 0, 1};
    for (int i = 0; i < 5; i++)
        Check(gChatSatelliteValues[i] == want[i], "satellite " + std::to_string(i) + " has " + std::to_string(gChatSatelliteValues[i]));
    ProcessGetChatSatellitePlayers();
    Check(gChatSatelliteFinished == 0, "the numbers were handed to the panel once");
    Check(PumpUntil([&] { return line->clientClosed.load(); }), "the connection is let go once the numbers are in");
}

// Fewer rooms than satellites: the server closes after its linger (chat.go chatRoomInfoLinger), which is -4 to the callback.
void TestTwoRoomsAndClose() {
    const std::vector<int32_t> ids = {8, 9};
    SetRooms(ids);
    std::memset(gChatSatelliteValues, 0, sizeof gChatSatelliteValues);   // the game clears only the rooms it asks about
    std::shared_ptr<FakeLine> line;
    const std::vector<uint32_t> seqs = Opened(ids, line);
    if (seqs.size() != ids.size()) return;
    Answer(*line, seqs[0], 7, {"", "", "8", "6"});
    Answer(*line, seqs[1], 0x2c, {"9"});
    Check(PumpUntil([] { return gChatSatelliteAnswers == 2; }), "two answers counted");
    Check(gChatSatelliteFinished == 0, "not finished with two answers");
    {
        std::lock_guard<std::mutex> hold(line->lock);
        line->closed = true;
    }
    Check(PumpUntil([] { return gChatSatelliteFinished == 1; }), "the server's close finishes it");
    Check(gChatSatelliteValues[0] == 6 && gChatSatelliteValues[1] == 0, "the two rooms' numbers");
    Check(gChatSatelliteValues[2] == 0 && gChatSatelliteValues[3] == 0 && gChatSatelliteValues[4] == 0, "no number for the rest");
    ProcessGetChatSatellitePlayers();
    Check(gChatSatelliteFinished == 0, "the numbers were handed to the panel once");
}

// More rooms than satellites: the first five. A second opening of the globe asks again and takes the new numbers.
void TestSixRoomsAndAgain() {
    const std::vector<int32_t> ids = {1, 2, 3, 4, 5};
    SetRooms({1, 2, 3, 4, 5, 6});
    for (int round = 0; round < 2; round++) {
        std::shared_ptr<FakeLine> line;
        const std::vector<uint32_t> seqs = Opened(ids, line);
        if (seqs.size() != ids.size()) return;
        {
            std::lock_guard<std::mutex> hold(line->lock);
            Check(line->fromClient.empty(), "nothing is asked of the sixth room");
        }
        for (size_t i = 0; i < ids.size(); i++)
            Answer(*line, seqs[i], 7, {"", "", std::to_string(ids[i]), std::to_string((round + 1) * 10 + static_cast<int>(i))});
        Check(PumpUntil([] { return gChatSatelliteFinished == 1; }), "finished at the fifth answer");
        for (size_t i = 0; i < ids.size(); i++)
            Check(gChatSatelliteValues[i] == (round + 1) * 10 + static_cast<int>(i), "this opening's numbers");
        ProcessGetChatSatellitePlayers();
    }
}

}  // namespace

int main() {
    tpw::ArenaBase();
    boss::SetTransportMaker(MakeFake);
    boss::SetServer("127.0.0.1");
    gOnlineConfig.mChatServer = "chat.eagames.invalid";
    gOnlineConfig.mChatPort = 6002;

    TestFiveRooms();
    TestTwoRoomsAndClose();
    TestSixRoomsAndAgain();

    boss::SetServer("");
    boss::Pump();
    Check(gStandIns == 0, "no stand-in was reached");
    std::printf("chat_satellites: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
