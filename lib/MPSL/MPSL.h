#pragma once

#include <mpsl_timeslot.h>

#include "TimedEventHandler.h"
#include "Timeline.h"
#include "Container.h"


class MPSLEventHandler
{
public:
    // basically extend by the duration of the worst-case-scenario longest packet rx/tx time
    static const uint32_t DEFAULT_EXTENSION_LEN_US = 2'500;

public:
    virtual void OnTimeslotStart(){};
    virtual bool WantsExtension(){ return false; };
    virtual uint32_t GetExtensionDurationUs() { return DEFAULT_EXTENSION_LEN_US; }
    virtual bool WantsNextSlot(){ return true; };
    virtual void OnTimeslotEnd(){};
    virtual void OnTimeslotNoMoreComing(){};
    virtual void OnRadioAvailable(){};
    virtual void OnAssert(){}
};


class MPSL
{
    // measured at the radio, not at the API
    static const uint32_t PROCESSING_LEAD_TIME_US = 2'400;
    static const uint32_t EARLY_EXPIRE_US = 700;

public:
    // API
    static void OpenSession(MPSLEventHandler *handler, mpsl_timeslot_session_id_t *sessionId);
    static void CloseSession(mpsl_timeslot_session_id_t sessionId);
    static void RequestTimeslot(mpsl_timeslot_session_id_t sessionId, uint32_t periodUs, uint32_t durationUs, MPSL_TIMESLOT_PRIORITY priority = MPSL_TIMESLOT_PRIORITY_NORMAL);
    static void EndThisTimeslot(mpsl_timeslot_session_id_t sessionId);

    static void Init();
    static void SetupRadioNotifications();
    static void SetupShell();
    static void SetupFatalHandler();

private:
    // constants
    static const IRQn_Type MPSL_BOUNCE_IRQ          = QDEC_IRQn;
    static const uint8_t   MPSL_BOUNCE_IRQ_PRIORITY = 4;
    static const uint8_t   MPSL_BOUNCE_IRQ_FLAGS    = 0;

    static const IRQn_Type MPSL_RADIO_NOTIFY_IRQ      = SWI4_EGU4_IRQn;
    static const uint8_t   MPSL_RADIO_NOTIFY_PRIORITY = 5;
    static const uint8_t   MPSL_RADIO_NOTIFY_FLAGS    = 0;

    // IRQ bounce
    enum class BounceSignal : uint8_t
    {
        START,
        END,
        NO_MORE_COMING,
    };

    struct BounceState
    {
        BounceSignal signal = BounceSignal::START;
        uint8_t sessionId = 0;
    };

    static const uint8_t PIPE_CAPACITY = 5;
    static CircularBuffer<BounceState> bouncePipe_;


    enum class HandlerState : uint8_t
    {
        NONE,
        IDLE,
        PENDING_START,
        IN_TIMESLOT,
        PENDING_EXTENSION,
    };

    // Per-session State
    struct SessionState
    {
        // cached client values
        MPSLEventHandler *handler = nullptr;
        uint32_t periodUs   = 0;
        uint32_t durationUs = 0;
        uint32_t earlyExpireUs = 0;
        MPSL_TIMESLOT_PRIORITY priority = MPSL_TIMESLOT_PRIORITY_NORMAL;

        volatile HandlerState handlerState = HandlerState::NONE;
        volatile bool         tryToEndSlot = false;

        // storage
        mpsl_timeslot_request_t req;

        // array always has elements, so just a question of whether this
        // element is valid right now or not
        bool isValidData = false;
        bool closedOnPurpose = false;
    };

    static void InstallIRQs();
    static void IRQBounceHandler();
    static void IRQRadioNotificationHandler();

    static mpsl_timeslot_signal_return_param_t *
    TimeslotCallback(mpsl_timeslot_session_id_t sessionId, uint32_t signalType);

    static void KillRadio();

public:
    static void FatalHandler();

private:

    static SessionState ssMap_[CONFIG_MPSL_TIMESLOT_SESSION_COUNT];

    inline static Timeline timeline_;
};

