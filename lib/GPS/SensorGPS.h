#pragma once

#include <math.h>

#include "PAL.h"
#include "TimedEventHandler.h"

#include "GPS.h"


class GPSController
{
public:
    
    // Account for the minimum of 1 NMEA sentences which have to be consumed
    // in order to get a fresh synchronous time lock.
    // This is ~70 bytes at 9600 baud.
    // Each byte is a bit under 1ms with 8N1 encoding, so we'll leave
    // it calculated as 70ms.
    static const uint32_t MIN_DELAY_NEW_TIME_LOCK_MS = 70;
    
    enum class ResetType : uint16_t
    {
        HOT  = 0x0000,
        WARM = 0x0001,
        COLD = 0xFFFF,
    };
    
    enum class ResetMode : uint8_t
    {
        HW                  = 0x00,
        SW                  = 0x01,
        SW_GPS_ONLY         = 0x02,
        HW_AFTER_SD         = 0x04,
        CONTROLLED_GPS_DOWN = 0x08,
        CONTROLLED_GPS_UP   = 0x09,
    };

    
private:
    static const uint32_t BAUD                  = 9600;
    static const uint32_t POLL_PERIOD_MS        = 5;
    static const uint16_t GPS_WARMUP_TIME_MS    = 200;
    
    using FnGetAbstractMeasurement               = uint8_t (SensorGPS::*)(Measurement *);
    using FnGetNewAbstractMeasurementSynchronous = uint8_t (SensorGPS::*)(Measurement *, uint32_t, uint32_t *);
    
    
public:

    ///////////////////////////////////////////////////////////////////////////
    //
    // Ctor / Init / Debug
    //
    ///////////////////////////////////////////////////////////////////////////

    // All configuration has to take place post-Init, as messages are relayed
    // to the GPS, and that isn't possible until after Init.
    void Init()
    {
        // Give the GPS a moment to warm up and do stuff
        PAL.Delay(GPS_WARMUP_TIME_MS);

        // Interface with serial
        EnableSerialInput();
    }
    
    ///////////////////////////////////////////////////////////////////////////
    //
    // Fine-grained control over active interface to GPS to account for
    // garbage knockoff GPS modules which try to power themselves this way.
    //
    ///////////////////////////////////////////////////////////////////////////

    void DisableSerialInput()
    {
        // Drastic requirement discovered through trial an error when working
        // with knockoff gps module.
        // PAL.PinMode(pinRx_, OUTPUT);
        // PAL.DigitalWrite(pinRx_, LOW);
        
        // ss_.stopListening();
        
        timer_.DeRegisterForTimedEvent();
    }
    
    void EnableSerialInput()
    {
        // Drastic requirement discovered through trial an error when working
        // with knockoff gps module.
        // PAL.PinMode(pinRx_, INPUT);
        
        // ss_.begin(BAUD);
        // ss_.listen();
        
        // Set timers to come read serial data periodically
        timer_.SetCallback([this]() { OnPoll(); });
        timer_.RegisterForTimedEventInterval(POLL_PERIOD_MS);
    }
    
    void DisableSerialOutput()
    {
        // Not really disabled, obviously, more like the pin put low since there
        // are some garbage GPS modules which will sink huge current through
        // serial-in.
        //
        // This is a desperation workaround to allow calling code to deal with
        // that themselves.
        // PAL.DigitalWrite(pinTx_, LOW);
    }
    
    void EnableSerialOutput()
    {
        // PAL.DigitalWrite(pinTx_, HIGH);
    }
    
    
    ///////////////////////////////////////////////////////////////////////////
    //
    // Configure behavior of and Command GPS module (post-Init)
    //
    ///////////////////////////////////////////////////////////////////////////

    void SetHighAltitudeMode()
    {
        uint8_t bufLen = 44;
        // uint8_t buf[] = {
        //     0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0x01, 0x00,
        //     0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0x55, 0xB4,
        // };
        
        // ss_.write(buf, bufLen);
        
        // Sync-wait for all bytes to be written.
        // At 9600 baud, each byte is just under 1ms, safe to delay by bufLen.
        PAL.Delay(bufLen);
    }

    void SaveConfiguration()
    {
        uint8_t bufLen = 21;
        // uint8_t buf[] = {
        //     0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0x01, 0x1B, 0xA9,
        // };

        // ss_.write(buf, bufLen);
        
        // Sync-wait for all bytes to be written.
        // At 9600 baud, each byte is just under 1ms, safe to delay by bufLen.
        PAL.Delay(bufLen);
    }
    
    // COLD HW reset
    void ResetModule()
    {
        uint8_t bufLen = 12;
        // uint8_t buf[] = {
        //     0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0xFF, 0xFF,
        //     0x00, 0x00, 0x0C, 0x5D,
        // };

        // ss_.write(buf, bufLen);
        
        // Sync-wait for all bytes to be written.
        // At 9600 baud, each byte is just under 1ms, safe to delay by bufLen.
        PAL.Delay(bufLen);
    }
    
    
    ///////////////////////////////////////////////////////////////////////////
    //
    // Runtime interface, getting/reseting locks on GPS data
    //
    // 2 things you can use the GPS for:
    // - locking onto time
    // - locking onto location
    // 
    // There are multiple similar interfaces to interact with each type.
    //
    // Note: Once Init() is called, data from GPS (when on) is polled continuously
    //       and used to maintain state about whether a lock has been acquired.
    // 
    // Interfaces for time/location below fall into two categories:
    // - Accessing already-acquired data which may reveal we have a lock
    // - Synchronously discarding any lock and re-acquiring a lock
    //
    //
    // Accessing already-acquired data which may reveal we have a lock
    // ---------------------------------------------------------------
    // GetTimeMeasurement / GetLocationMeasurement
    // 
    // 
    // Synchronously discarding any lock and re-acquiring a lock
    // ---------------------------------------------------------
    // GetNewTimeMeasurementSynchronous              / GetNewLocationMeasurementSynchronous
    // GetNewTimeMeasurementSynchronousUnderWatchdog / GetNewLocationMeasurementSynchronousUnderWatchdog
    // GetNewTimeMeasurementSynchronousTwoMinuteMarkUnderWatchdog / -
    //
    //
    ///////////////////////////////////////////////////////////////////////////
    
    void ResetFix()
    {
        nmeaState_.Reset();
    }
    
    
    ///////////////////////////////////////////////////////////////////////////
    //
    // Time-related Locks
    //
    ///////////////////////////////////////////////////////////////////////////
    
    // May not return true if the async polling hasn't acquired enough data yet
    uint8_t GetTimeMeasurement(Measurement *m)
    {
        uint8_t retVal = 0;
        
        // Check if both necessary sentences have been received
        if (nmeaState_.HasFixTime())
        {
            retVal = 1;
            
            // Only the time-related fields will be valid
            GetMeasurementInternal(m);
        }
        
        return retVal;
    }
    
    // This function ignores the fact that other async polling events may be
    // going on.  They do not impact this functionality.
    uint8_t GetNewTimeMeasurementSynchronous(Measurement   *m,
                                             uint32_t       timeoutMs,
                                             uint32_t      *timeUsedMs = NULL)
    {
        return GetNewAbstractMeasurementSynchronous(&SensorGPS::GetTimeMeasurement, m, timeoutMs, timeUsedMs);
    }

    uint8_t GetNewTimeMeasurementSynchronousUnderWatchdog(Measurement *m, uint32_t timeoutMs, uint32_t *timeUsedMs = NULL)
    {
        return GetNewAbstractMeasurementSynchronousUnderWatchdog(&SensorGPS::GetNewTimeMeasurementSynchronous, m, timeoutMs, timeUsedMs);
    }
    
    uint8_t GetNewTimeMeasurementSynchronousTwoMinuteMarkUnderWatchdog(
        Measurement             *m,
        uint32_t                 durationEachAttemptMs,
        function<void(void)>    &fnBeforeAttempt,
        function<void(void)>    &fnAfterAttempt)
    {
        uint8_t retVal = 0;
        
        fnBeforeAttempt();
        uint8_t gpsLockOk = GetNewTimeMeasurementSynchronousUnderWatchdog(m, durationEachAttemptMs);
        fnAfterAttempt();
        
        // Sleep until next GPS lock time or two min mark, whichever is
        // appropriate.
        uint32_t durationActualBeforeTwoMinMark = 0;
        if (gpsLockOk)
        {
            // Now we're able to know how long we are before the two min mark by
            // looking at the GPS data.
            // Instead of running the GPS constantly, sleep off enough time that
            // we can take one final shot at a fresh lock right beforehand and
            // fine tune our internal clock targeting by using a fresh GPS
            // time lock.
            uint32_t durationTargetBeforeTwoMinMarkToWakeMs = (durationEachAttemptMs + 2000);
            durationActualBeforeTwoMinMark                  = SleepToCloseToTwoMinMark(m, durationTargetBeforeTwoMinMarkToWakeMs);
        }
        
        // If time available to get another GPS lock, do it.
        if (gpsLockOk)
        {
            // We now know the actual duration remaining before the two min mark.
            // Possible we don't have time to try to lock again, due to,
            // for example, getting a time lock with 1 second remaining before
            // the two minute mark.  This is ok, we'll skip the second lock
            // in this case.
            if (durationActualBeforeTwoMinMark >= durationEachAttemptMs)
            {
                // We don't record the success of this lock, because whether it
                // did or didn't, we have a valid lock from previously that
                // we're going to use for the final sleep
                fnBeforeAttempt();
                GetNewTimeMeasurementSynchronousUnderWatchdog(m, durationEachAttemptMs);
                fnAfterAttempt();
            }
        }
        
        // Sleep until two min mark.  Either because we got a new more precise
        // time lock, or because we are burning off the remaining time from
        // the original lock.
        if (gpsLockOk)
        {
            // We have now locked once, and we're about to sleep off any
            // remaining time before the two min mark.
            // Whether we got two locks or one, this code should execute in
            // order to do our best to lock to the two min mark.  Therefore this
            // is a success.
            retVal = 1;
            
            // We want to now burn off remaining time (probably seconds)
            // right up to the two minute mark.
            // If we succeeded at the last lock attempt, we'll be using
            // that time as the reference.  Otherwise we're using the
            // original lock time as reference.
            SleepToCloseToTwoMinMark(m, 0);
        }
        
        return retVal;
    }
    
    
    ///////////////////////////////////////////////////////////////////////////
    //
    // Location-related Locks
    //
    ///////////////////////////////////////////////////////////////////////////
    
    // May not return true if the async polling hasn't acquired enough data yet
    uint8_t GetLocationDMSMeasurement(MeasurementLocationDMS *mdms)
    {
        uint8_t retVal = 0;
        
        // Check if both necessary sentences have been received
        if (nmeaState_.HasFix2D())
        {
            retVal = 1;
            
            GetMeasurementLocationDMSInternal(mdms);
        }
        
        return retVal;
    }

    // May not return true if the async polling hasn't acquired enough data yet
    uint8_t GetLocationMeasurement(Measurement *m)
    {
        uint8_t retVal = 0;
        
        // Check if both necessary sentences have been received
        if (nmeaState_.HasFix3DPlus())
        {
            retVal = 1;
            
            GetMeasurementInternal(m);
        }
        
        return retVal;
    }
    
    // This function ignores the fact that other async polling events may be
    // going on.  They do not impact this functionality.
    uint8_t GetNewLocationMeasurementSynchronous(Measurement   *m,
                                                 uint32_t       timeoutMs,
                                                 uint32_t      *timeUsedMs = NULL)
    {
        return GetNewAbstractMeasurementSynchronous(&SensorGPS::GetLocationMeasurement, m, timeoutMs, timeUsedMs);
    }
    
    uint8_t GetNewLocationMeasurementSynchronousUnderWatchdog(Measurement *m, uint32_t timeoutMs, uint32_t *timeUsedMs = NULL)
    {
        return GetNewAbstractMeasurementSynchronousUnderWatchdog(&SensorGPS::GetNewLocationMeasurementSynchronous, m, timeoutMs, timeUsedMs);
    }


private:

    void GetMeasurementLocationDMSInternal(MeasurementLocationDMS *mdms)
    {
        auto fix2d = nmeaState_.GetFix2D();

        mdms->latitudeDegrees = fix2d.latitudeDegrees;
        mdms->latitudeMinutes = fix2d.latitudeMinutes;
        mdms->latitudeSeconds = fix2d.latitudeSeconds;
        mdms->longitudeDegrees = fix2d.longitudeDegrees;
        mdms->longitudeMinutes = fix2d.longitudeMinutes;
        mdms->longitudeSeconds = fix2d.longitudeSeconds;
    }
    
    void GetMeasurementInternal(Measurement *m)
    {
        // Timestamp this moment as being the moment the lock was considered acquired
        m->clockTimeAtMeasurement = PAL.Millis();
        
        auto fixAll = nmeaState_.GetFixAll();

        m->latitudeDegreesMillionths  = fixAll.latitudeDegreesMillionths;
        m->longitudeDegreesMillionths = fixAll.longitudeDegreesMillionths;

        m->year        = fixAll.year;
        m->month       = fixAll.month;
        m->day         = fixAll.day;
        m->hour        = fixAll.hour;
        m->minute      = fixAll.minute;
        m->second      = fixAll.second;
        m->millisecond = fixAll.millisecond;
 
        m->courseDegrees = fixAll.courseDegrees;
        m->speedKnots    = fixAll.speedKnots;
 
        m->locationDms.latitudeDegrees  = fixAll.latitudeDegrees;
        m->locationDms.latitudeMinutes  = fixAll.latitudeMinutes;
        m->locationDms.latitudeSeconds  = fixAll.latitudeSeconds;
        m->locationDms.longitudeDegrees = fixAll.longitudeDegrees;
        m->locationDms.longitudeMinutes = fixAll.longitudeMinutes;
        m->locationDms.longitudeSeconds = fixAll.longitudeSeconds;

        m->maidenheadGrid = fixAll.maidenheadGrid;

        m->altitudeFt = fixAll.altitudeFt;
    }

public:

    void OnPoll()
    {

    }
    
    void OnLine(const string &line)
    {
        // We only want to consume serial data, not extract measurements.
        nmeaState_.AccumulateLine(line);
    }

    
private:
public: // for testing

    // This function ignores the fact that other async polling events may be
    // going on.  They do not impact this functionality.
    uint8_t GetNewAbstractMeasurementSynchronous(FnGetAbstractMeasurement  fn,
                                                 Measurement              *m,
                                                 uint32_t                  timeoutMs,
                                                 uint32_t                 *timeUsedMs = NULL)
    {
        ResetFix();
        
        uint8_t retVal = 0;
        
        uint32_t timeStart = PAL.Millis();
        
        uint8_t cont = 1;
        while (cont)
        {
            // OnPoll();
            
            uint8_t measurementOk = (*this.*fn)(m);

            uint32_t timeUsedMsInternal = PAL.Millis() - timeStart;
            
            if (timeUsedMs)
            {
                *timeUsedMs = timeUsedMsInternal;
            }
            
            if (measurementOk)
            {
                retVal = 1;
                
                cont = 0;
            }
            else
            {
                if (timeUsedMsInternal >= timeoutMs)
                {
                    cont = 0;
                }
            }
        }
        
        return retVal;
    }
    
    uint8_t GetNewAbstractMeasurementSynchronousUnderWatchdog(FnGetNewAbstractMeasurementSynchronous  fn,
                                                              Measurement                            *m,
                                                              uint32_t                                timeoutMs,
                                                              uint32_t                               *timeUsedMs)
    {
        const uint32_t ONE_SECOND_MS = 1000L;
        
        uint8_t retVal = 0;
        
        uint32_t timeStart = PAL.Millis();
        
        uint32_t durationRemainingMs = timeoutMs;
        
        uint8_t cont = 1;
        while (cont)
        {
            // Kick the watchdog
            // PAL.WatchdogReset();
            
            // Calculate how long to wait for, one second or less each time
            uint32_t timeoutMsGps = durationRemainingMs > ONE_SECOND_MS ? ONE_SECOND_MS : durationRemainingMs;
            
            // Attempt lock
            retVal = (*this.*fn)(m, timeoutMsGps, timeUsedMs);
            
            if (retVal)
            {
                cont = 0;
            }
            else
            {
                durationRemainingMs -= timeoutMsGps;
                
                if (durationRemainingMs == 0)
                {
                    cont = 0;
                }
            }
        }
        
        if (timeUsedMs)
        {
            *timeUsedMs = PAL.Millis() - timeStart;
        }
        
        return retVal;
    }
    
    // Return the duration remaining before the two min mark.
    // Ideally this would match the durationTargetBeforeTwoMinMarkToWakeMs, but several
    // factors make it very possible that we're not able to achieve this.
    //
    // If the two min mark is now or in the past, return 0.
    uint32_t SleepToCloseToTwoMinMark(Measurement *m, uint32_t durationTargetBeforeTwoMinMarkToWakeMs)
    {
        uint32_t retVal = 0;
        
        // Give this a convenient name
        uint32_t timeGpsLock = m->clockTimeAtMeasurement;
        
        // Determine how long after the GPS lock the time will be at the
        // 2 min mark.
        uint32_t durationBeforeMarkMs = CalculateDurationMsToNextTwoMinuteMark(m);
        
        // Consider whether you have any time to burn by sleeping to get to the
        // desired time before the mark.
        if (durationBeforeMarkMs <= durationTargetBeforeTwoMinMarkToWakeMs)
        {
            // Already at or within, and that's not even considering the fact
            // that timeNow is later than the time the lock occurred.
            
            // Calculate time remaining before the mark as the return value.
            uint32_t timeNow = PAL.Millis();
            uint32_t timeDiff = timeNow - timeGpsLock;
            
            if (timeDiff <= durationBeforeMarkMs)
            {
                retVal = durationBeforeMarkMs - timeDiff;
            }
            else
            {
                retVal = 0;
            }
        }
        else
        {
            // At the time of the lock, we were earlier than the duration before the
            // mark that we were aiming for.
            
            // We know we can sleep for this long because we know the duration
            // remaining to the mark is greater than how much before the mark
            // we're supposed to wake.
            //
            // Notably we're doing this calculation based on the time the lock
            // was actually acquired, not the current time.
            uint32_t durationToSleepMs = durationBeforeMarkMs - durationTargetBeforeTwoMinMarkToWakeMs;
            
            // Now account for the actual time possibly having eaten up part or
            // all of the sleep duration.
            uint32_t timeNow = PAL.Millis();
            uint32_t timeDiff = timeNow - timeGpsLock;
            
            if (timeDiff < durationToSleepMs)
            {
                // Ok, difference still small enough that we should sleep.
                // Remove the time skew from lock to now though so we actually
                // wake at the correct time.
                durationToSleepMs -= timeDiff;
                
                // PAL.DelayUnderWatchdog(durationToSleepMs);
                
                retVal = durationTargetBeforeTwoMinMarkToWakeMs;
            }
            else
            {
                // Oops, enough time has passed since the gps lock that we're
                // at or within the window before the mark we're supposed to
                // wake at.
                uint32_t timeDiffTargetVsLock = durationBeforeMarkMs - durationTargetBeforeTwoMinMarkToWakeMs;
                
                if (timeDiff - timeDiffTargetVsLock < durationTargetBeforeTwoMinMarkToWakeMs)
                {
                    retVal = durationTargetBeforeTwoMinMarkToWakeMs - (timeDiff - timeDiffTargetVsLock);
                }
                else
                {
                    retVal = 0;
                }
            }
        }
        
        return retVal;
    }

    uint32_t CalculateDurationMsToNextTwoMinuteMark(Measurement *m)
    {
        // Determine the digits on the clock of the next 2-minute mark
        // We know the seconds on the clock will be :00
        // The minute is going to be even.
        // So, if the current minute is:
        // - odd,  that means we're at most  60 sec away from the mark
        // - even, that means we're at most 120 sec away from the mark
        //
        // Later we deal with the case where we're already exactly at a
        // two-minute mark.
        uint8_t currentMinIsOdd = m->minute & 0x01;
        uint8_t maximumMinutesBeforeMark = currentMinIsOdd ? 1 : 2;
        
        uint8_t markMin = (m->minute + maximumMinutesBeforeMark) % 60;
        
        // Determine how far into the future the mark time is.
        if (((m->minute + 1) % 60) == markMin)
        {
            // We're at most 1 minute away.
            maximumMinutesBeforeMark = 1;
        }
        else // (((m->minute + 2) % 60) == markMin)
        {
            // We're at most 2 minutes away.
            maximumMinutesBeforeMark = 2;
        }
        
        // Calculate how long until the mark.
        // Take into consideration the seconds and milliseconds on the
        // clock currently.
        //
        // eg if we're at 01:27.250
        // then
        // - markMin = 2
        // - maximumMinutesBeforeMark = 1
        // - durationBeforeMarkMs = 
        //     (60 *  1 * 1000) -     // 60,000
        //     (     27 * 1000) -     // 27,000
        //     (           250)       //    250
        // - durationBeforeMarkMs  = 32,750 // 32 sec, 750ms, correct
        // 
        uint32_t durationBeforeMarkMs =
            (60L * maximumMinutesBeforeMark * 1000L) -
            (m->second * 1000L) -
            (m->millisecond);
        
        return durationBeforeMarkMs;
    }
    
    
private:

    NmeaStringAccumulatedState nmeaState_;
    
    TimedEventHandlerDelegate timer_;
};




