#pragma once

#include <unordered_map>
#include <functional>
using namespace std;

#include "ClassTypes.h"
#include "MPSL.h"

// Will get callbacks of start/end of timeslot
// Can end slot early, but only between start/end
class MpslSession
: private NonCopyable
, private NonMovable
, private MPSLEventHandler
{
public:
    ~MpslSession();

    void OpenSession();
    void SetCbOnStart(function<void()> cbFnOnStart);
    void SetCbOnEnd(function<void()> cbFnOnEnd);
    void SetCbOnNoMoreComing(function<void()> cbFnOnNoMoreComing);
    void SetCbOnRadioAvailable(function<void()> cbFnOnRadioAvailable);
    void SetCbOnAssert(function<void()> cbFnOnAssert);
    void RequestTimeslots(uint32_t periodUs, uint32_t durationUs, bool highPriority = false);
    void EndThisTimeslot();
    void CancelTimeslots();
    void EnableTimeslotExtensions();
    void DisableTimeslotExtensions();
    void CloseSession();

    static void SetupShell();

// Instance
private:
    mpsl_timeslot_session_id_t sessionId_ = 0xFF;
    
    bool wantsNextSlot_ = false;
    bool wantsMaxSlotTime_ = false;
    bool isInTimeslot_ = false;

    function<void()> cbFnOnStart_          = []{};
    function<void()> cbFnOnEnd_            = []{};
    function<void()> cbFnOnNoMoreComing_   = []{};
    function<void()> cbFnOnRadioAvailable_ = []{};
    function<void()> cbFnOnAssert_         = []{};

    uint32_t periodUs_;
    uint32_t durationUs_;
    mpsl_timeslot_request_t timeslotReq_;


// Inherited
private:
    virtual void OnTimeslotStart();
    virtual bool WantsExtension();
    virtual bool WantsNextSlot();
    virtual void OnTimeslotEnd();
    virtual void OnTimeslotNoMoreComing();
    virtual void OnRadioAvailable();
    virtual void OnAssert();
};


