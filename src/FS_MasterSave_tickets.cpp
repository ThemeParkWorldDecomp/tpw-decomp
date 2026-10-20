// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3), third part (unit w89): the golden tickets the player has won
// and spent, the current theme, the research and first-time-player flags, the swear filter, and the destructors. The ticket
// kinds are indexes (no range check anywhere): a local ticket indexes a park's six bytes, a global one the save's four bytes and
// the park's four awards, a secret one the save's two bytes.
#include "FS_MasterSave.h"

#include "Debug.h"

// ---- CFSGlobalParkData

// Takes the balance table down when the park has read one: the table's own destructor releases its rows (CBalanceTable::UnInit)
// and the memory is freed. The name is destroyed with the record.
CFSGlobalParkData::~CFSGlobalParkData() { delete mBalance; }

bool CFSGlobalParkData::HaveTicket(GoldenTicketTypeLocal type) const { return mTickets[type] != 0; }

// A ticket counts as won only when its byte is exactly 1; any other value (the saved file could hold one) is won again.
bool CFSGlobalParkData::EarnedTicket(GoldenTicketTypeLocal type) {
    if (mTickets[type] == 1) return false;
    mTickets[type] = 1;
    return true;
}

bool CFSGlobalParkData::AwardInThisPark(GoldenTicketTypeGlobal type) const { return mAwards[type] != 0; }

int32_t CFSGlobalParkData::ScoreForAward(GoldenTicketTypeGlobal type) const {
    if (mAwards[type] != 0) return mAwardScores[type];
    return -1;
}

void CFSGlobalParkData::ClearAward(GoldenTicketTypeGlobal type) { mAwards[type] = 0; }

void CFSGlobalParkData::SetAward(GoldenTicketTypeGlobal type, int32_t score) {
    mAwards[type] = 1;
    mAwardScores[type] = score;
}

// ---- CFSGlobalMasterSave

// The records are deleted by the theme map's clearing; the containers are taken down with the object.
CFSGlobalMasterSave::~CFSGlobalMasterSave() { ClearThemeMap(); }

// Every record is deleted, then the map is emptied. The current theme's pointer is left as it is (it dangles).
void CFSGlobalMasterSave::ClearThemeMap() {
    for (const auto& theme : mThemes) delete theme.second;
    mThemes.clear();
}

// The tickets that count: the global ones won, and the local ones of every park whose balance file reads (each park is asked
// in the map's order, which makes its file be read when it was not yet).
int32_t CFSGlobalMasterSave::HowManyTickets() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    return tickets;
}

int32_t CFSGlobalMasterSave::HowManyTicketsIncludingSecrets() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
    return tickets;
}

// The tickets that count, secret ones included, less what was spent.
int32_t CFSGlobalMasterSave::SpendingPower() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
    return tickets - mTicketsSpent;
}

// A thing is bought once: the cost is added to what was spent only the first time.
bool CFSGlobalMasterSave::SpendTicketOn(uint16_t thing, int32_t cost) {
    const bool bought = mBought.insert(thing).second;
    if (bought) mTicketsSpent += cost;
    return bought;
}

bool CFSGlobalMasterSave::HasBeenBought(uint16_t thing) const { return mBought.find(thing) != mBought.end(); }

bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeLocal type) const { return mCurrent->HaveTicket(type); }

bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeSecret type) const { return mSecretTickets[type] != 0; }

int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeLocal type) {
    if (!mCurrent->EarnedTicket(type)) return 0;
    return CheckTicketTypeWon();
}

// A global ticket won already can still move its award: when the current park does not hold the award, the first park (in the
// map's order) that holds it with a lower score loses it to the current park, and the answer is 4. A ticket won for the first
// time sets the award in the current park and answers like the others.
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeGlobal type, int32_t score) {
    if (mGlobalTickets[type] == 1) {
        if (!mCurrent->AwardInThisPark(type)) {
            for (const auto& theme : mThemes) {
                if (theme.second->AwardInThisPark(type) && theme.second->ScoreForAward(type) < score) {
                    theme.second->ClearAward(type);
                    mCurrent->SetAward(type, score);
                    return 4;
                }
            }
        }
        return 0;
    }
    mGlobalTickets[type] = 1;
    mCurrent->SetAward(type, score);
    return CheckTicketTypeWon();
}

int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeSecret type) {
    if (mSecretTickets[type] == 1) return 0;
    mSecretTickets[type] = 1;
    return CheckTicketTypeWon();
}

// What a newly won ticket did: 1 nothing (no keys held, or the tickets are not a whole number of keys), 2 a key was won and no
// park costs exactly the keys held, 3 a key was won and a park costs exactly that many (it can be entered now). The total is
// tested the way the original does, `total == (total / 3) * 3`; HowManyKeys is asked again for every park.
int32_t CFSGlobalMasterSave::CheckTicketTypeWon() {
    if (HowManyKeys() > 0) {
        int32_t tickets = 0;
        for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
        for (const auto& theme : mThemes) {
            if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
        }
        for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
        if (tickets == (tickets / 3) * 3) {
            for (const auto& theme : mThemes) {
                const int32_t cost = theme.second->KeyCostToEnter();
                if (HowManyKeys() == cost) return 3;
            }
            return 2;
        }
    }
    return 1;
}

// The theme becomes the current one; a theme the map does not have gets a record (and a trace line says so). The balance file
// of the current theme is read at once (the answer is not used).
void CFSGlobalMasterSave::ChangeCurrentTheme(const TbDynamicStringTemplate<char>& name) {
    const auto found = mThemes.find(name);
    if (found == mThemes.end()) {
        CTRACE(3, "Trying to change to a theme (%s) which doesn't exist, better create one then \n", name.mData);
        mCurrent = CreateNewTheme(name);
    } else {
        mCurrent = found->second;
    }
    mCurrent->CheckBalanceFile();
}

void CFSGlobalMasterSave::SetSignNameA(const char16_t* text) {
    if (mCurrent != nullptr) mCurrent->SetSignNameA(text);
}

void CFSGlobalMasterSave::SetSignNameB(const char16_t* text) {
    if (mCurrent != nullptr) mCurrent->SetSignNameB(text);
}

void CFSGlobalMasterSave::SetSwearFilter(bool on) { mSwearFilter = on; }

bool CFSGlobalMasterSave::IsSwearFilterOn() const { return mSwearFilter != 0; }

bool CFSGlobalMasterSave::HasAllResearchBeenCompleted() {
    for (const auto& theme : mThemes) {
        if (theme.second->mAllResearchCompleted == 0) return false;
    }
    return true;
}

void CFSGlobalMasterSave::SetAllResearchCompletedInCurrentTheme() {
    if (mCurrent != nullptr) mCurrent->mAllResearchCompleted = 1;
}

bool CFSGlobalMasterSave::IsFirstTimePlayer() { return mFirstTimePlayer != 0; }

void CFSGlobalMasterSave::ClearFirstTimePlayer() { mFirstTimePlayer = 0; }
