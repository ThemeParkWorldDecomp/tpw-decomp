// Test library for Outbox::Save and Outbox::WriteMailData (unit v5530; libtpw_unit_Outbox_WriteMailData.dylib, tests/test_Outbox_WriteMailData.py), main app:
//   src/game/online/email.cpp   Outbox::Save, Outbox::WriteMailData
//
// The stream functions (SaveStream*), time, localtime and MoveFile are recording fakes that answer from the case's queues. The Outbox is a
// host object (std::string, std::list); the test sets and reads its parts through the tpw_ow_* functions. Outbox::Outbox is defined here
// (the real one is in a file this library does not compile).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <string>

#include "calllog.h"
#include "email.h"
#include "platform/arena.h"
#include "platform/macdoze.h"

using namespace calllog;

Outbox::Outbox() {
    mEmail = nullptr;
    mF54 = 25;
    mF58 = 0;
    std::memset(mF1C, 0, sizeof(mF1C));
    std::memset(mF4C, 0, sizeof(mF4C));
}

Outbox::~Outbox() {}   // the real one deletes mEmail, which stays NULL here

namespace {
Outbox* gBox = nullptr;
int32_t gTm[9];
}  // namespace

// ---- the fakes (same event names and arguments as the module's PowerPC stubs)
void SaveStreamCtor(SaveStream* s) {
    ev("SaveStreamCtor", {});
    s->ios = static_cast<SaveIos*>(tpw::ArenaAllocZeroed(sizeof(SaveIos)));
    s->filebufOpen = 0;
    s->impl = nullptr;
    s->ios->mRdbuf = static_cast<uint32_t>(ret("rdbuf"));
    s->ios->mState = static_cast<uint32_t>(ret("state"));
    s->ios->mMask = static_cast<uint32_t>(ret("mask"));
}

bool SaveStreamOpen(SaveStream* s, const char* name) {
    ev("SaveStreamOpen", {}, {name});
    s->filebufOpen = static_cast<uint32_t>(ret("opened"));
    return ret("open") != 0;
}

void SaveStreamWrite(SaveStream*, const char* text) { ev("SaveStreamWrite", {}, {text}); }

bool SaveStreamClose(SaveStream*) {
    ev("SaveStreamClose", {});
    return ret("close") != 0;
}

void SaveStreamDestroy(SaveStream*) { ev("SaveStreamDestroy", {}); }

void SaveStreamThrow(const char* what) { ev("SaveStreamThrow", {}, {what}); }

namespace outboxwrite_callee {
int32_t Time() {
    ev("Time", {});
    return static_cast<int32_t>(ret("Time"));
}
const int32_t* LocalTime(const int32_t* seconds) {
    ev("LocalTime", {static_cast<uint32_t>(*seconds)});
    std::memset(gTm, 0, sizeof(gTm));
    gTm[3] = static_cast<int32_t>(ret("tm_mday"));
    gTm[4] = static_cast<int32_t>(ret("tm_mon"));
    gTm[5] = static_cast<int32_t>(ret("tm_year"));
    return gTm;
}
}  // namespace outboxwrite_callee

namespace NS_MacDoze {
int32_t MoveFile(const char* from, const char* to) {
    ev("MoveFile", {}, {from, to});
    return static_cast<int32_t>(ret("MoveFile"));
}
}  // namespace NS_MacDoze

extern "C" {

int32_t tpw_ow_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SaveIos, mRdbuf));
    out[n++] = static_cast<int64_t>(offsetof(SaveIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(SaveIos, mMask));
    return n;
}

// ---- the Outbox
void tpw_ow_reset() {
    delete gBox;
    gBox = new Outbox();
}

// which: 0 mF00, 1 mF04, 2 mF08, 3 mF18
void tpw_ow_set_str(int32_t which, const char* text) {
    std::string* targets[] = {&gBox->mF00, &gBox->mF04, &gBox->mF08, &gBox->mF18};
    *targets[which] = text;
}

const char* tpw_ow_get_str(int32_t which) {
    std::string* targets[] = {&gBox->mF00, &gBox->mF04, &gBox->mF08, &gBox->mF18};
    return targets[which]->c_str();
}

void tpw_ow_add_mail_text(const char* text) { gBox->mF0C.push_back(text); }

int32_t tpw_ow_mail_count() { return static_cast<int32_t>(gBox->mF0C.size()); }

const char* tpw_ow_mail_text(int32_t index) {
    auto it = gBox->mF0C.begin();
    std::advance(it, index);
    return it->c_str();
}

// which: 0 mF28, 1 mF34, 2 mF40
void tpw_ow_push(int32_t which, uint32_t value) {
    std::list<uint32_t>* targets[] = {&gBox->mF28, &gBox->mF34, &gBox->mF40};
    targets[which]->push_back(value);
}

int32_t tpw_ow_count(int32_t which) {
    std::list<uint32_t>* targets[] = {&gBox->mF28, &gBox->mF34, &gBox->mF40};
    return static_cast<int32_t>(targets[which]->size());
}

uint32_t tpw_ow_value(int32_t which, int32_t index) {
    std::list<uint32_t>* targets[] = {&gBox->mF28, &gBox->mF34, &gBox->mF40};
    auto it = targets[which]->begin();
    std::advance(it, index);
    return *it;
}

void tpw_ow_f1c(uint8_t* out) { std::memcpy(out, gBox->mF1C, 12); }

void tpw_ow_set_f1c(const uint8_t* in) { std::memcpy(gBox->mF1C, in, 12); }

// ---- the functions
void tpw_ow_save() { gBox->Save(); }

int32_t tpw_ow_write_mail_data() { return gBox->WriteMailData(); }

}  // extern "C"
