#include <kernel.h>
#include <mpsl_radio_notification.h>
#include <mpsl/mpsl_assert.h>

#include <esb.h>

#include "Utl.h"
#include "MPSL.h"
#include "Timer.h"


///////////////////////////////////////////////////////////////////////////////
// External API Implementation
///////////////////////////////////////////////////////////////////////////////

void MPSL::OpenSession(MPSLEventHandler *handler, mpsl_timeslot_session_id_t *sessionId)
{
    timeline_.Event("MPSL_OpenSession");

    if (sessionId == nullptr)
    {
        printk("ERR: MPSL Session Open - null Session ID\n");
    }
    else if (handler == nullptr)
    {
        printk("ERR: MPSL Session Open - null handler\n");
    }
    else
    {
        int err = mpsl_timeslot_session_open(TimeslotCallback, sessionId);
        if (err)
        {
            printk("ERR: MPSL Session Open: %i\n", err);
            PAL.Fatal("MPSL::OpenSession");
        }

        if ((size_t)*sessionId >= CONFIG_MPSL_TIMESLOT_SESSION_COUNT)
        {
            printk("ERR: MPSL Session Open - Session ID too large %i - %i\n", *sessionId, CONFIG_MPSL_TIMESLOT_SESSION_COUNT);
            PAL.Fatal("MPSL::OpenSession");
        }
        else if (ssMap_[*sessionId].isValidData == true)
        {
            printk("ERR: MPSL Session Open - Duplicate Session ID %i\n", *sessionId);
            PAL.Fatal("MPSL::OpenSession");
        }
        else
        {
            // keep track of session state
            ssMap_[*sessionId] = {
                .handler      = handler,
                .handlerState = HandlerState::IDLE,
                .tryToEndSlot = false,
                .isValidData  = true,
            };
        }
    }
}

void MPSL::CloseSession(mpsl_timeslot_session_id_t sessionId)
{
    timeline_.Event("MPSL_CloseSession");

    if ((size_t)sessionId >= CONFIG_MPSL_TIMESLOT_SESSION_COUNT)
    {
        printk("ERR: MPSL Session Close - Session ID too large %i\n", sessionId);
    }
    else if (ssMap_[sessionId].isValidData == false)
    {
        printk("ERR: MPSL Session Close - Invalid Session ID %i\n", sessionId);
    }
    else
    {
        SessionState &ss = ssMap_[sessionId];

        int err = mpsl_timeslot_session_close(sessionId);
        if (err)
        {
            printk("ERR: MPSL Session Close: %i\n", err);
            PAL.Fatal("MPSL::CloseSession");
        }

        // keep track of session state
        ss = {
            .handler      = nullptr,
            .handlerState = HandlerState::NONE,
            .tryToEndSlot = false,
            .isValidData  = false,
            .closedOnPurpose = true,
        };
    }
}

void MPSL::RequestTimeslot(mpsl_timeslot_session_id_t sessionId,
                           uint32_t periodUs,
                           uint32_t durationUs,
                           MPSL_TIMESLOT_PRIORITY priority)
{
    timeline_.Event("MPSL_RequestTimeslot");

    if ((size_t)sessionId >= CONFIG_MPSL_TIMESLOT_SESSION_COUNT)
    {
        printk("ERR: MPSL Session Close - Session ID too large %i\n", sessionId);
    }
    else if (ssMap_[sessionId].isValidData == false)
    {
        printk("ERR: MPSL Session Close - Invalid Session ID %i\n", sessionId);
    }
    else
    {
        // Cache request values and fill out request structure
        SessionState &ss = ssMap_[sessionId];
        ss.handlerState  = HandlerState::PENDING_START;
        ss.tryToEndSlot  = false;
        ss.periodUs      = periodUs;
        ss.durationUs    = durationUs;
        ss.earlyExpireUs = EARLY_EXPIRE_US;
        ss.priority      = priority;
        ss.req = {
            .request_type = MPSL_TIMESLOT_REQ_TYPE_EARLIEST,
            .params = {
                .earliest = {
                    .hfclk = MPSL_TIMESLOT_HFCLK_CFG_XTAL_GUARANTEED,
                    .priority = (uint8_t)ss.priority,
                    .length_us = ss.durationUs,
                    .timeout_us = 1'000'000,
                }
            },
        };

        int err = mpsl_timeslot_request(sessionId, &ss.req);
        if (err)
        {
            printk("ERR: MPSL Timeslot Request: %i\n", err);
            PAL.Fatal("MPSL::RequestTimeslot");
        }
    }
}

void MPSL::EndThisTimeslot(mpsl_timeslot_session_id_t sessionId)
{
    timeline_.Event("MPSL_EndThisTimeslot");

    if ((size_t)sessionId >= CONFIG_MPSL_TIMESLOT_SESSION_COUNT)
    {
        // printk("ERR: MPSL End Timeslot Early - Session ID too large %i\n", sessionId);
    }
    else if (ssMap_[sessionId].isValidData == false)
    {
        // printk("ERR: MPSL End Timeslot Early - Invalid Session ID %i\n", sessionId);
    }
    else
    {
        SessionState &ss = ssMap_[sessionId];

        if (ss.tryToEndSlot)
        {
            timeline_.Event("MPSL_CAUGHT_DUPLICATE_END_TIMESLOT");
            // do nothing
            // protect against this getting called multiple times
        }
        else if (ss.handlerState == HandlerState::NONE ||
                 ss.handlerState == HandlerState::IDLE)
        {
            timeline_.Event("MPSL_NO_TRIGGER_TIMER0_CH1_TOO_EARLY");

            // printk("ERR: MPSL End Timeslot Early - Not in a timeslot %i\n", sessionId);
        }
        else if (ss.handlerState == HandlerState::PENDING_START)
        {
            timeline_.Event("MPSL_NO_TRIGGER_TIMER0_CH1_WAIT_START");

            ss.tryToEndSlot = true;
        }
        else if (ss.handlerState == HandlerState::IN_TIMESLOT)
        {
            timeline_.Event("MPSL_YES_TRIGGER_TIMER0_CH1");

            ss.tryToEndSlot = true;

            // Trigger the TIMER0 to fire, which will result in the MPSL system sending
            Timer0::Chan1TriggerNow();
        }
        else // ss.handlerState == HandlerState::PENDING_EXTENSION
        {
            timeline_.Event("MPSL_NO_TRIGGER_TIMER0_CH1_WAIT_PEND");

            // Channel 1 already triggered, and there's an extremely fast
            // pending operation in progress, just let it finish and it will
            // notice the state.

            ss.tryToEndSlot = true;
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
// MPSL Callback Handler and related functionality
///////////////////////////////////////////////////////////////////////////////

// rule appears to be
// - when chan 1 fires, you can only request extension or return no action
// - when chan 0 fires, you can only request next or return no action
// - you never get to have these timers go off again (except chan 1 on extension)


mpsl_timeslot_signal_return_param_t *
MPSL::TimeslotCallback(mpsl_timeslot_session_id_t sessionId, uint32_t signalType)
{
    // static Pin pSlot{0, 7};

    // set up default return parameter
    static mpsl_timeslot_signal_return_param_t retVal;
    retVal = {
        .callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_NONE,
    };

    // lookup and validate known session
    SessionState &ss = ssMap_[sessionId];
    if (ss.isValidData != true) { return &retVal; }

    // most frequent occurance, just put at front of list
    if (signalType == MPSL_TIMESLOT_SIGNAL_RADIO)
    {
        timeline_.Event("MPSL_RADIO");
        RADIO_IRQHandler();
    }

    // rest of sequence of operations below, in basiclly state transition order

    else if (signalType == MPSL_TIMESLOT_SIGNAL_START)
    {
        timeline_.Event("MPSL_START");
        // pSlot.DigitalToggle();

        // note actual current state
        ss.handlerState = HandlerState::IN_TIMESLOT;

        Timer0::Chan1SetByIncr(ss.durationUs - ss.earlyExpireUs - PROCESSING_LEAD_TIME_US);
        Timer0::Chan0SetByIncr(ss.durationUs - ss.earlyExpireUs);

        // check if we're in the desired state
        if (!ss.tryToEndSlot)
        {
            // bounce signal to low-priority irq handler
            bouncePipe_.PushBack({
                .signal = BounceSignal::START,
                .sessionId = sessionId,
            });
            NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);
        }
        else
        {
            // Let's end this as quickly as possible
            timeline_.Event("MPSL_TRIGGER_TIMER0_CH1");
            Timer0::Chan1TriggerNow();
        }
    }


    else if (signalType == MPSL_TIMESLOT_SIGNAL_TIMER0 && Timer0::Chan1IsTriggered())
    {
        timeline_.Event("MPSL_TIMER0_CH1");
        Timer0::Chan1ClearInterrupt();

        // We're handling timer for requesting an extension

        // check if we're in the desired state
        bool shouldExtend;
        if (!ss.tryToEndSlot)
        {
            shouldExtend = ss.handler->WantsExtension();
        }
        else
        {
            shouldExtend = false;
        }

        // extend
        if (shouldExtend)
        {
            timeline_.Event("MPSL_REQ_EXTEND");

            // We try to get extension
            retVal.callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_EXTEND;
            retVal.params.extend.length_us = ss.handler->GetExtensionDurationUs();

            // track state
            ss.handlerState = HandlerState::PENDING_EXTENSION;
        }
        else
        {
            timeline_.Event("MPSL_NO_EXTEND");

            // We know the timeslot will be ending now
            KillRadio();
        }

        if (ss.tryToEndSlot)
        {
            // speed up expiry of chan 0
            timeline_.Event("MPSL_TRIGGER_TIMER0_CH0");
            Timer0::Chan0TriggerNow();
        }
    }
    else if (signalType == MPSL_TIMESLOT_SIGNAL_EXTEND_SUCCEEDED)
    {
        timeline_.Event("MPSL_EXTEND_OK");

        // track state
        ss.handlerState = HandlerState::IN_TIMESLOT;

        // extend timers
        Timer0::Chan0SetByIncr(ss.handler->GetExtensionDurationUs());
        Timer0::Chan1SetByIncr(ss.handler->GetExtensionDurationUs());

        // check if we're in the desired state
        if (ss.tryToEndSlot)
        {
            // Let's end this as quickly as possible
            timeline_.Event("MPSL_TRIGGER_TIMER0_CH1");
            Timer0::Chan1TriggerNow();
        }
    }
    else if (signalType == MPSL_TIMESLOT_SIGNAL_EXTEND_FAILED)
    {
        // You didn't get granted your extension, you should request a new slot
        timeline_.Event("MPSL_EXTEND_FAIL");

        // We know the timeslot will be ending now
        KillRadio();

        if (ss.tryToEndSlot)
        {
            // Let's end this as quickly as possible
            timeline_.Event("MPSL_TRIGGER_TIMER0_CH0");
            Timer0::Chan0TriggerNow();
        }
    }


    else if (signalType == MPSL_TIMESLOT_SIGNAL_TIMER0 && Timer0::Chan0IsTriggered())
    {
        // pSlot.DigitalToggle();
        timeline_.Event("MPSL_TIMER0_CH0");
        Timer0::Chan0ClearInterrupt();

        // We held off informing the handler of end of slot until their
        // requested timeout.  Do it now.
        bouncePipe_.PushBack({
            .signal = BounceSignal::END,
            .sessionId = sessionId,
        });
        NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);



        // by this point, the slot is over, we can safely reset state variable
        // pushing for end of slot
        ss.tryToEndSlot = false;

        // We're handling timer for requesting next slot
        if (ss.handler->WantsNextSlot())
        {
            timeline_.Event("MPSL_REQ_NEXT");

            ss.req = {
                .request_type = MPSL_TIMESLOT_REQ_TYPE_NORMAL,
                .params = {
                    .normal = {
                        .hfclk = MPSL_TIMESLOT_HFCLK_CFG_XTAL_GUARANTEED,
                        .priority = (uint8_t)ss.priority,
                        .distance_us = ss.periodUs,
                        .length_us = ss.durationUs,
                    }
                },
            };

            retVal.callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_REQUEST;
            retVal.params.request.p_next = &ss.req;

            // track state
            ss.handlerState = HandlerState::PENDING_START;
        }
        else
        {
            timeline_.Event("MPSL_NO_NEXT");

            // ok no more coming, leave it to the caller to request next
            bouncePipe_.PushBack({
                .signal = BounceSignal::NO_MORE_COMING,
                .sessionId = sessionId,
            });
            NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);

            // track state
            ss.handlerState = HandlerState::IDLE;
        }
    }
    else if (signalType == MPSL_TIMESLOT_SIGNAL_BLOCKED)
    {
        // last request to get a timeslot collided with another, you should request a new slot
        timeline_.Event("MPSL_BLOCKED");

        if (ss.handlerState != HandlerState::IDLE)
        {
            timeline_.Event("MPSL_BLOCKED_AUTO_RETRY");

            // this means we are coming from within a timeslot and our attempt to
            // get the next slot failed.

            if (ss.handler->WantsNextSlot())
            {
                timeline_.Event("MPSL_AUTO_REQUEST_EARLIEST");

                // Let's auto-retry.
                RequestTimeslot(sessionId, ss.periodUs, ss.durationUs, ss.priority);

                ss.handlerState = HandlerState::PENDING_START;
            }
            else
            {
                timeline_.Event("MPSL_NO_MORE_COMING");

                // ok no more coming, leave it to the caller to request next
                bouncePipe_.PushBack({
                    .signal = BounceSignal::NO_MORE_COMING,
                    .sessionId = sessionId,
                });
                NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);

                ss.handlerState = HandlerState::IDLE;
            }
        }
        else
        {
            timeline_.Event("MPSL_NO_MORE_COMING");

            // ok no more coming, leave it to the caller to request next
            bouncePipe_.PushBack({
                .signal = BounceSignal::NO_MORE_COMING,
                .sessionId = sessionId,
            });
            NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);

            ss.handlerState = HandlerState::IDLE;
        }
    }


    else if (signalType == MPSL_TIMESLOT_SIGNAL_CANCELLED)
    {
        // your prior slot was ok'd, but subsequently canceled.  you should request a new slot.
        timeline_.Event("MPSL_CANCELLED");

        // this means we are coming from within a timeslot and our attempt to
        // get the next slot was originally accepted, but got cancelled later

        if (ss.handler->WantsNextSlot())
        {
            timeline_.Event("MPSL_AUTO_REQUEST_EARLIEST");

            // Let's auto-retry.
            RequestTimeslot(sessionId, ss.periodUs, ss.durationUs, ss.priority);

            ss.handlerState = HandlerState::PENDING_START;
        }
        else
        {
            timeline_.Event("MPSL_NO_MORE_COMING");

            ss.handlerState = HandlerState::IDLE;

            // ok no more coming, leave it to the caller to request next
            bouncePipe_.PushBack({
                .signal = BounceSignal::NO_MORE_COMING,
                .sessionId = sessionId,
            });
            NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);
        }
    }


    else if (signalType == MPSL_TIMESLOT_SIGNAL_SESSION_IDLE)
    {
        // means you no longer have any scheduled timeslots
        timeline_.Event("MPSL_IDLE");

        if (ss.handlerState != HandlerState::IDLE)
        {
            // Log("ERR: Got SESSION_IDLE when current app state is ", (uint8_t)ss.handlerState);
            // timeline_.Report("MPSL_IDLE");
        }

        {
            // by this point, the slot is over, we can safely reset state variable
            // pushing for end of slot
            ss.tryToEndSlot = false;

            // inform of the end of the session, this won't have happened if the
            // CC0 timer never fired
            bouncePipe_.PushBack({
                .signal = BounceSignal::END,
                .sessionId = sessionId,
            });
            NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);


            // decide whether to auto-renew
            if (ss.handler->WantsNextSlot())
            {
                timeline_.Event("MPSL_IDLE_AUTO_REQUEST_EARLIEST");

                // Let's auto-retry.
                RequestTimeslot(sessionId, ss.periodUs, ss.durationUs, ss.priority);

                ss.handlerState = HandlerState::PENDING_START;
            }
            else
            {
                timeline_.Event("MPSL_IDLE_NO_MORE_COMING");

                // ok no more coming, leave it to the caller to request next
                bouncePipe_.PushBack({
                    .signal = BounceSignal::NO_MORE_COMING,
                    .sessionId = sessionId,
                });
                NVIC_SetPendingIRQ(MPSL_BOUNCE_IRQ);

                ss.handlerState = HandlerState::IDLE;
            }
        }
    }
    else if (signalType == MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED)
    {
        // not an event we need to handle, it's only on session close
        timeline_.Event("MPSL_CLOSED");

        // track state
        ss.handlerState = HandlerState::NONE;
    }


    else if (signalType == MPSL_TIMESLOT_SIGNAL_OVERSTAYED)
    {
        // you took too long, assert inbound
        timeline_.Event("MPSL_OVERSTAYED");

        printk("ERR: MPSL Overstayed: %i\n", sessionId);
    }
    else if (signalType == MPSL_TIMESLOT_SIGNAL_INVALID_RETURN)
    {
        // you provided the wrong parameters, this is a programming error
        // no runtime solution
        printk("ERR: MPSL Invalid Return: %i\n", sessionId);
        PAL.Fatal("MPSL::MPSL_TIMESLOT_SIGNAL_INVALID_RETURN");
    }
    else
    {
        printk("ERR: MPSL Unexpected signal: %i - %u\n", sessionId, signalType);
        PAL.Fatal("MPSL::default");
    }

    return &retVal;
}

void MPSL::KillRadio()
{
    // Kill the radio at this point to make sure there's no stray
    // RADIO signals after ch0 fires.
    // No matter what I've tried I can't seem to stop a lingering signal
    // from firing.  Everything past esb_disable didn't have any effect,
    // just leaving it for postarity.
    esb_disable();
    // prevent spurrious tx complete signal on next ESB startup
    NRF_RADIO->INTENCLR      = 0xFFFFFFFF;
    NRF_RADIO->TASKS_DISABLE = 1;
    NVIC_ClearPendingIRQ(RADIO_IRQn);
}


///////////////////////////////////////////////////////////////////////////////
// IRQ Priority Bounce
///////////////////////////////////////////////////////////////////////////////

void MPSL::IRQBounceHandler()
{
    while (bouncePipe_.Size())
    {
        BounceState bounceState = bouncePipe_[0];
        bouncePipe_.PopFront();

        SessionState &ss = ssMap_[bounceState.sessionId];
        if (ss.isValidData != true)
        {
            timeline_.Event("MPSL_ISR_INVALID_DATA");
            if (!ss.closedOnPurpose)
            {
                printk("ERR: MPSL IRQHandler Unrecognized Session ID %i", bounceState.sessionId);
            }
        }
        else if (bounceState.signal == BounceSignal::START)
        {
            timeline_.Event("MPSL_B_START");
            ss.handler->OnTimeslotStart();
        }
        else if (bounceState.signal == BounceSignal::END)
        {
            timeline_.Event("MPSL_B_END");
            ss.handler->OnTimeslotEnd();
        }
        else if (bounceState.signal == BounceSignal::NO_MORE_COMING)
        {
            timeline_.Event("MPSL_B_NO_MORE_COMING");
            ss.handler->OnTimeslotNoMoreComing();
        }
        else
        {
            printk("ERR: MPSL IRQHandler Unknown signal type: %u\n", (uint8_t)bounceState.signal);
        }
    }
}

void MPSL::IRQRadioNotificationHandler()
{
    // static Pin pRadio{0, 8};
    // pRadio.DigitalToggle();
    for (auto &ss : ssMap_)
    {
        if (ss.isValidData)
        {
            ss.handler->OnRadioAvailable();
        }
    }
    // pRadio.DigitalToggle();
}


///////////////////////////////////////////////////////////////////////////////
// Init
///////////////////////////////////////////////////////////////////////////////

void MPSL::Init()
{
    InstallIRQs();
    SetupRadioNotifications();
    SetupFatalHandler();
}

void MPSL::SetupRadioNotifications()
{
    // tell me when the radio frees up, quickly, but with enough
    // time for my bounced signals to pick up on the end of the
    // timeslot
    int err =
    mpsl_radio_notification_cfg_set(
        MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_INACTIVE,
        MPSL_RADIO_NOTIFICATION_DISTANCE_800US,
        MPSL_RADIO_NOTIFY_IRQ
    );

    if (err)
    {
        printk("ERR: SetupRadioNotifications error %i\n", err);
        PAL.Fatal("MPSL::SetupRadioNotifications");
    }
}

void MPSL::SetupShell()
{
    Shell::AddCommand("mpsl.report", [](vector<string> argList){
        timeline_.Report();
    }, { .argCount = 0, .help = "MPSL Timeline Report"});
}

void MPSL::SetupFatalHandler()
{
    PAL.RegisterOnFatalHandler("MPSL Fatal Handler", []{
        MPSL::FatalHandler();
    });
}

void MPSL::InstallIRQs()
{
    IRQ_CONNECT(MPSL_BOUNCE_IRQ,
                MPSL_BOUNCE_IRQ_PRIORITY,
                IRQBounceHandler,
                nullptr,
                MPSL_BOUNCE_IRQ_FLAGS);

    irq_enable(MPSL_BOUNCE_IRQ);

    IRQ_CONNECT(MPSL_RADIO_NOTIFY_IRQ,
                MPSL_RADIO_NOTIFY_PRIORITY,
                IRQRadioNotificationHandler,
                nullptr,
                MPSL_RADIO_NOTIFY_FLAGS);

    irq_enable(MPSL_RADIO_NOTIFY_IRQ);
}


///////////////////////////////////////////////////////////////////////////////
// MPSL Assert Handler
///////////////////////////////////////////////////////////////////////////////

void mpsl_assert_handle(char */*file*/, uint32_t /*line*/)
{
    PAL.Fatal("mpsl_assert_handle");
}


///////////////////////////////////////////////////////////////////////////////
// MPSL Fatal Handler
///////////////////////////////////////////////////////////////////////////////

void MPSL::FatalHandler()
{
    timeline_.ReportNow("MPSL FatalHandler");

    // no k_oops; a reset occurrs after this anyway, and the
    // error text from oops doesn't add anything, it just
    // tells me there is a problem and that oops got invoked
    // from this function, but yeah, I know.

    // Notify all sessions
    int i = 0;
    for (auto &ss : ssMap_)
    {
        if (ss.isValidData)
        {
            printk("MPSL: Forwarding assert to session %i\n", i);
            ss.handler->OnAssert();
        }

        ++i;
    }
}


///////////////////////////////////////////////////////////////////////////////
// Storage
///////////////////////////////////////////////////////////////////////////////

MPSL::SessionState MPSL::ssMap_[CONFIG_MPSL_TIMESLOT_SESSION_COUNT];
CircularBuffer<MPSL::BounceState> MPSL::bouncePipe_(MPSL::PIPE_CAPACITY);

