#pragma once

#include "ClassTypes.h"
#include "Container.h"

#include <cstdint>
#include <functional>


class Timeline
: private NonCopyable
, private NonMovable
{
public:
    Timeline(const char *str = "");
    void SetMaxEvents(uint32_t maxEvents);
    uint32_t GetMaxEvents();
    void KeepOldest();
    void KeepNewest();
    uint64_t Event(const char *name);
    uint64_t GetTimeAtEvent(const char *name);
    void Report(const char *title = nullptr);
    void ReportNow(const char *title = nullptr);
    void Reset();

    static uint64_t Measure(std::function<void(Timeline &t)> fn, const char *title = nullptr);

    static void EnableCcGlobal();
    static void DisableCcGlobal();
    static Timeline &Global();

    static void Init();
    static void SetupShell();


private:
    struct EventData
    {
        const char *name;
        uint64_t timeUs = 0;
    };

    CircularBuffer<EventData> eventList_;

    bool keepOldest_ = false;
    uint32_t eventsLost_ = 0;
    bool currentlyReporting_ = false;

    uint32_t firstTimeUs_ = 0;

    bool iAmTheGlobal_ = false;

    // "carbon copy" to the global, as in, instances also post to the global
    inline static bool ccGlobal_ = false;
};


inline static Timeline timelineGlobal_("GLOBAL");
