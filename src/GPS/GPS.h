#pragma once

#include "Evm.h"
#include "Log.h"
#include "NMEAStringMaker.h"
#include "NMEAStringParser.h"
#include "UbxMessage.h"
#include "Utl.h"

#include <string>
#include <vector>
#include <unordered_set>
using namespace std;



// https://receiverhelp.trimble.com/alloy-gnss/en-us/NMEA-0183messages_CommonMessageElements.html
// Actual device I'm aiming to support specifically: ATGM336H-5N
//   https://www.icofchina.com/d/file/xiazai/2016-12-05/b5c57074f4b1fcc62ba8c7868548d18a.pdf
//   NMEA0183 protocol
//   - v4.10 published May 2012 (and erratum May 12 2012)
//   - v4.11 published Nov 2018 - supports GNSS (global navigation satellite systems) beyond GPS
//   "CASIC multi mode satellite navigation receiver protocol specification
//     https://docs.datagnss.com/rtk-board/files/T-5-1908-001-GNSS_Protocol_Specification-V2.3.pdf
// https://www.icofchina.com/d/file/xiazai/2016-12-05/b5c57074f4b1fcc62ba8c7868548d18a.pdf

struct FixTime
{
    uint16_t year = 0;
    uint8_t  month = 0;
    uint8_t  day = 0;
    uint8_t  hour = 0;
    uint8_t  minute = 0;
    uint8_t  second = 0;
    uint16_t millisecond = 0;
    string   dateTime;

    void Print() const
    {
        Log("FixTime");
        Log("year        = ", year);
        Log("month       = ", month);
        Log("day         = ", day);
        Log("hour        = ", hour);
        Log("minute      = ", minute);
        Log("second      = ", second);
        Log("millisecond = ", millisecond);
        Log("dateTime    = ", dateTime);
    }
};

struct Fix2D
: public FixTime
{
    int16_t  latDeg = 0;
    uint8_t  latMin = 0;
    uint8_t  latSec = 0;
    int32_t latDegMillionths = 0;
    
    int16_t  lngDeg = 0;
    uint8_t  lngMin = 0;
    uint8_t  lngSec = 0;
    int32_t lngDegMillionths = 0;

    static const uint8_t MAIDENHEAD_GRID_LEN = 6;
    string maidenheadGrid;

    void Print() const
    {
        FixTime::Print();
        Log("Fix2D");
        Log("latDeg           = ", latDeg);
        Log("latMin           = ", latMin);
        Log("latSec           = ", latSec);
        Log("latDegMillionths = ", latDegMillionths);
        Log("lngDeg           = ", lngDeg);
        Log("lngMin           = ", lngMin);
        Log("lngSec           = ", lngSec);
        Log("lngDegMillionths = ", lngDegMillionths);
        Log("maidenheadGrid   = ", maidenheadGrid);
    }
};

struct Fix3D
: public Fix2D
{
    int32_t altitudeM = 0;
    int32_t altitudeFt = 0;

    void Print() const
    {
        Fix2D::Print();
        Log("Fix3D");
        Log("altitudeM  = ", altitudeM);
        Log("altitudeFt = ", altitudeFt);
    }
};

struct Fix3DPlus
: public Fix3D
{
    uint32_t courseDegrees = 0;
    uint32_t speedKnots = 0;
    uint32_t speedMph = 0;
    uint32_t speedKph = 0;

    void Print() const
    {
        Fix3D::Print();
        Log("Fix3DPlus");
        Log("courseDegrees = ", courseDegrees);
        Log("speedKnots    = ", speedKnots);
        Log("speedMph      = ", speedMph);
        Log("speedKph      = ", speedKph);
    }
};

struct FixSatelliteData
{
    uint16_t id       = 0;
    uint8_t elevation = 0;
    uint16_t azimuth  = 0;
};


class GPSReader
{
    // Which UART to use to talk to the GPS hardware
    UART uart_ = UART::UART_1;

private:

    struct AccumulatedData
    {
        // Time
        string timeStr;
        string dateStr;
        uint64_t timeAtTimeLock = 0;
        string fixTimeSource;

        uint8_t consecutiveRoundCount = 0;
        string timeRoundLast;

        // 2D Fix
        string latStr;
        char latNorthSouth = 'N';
        string lngStr;
        char lngEastWest = 'W';
        uint64_t timeAtFix2d = 0;
        string fix2dSource;

        // 3D Fix (requires 2D fix also)
        string altitudeStr;
        uint64_t timeAtFix3d = 0;
        vector<string> fix3dSourceList;

        // 3D Plus Fix
        string speedKnotsStr;
        string courseDegreesStr;
        uint64_t timeAtSpeedCourse = 0;
        vector<string> fix3dPlusSourceList;

        // Satellite list
        vector<FixSatelliteData> satDataList;
    };
    AccumulatedData data_;

    // Callbacks
    function<void(const FixTime &)>                             cbOnFixTime_        = [](const FixTime &){};
    function<void(const Fix2D &)>                               cbOnFix2D_          = [](const Fix2D &){};
    function<void(const Fix3D &)>                               cbOnFix3D_          = [](const Fix3D &){};
    function<void(const Fix3DPlus &)>                           cbOnFix3DPlus_      = [](const Fix3DPlus &){};
    function<void(const vector<FixSatelliteData> &satDataList)> cbOnFixSatDataList_ = [](const vector<FixSatelliteData> &){};

    bool cbOnFixTimeSet_ = false;
    bool cbOnFix2DSet_ = false;
    bool cbOnFix3DSet_ = false;
    bool cbOnFix3DPlusSet_ = false;
    bool cbOnFixSatDataListSet_ = false;

    // UBX message monitoring state
    uint8_t idNmea_ = 0;

    // Stats
    struct Stats
    {
        uint32_t countNmeaMessagesSeen = 0;
    };

    Stats stats_;

    bool verboseLogging_ = true;

    bool processData_ = true;


public:

    void DisableVerboseLogging()
    {
        verboseLogging_ = false;
    }

    /////////////////////////////////////////////////////////////////
    // Actions - Sync
    /////////////////////////////////////////////////////////////////

    // Assumes one line at a time, ascii only, no newlines
    uint64_t ReadLine(const string &line)
    {
        // We know about these, so don't warn if seen
        static const unordered_set<string> msgIgnoreSet = {
            "GSA",
            "GGA",
            "GLL",
            "GSV",
            "RMC",
            "VTG",
            "ZDA",
            "TXT",
        };

        uint64_t timeStart = PAL.Micros();

        if (NMEAStringParser::IsValid(line))
        {
            vector<string> linePartList = NMEAStringParser::GetLineDataPartList(line);

            string &type = linePartList[0];

            // Get last 3 chars representing the message
            if (type.size() >= 3)
            {
                type = type.substr(type.size() - 3);
            }

            bool ok = true;

            // "The default output is GGA, GSA, GSV and RMC in 1 second period"
            //
            // GP prefix is not universal.
            // GN is also used.  I think these may differ.  Need to confirm case-by-case.
            // Sometimes both.
            //
            // "NMEA is the standard of GNSS protocol"
            // GP for GPS-QZSS-SBAS - USA
            // BD for BEIDOU-only   - China
            // GL for GLONASS-only  - Russia
            // GI for INSAT-only    - India
            // GA for GALILEO-only  - EU
            // GN is for GNSS, combination of different global position satellite systems
            
            if (type == "RMC") // RMC - Recommended Minimum data for GPS
            {
                ok = OnRMC(linePartList, line);
            }
            else if (type == "GGA") // GGA - Fix information
            {
                ok = OnGGA(linePartList, line);
            }
            else if (type == "GSV") // GSV - Detailed satellite data (multi-row)
            {
                ok = OnGSV(linePartList);
            }
            // VTG - Vector track and speed over ground
            // GSA - Overall satellite data (multi-row)
            // GLL - Lat/Ln data
            // GRS - GNSS range residuals
            // ZDA - Date and time
            // GST - Pseudorange Error Statistics
            // TXT - Antenna status message
            else
            {
                if (!msgIgnoreSet.contains(type))
                {
                    ok = false;
                    Log("Unknown message: ", linePartList[0]);
                }
            }

            if (!ok)
            {
                LogNL();
                Log("Line not processed correctly");
                Log("\"", line, "\"");
                for (int i = 0; const string &linePart : linePartList)
                {
                    Log(i, ": \"", linePart, "\"");
                    ++i;
                }
                LogNL();
            }
        }
        else
        {
            if (verboseLogging_)
            {
                Log("Invalid line: \"", line, "\"");
            }
        }

        uint64_t diffUs = PAL.Micros() - timeStart;

        return diffUs;
    }

    // cause all current data to be considered out of date
    // new data needs to be processed before being used
    void Reset()
    {
        data_ = AccumulatedData{};
    }

    void ResetAndDelayProcessing(uint64_t msDelay)
    {
        Reset();

        processData_ = false;
        static Timer tTimeout;
        tTimeout.SetCallback([this]{
            processData_ = true;
        }, "GPS_READER_RE-ENABLE");
        tTimeout.TimeoutInMs(msDelay);
    }

    void DumpState()
    {
        LogNL();

        // Stats
        Log("Stats:");
        Log("- NMEA Messages Seen: ", stats_.countNmeaMessagesSeen);

        // Monitoring status
        LogNL();
        Log("Monitoring:");
        Log("- NMEA Messages: ", idNmea_ != 0);

        LogNL();
    }


    /////////////////////////////////////////////////////////////////
    // Actions - Async
    /////////////////////////////////////////////////////////////////

    void StartMonitoring()
    {
        StopMonitoring();

        if (verboseLogging_)
        {
            Log("Reader starting monitoring for NMEA");
        }

        auto [ok, id] = UartAddLineStreamCallback(uart_, [this](const string &line){
            UartTarget target(UART::UART_0);

            if (processData_)
            {
                ++stats_.countNmeaMessagesSeen;

                // Send to sync line processor
                ReadLine(line);
            }
        });

        idNmea_ = ok ? id : 0;
    }

    void StopMonitoring()
    {
        if (idNmea_ != 0)
        {
            Log("Reader stopping monitoring for NMEA");

            UartRemoveLineStreamCallback(uart_, idNmea_);

            idNmea_ = 0;
        }
    }


    /////////////////////////////////////////////////////////////////
    // Checkers
    /////////////////////////////////////////////////////////////////

    // can extract
    // - time (not date)
    // - duration since acquire
    bool HasFixTime()
    {
        return data_.timeAtTimeLock != 0;
    }

    // can extract:
    // - lat/lng
    // - duration since fix
    bool HasFix2D()
    {
        return data_.timeAtFix2d != 0;
    }

    // can extract:
    // - time
    // - lat/lng
    // - altitude
    // - duration since fix
    bool HasFix3D()
    {
        return data_.timeAtFix3d != 0;
    }

    // can extract
    // - speed
    // - course
    bool HasFix3DPlus()
    {
        return HasFix3D() && data_.timeAtSpeedCourse != 0;
    }

    // can extract
    // - satellite data [{id, elev, az}, ...]
    bool HasSatDataList()
    {
        return data_.satDataList.size();
    }


    /////////////////////////////////////////////////////////////////
    // Getters - Sync
    /////////////////////////////////////////////////////////////////

    vector<FixSatelliteData> GetSatelliteDataList()
    {
        return data_.satDataList;
    }

    FixTime GetFixTime()
    {
        auto TwoCharToInt = [](const char *str) {
            uint16_t val = 0;

            val = ((str[0] - '0') * 10) + (str[1] - '0');

            return val;
        };

        string dateTime;

        // UTC DDMMYY
        uint16_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        if (data_.dateStr.size() == 6)
        {
            year  = 2000 + TwoCharToInt(&data_.dateStr.c_str()[4]);
            month = TwoCharToInt(&data_.dateStr.c_str()[2]);
            day   = TwoCharToInt(&data_.dateStr.c_str()[0]);
        }
        dateTime += FormatStr("%04u", year)  + "-" +
                    FormatStr("%02u", month) + "-" +
                    FormatStr("%02u", day);
        dateTime += " ";

        uint8_t hour = 0;
        uint8_t minute = 0;
        uint8_t second = 0;
        uint16_t millisecond = 0;
        if (data_.timeStr.size() >= 6)
        {
            double timeFloat = fabs(atof(data_.timeStr.c_str()));

            hour = (int)timeFloat / 10000;
            timeFloat = timeFloat - (hour * 10000);

            minute = (int)timeFloat / 100;
            timeFloat = timeFloat - (minute * 100);

            second = (int)timeFloat;
            if (second >= 59) { second = 59; }  // beware leap seconds
            timeFloat = timeFloat - second;

            millisecond = round(timeFloat * 1000);
        }
        dateTime += FormatStr("%02u", hour)   + ":" +
                    FormatStr("%02u", minute) + ":" +
                    FormatStr("%02u", day)    + "." +
                    FormatStr("%06u", millisecond);

        FixTime retVal = {
            .year = year,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .second = second,
            .millisecond = millisecond,
            .dateTime = dateTime,
        };

        return retVal;
    }

    string GetFixTimeSource()
    {
        return data_.fixTimeSource;
    }

    string ConvertFixTimeToString(const FixTime &fix)
    {
        string time;

        time += StrUtl::PadLeft(fix.year, '0', 4);
        time += "-";
        time += StrUtl::PadLeft(fix.month, '0', 2);
        time += "-";
        time += StrUtl::PadLeft(fix.day, '0', 2);
        time += " ";
        time += StrUtl::PadLeft(fix.hour, '0', 2);
        time += ":";
        time += StrUtl::PadLeft(fix.minute, '0', 2);
        time += ":";
        time += StrUtl::PadLeft(fix.second, '0', 2);
        time += ".";
        time += StrUtl::PadLeft(fix.millisecond, '0', 3);

        return time;
    }

    Fix2D GetFix2D()
    {
        auto dmsLat = ToDegMinSec(data_.latStr, data_.latNorthSouth);
        auto dmsLng = ToDegMinSec(data_.lngStr, data_.lngEastWest);

        string maidenheadGrid = ToMaidenheadGrid(dmsLat.degMillionths, dmsLng.degMillionths);

        Fix2D retVal;

        *(FixTime *)&retVal = GetFixTime();

        retVal.latDeg = dmsLat.deg;
        retVal.latMin = dmsLat.min;
        retVal.latSec = dmsLat.sec;
        retVal.latDegMillionths  = dmsLat.degMillionths;

        retVal.lngDeg = dmsLng.deg;
        retVal.lngMin = dmsLng.min;
        retVal.lngSec = dmsLng.sec;
        retVal.lngDegMillionths = dmsLng.degMillionths;

        retVal.maidenheadGrid = maidenheadGrid;

        return retVal;
    }

    string GetFix2DSource()
    {
        return data_.fix2dSource;
    }

    Fix3D GetFix3D()
    {
        Fix3D retVal;

        *(Fix2D *)&retVal = GetFix2D();

        static const double METERS_TO_FEET = 3.28084;

        double altitudeM = atof(data_.altitudeStr.c_str());

        retVal.altitudeM  = round(altitudeM);
        retVal.altitudeFt = round(altitudeM * METERS_TO_FEET);

        return retVal;
    }

    vector<string> GetFix3DSourceList()
    {
        return data_.fix3dSourceList;
    }

    Fix3DPlus GetFix3DPlus()
    {
        Fix3DPlus retVal;

        *(Fix3D *)&retVal = GetFix3D();

        static const double KNOTS_TO_MPH = 1.15078;
        static const double KNOTS_TO_KPH = 1.852;

        retVal.courseDegrees = round(atof(data_.courseDegreesStr.c_str()));

        double speedKnots = atof(data_.speedKnotsStr.c_str());
        retVal.speedKnots = round(speedKnots);
        retVal.speedMph   = round(speedKnots * KNOTS_TO_MPH);
        retVal.speedKph   = round(speedKnots * KNOTS_TO_KPH);
        
        return retVal;
    }

    static Fix3DPlus GetFix3DPlusExample()
    {
        Fix3DPlus retVal;

        retVal.year             = 2025;
        retVal.month            = 1;
        retVal.day              = 2;
        retVal.hour             = 19;
        retVal.minute           = 42;
        retVal.second           = 1;
        retVal.millisecond      = 25;
        retVal.latDeg           = 40;
        retVal.latMin           = 44;
        retVal.latSec           = 30;
        retVal.latDegMillionths = 40741668;
        retVal.lngDeg           = -74;
        retVal.lngMin           = 1;
        retVal.lngSec           = 59;
        retVal.lngDegMillionths = -74032986;
        retVal.maidenheadGrid   = "FN20XR";
        retVal.altitudeFt       = 426;
        retVal.altitudeM        = 130;
        retVal.speedKnots       = 19;
        retVal.speedMph         = 22;
        retVal.speedKph         = 35;
        retVal.courseDegrees    = 206;

        return retVal;
    }

    vector<string> GetFix3DPlusSourceList()
    {
        return data_.fix3dPlusSourceList;
    }


    /////////////////////////////////////////////////////////////////
    // Getters - Async
    /////////////////////////////////////////////////////////////////

public:

    void SetCallbackOnFixTime(function<void(const FixTime &)> cb)
    {
        cbOnFixTime_ = cb;
        cbOnFixTimeSet_ = true;
    }

    void UnSetCallbackOnFixTime()
    {
        cbOnFixTimeSet_ = false;
    }

    void SetCallbackOnFix2D(function<void(const Fix2D &cbOn)> cb)
    {
        cbOnFix2D_ = cb;
        cbOnFix2DSet_ = true;
    }

    void UnSetCallbackOnFix2D()
    {
        cbOnFix2DSet_ = false;
    }

    void SetCallbackOnFix3D(function<void(const Fix3D &)> cb)
    {
        cbOnFix3D_ = cb;
        cbOnFix3DSet_ = true;
    }

    void UnSetCallbackOnFix3D()
    {
        cbOnFix3DSet_ = false;
    }

    void SetCallbackOnFix3DPlus(function<void(const Fix3DPlus &)> cb)
    {
        cbOnFix3DPlus_ = cb;
        cbOnFix3DPlusSet_ = true;
    }
    
    void UnSetCallbackOnFix3DPlus()
    {
        cbOnFix3DPlusSet_ = false;
    }

    void SetCallbackOnFixSatelliteList(function<void(const vector<FixSatelliteData> &satDataList)> cb)
    {
        cbOnFixSatDataList_ = cb;
        cbOnFixSatDataListSet_ = true;
    }

    void UnSetCallbackOnFixSatelliteList()
    {
        cbOnFixSatDataListSet_ = false;
    }

private:

    void OnFixTime()
    {
        if (cbOnFixTimeSet_ && HasFixTime())
        {
            FixTime fix = GetFixTime();

            cbOnFixTime_(fix);
        }
    }

    void OnFix2D()
    {
        if (cbOnFix2DSet_ && HasFix2D())
        {
            Fix2D fix = GetFix2D();

            cbOnFix2D_(fix);
        }
    }

    void OnFix3DMaybe()
    {
        if (cbOnFix3DSet_ && HasFix3D())
        {
            Fix3D fix = GetFix3D();

            cbOnFix3D_(fix);
        }
    }

    void OnFix3DPlusMaybe()
    {
        if (cbOnFix3DPlusSet_ && HasFix3DPlus())
        {
            Fix3DPlus fix = GetFix3DPlus();

            cbOnFix3DPlus_(fix);
        }
    }

    void OnSatDataList()
    {
        if (cbOnFixSatDataListSet_ && HasSatDataList())
        {
            cbOnFixSatDataList_(data_.satDataList);
        }
    }



private:

    /////////////////////////////////////////////////////////////////
    // Converters
    /////////////////////////////////////////////////////////////////

    //
    // https://en.wikipedia.org/wiki/Maidenhead_Locator_System
    //
    // Implementation adapted from TT7:
    // https://github.com/TomasTT7/TT7F-Float-Tracker/blob/master/Software/ARM_WSPR.h
    // https://github.com/TomasTT7/TT7F-Float-Tracker/blob/master/Software/ARM_WSPR.c
    //
    // test here: http://k7fry.com/grid/
    //
    //  40.738059  -74.036227 -> FN20XR
    // -26.30196   -66.709667 -> FG63PQ
    // -31.951585  115.824861 -> OF78VB
    //  10.813707  106.609537 -> OK30HT
    //
    // Will fill out 6-char maidenhead grid
    static string ToMaidenheadGrid(int32_t latDegMillionths,
                                   int32_t lngDegMillionths)
    {
        char grid[7];

        // Put lat/long in 10-thousandths
        int32_t lat = latDegMillionths  / 100;
        int32_t lng = lngDegMillionths / 100;
        
        // Put into grid space, no negative degrees
        lat += (90  * 10000UL);
        lng += (180 * 10000UL);
        
        grid[0] = 'A' +   (lng / 200000);
        grid[1] = 'A' +   (lat / 100000);
        grid[2] = '0' +  ((lng % 200000) / 20000);
        grid[3] = '0' +  ((lat % 100000) / 10000);
        grid[4] = 'A' + (((lng % 200000) % 20000) / 834);
        grid[5] = 'A' + (((lat % 100000) % 10000) / 417);
        grid[6] = '\0';

        string retVal = grid;

        return retVal;
    }

    /* 
     * Formats
     * -------
     * LAT:  4044.21069,N in TinyGPS is  40736878
     * LNG: 07402.03027,W in TinyGPS is -74033790
     *
     * The above GPGGA NMEA message lat/long values are defined as
     * being degrees and minutes with fractional minutes
     * eg: 4807.038,N   Latitude 48 deg 07.038' N
     * (http://www.gpsinformation.org/dale/nmea.htm#GGA)
     *
     * Whereas the TinyGPS values are in millionths of a degree.
     * Their example is:
     *   so instead of 90°30’00″, get_position() returns a longitude
     *        value of 90,500,000, or 90.5 degrees.
     * (http://arduiniana.org/libraries/tinygps/)
     *
     *
     * Convert from TinyGPS to deg/min/sec
     * -----------------------------------
     * 40736878
     * 40 and (0.736878 * 60) minutes
     * -> 40 deg 44.21268 minutes
     *
     * extract seconds from 44.21268 minutes
     * .21268 * 60 = 12.7608 seconds
     *
     * so should be
     * 40 deg 44 min 12.7608 sec
     *
     * or in Google Maps terms
     * 40 44 12.7608
     *
     *
     * Convert from GPS to deg/min/sec
     * -------------------------------
     * 4044.21069,N
     *
     * 40 deg and 44.21069 minutes
     *
     * extract seconds from 44.21069 minutes
     * .21069 * 60 = 12.6414 seconds
     *
     * so should be
     * 40 deg 44 min 12.6414 seconds
     *
     * or in Google Maps terms
     * 40 44 12.6414
     *
     *
     * Google Maps Notation
     * --------------------
     * Google Maps for TinyGPS Conversion
     * 40 44 12.7608, -74 2 2.32
     *
     * Google Maps for GPS Conversion
     * 40 44 12.6414, -74 2 2.32
     *
     * They're right next to each other.  Ok tolerance.
     * https://www.google.com/maps/dir/40+44+12.6414,+-74+2+2.32/40.736878,-74.0339778/@40.7368945,-74.0342609,19.67z/data=!4m7!4m6!1m3!2m2!1d-74.0339778!2d40.7368448!1m0!3e3
     *
     */
    struct DegMinSec
    {
        int16_t  deg = 0;
        uint8_t  min = 0;
        uint8_t  sec = 0;
        int32_t  degMillionths = 0;
    };

    // input format ddmm.mmmm[...]
    static DegMinSec ToDegMinSec(const string &val, char nsew)
    {
        bool isNegative = (nsew == 'S' || nsew == 'W');

        double valFloat = atof(val.c_str()) * (isNegative ? -1 : 1);

        // get degrees (whole)
        int16_t deg = (int)(valFloat / 100);

        // get minutes (ddmm.mmmmm... -> mm.mmmmm...)
        double minFloat = valFloat - (deg * 100);

        // get minutes (mm.mmmmm... -> mm)
        uint8_t min = (uint8_t)fabs(minFloat);

        // get remaining sub-minutes (0.mmmmm...)
        double minFloatRemaining = fabs(minFloat) - min;

        // get seconds whole from remaining (ss.ssss....)
        uint8_t sec = (uint8_t)round(minFloatRemaining * 60);

        // now re-combine to put into millionths of a degree
        // in a way that maximizes original precision
        int32_t degMillionths = round(((double)deg * 1'000'000UL) + (minFloat * 1'000'000UL / 60));

        return { deg, min, sec, degMillionths };
    }


    /////////////////////////////////////////////////////////////////
    // Message Handlers
    /////////////////////////////////////////////////////////////////

    static bool TimeIsRoundSecond(const string &time)
    {
        bool retVal = true;

        // format is something like this
        // GP - hhmmss.ss
        // GN - hhmmss.fff
        // 
        // so find the decimal if there is one, and ensure everything beyond
        // it is 0

        auto pos = time.find('.');

        // if no decimal point, the second is round
        // if there is, confirm
        if (pos != string::npos)
        {
            ++pos;

            while (retVal && pos < time.length())
            {
                retVal = time[pos] == '0';
                ++pos;
            }
        }

        return retVal;
    }

    // Used to determine whether data should be processed.
    // Not stateless.
    // Criteria:
    // - time has to be non-blank
    // - time has to be round seconds (no fractional ms component)
    // - it has to have been that way for 2 consecutive times
    //   - as in, different times seen
    bool TimeStateIsValid(const string &time)
    {
        static const uint8_t COUNT_THRESHOLD = 2;

        bool retVal = false;

        if (time.length())
        {
            if (time != data_.timeRoundLast)
            {
                if (TimeIsRoundSecond(time))
                {
                    if (data_.consecutiveRoundCount < COUNT_THRESHOLD)
                    {
                        ++data_.consecutiveRoundCount;
                        data_.timeRoundLast = time;
                    }

                    if (data_.consecutiveRoundCount == COUNT_THRESHOLD)
                    {
                        retVal = true;
                    }
                }
                else
                {
                    data_.consecutiveRoundCount = 0;
                    data_.timeRoundLast = "";
                }
            }
            else
            {
                // do nothing, it is expected that many sentences
                // will have the same time
            }
        }
        else
        {
            data_.consecutiveRoundCount = 0;
            data_.timeRoundLast = "";
        }

        return retVal;
    }

    // from neo-6
    // $GPRMC,133735.00,A,4044.56345,N,07401.96989,W,0.329,,160323,,,A*6F
    //
    // from atgm336h-5n31
    // $GNRMC,160755.000,A,4044.51805,N,07401.96538,W,0.21,165.84,200323,,,E*65
    bool OnRMC(const vector<string> &linePartList, const string &line)
    {
        bool retVal = true;

        if (linePartList.size() >= 13)
        {
            int i = 1;

            // 1 - Time
            // GP - hhmmss.ss
            // GN - hhmmss.fff
            const string &time = linePartList[i];
            ++i;

            // 2 - Status
            // A = Valid, V = Invalid
            char status = 'V';
            if (linePartList[i].size() >= 1)
            {
                status = linePartList[i][0];
            }
            ++i;

            // 3 - Latitude
            // GP - ddmm.mmmmm
            // GN - llll.lllllll
            const string &latStr = linePartList[i];
            ++i;

            // 4 - North/South indicator
            // N or S
            char northSouth = 'N';
            if (linePartList[i].size() >= 1)
            {
                northSouth = linePartList[i][0];
            }
            ++i;

            // 5 - Longitude
            // GP - dddmm.mmmmm
            // GN - yyyyy.yyyyyyy
            const string &lngStr = linePartList[i];
            ++i;

            // 6 - East/West indicator
            // E or W
            char eastWest = 'W';
            if (linePartList[i].size() >= 1)
            {
                eastWest = linePartList[i][0];
            }
            ++i;

            // 7 - Speed over ground (knots)
            // GP - numeric (eg 0.004)
            // GN - x.x
            const string &speedKnotsStr = linePartList[i];
            ++i;

            // 8 - Course over ground (degrees)
            // GP - numeric (eg 77.52)
            // GN - x.x
            const string &courseDegrees = linePartList[i];
            ++i;

            // 9 - Date (UTC DDMMYY)
            const string &date = linePartList[i];
            ++i;

            // 10 - Magnetic variation
            ++i;

            // 11 - Magnetic variation E/W indicator
            ++i;

            // 12 - Mode Indicator
            // GP - V = Invalid, A = Valid, N = No Fix, E = Estimated, A = Autonomous GNSS Fix, D = Differential GNSS Fix
            // GN - V = Invalid, A = Autonomous, D = Differential, F = Float RTK, P = Precise,  R = Real Time Kinematic
            ++i;

            // 13 - ?
            // GP - unspecified
            // GN - navStatus (NMEA v4.10 and above)
            // ignore this
            ++i;

            // Note the current time
            uint64_t timeNowMs = PAL.Millis();

            bool timeStateIsValid = TimeStateIsValid(time);

            // time is good when non-blank
            // date may not be set yet
            if (timeStateIsValid)
            {
                data_.timeStr = move(time);
                data_.dateStr = move(date);

                data_.timeAtTimeLock = timeNowMs;

                // capture source of lock
                data_.fixTimeSource = line;

                // notify change in data
                OnFixTime();
            }

            // check if 2D location is good
            if (status == 'A' && timeStateIsValid)
            {
                // add to 2D/3D
                data_.latStr = move(latStr);
                data_.latNorthSouth = northSouth;
                data_.lngStr = move(lngStr);
                data_.lngEastWest = eastWest;

                data_.timeAtFix2d = timeNowMs;

                // add to the 3D plus
                data_.speedKnotsStr = move(speedKnotsStr);
                data_.courseDegreesStr = move(courseDegrees);

                data_.timeAtSpeedCourse = timeNowMs;

                // capture source of lock
                data_.fix2dSource = line;
                if (data_.fix3dSourceList.size() == 2)
                {
                    data_.fix3dSourceList.erase(data_.fix3dSourceList.begin());
                }
                data_.fix3dSourceList.push_back(line);
                if (data_.fix3dPlusSourceList.size() == 2)
                {
                    data_.fix3dPlusSourceList.erase(data_.fix3dPlusSourceList.begin());
                }
                data_.fix3dPlusSourceList.push_back(line);

                // notify change in data
                OnFix2D();
                OnFix3DMaybe();
                OnFix3DPlusMaybe();
            }
        }
        else
        {
            retVal = false;
            Log("ERR: Bad RMC");
        }

        return retVal;
    }

    // from neo-6
    // $GPGGA,133735.00,4044.56345,N,07401.96989,W,1,09,1.02,20.8,M,-34.4,M,,*50
    //
    // from atgm336h-5n31
    // $GNGGA,160755.000,4044.51805,N,07401.96538,W,6,04,7.3,115.8,M,0.0,M,,*64
    bool OnGGA(const vector<string> &linePartList, const string &line)
    {
        bool retVal = true;

        if (linePartList.size() >= 15)
        {
            int i = 1;

            // 1 - Time
            // GP - hhmmss.ss
            // GN - hhmmss.fff
            const string &time = linePartList[i];
            ++i;

            // 2 - Latitude
            // GP - ddmm.mmmm
            // GN - llll.lllllll
            const string &latStr = linePartList[i];
            ++i;

            // 3 - North/South indicator
            char northSouth = 'N';
            if (linePartList[i].size() >= 1)
            {
                northSouth = linePartList[i][0];
            }
            ++i;

            // 4 - Longitude
            // GP - dddmm.mmmmm
            // GN - yyyyy.yyyyyyy
            const string &lngStr = linePartList[i];
            ++i;

            // 5 - East/West indicator
            char eastWest = 'W';
            if (linePartList[i].size() >= 1)
            {
                eastWest = linePartList[i][0];
            }
            ++i;

            // 6 - Quality of position fix
            // GP - 0 = No Fix, 1 = Autonomous GNSS Fix, 2 = Differential GNSS Fix, 6 = Estimated
            // GN - 0 = No Fix, 1 = GNSS Fix, 2 = Differential GNSS Fix, 3 = PPS Fix, 4 = Real Time Kinematic, 5 = Float RTK, 6 = Estimated, 7 = Manual input mode, 8 = Simulation mode
            //   (values above 2 are 2.3 features)
            char quality = '0';
            if (linePartList[i].size() >= 1)
            {
                quality = linePartList[i][0];
            }
            ++i;

            // 7 - Number of satellites used (range 0-12)
            // GP - numeric (eg 08)
            // GN - (range 00-40)
            // const string &numSatStr = linePartList[i];
            ++i;

            // 8 - HDOP Horizontal Dilution of Precision
            ++i;

            // 9 - Altitude above mean sea level (meters)
            // GN - numeric (eg 499.6)
            // GP - x.x
            const string &altitudeStr = linePartList[i];
            ++i;

            // 10 - Altitude units (meters - fixed constant field)
            ++i;

            // 11 - Geoid separation: diff between geoid and mean sea level (meters)
            ++i;

            // 12 - Separation units (meters - fixed constant field)
            ++i;

            // 13 - Age of differential corrections (blank when DGPS is not used) (seconds)
            ++i;

            // 14 - ID of station providing differential corrections (blank when DGPS is not used)
            ++i;

            // Note the current time
            uint64_t timeNowMs = PAL.Millis();

            bool timeStateIsValid = TimeStateIsValid(time);

            // time is good when non-blank
            // date may not be set yet
            if (timeStateIsValid)
            {
                data_.timeStr = move(time);

                data_.timeAtTimeLock = timeNowMs;

                // capture source of lock
                data_.fixTimeSource = line;

                // notify change in data
                OnFixTime();
            }

            // check if 2D location is good
            if (quality > '0' && quality < '6' && timeStateIsValid)
            {
                // add to the 2D/3D
                data_.latStr = move(latStr);
                data_.latNorthSouth = northSouth;
                data_.lngStr = lngStr;
                data_.lngEastWest = move(eastWest);
                data_.timeAtFix2d = timeNowMs;

                // add to the 3D
                data_.altitudeStr = move(altitudeStr);
                data_.timeAtFix3d = timeNowMs;

                // capture source of lock
                data_.fix2dSource = line;
                if (data_.fix3dSourceList.size() == 2)
                {
                    data_.fix3dSourceList.erase(data_.fix3dSourceList.begin());
                }
                data_.fix3dSourceList.push_back(line);
                if (data_.fix3dPlusSourceList.size() == 2)
                {
                    data_.fix3dPlusSourceList.erase(data_.fix3dPlusSourceList.begin());
                }
                data_.fix3dPlusSourceList.push_back(line);

                // notify on data change
                OnFix2D();
                OnFix3DMaybe();
                OnFix3DPlusMaybe();
            }
        }
        else
        {
            retVal = false;
            Log("ERR: Bad GGA");
        }

        return retVal;
    }

    // from neo-6
    // $GPGSV,4,1,14,02,01,041,,03,17,233,31,04,60,293,25,07,05,288,*77
    // $GPGSV,4,2,14,08,12,185,27,09,28,313,19,16,83,262,28,18,00,079,*75
    // $GPGSV,4,3,14,22,13,148,17,26,54,050,10,27,36,161,38,28,17,098,14*76
    // $GPGSV,4,4,14,29,03,029,,31,28,092,13*7E
    //
    // from atgm336h-5n31
    // $BDGSV,1,1,03,11,52,179,30,34,68,148,29,43,28,200,29*5B
    bool OnGSV(const vector<string> &linePartList)
    {
        bool retVal = true;

        // Function-local cache of data before exposing
        struct SatelliteDataParseCache
        {
            uint8_t seqNoNextExpected = 1;
            uint8_t endSeqNo = 0;
            vector<FixSatelliteData> satDataList;
        };

        static SatelliteDataParseCache satCache_;

        auto ResetCache = []{
            satCache_ = SatelliteDataParseCache{};
        };

        // Processing message
        if (linePartList.size() >= 4)
        {
            int i = 1;

            // examine "header" of this message type

            // 0 - Number of GSV messages to expect in this batch (repeated each msg in batch)
            uint8_t endSeqNo = stoi(linePartList[i]);
            // Log("endSeqNo: ", endSeqNo);
            ++i;
            
            // 1 - The sequnce number of this message (eg 3 of 4) (diff each batch)
            uint8_t seqNo = stoi(linePartList[i]);
            // Log("seqNo: ", seqNo);
            ++i;

            // 2 - Num satellites to be told about during whole batch (repeated each msg in batch)
            // ignore
            ++i;

            // if first line of a batch, we know we're resetting cache and accumulating new
            if (seqNo == 1)
            {
                // Log("Resetting cache");
                // reset cache
                ResetCache();

                // we only set this once
                satCache_.endSeqNo = endSeqNo;
            }

            // Batch processing logic:
            // - cache until whole batch received
            // - don't use incomplete batch
            // - you know this batch is busted when
            //   - seqNo != seqNoNextExpected
            //   - seqNo > endSeqNo
            // - you know this batch is done when seqNo = endSeqNo
            bool shouldProcess =
                (seqNo == satCache_.seqNoNextExpected) &&
                (seqNo <= satCache_.endSeqNo);

            if (shouldProcess)
            {
                ++satCache_.seqNoNextExpected;

                bool ok = true;

                // how many cells left?
                int cellsRemaining = linePartList.size() - 4;
                // Log("cellsRemaining: ", cellsRemaining);

                // how many groupings?
                int groupCount = cellsRemaining / 4;
                // Log("groupCount: ", groupCount);

                // if any mismatch in count, bad data, kill batch
                bool mismatch = cellsRemaining % 4;
                // Log("mismatch: ", mismatch);

                if (!mismatch)
                {
                    // process groups
                    for (int j = 0; j < groupCount; ++j)
                    {
                        // Repeating sequence of 4 fields, one set for each satellite
                        FixSatelliteData satData;

                        // 4 + (4N) - Satellite ID (can be in 16-bit range eg 901)
                        const string &strId = linePartList[i];
                        satData.id = atoi(strId.c_str());
                        // Log("id: ", satData.id);
                        ++i;

                        // 4 + (5N) - Elevation (degrees, range 0-90)
                        const string &strElevation = linePartList[i];
                        satData.elevation = atoi(strElevation.c_str());
                        if (satData.elevation > 90)
                        {
                            // Log("Bad Elevation");
                            ok = false;
                        }
                        // Log("elevation: ", satData.elevation);
                        ++i;

                        // 4 + (6N) - Azimuth (degrees, range 0-359)
                        const string &strAzimuth = linePartList[i];
                        satData.azimuth = atoi(strAzimuth.c_str());
                        if (satData.azimuth > 360)
                        {
                            // Log("Bad Azimuth");
                            ok = false;
                        }
                        // Log("azimuth: ", satData.azimuth);
                        ++i;

                        // 4 + (7N) - Signal Strength (dBH; C/N0, range 0-99), blank when not tracking
                        // ignore
                        ++i;

                        // commit to cache
                        if (ok)
                        {
                            satCache_.satDataList.push_back(satData);
                        }
                        else
                        {
                            retVal = false;

                            Log("ERR: Malformed GSV (", strId, ", ", strElevation, ", ", strAzimuth, ")");
                        }
                    }

                    // only need to check if done, otherwise we've done our job
                    // of accumulating
                    if (seqNo == satCache_.endSeqNo)
                    {
                        // Log("Committing cache");
                        // commit batch
                        data_.satDataList = satCache_.satDataList;

                        // Log("Resetting cache");
                        // reset cache
                        ResetCache();

                        // Declare data updated
                        OnSatDataList();
                    }
                }
                else
                {
                    retVal = false;

                    Log("ERR: Mismatch in remaining GSV");

                    ResetCache();
                }
            }
            else
            {
                retVal = false;

                Log("ERR: Discarding message from incomplete or malformed batch");

                ResetCache();
            }
        }
        else
        {
            retVal = false;

            Log("ERR: Bad GSV");
        }

        return retVal;
    }


    /////////////////////////////////////////////////////////////////
    // Misc
    /////////////////////////////////////////////////////////////////

    void DumpLinePartList(const vector<string> &linePartList)
    {
        for (int i = 0; i < (int)linePartList.size(); ++i)
        {
            Log(i, ": ", linePartList[i]);
        }
    }


    /////////////////////////////////////////////////////////////////
    // Shell
    /////////////////////////////////////////////////////////////////

    static void SetupShell()
    {
        Shell::AddCommand("gpsr.todms", [](vector<string> argList){

        }, { .argCount = 1, .help = "gps reader parse"});


    }

    /////////////////////////////////////////////////////////////////
    // Test
    /////////////////////////////////////////////////////////////////

public:

    // $GNGGA,233604.000,4044.55554,N,07401.96536,W,6,04,2.9,-18.1,M,0.0,M,,*70
    // $GNGGA,233606.000,4044.55575,N,07401.96533,W,1,04,2.9,-19.1,M,0.0,M,,*72
    //
    // Used:
    // - https://rl.se/gprmc to convert to decimal degrees
    // - https://maps.google.com to plug that value in and get out deg/min/sec
    static void Test()
    {
        // Test parsing lat

        struct TestResult
        {
            string str;
            char dir;
            DegMinSec dms;
        };

        auto ErrLogger = [](string str, char dir, DegMinSec got, DegMinSec want){
            if (got.deg != want.deg || got.min != want.min || got.sec != want.sec || got.degMillionths != want.degMillionths)
            {
                Log("ERR: ", str, ",", dir, ":");
                if (got.deg != want.deg) { Log ("  deg ", got.deg, " ?= ", want.deg); }
                if (got.min != want.min) { Log ("  min ", got.min, " ?= ", want.min); }
                if (got.sec != want.sec) { Log ("  sec ", got.sec, " ?= ", want.sec); }
                if (got.degMillionths != want.degMillionths) { Log ("  dml ", got.degMillionths, " ?= ", want.degMillionths); }

                LogNL();
            }
        };

        vector<TestResult> latTestResultList = {
            { "4044.55554", 'N', { .deg =  40, .min = 44, .sec = 33, .degMillionths =  40742592, }, },
            { "4044.55554", 'S', { .deg = -40, .min = 44, .sec = 33, .degMillionths = -40742592, }, },
        };

        for (auto &tr : latTestResultList)
        {
            DegMinSec dms = ToDegMinSec(tr.str, tr.dir);

            ErrLogger(tr.str, tr.dir, dms, tr.dms);
        }

        vector<TestResult> lngTestResultList = {
            { "07401.96536", 'W', { .deg = -74, .min = 1, .sec = 58, .degMillionths = -74032756, }, },
            { "07401.96536", 'E', { .deg =  74, .min = 1, .sec = 58, .degMillionths =  74032756, }, },
        };

        for (auto &tr : lngTestResultList)
        {
            DegMinSec dms = ToDegMinSec(tr.str, tr.dir);

            ErrLogger(tr.str, tr.dir, dms, tr.dms);
        }
    }


};




































class GPSWriter
{
    // Which UART to use to talk to the GPS hardware
    UART uart_ = UART::UART_1;
    
    // UBX message monitoring state
    UbxMessageParser pUbx_;
    uint8_t idUbx_ = 0;

    // UBX message monitoring state
    uint8_t idNmea_ = 0;

    // Stats
    struct Stats
    {
        uint32_t countUbxMessagesSent = 0;
        uint32_t countUbxMessagesSeen = 0;
        uint32_t countNmeaMessagesSeen = 0;
    };

    Stats stats_;

    bool verboseLogging_ = true;


public:

    void StartMonitorForReplies()
    {
        StartMonitorForNmea();
        StartMonitorForUbx();
    }

    void StopMonitorForReplies()
    {
        StopMonitorForNmea();
        StopMonitorForUbx();
    }

    void Reset()
    {
        pUbx_.Reset();
    }

    void DisableVerboseLogging()
    {
        verboseLogging_ = false;
    }

private:

    /////////////////////////////////////////////////////////////////
    // NMEA Messages -- receive from module
    /////////////////////////////////////////////////////////////////

    void StopMonitorForNmea()
    {
        if (idNmea_ != 0)
        {
            Log("Writer topping monitoring for NMEA");
            UartRemoveLineStreamCallback(uart_, idNmea_);
            idNmea_ = 0;
        }
    }

    void StartMonitorForNmea()
    {
        StopMonitorForNmea();

        Log("Writer tarting monitoring for NMEA");
        // auto [ok, id] = UartAddLineStreamCallback(uart_, [this](const string &line){
        //     UartTarget target(UART::UART_0);

        //     ++stats_.countNmeaMessagesSeen;

        //     // For now just replicate to UART0
        //     Log(line);
        // });

        // idNmea_ = ok ? id : 0;
    }


public:

    /////////////////////////////////////////////////////////////////
    // CASIC Messages -- send to module
    /////////////////////////////////////////////////////////////////

    void SendCASIC(const string &line)
    {
        Log("Sending: \"", line, "\"");
        Log("Valid  : ", NMEAStringParser::IsValid(line));

        UartPush(UART::UART_1);
        LogNNL(line, "\r\n");
        UartPop();
    }

    void SendModuleResetHotCasic()
    {
        string line = NMEAStringMaker::Make({"PCAS10", "0"});

        if (verboseLogging_)
        {
            LogNL();
            Log("SendModuleResetHotCasic");
        }

        SendCASIC(line);
        LogNL();
    }

    void SendModuleResetWarmCasic()
    {
        string line = NMEAStringMaker::Make({"PCAS10", "1"});

        if (verboseLogging_)
        {
            LogNL();
            Log("SendModuleResetWarmCasic");
        }

        SendCASIC(line);
        LogNL();
    }

    void SendModuleResetColdCasic()
    {
        string line = NMEAStringMaker::Make({"PCAS10", "2"});

        if (verboseLogging_)
        {
            LogNL();
            Log("SendModuleResetColdCasic");
        }

        SendCASIC(line);
        LogNL();
    }


    /////////////////////////////////////////////////////////////////
    // UBX Messages -- send to module
    /////////////////////////////////////////////////////////////////

    void SendHighAltitudeMode()
    {
        UbxMsgCfgNav5GetSet msg;
        msg.SetDynModel(6);  // Airborne < 1g Accel
        msg.Finish();

        if (verboseLogging_)
        {
            LogNL();
            Log("SendHighAltitudeMode");
            msg.Dump();
            LogBlob(msg.GetData());
        }

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModuleResetHot()
    {
        LogNL();
        Log("SendModuleResetHot");
        UbxMsgCfgRst msg;
        msg.SetResetTypeHotImmediate();
        msg.Finish();
        msg.Dump();
        LogBlob(msg.GetData());

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    // ATGM336H NAKs this
    void SendModuleResetWarm()
    {
        LogNL();
        Log("SendModuleResetWarm");
        UbxMsgCfgRst msg;
        msg.SetResetTypeWarmImmediate();
        msg.Finish();
        msg.Dump();
        LogBlob(msg.GetData());

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModuleResetCold()
    {
        UbxMsgCfgRst msg;
        msg.SetResetTypeColdImmediate();
        msg.Finish();

        if (verboseLogging_)
        {
            LogNL();
            Log("SendModuleResetCold");
            msg.Dump();
            LogBlob(msg.GetData());
        }

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModuleLegacyPulsePoll()
    {
        LogNL();
        Log("SendModuleLegacyPulsePoll");
        UbxMsgCfgTpPoll msg;
        msg.Finish();
        msg.Dump();
        LogBlob(msg.GetData());

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModulePulsePoll()
    {
        LogNL();
        Log("SendModulePulsePoll");
        UbxMsgCfgTp5Poll msg;
        msg.Finish();
        msg.Dump();
        LogBlob(msg.GetData());

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModulePulseAlways()
    {
        LogNL();
        Log("SendModulePulseAlways");
        UbxMsgCfgTp5 msg;
        msg.SetTimepulseAlways();
        msg.Finish();
        msg.Dump();
        LogBlob(msg.GetData());

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModuleFactoryResetConfiguration()
    {
        UbxMsgCfgCfg msg;
        msg.SetFactoryReset();
        msg.Finish();

        if (verboseLogging_)
        {
            LogNL();
            Log("SendModuleFactoryResetConfiguration");
            msg.Dump();
            LogBlob(msg.GetData());
        }

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    void SendModuleSaveConfiguration()
    {
        UbxMsgCfgCfg msg;
        msg.SetSaveConfigurationToBatteryBackedRAM();
        msg.Finish();

        if (verboseLogging_)
        {
            LogNL();
            Log("SendModuleSaveConfiguration");
            msg.Dump();
            LogBlob(msg.GetData());
        }

        UartTarget target(uart_);
        UartSend(msg.GetData());

        // Sleep for as many ms as bytes to make sure they're all sent
        PAL.Delay(msg.GetData().size());

        ++stats_.countUbxMessagesSent;
        LogNL();
    }

    struct MsgCfg
    {
        uint8_t msgClass = 0;
        uint8_t msgId    = 0;
        uint8_t rateSecs = 0;
        string  note;
    };

    void SendModuleMessageRateConfigurationMinimal()
    {
        vector<MsgCfg> msgCfgList = {
            { 0xF0, 0x02, 0, "GSA (satellite id list)"            },
            { 0xF0, 0x00, 1, "GGA (time, lat/lng, altitude)"      },
            { 0xF0, 0x01, 0, "GLL (time, lat/lng)"                },
            { 0xF0, 0x03, 0, "GSV (satellite locations)"          },
            { 0xF0, 0x04, 1, "RMC (time, lat/lng, speed, course)" },
            { 0xF0, 0x05, 0, "VTG (speed, course)"                },
            { 0xF0, 0x08, 0, "ZDA (time, timezone)"               },
            { 0xF0, 0x41, 0, "TXT (text transmission)"            },
        };

        SendModuleMessageRateConfiguration(msgCfgList);
    }

    void SendModuleMessageRateConfigurationMaximal()
    {
        vector<MsgCfg> msgCfgList = {
            { 0xF0, 0x02, 1, "GSA (satellite id list)"            },
            { 0xF0, 0x00, 1, "GGA (time, lat/lng, altitude)"      },
            { 0xF0, 0x01, 1, "GLL (time, lat/lng)"                },
            { 0xF0, 0x03, 1, "GSV (satellite locations)"          },
            { 0xF0, 0x04, 1, "RMC (time, lat/lng, speed, course)" },
            { 0xF0, 0x05, 1, "VTG (speed, course)"                },
            { 0xF0, 0x08, 1, "ZDA (time, timezone)"               },
            { 0xF0, 0x41, 1, "TXT (text transmission)"            },
        };

        SendModuleMessageRateConfiguration(msgCfgList);
    }

    void SendModuleMessageRateConfiguration(const vector<MsgCfg> &msgCfgList)
    {
        if (verboseLogging_)
        {
            Log("SendModuleMessageRateConfiguration");
        }

        for (auto &msgCfg : msgCfgList)
        {
            UbxMsgCfgMsg msg;
            msg.SetClassIdRate(msgCfg.msgClass, msgCfg.msgId, msgCfg.rateSecs);
            msg.Finish();

            if (verboseLogging_)
            {
                LogNL();
                Log("Setting configuration for ", msgCfg.note);
                msg.Dump();
                LogBlob(msg.GetData());
            }

            UartTarget target(uart_);
            UartSend(msg.GetData());
            
            // Sleep for as many ms as bytes to make sure they're all sent
            PAL.Delay(msg.GetData().size());

            ++stats_.countUbxMessagesSent;
        }

        LogNL();
    }

private:
    /////////////////////////////////////////////////////////////////
    // UBX Messages -- receive from module
    /////////////////////////////////////////////////////////////////

    void StopMonitorForUbx()
    {
        if (idUbx_ != 0)
        {
            Log("Stopping monitoring for UBX");
            UartRemoveDataStreamCallback(UART::UART_1, idUbx_);
            idUbx_ = 0;
        }
    }

    void StartMonitorForUbx()
    {
        StopMonitorForUbx();
        Log("Starting monitoring for UBX");
        
        // Set up handling to watch for UBX messages
        pUbx_.Reset();
        auto [ok, id] = UartAddDataStreamCallback(uart_, [this](const vector<uint8_t> &byteList){
            UartTarget target(UART::UART_0);

            for (auto b : byteList)
            {
                pUbx_.AddByte(b);

                if (pUbx_.MessageFound())
                {
                    const vector<uint8_t> &dat = pUbx_.GetData();

                    ++stats_.countUbxMessagesSeen;

                    if (verboseLogging_)
                    {
                        LogNL();
                        Log("UBX Msg found -- ", ToHex(pUbx_.GetClassId()));

                        if (UbxMsgAckAck::MessageValid(pUbx_))
                        {
                            UbxMsgAckAck msg(pUbx_);
                            Log("ACK from ", ToHex(msg.GetAckClassId()));
                        }
                        else if (UbxMsgAckNak::MessageValid(pUbx_))
                        {
                            UbxMsgAckNak msg(pUbx_);
                            Log("NAK from ", ToHex(msg.GetNakClassId()));
                        }
                        else
                        {
                            Log("Unknown message: ", ToHex(pUbx_.GetClassId()));
                            LogBlob(dat.data(), dat.size());
                        }
                        LogNL();
                    }

                    pUbx_.Reset();
                }
                else if (pUbx_.ErrorEncountered())
                {
                    Log("ERR occurred during parsing, resetting");
                    LogNL();

                    pUbx_.Reset();
                }
            }
        });

        idUbx_ = ok ? id : 0;
    }


public:

    /////////////////////////////////////////////////////////////////
    // Misc
    /////////////////////////////////////////////////////////////////

    void DumpState()
    {
        LogNL();

        // Stats
        Log("Stats:");
        Log("- UBX Messages Sent : ", stats_.countUbxMessagesSent);
        Log("- UBX Messages Seen : ", stats_.countUbxMessagesSeen);
        Log("- NMEA Messages Seen: ", stats_.countNmeaMessagesSeen);

        // Monitoring status
        LogNL();
        Log("Monitoring:");
        Log("- UBX Messages : ", idUbx_ != 0);
        Log("- NMEA Messages: ", idNmea_ != 0);

        // Lock status?

        LogNL();
    }


    /////////////////////////////////////////////////////////////////
    // Shell
    /////////////////////////////////////////////////////////////////

    void SetupShell(string prefix)
    {
        Shell::AddCommand(prefix + ".mon.nmea", [this](vector<string> argList){
            if (argList[0] == "on") { StartMonitorForNmea(); }
            else                    { StopMonitorForNmea();  }
        }, { .argCount = 1, .help = "GPS monitor for nmea <on/off>"});

        Shell::AddCommand(prefix + ".mon.ubx", [this](vector<string> argList){
            if (argList[0] == "on") { StartMonitorForUbx(); }
            else                    { StopMonitorForUbx();  }
        }, { .argCount = 1, .help = "GPS monitor for ubx <on/off>"});

        Shell::AddCommand(prefix + ".stats", [this](vector<string> argList){
            DumpState();
        }, { .argCount = 0, .help = "GPS dump stats"});

        Shell::AddCommand(prefix + ".send.high", [this](vector<string> argList){
            SendHighAltitudeMode();
        }, { .argCount = 0, .help = "GPS send high altitude mode"});

        Shell::AddCommand(prefix + ".send.reset", [this](vector<string> argList){
            string temp = argList[0];

            if      (temp == "hot")  { SendModuleResetHot();  }
            else if (temp == "warm") { SendModuleResetWarm(); }
            else                     { SendModuleResetCold(); }
        }, { .argCount = 1, .help = "GPS send module reset <hot/warm/cold>"});

        Shell::AddCommand(prefix + ".send.legacypulsepoll", [this](vector<string> argList){
            SendModuleLegacyPulsePoll();
        }, { .argCount = 0, .help = "GPS send pulse poll"});

        Shell::AddCommand(prefix + ".send.pulsepoll", [this](vector<string> argList){
            SendModulePulsePoll();
        }, { .argCount = 0, .help = "GPS send pulse poll"});

        Shell::AddCommand(prefix + ".send.pulse", [this](vector<string> argList){
            SendModulePulseAlways();
        }, { .argCount = 0, .help = "GPS send pulse configuration"});

        Shell::AddCommand(prefix + ".send.factory", [this](vector<string> argList){
            SendModuleFactoryResetConfiguration();
        }, { .argCount = 0, .help = "GPS send factory reset"});

        Shell::AddCommand(prefix + ".send.save", [this](vector<string> argList){
            SendModuleSaveConfiguration();
        }, { .argCount = 0, .help = "GPS send save configuration"});

        Shell::AddCommand(prefix + ".send.rate.max", [this](vector<string> argList){
            SendModuleMessageRateConfigurationMaximal();
        }, { .argCount = 0, .help = "GPS send message rate configuration"});

        Shell::AddCommand(prefix + ".send.rate.min", [this](vector<string> argList){
            SendModuleMessageRateConfigurationMinimal();
        }, { .argCount = 0, .help = "GPS send message rate configuration"});


        Shell::AddCommand(prefix + ".send.cas00", [this](vector<string> argList){
            string line = NMEAStringMaker::Make({
                "PCAS00",
            });
            SendCASIC(line);
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".send.cas02", [this](vector<string> argList){
            uint32_t val = atoi(argList[0].c_str());

            string line = NMEAStringMaker::Make({
                "PCAS02",
                to_string(val),
            });

            SendCASIC(line);
        }, { .argCount = 1, .help = ""});

        Shell::AddCommand(prefix + ".send.cas03", [this](vector<string> argList){
            string line = NMEAStringMaker::Make({
                "PCAS03",
                to_string(2),  // GGA
                to_string(0),  // GLL
                to_string(0),  // GSA
                to_string(0),  // GSA
                to_string(2),  // GSV
                to_string(2),  // RMC
                to_string(0),  // VTG
                to_string(0),  // ZDA
                to_string(0),  // Reserved
            });

            SendCASIC(line);
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".send.cas04", [this](vector<string> argList){
            uint32_t val = atoi(argList[0].c_str());

            string line = NMEAStringMaker::Make({
                "PCAS04",
                to_string(val),
            });

            SendCASIC(line);
        }, { .argCount = 1, .help = ""});

        Shell::AddCommand(prefix + ".send.cas06", [this](vector<string> argList){
            string line = NMEAStringMaker::Make({
                "PCAS06",
                to_string(1),
            });

            SendCASIC(line);
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".send.cas10", [this](vector<string> argList){
            uint32_t val = atoi(argList[0].c_str());

            string line = NMEAStringMaker::Make({
                "PCAS10",
                to_string(val),
            });

            SendCASIC(line);
        }, { .argCount = 1, .help = ""});

        Shell::AddCommand(prefix + ".send.cas11", [this](vector<string> argList){
            uint32_t val = atoi(argList[0].c_str());

            string line = NMEAStringMaker::Make({
                "PCAS11",
                to_string(val),
            });

            SendCASIC(line);
        }, { .argCount = 1, .help = ""});
    }
};


