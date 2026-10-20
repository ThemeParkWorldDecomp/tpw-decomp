// The parts of the singleton things that their constructors build (unit w122a, src/ThingCtors.cpp). Mac layouts, no pointers. Each
// constructor is inline here because the Mac's leaf constructor is only a few stores; the address of the original is in the comment.
// Names of members are the unit's own (the binary has none for these fields).
#pragma once
#include <cstdint>

// 0x18 bytes, held by CStaffHQ at +0xc. 0x1012ffb0 (44 bytes): the five words in a signed counting loop, then the byte.
struct CTrainingCentre {
    CTrainingCentre() {
        for (int32_t i = 0; i < 5; ++i) mF00[i] = 0;
        mF14 = 0;
    }
    uint32_t mF00[5];   // 0x00
    uint8_t mF14;       // 0x14  (0x15 to 0x17: padding, never written)
    // Unit CTrainingCentre_Serialise (src/TrackCell_CTrainingCentre_Serialise.cpp)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10130008 (332 bytes)
    void UpdateMonthly();   // 0x1013018c (928 bytes)
};
// Initial staff counts per kind (words: kinds 5, 4, 6, 7, 8) copied by UpdateMonthly, 0x102a4408; and the two end markers of the used-thing walks.
extern int32_t gTrainingCounts[5];   // 0x102a4408
extern uint16_t gTrainingEnd1;       // 0x102a441c
extern uint16_t gTrainingEnd2;       // 0x102a441e
static_assert(sizeof(CTrainingCentre) == 0x18, "CTrainingCentre is 0x18 bytes");

// 0x10 bytes, held by CWeather at +0x20. 0x10139674 (20 bytes): +4, +0 and the byte +0xc are cleared, in that order; +8 is never written.
struct CWeatherForecast {
    CWeatherForecast() {
        mF04 = 0;
        mF00 = 0;
        mF0C = 0;
    }
    uint32_t mF00;   // 0x00  mTime (the save name; a TbTimeStamp with mF04)
    uint32_t mF04;   // 0x04
    uint32_t mF08;   // 0x08  mQuality (the save name; an int)
    uint8_t mF0C;    // 0x0c  mValid (the save name; a bool) (0x0d to 0x0f: padding, never written)
    // 0x10139854, 12 bytes (src/Weather.cpp, unit w181): clears the byte at +0xc.
    void Invalidate();
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x101396b8, 356 bytes (src/Weather.cpp, unit w181)
    void Set(int32_t days, int32_t quality);   // 0x10139894, 208 bytes (src/Weather.cpp, unit w181)
};
static_assert(sizeof(CWeatherForecast) == 0x10, "CWeatherForecast is 0x10 bytes");

// 0x14 bytes, 0x100 of them in CResearchLab at +0xc. 0x10123abc (4 bytes): a bare blr.
struct RideInfo;
struct CResearchProject {
    CResearchProject() {}
    // Unit w211 (src/ResearchLab_projects.cpp): a project's setup, progress and remaining points.
    char SetupProject(const RideInfo* ride);       // 0x10123d7c (176 bytes): nonzero when the ride gives a project (a ride with no research cost is made available instead)
    char SetupStatUpgrade(const RideInfo* ride);   // 0x10123e6c (280 bytes): nonzero when the ride gives a statistics upgrade (skips and records the levels with no cost)
    void ContributeResearch(float points);         // 0x10123fc8 (296 bytes): adds the points; the finished project's object becomes available and the park is told
    bool IsProjectCompleted() const;               // 0x101241ac (36 bytes): the progress has reached the score
    uint32_t GetRemainingPoints(float best) const; // 0x1012420c (88 bytes): the percentage of the best score still to do
    uint16_t mObjectId;   // 0x00  the ride's object id (the name is mine)
    uint8_t mPad02[2];    // 0x02  never written
    int32_t mCategory;    // 0x04  the research group (0 to 3 the object kind, 4 a statistics upgrade) SetupCategories matches
    uint8_t mF08;         // 0x08  the group (the ride's Group slot); compared with the category's level byte (+0xc), unsigned
    uint8_t mUpgradeLevel;   // 0x09  the upgrade level of a statistics upgrade (the name is mine)
    uint8_t mPad0A[2];    // 0x0a  never written
    float mScore;         // 0x0c  the research cost; SetupProjects keeps the project with the highest one
    float mProgress;      // 0x10  the points done (the name is mine)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x10123af0 (596 bytes, untranslated)
};
static_assert(sizeof(CResearchProject) == 0x14, "CResearchProject is 0x14 bytes");
static_assert(offsetof(CResearchProject, mCategory) == 0x04 && offsetof(CResearchProject, mF08) == 0x08 &&
              offsetof(CResearchProject, mUpgradeLevel) == 0x09 && offsetof(CResearchProject, mScore) == 0x0c &&
              offsetof(CResearchProject, mProgress) == 0x10, "CResearchProject layout");

// 0x10 bytes, five of them in CResearchLab at +0x140c. 0x101242a0 (20 bytes): the byte +0xc = 1, then the word +8 = -1.
struct CResearchCategory {
    CResearchCategory() {
        mF0C = 1;
        mF08 = -1;
    }
    uint32_t mEffort;   // 0x00  the effort given to the category, 0 to 100 (the name is mine)
    uint32_t mOutput;   // 0x04  the category's share of the work load, 0 to 100 (CalculateOutputs; the name is mine)
    int32_t mF08;       // 0x08  the current project's index in the lab's list, -1 for none
    uint8_t mF0C;       // 0x0c  the highest group open to the category (0x0d to 0x0f: padding, never written)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x101242e4 (436 bytes, untranslated)
};
static_assert(sizeof(CResearchCategory) == 0x10, "CResearchCategory is 0x10 bytes");
static_assert(offsetof(CResearchCategory, mEffort) == 0x00 && offsetof(CResearchCategory, mOutput) == 0x04 &&
              offsetof(CResearchCategory, mF08) == 0x08 && offsetof(CResearchCategory, mF0C) == 0x0c, "CResearchCategory layout");
