

#include "NMEA.h"
#include "SensorGPSUblox.h"
#include "SensorGPS.h"


        // auto fn = [](string line){
        //     static Timeline t;

        //     t.Event("start");
        //     Log("String: \"", line, "\"");
        //     Log("IsValid: ", NmeaStringParser::IsValid(line));
        //     Log("GetDataLength: ", NmeaStringParser::GetDataLength(line));
        //     Log("LineData: \"", NmeaStringParser::GetLineData(line), "\"");
        //     Log("CalcdChecksum: \"", NmeaStringParser::GetCalcdChecksum(line), "\"");
        //     Log("NmeaChecksum: \"", NmeaStringParser::GetNmeaStringChecksum(line), "\"");
        //     LogNNL("LinePartList: ");
        //     string sep = "";
        //     for (auto &linePart : NmeaStringParser::GetLinePartList(line))
        //     {
        //         LogNNL(sep, linePart);
        //         sep = ", ";
        //     }
        //     LogNL();
        //     t.Event("stop");
        //     t.Report();

        //     LogNL();
        // };

        // fn("$GPGGA,004721.00,4044.56719,N,07401.97214,W,1,09,1.03,14.5,M,-34.4,M,,*58");




vector<string> testStrList = {
    // Just started
    "$GPGGA,,,,,,0,00,25.5,,,,,,*7A",
    "$GPGLL,,,,,,V,N*64",
    "$GPGSA,A,1,,,,,,,,,,,,,25.5,25.5,25.5*02",
    "$GPGSV,1,1,00*79",
    "$GPRMC,,V,,,,,,,,,,N*53",
    "$GPVTG,,,,,,,,,N*30",
    "$GPZDA,,,,,,*48",
    "$GPTXT,01,01,01,ANTENNA OPEN*25",

    // Time available
    "$GPGGA,005544.183,,,,,0,00,25.5,,,,,,*6E",
    "$GPGLL,,,,,005544.183,V,N*70",
    "$GPGSA,A,1,,,,,,,,,,,,,25.5,25.5,25.5*02",
    "$GPGSV,1,1,03,02,,,34,03,,,34,05,,,37*7A",
    "$GPRMC,005544.183,V,,,,,,,,,,N*47",
    "$GPVTG,,,,,,,,,N*30",
    "$GPZDA,005544.183,,,,,*5C",
    "$GPTXT,01,01,01,ANTENNA OPEN*25",

    // locking
    "$GPGGA,005630.187,4044.2207,N,07402.0695,W,6,04,3.7,-1.9,M,0.0,M,,*5A",
    "$GPGLL,4044.2207,N,07402.0695,W,005630.187,A,E*49",
    "$GPGSA,A,3,05,02,03,12,,,,,,,,,7.0,3.7,6.0*30",
    "$GPGSV,2,1,05,02,47,272,30,03,06,038,26,05,12,205,37,12,36,307,31*75",
    "$GPGSV,2,2,05,17,,,30*79",
    "$GPRMC,005630.187,A,4044.2207,N,07402.0695,W,0.30,0.00,130419,,,E*73",
    "$GPVTG,0.00,T,,M,0.30,N,0.55,K,E*3A",
    "$GPZDA,005630.187,13,04,2019,00,00*54",
    "$GPTXT,01,01,01,ANTENNA OPEN*25",

    // locked
    "$GPGGA,005648.000,4044.2188,N,07402.0574,W,1,04,3.7,-2.3,M,0.0,M,,*5D",
    "$GPGLL,4044.2188,N,07402.0574,W,005648.000,A,A*44",
    "$GPGSA,A,3,05,02,03,12,,,,,,,,,7.1,3.7,6.0*31",
    "$GPGSV,2,1,07,02,47,272,32,03,06,038,24,05,12,205,34,06,,,30*41",
    "$GPGSV,2,2,07,12,36,307,21,17,,,35,19,68,080,25*76",
    "$GPRMC,005648.000,A,4044.2188,N,07402.0574,W,0.00,0.00,130419,,,A*7D",
    "$GPVTG,0.00,T,,M,0.00,N,0.00,K,A*3D",
    "$GPZDA,005648.000,13,04,2019,00,00*55",
    "$GPTXT,01,01,01,ANTENNA OPEN*25",
};

// testStrList = {
//     "$GPRMC,005630.187,A,4044.2207,N,07402.0695,W,0.30,0.00,130419,,,E*73",
// };


set<string> knownOkResetModeSet = {
    "$GPGGA,005630.187,4044.2207,N,07402.0695,W,6,04,3.7,-1.9,M,0.0,M,,*5A",
    "$GPRMC,005630.187,A,4044.2207,N,07402.0695,W,0.30,0.00,130419,,,E*73",
    "$GPGGA,005648.000,4044.2188,N,07402.0574,W,1,04,3.7,-2.3,M,0.0,M,,*5D",
    "$GPRMC,005648.000,A,4044.2188,N,07402.0574,W,0.00,0.00,130419,,,A*7D",
};

set<string> knownOkCumulativeModeSet = {
    "$GPGGA,005630.187,4044.2207,N,07402.0695,W,6,04,3.7,-1.9,M,0.0,M,,*5A",
};




        static SensorGPSUblox gpsOld;
        gpsOld.Init();

        static SensorGPS gpsNew;
        gpsNew.Init();

        


        auto TestLine = [&](const string &line, bool resetAfterEach, set<string> exceptionSet)
        {
            Log(line);
            // LogNL();

            // Old -- pre-line
            SensorGPSUblox::Measurement mTimeOld;
            uint8_t retTimeOld = 0;
            SensorGPSUblox::MeasurementLocationDMS mdmsOld;
            uint8_t retDmsOld = 0;
            SensorGPSUblox::Measurement mLocOld;
            uint8_t retLocOld = 0;
            SensorGPSUblox::Measurement mOld;
            // memset(&mOld, 0, sizeof(mOld));

            retTimeOld = gpsOld.GetTimeMeasurement(&mTimeOld);
            retDmsOld = gpsOld.GetLocationDMSMeasurement(&mdmsOld);
            retLocOld = gpsOld.GetLocationMeasurement(&mLocOld);
            gpsOld.GetLocationMeasurement(&mOld);
            // Log("T_OK(", retTimeOld, "), DMS_OK(", retDmsOld, "), LOC_OK(", retLocOld, ")");

            // New -- pre-line
            SensorGPS::Measurement mTimeNew;
            uint8_t retTimeNew = 0;
            SensorGPS::MeasurementLocationDMS mdmsNew;
            uint8_t retDmsNew = 0;
            SensorGPS::Measurement mLocNew;
            uint8_t retLocNew = 0;
            SensorGPS::Measurement mNew;
            // memset(&mNew, 0, sizeof(mNew));

            retTimeNew = gpsNew.GetTimeMeasurement(&mTimeNew);
            retDmsNew = gpsNew.GetLocationDMSMeasurement(&mdmsNew);
            retLocNew = gpsNew.GetLocationMeasurement(&mLocNew);
            gpsNew.GetLocationMeasurement(&mNew);
            // Log("T_OK(", retTimeNew, "), DMS_OK(", retDmsNew, "), LOC_OK(", retLocNew, ")");


            // Diff data
            bool diff = false;
            static const uint8_t MAIDENHEAD_GRID_LEN = 6;


            bool diffBefore = false;
            if (retTimeOld != retTimeNew || retDmsOld != retDmsNew || retLocOld != retLocNew)
            {
                diffBefore = true;
            }




            gpsOld.OnLine(line);
            gpsNew.OnLine(line);



            // zero out
            // memset(&mOld, 0, sizeof(mOld));
            // memset(&mNew, 0, sizeof(mNew));

            
            // Old -- post-line
            retTimeOld = gpsOld.GetTimeMeasurement(&mTimeOld);
            retDmsOld = gpsOld.GetLocationDMSMeasurement(&mdmsOld);
            retLocOld = gpsOld.GetLocationMeasurement(&mLocOld);
            gpsOld.GetTimeMeasurement(&mOld);
            // Log("T_OK(", retTimeOld, "), DMS_OK(", retDmsOld, "), LOC_OK(", retLocOld, ")");


            // New -- post-line
            retTimeNew = gpsNew.GetTimeMeasurement(&mTimeNew);
            retDmsNew = gpsNew.GetLocationDMSMeasurement(&mdmsNew);
            retLocNew = gpsNew.GetLocationMeasurement(&mLocNew);
            gpsNew.GetTimeMeasurement(&mNew);
            // Log("T_OK(", retTimeNew, "), DMS_OK(", retDmsNew, "), LOC_OK(", retLocNew, ")");
            // LogNL();



            diff = false;

            
            auto fdiff = []<typename T>(T val1, T val2){
                return fabs(fabs(val1) - fabs(val2)) > 0.01;
            };


            if (mOld.year != mNew.year) { Log("year: old(", mOld.year, ") != new(", mNew.year, ")"); }
            if (mOld.month != mNew.month) { Log("month: old(", mOld.month, ") != new(", mNew.month, ")"); }
            if (mOld.day != mNew.day) { Log("day: old(", mOld.day, ") != new(", mNew.day, ")"); }
            if (mOld.hour != mNew.hour) { Log("hour: old(", mOld.hour, ") != new(", mNew.hour, ")"); }
            if (mOld.minute != mNew.minute) { Log("minute: old(", mOld.minute, ") != new(", mNew.minute, ")"); }
            if (mOld.second != mNew.second) { Log("second: old(", mOld.second, ") != new(", mNew.second, ")"); }
            if (fdiff(mOld.millisecond / 1000, mNew.millisecond / 1000)) { Log("millisecond: old(", mOld.millisecond, ") != new(", mNew.millisecond, ")"); }
            if (mOld.courseDegrees != mNew.courseDegrees) { Log("courseDegrees: old(", mOld.courseDegrees, ") != new(", mNew.courseDegrees, ")"); }
            if (mOld.speedKnots != mNew.speedKnots) { Log("speedKnots: old(", mOld.speedKnots, ") != new(", mNew.speedKnots, ")"); }
            if (mOld.latitudeDegreesMillionths != mNew.latitudeDegreesMillionths) { Log("latitudeDegreesMillionths: old(", mOld.latitudeDegreesMillionths, ") != new(", mNew.latitudeDegreesMillionths, ")"); }
            if (mOld.longitudeDegreesMillionths != mNew.longitudeDegreesMillionths) { Log("longitudeDegreesMillionths: old(", mOld.longitudeDegreesMillionths, ") != new(", mNew.longitudeDegreesMillionths, ")"); }
            if (mOld.locationDms.latitudeDegrees != mNew.locationDms.latitudeDegrees) { Log("latitudeDegrees: old(", mOld.locationDms.latitudeDegrees, ") != new(", mNew.locationDms.latitudeDegrees, ")"); }
            if (mOld.locationDms.latitudeMinutes != mNew.locationDms.latitudeMinutes) { Log("latitudeMinutes: old(", mOld.locationDms.latitudeMinutes, ") != new(", mNew.locationDms.latitudeMinutes, ")"); }
            if (fdiff(mOld.locationDms.latitudeSeconds, mNew.locationDms.latitudeSeconds)) { Log("latitudeSeconds: old(", mOld.locationDms.latitudeSeconds, ") != new(", mNew.locationDms.latitudeSeconds, ")"); }
            if (mOld.locationDms.longitudeDegrees != mNew.locationDms.longitudeDegrees) { Log("longitudeDegrees: old(", mOld.locationDms.longitudeDegrees, ") != new(", mNew.locationDms.longitudeDegrees, ")"); }
            if (mOld.locationDms.longitudeMinutes != mNew.locationDms.longitudeMinutes) { Log("longitudeMinutes: old(", mOld.locationDms.longitudeMinutes, ") != new(", mNew.locationDms.longitudeMinutes, ")"); }
            if (fdiff(mOld.locationDms.longitudeSeconds, mNew.locationDms.longitudeSeconds)) { Log("longitudeSeconds: old(", mOld.locationDms.longitudeSeconds, ") != new(", mNew.locationDms.longitudeSeconds, ")"); }
            if (strncmp(mOld.maidenheadGrid, mNew.maidenheadGrid.c_str(), MAIDENHEAD_GRID_LEN)) { Log("maidenheadGrid: old(", mOld.maidenheadGrid, "), new(", mNew.maidenheadGrid, ")"); }
            if (mOld.altitudeFt != mNew.altitudeFt) { Log("altitudeFt: old(", mOld.altitudeFt, ") != new(", mNew.altitudeFt, ")"); }













            bool diffDataAfter = diff;





            bool diffAfter = false;
            if (retTimeOld != retTimeNew || retDmsOld != retDmsNew || retLocOld != retLocNew)
            {
                diffAfter = true;
            }

            if (diffDataAfter)
            {
                diffAfter = true;
            }




            if (resetAfterEach)
            {
                gpsOld.ResetFix();
                gpsNew.ResetFix();
            }




            if (exceptionSet.contains(line))
            {
                diffBefore = false;
                diffAfter = false;
            }



            if (diffBefore)
            {
                Log("Diff Before");
                Log("T_OK(", retTimeOld, "), DMS_OK(", retDmsOld, "), LOC_OK(", retLocOld, ")");
                Log("T_OK(", retTimeNew, "), DMS_OK(", retDmsNew, "), LOC_OK(", retLocNew, ")");
            }

            if (diffAfter)
            {
                Log("Diff After");
                Log("T_OK(", retTimeOld, "), DMS_OK(", retDmsOld, "), LOC_OK(", retLocOld, ")");
                Log("T_OK(", retTimeNew, "), DMS_OK(", retDmsNew, "), LOC_OK(", retLocNew, ")");
            }


            if (diffBefore || diffAfter)
            {
                Log(line);

                auto linePartList = NmeaStringParser::GetLineDataPartList(line);

                Log("Dumping ", linePartList[0]);
                for (int i = 0; i < (int)linePartList.size(); ++i)
                {
                    Log(i, ": ", linePartList[i]);
                }

                LogNL();

                LogNL();
                LogNL();
            }

            if (diffAfter && resetAfterEach == false)
            {
                // They differed, so they'll keep differing on lots of messages
                // that don't really change anything.
                // Reset to get back to a starting position.
                Log("Resetting");
                gpsOld.ResetFix();
                gpsNew.ResetFix();
            }



        };

        LogModeSync();
        Log("Testing reset each time");
        set<string> typeSet = {
            "GPGGA",
            "GPGSV",
            "GPRMC",
        };
        for (const auto &testStr : testStrList)
        {
            if (typeSet.contains(NmeaStringParser::GetMessageType(testStr)))
            {
                if (!knownOkResetModeSet.contains(testStr))
                {
                    TestLine(testStr, true, knownOkResetModeSet);
                }
                else
                {
                    gpsOld.ResetFix();
                    gpsNew.ResetFix();
                }
            }
        }
        LogNL();
        Log("Testing cumulative");
        for (const auto &testStr : testStrList)
        {
            if (typeSet.contains(NmeaStringParser::GetMessageType(testStr)))
            {
                if (!knownOkCumulativeModeSet.contains(testStr))
                {
                    TestLine(testStr, false, knownOkResetModeSet);
                }
                else
                {
                    gpsOld.ResetFix();
                    gpsNew.ResetFix();
                }
            }

        }


        LogModeAsync();
        Log("===================================");

        UartSetInputCallback(UART::UART_1, [&](const string &line){
            UartTarget target(UART::UART_0);

            if (NmeaStringParser::IsValid(line))
            {
                // Log(line);

                TestLine(line, true, {});
            }
        });




        // ResetFix

        static NmeaStringAccumulatedState nmea;

        auto fn = [&](const string &line){
            LogNL();
            Log(line);
            Log("T(", nmea.HasFixTime(), "), 2D(", nmea.HasFix2D(), "), 3D(", nmea.HasFix3D(), "), 3D+(", nmea.HasFix3DPlus(), "), S(", nmea.HasSatelliteData(), ")");
            uint32_t diff = nmea.AccumulateLine(line);
            Log(diff, " us");
            Log("T(", nmea.HasFixTime(), "), 2D(", nmea.HasFix2D(), "), 3D(", nmea.HasFix3D(), "), 3D+(", nmea.HasFix3DPlus(), "), S(", nmea.HasSatelliteData(), ")");
            if (nmea.HasSatelliteData())
            {
                auto satDataList = nmea.GetSatelliteDataList();
                Log("Sat Data (", satDataList.size(), ")");
                for (const auto &satData : nmea.GetSatelliteDataList())
                {
                    Log("id: ", satData.id, ", el: ", satData.elevation, ", az: ", satData.azimuth);
                }
            }
            LogNL();
        };

