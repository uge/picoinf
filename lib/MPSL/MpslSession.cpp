#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <string.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/types.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

#include <mpsl_timeslot.h>
#include <mpsl.h>
#include <hal/nrf_timer.h>

#include "MpslSession.h"
#include "Evm.h"
#include "Utl.h"



///////////////////////////////////////////////////////////////////////////////
// Startup / Shutdown
///////////////////////////////////////////////////////////////////////////////

MpslSession::~MpslSession()
{
    CloseSession();
}


///////////////////////////////////////////////////////////////////////////////
// External API Implementation
///////////////////////////////////////////////////////////////////////////////

void MpslSession::OpenSession()
{
    // We know we've never called open since no session id
    if (sessionId_ == 0xFF)
    {
        // Get session ID
        MPSL::OpenSession(this, &sessionId_);
    }
    else
    {
        Log("ERR: MpslSession: OpenSession on existing session");
    }
}

void MpslSession::CloseSession()
{
    if (sessionId_ != 0xFF)
    {
        MPSL::CloseSession(sessionId_);

        sessionId_ = 0xFF;
    }
    else
    {
        Log("ERR: MpslSession: CloseSession before OpenSession");
    }
}

void MpslSession::SetCbOnStart(function<void()> cbFnOnStart)
{
    cbFnOnStart_ = cbFnOnStart;
}

void MpslSession::SetCbOnEnd(function<void()> cbFnOnEnd)
{
    cbFnOnEnd_ = cbFnOnEnd;
}

void MpslSession::SetCbOnNoMoreComing(function<void()> cbFnOnNoMoreComing)
{
    cbFnOnNoMoreComing_ = cbFnOnNoMoreComing;
}

void MpslSession::SetCbOnRadioAvailable(function<void()> cbFnOnRadioAvailable)
{
    cbFnOnRadioAvailable_ = cbFnOnRadioAvailable;
}

void MpslSession::SetCbOnAssert(function<void()> cbFnOnAssert)
{
    cbFnOnAssert_ = cbFnOnAssert;
}

/*
// The longest allowed timeslot event in microseconds
MPSL_TIMESLOT_LENGTH_MIN_US = 100

// The longest allowed timeslot event in microseconds.
MPSL_TIMESLOT_LENGTH_MAX_US = 100'000

// The longest timeslot distance in microseconds allowed for the distance parameter
MPSL_TIMESLOT_DISTANCE_MAX_US = 256,000,000 - 1

// The longest timeout in microseconds allowed when requesting the earliest possible timeslot
MPSL_TIMESLOT_EARLIEST_TIMEOUT_MAX_US = 256,000,000 - 1

// The minimum allowed timeslot extension time
MPSL_TIMESLOT_EXTENSION_TIME_MIN_US = 200

// The maximum processing time to handle a timeslot extension
MPSL_TIMESLOT_EXTENSION_PROCESSING_TIME_MAX_US = 25

// The latest time before the end of a timeslot when timeslot can be extended
MPSL_TIMESLOT_EXTENSION_MARGIN_MIN_US = 87

// Maximum number of timeslot sessions
MPSL_TIMESLOT_CONTEXT_COUNT_MAX = 8
*/

void MpslSession::RequestTimeslots(uint64_t periodUs, uint64_t durationUs, bool highPriority)
{
    if (sessionId_ != 0xFF)
    {
        durationUs_    = min(durationUs, (uint64_t)MPSL_TIMESLOT_LENGTH_MAX_US);
        periodUs_      = max(periodUs, durationUs_);
        wantsNextSlot_ = true;

        LogNNL("MpslSession RequestTimeslots(", Commas(periodUs), ", ", Commas(durationUs), ")");
        Log(" -> (", Commas(periodUs_), ", ", Commas(durationUs_), ")");

        MPSL::RequestTimeslot(sessionId_, periodUs_, durationUs_, highPriority ? MPSL_TIMESLOT_PRIORITY_HIGH : MPSL_TIMESLOT_PRIORITY_NORMAL);
    }
    else
    {
        Log("ERR: MpslSession: RequestTimeslots before OpenSession");
    }
}

void MpslSession::CancelTimeslots()
{
    if (sessionId_ != 0xFF)
    {
        wantsNextSlot_ = false;
        if (isInTimeslot_)
        {
            MPSL::EndThisTimeslot(sessionId_);
        }
    }
    else
    {
        Log("ERR: MpslSession: CancelTimeslots before OpenSession");
    }
}

void MpslSession::EndThisTimeslot()
{
    if (sessionId_ != 0xFF)
    {
        if (isInTimeslot_)
        {
            MPSL::EndThisTimeslot(sessionId_);
        }
        else
        {
            Log("ERR: MpslSession: EndThisTimeslot when not in a timeslot");
        }
    }
    else
    {
        Log("ERR: MpslSession: EndThisTimeslot before OpenSession");
    }
}

void MpslSession::EnableTimeslotExtensions()
{
    wantsMaxSlotTime_ = true;
}

void MpslSession::DisableTimeslotExtensions()
{
    wantsMaxSlotTime_ = false;
}


///////////////////////////////////////////////////////////////////////////////
// Inherited Callbacks
///////////////////////////////////////////////////////////////////////////////

void MpslSession::OnTimeslotStart()
{
    isInTimeslot_ = true;
    Evm::QueueWork("MPSL_SESSION_EVM_QUEUE_TIMESLOT_START", [this](){
        cbFnOnStart_();
    });
}

bool MpslSession::WantsExtension()
{
    return wantsMaxSlotTime_;
}

bool MpslSession::WantsNextSlot()
{
    return wantsNextSlot_;
}

void MpslSession::OnTimeslotEnd()
{
    isInTimeslot_ = false;
    Evm::QueueWork("MPSL_SESSION_EVM_QUEUE_SLOT_END", [this](){
        cbFnOnEnd_();
    });
}

void MpslSession::OnTimeslotNoMoreComing()
{
    Evm::QueueWork("MPSL_SESSION_EVM_QUEUE_NO_MORE_COMING", [this](){
        cbFnOnNoMoreComing_();
    });
}

void MpslSession::OnRadioAvailable()
{
    Evm::QueueWork("MPSL_SESSION_EVM_QUEUE_RADIO_AVAILABLE", [this](){
        cbFnOnRadioAvailable_();
    });
}

void MpslSession::OnAssert()
{
    cbFnOnAssert_();
}


///////////////////////////////////////////////////////////////////////////////
// Init
///////////////////////////////////////////////////////////////////////////////

void MpslSession::SetupShell()
{
    static uint64_t periodUs   = 5'000'000;
    static uint64_t durationUs =    25'000;
    static MpslSession sess;
    static MpslSession sess2;
    static Timeline timeline;
    static bool endInTimeslot = false;
    static bool cancelInTimeslot = false;
    static bool endOutTimeslot = false;
    static bool cancelOutTimeslot = false;
    static uint64_t timeStartLast = 0;
    static uint64_t timeStart = 0;
    static uint64_t timeEnd = 0;
    static TimedEventHandlerDelegate ted;
    static TimedEventHandlerDelegate ted2;

    sess.SetCbOnStart([&](){
        timeStartLast = timeStart;
        timeStart = PAL.Micros();
        uint64_t timeDiffEnd = timeStart - timeEnd;
        uint64_t timeDiffStart = timeStart - timeStartLast;
        timeline.Event("OnStart");
        if (timeEnd && timeStartLast)
        {
            Log(TS(), "OnStart - ", StrUtl::PadLeft(Commas(timeDiffEnd), ' ', 7), " from end, ", Commas(timeDiffStart), " from last");
        }
        else
        {
            Log(TS(), "OnStart");
        }

        if (endInTimeslot)
        {
            sess.EndThisTimeslot();
            // Timeline::Global().Report();
        }
        if (cancelInTimeslot)
        {
            sess.CancelTimeslots();
            // Timeline::Global().Report();
        }
    });
    sess.SetCbOnEnd([&](){
        timeEnd = PAL.Micros();
        uint64_t timeDiff = timeEnd - timeStart;
        timeline.Event("OnEnd");
        Log(TS(), "OnEnd   - ", StrUtl::PadLeft(Commas(timeDiff), ' ', 7), " from start");

        if (endOutTimeslot)
        {
            sess.EndThisTimeslot();
            // Timeline::Global().Report();
        }
        else if (cancelOutTimeslot)
        {
            sess.CancelTimeslots();
            // Timeline::Global().Report();
        }
    });
    sess.SetCbOnNoMoreComing([&](){
        timeline.Event("OnNoMoreComing");
        Log(TS(), "OnNoMoreComing");
    });
    sess.SetCbOnRadioAvailable([]{
        timeline.Event("OnRadioAvailable");
        // Log(TS(), "OnRadioAvailable");
    });

    Shell::AddCommand("sess.open", [&](vector<string> argList){
        Log("opening");
        sess.OpenSession();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sess.close", [&](vector<string> argList){
        Log("closing");
        sess.CloseSession();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sess.req", [&](vector<string> argList){
        Log("requesting periodUs ", periodUs, ", durationUs ", durationUs);
        sess.RequestTimeslots(periodUs, durationUs);
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sess.end", [&](vector<string> argList){
        Log("ending");
        sess.EndThisTimeslot();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sess.cancel", [&](vector<string> argList){
        Log("offing");
        sess.CancelTimeslots();
    }, { .argCount = 0, .help = "" });
    
    Shell::AddCommand("sess.max", [&](vector<string> argList){
        if (argList[0] == "on")
        {
            Log("max on");
            sess.EnableTimeslotExtensions();
        }
        else
        {
            Log("max off");
            sess.DisableTimeslotExtensions();
        }
    }, { .argCount = 1, .help = "" });
    
    Shell::AddCommand("sess.endInTimeslot", [&](vector<string> argList){
        if (argList[0] == "on")
        {
            Log("endInTimeslot on");
            endInTimeslot = true;
        }
        else
        {
            Log("endInTimeslot off");
            endInTimeslot = false;
        }
    }, { .argCount = 1, .help = "" });
    
    Shell::AddCommand("sess.cancelInTimeslot", [&](vector<string> argList){
        if (argList[0] == "on")
        {
            Log("cancelInTimeslot on");
            cancelInTimeslot = true;
        }
        else
        {
            Log("cancelInTimeslot off");
            cancelInTimeslot = false;
        }
    }, { .argCount = 1, .help = "" });

    Shell::AddCommand("sess.endOutTimeslot", [&](vector<string> argList){
        if (argList[0] == "on")
        {
            Log("endOutTimeslot on");
            endOutTimeslot = true;
        }
        else
        {
            Log("endOutTimeslot off");
            endOutTimeslot = false;
        }
    }, { .argCount = 1, .help = "" });
    
    Shell::AddCommand("sess.cancelOutTimeslot", [&](vector<string> argList){
        if (argList[0] == "on")
        {
            Log("cancelOutTimeslot on");
            cancelOutTimeslot = true;
        }
        else
        {
            Log("cancelOutTimeslot off");
            cancelOutTimeslot = false;
        }
    }, { .argCount = 1, .help = "" });
    
    Shell::AddCommand("sess.report", [&](vector<string> argList){
        timeline.Report();
    }, { .argCount = 0, .help = "" });


    Shell::AddCommand("sess2.open", [&](vector<string> argList){
        Log("opening2");
        sess2.OpenSession();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sess2.close", [&](vector<string> argList){
        Log("closing2");
        sess2.CloseSession();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sess2.reqhp", [&](vector<string> argList){
        Log("requesting high priority periodUs ", periodUs, ", durationUs ", durationUs);
        sess2.RequestTimeslots(periodUs, durationUs, true);
    }, { .argCount = 0, .help = "" });


    // this is meant to test a specific scenario
    // all events and other sessions must be off
    // this is a one-off command meant to be run on startup
    Shell::AddCommand("sess.testcancel", [&](vector<string> argList){
        // open the sessions
        sess.OpenSession();
        sess2.OpenSession();

        // first session requests a timeslot, which it will get uncontested.
        // every second, get 100ms of time
        sess.RequestTimeslots(1'000'000, 100'000);

        // wait until sess1 is about to get its second slot and
        // request a high-priority session.
        // this should cause sess1 to get a cancel, and have to reschedule
        ted.SetCallback([]{
            sess2.RequestTimeslots(1'000'000, 100'000, true);

            ted2.SetCallback([]{
                Timeline::Global().Report();
            });
            ted2.RegisterForTimedEvent(Micros{210'000});
        });
        ted.RegisterForTimedEvent(Micros{950'000});
    }, { .argCount = 0, .help = "" });



}
