#pragma once

#include "BleAdvertisingDataFormatter.h"
#include "Evm.h"
#include "KMessagePassing.h"
#include "Pin.h"
#include "Shell.h"
#include "Utl.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
using namespace std;

#include "btstack.h"


static void dump_advertisement_data(const uint8_t * adv_data, uint8_t adv_size);


class Ble;

class BleObserver
{
    friend class Ble;

public:

    enum class AdType : uint8_t
    {
        ADV_IND  = 0,
        SCAN_RSP = 4,
    };


    struct AdReport
    {
        friend class BleObserver;

        AdReport()
        {
            // make sure any holes in the structure are zero for hashing
            memset(this, 0, sizeof(AdReport));
            data = &dataBuf[0];
        }

        bd_addr_t  address;
        char       addrStr[6*3]; // 12:45:78:01:34:67\0
        uint8_t    addressType;
        uint8_t    eventType;
        int8_t     rssi;
        uint8_t    len;
        uint8_t   *data;

    private:
        uint8_t dataBuf[BleAdvertisingDataFormatter::ADV_BYTES];
    };

    struct Filter
    {
        struct Attr
        {
            enum class Type : uint8_t
            {
                AD_TYPE,
                MFR_DATA_SIZE,
                DEVICE_ADDR,
                UUID16,
                DEVICE_NAME,
            };

            // attribute type
            Type type;

            // attribute value, depending on type
            AdType  eventType;
            string  deviceAddr;
            UUID    uuid;
            uint8_t mfrDataSize;
            string  deviceName;
        };

        // list of attributes to filter against
        vector<Attr> attrList;
    };


public:

    /////////////////////////////////////////////////////////////////
    // Public Interface
    /////////////////////////////////////////////////////////////////

    static void Start(function<void(const AdReport &report)> cbFn, vector<Filter> filterList = {})
    {
        Stop();

        if (ready_ == false)
        {
            // can only get called after BLE available, which might not
            // have happened yet.  we want to support application not
            // knowing that detail.

            // We know that by the time Evm comes around, BLE is available
            ted_.SetCallback([=]{
                Start(cbFn, filterList);
            }, "BleObserver::Start Timer");
            ted_.RegisterForTimedEvent(0);
        }
        else
        {
            started_ = true;

            // capture state
            cbFn_ = cbFn;
            filterList_ = filterList;

            // Do some one-time adjustments to the filter
            PreprocessFilterList(filterList_);

            // Determine whether we can specify primary advertisements
            // only (as an optimization), or if we need scan response
            // also
            uint8_t scanType = GetRequiredScanType(filterList_);

            // scan_interval – range 0x0004..0x4000, unit 0.625 ms
            // scan_window – range 0x0004..0x4000, unit 0.625 ms
            // scanning_filter_policy – 0 = all devices, 1 = all from whitelist
            // uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window

            // Active scanning, 100% (scan interval = scan window)
            gap_set_scan_parameters(scanType, 48, 48);

            gap_start_scan();
        }
    }

    static void Stop()
    {
        ted_.DeRegisterForTimedEvent();

        if (!started_) return;
        started_ = false;

        gap_stop_scan();

        Evm::ClearLowPriorityWorkByLabel(LOW_PRIO_WORK_LABEL);
    }


private:

    /////////////////////////////////////////////////////////////////
    // Report Handling
    /////////////////////////////////////////////////////////////////

    static void OnAdvertisingReportEvm()
    {
        AdReport report;

        while (pipe_.Get(report, 0))
        {
            // re-point pointer after having been copied around
            report.data = &report.dataBuf[0];

            cbFn_(report);
        }
    }

    static void OnAdvertisingReport(uint8_t *packet)
    {
        uint8_t type = hci_event_packet_get_type(packet);

        if (type == GAP_EVENT_ADVERTISING_REPORT)
        {
            // Extract the report contents
            AdReport report;

            gap_event_advertising_report_get_address(packet, report.address);
            memcpy(report.addrStr, bd_addr_to_str(report.address), sizeof(report.addrStr));

            report.eventType    = gap_event_advertising_report_get_advertising_event_type(packet);
            report.addressType  = gap_event_advertising_report_get_address_type(packet);
            report.rssi         = gap_event_advertising_report_get_rssi(packet);
            report.len          = gap_event_advertising_report_get_data_length(packet);

            memcpy(report.data, gap_event_advertising_report_get_data(packet), report.len);

            // Do filtering
            bool match = FilterMatch(report, filterList_);

            if (match)
            {
                // pass upward if not already an event ascending
                auto count = pipe_.Count();

                if (pipe_.Put(report, 0))
                {
                    // send an event if there isn't one already pending
                    if (count == 0)
                    {
                        Evm::QueueLowPriorityWork(LOW_PRIO_WORK_LABEL, OnAdvertisingReportEvm);
                    }
                }
            }
        }
    }

private:

    /////////////////////////////////////////////////////////////////
    // Runtime Events
    /////////////////////////////////////////////////////////////////

    inline static bool ready_ = false;

    static void OnReady()
    {
        ready_ = true;
    }


    /////////////////////////////////////////////////////////////////
    // Filter Related
    /////////////////////////////////////////////////////////////////

    static uint8_t GetRequiredScanType(const vector<Filter> &filterList)
    {
        // scan_type – 0 = passive, 1 = active
        uint8_t retVal = 0;

        // check the filter to see if we can avoid doing active scanning
        // (ie avoid asking for a scan response).  If any specify scan response
        // or any don't specify at all, must do active scan.
        size_t countSpecified = 0;
        for (const auto &filter : filterList_)
        {
            bool didSpecifyType = false;

            for (const auto &attr : filter.attrList)
            {
                if (attr.type == Filter::Attr::Type::AD_TYPE)
                {
                    didSpecifyType = true;

                    if (attr.eventType == AdType::SCAN_RSP)
                    {
                        retVal = 1;
                    }
                }
            }

            if (didSpecifyType)
            {
                ++countSpecified;
            }
        }

        if (countSpecified != filterList.size())
        {
            retVal = 1;
        }

        return retVal;
    }


    /////////////////////////////////////////////////////////////////
    // Filter Matching
    /////////////////////////////////////////////////////////////////

//                    EVM_TIMED_END -              OnAdvertisingReport:  17 ms,  17,635 us - 0:00:01.838024
//              OnAdvertisingReport -                        Extracted:   0 ms,      22 us - 0:00:01.838046
//                        Extracted -                         Filtered:   0 ms,     142 us - 0:00:01.838188
//                         Filtered -              OnAdvertisingReport:   0 ms,     838 us - 0:00:01.839026
//              OnAdvertisingReport -                        Extracted:   0 ms,      10 us - 0:00:01.839036
//                        Extracted -                         Filtered:   0 ms,      84 us - 0:00:01.839120
//                         Filtered -              OnAdvertisingReport:   4 ms,   4,778 us - 0:00:01.843898
//              OnAdvertisingReport -                        Extracted:   0 ms,      17 us - 0:00:01.843915
//                        Extracted -                         Filtered:   0 ms,      95 us - 0:00:01.844010
//                         Filtered -              OnAdvertisingReport:   0 ms,     793 us - 0:00:01.844803
//              OnAdvertisingReport -                        Extracted:   0 ms,      11 us - 0:00:01.844814
//                        Extracted -                         Filtered:   0 ms,      96 us - 0:00:01.844910
//                         Filtered -                              Put:   0 ms,      41 us - 0:00:01.844951
//                              Put - BleObserver::OnAdvertisingReport:   0 ms,      15 us - 0:00:01.844966
// BleObserver::OnAdvertisingReport -                           Queued:   0 ms,      51 us - 0:00:01.845017
//                           Queued -          EVM_LOW_PRIO_WORK_START:   0 ms,     932 us - 0:00:01.845949
//          EVM_LOW_PRIO_WORK_START -           OnAdvertisingReportEvm:   0 ms,      15 us - 0:00:01.845964
//           OnAdvertisingReportEvm -                           Popped:   0 ms,      24 us - 0:00:01.845988
//                           Popped -                              App:   0 ms,       8 us - 0:00:01.845996


    static void PreprocessFilterList(vector<Filter> &filterList)
    {
        for (auto &filter : filterList_)
        {
            for (auto &attr : filter.attrList)
            {
                if (attr.type == Filter::Attr::Type::UUID16)
                {
                    attr.uuid.ReverseBytes();
                }
            }
        }
    }

    static auto GetDataByType(const AdReport &report, uint8_t type)
    {
        const uint8_t *retVal = nullptr;
        uint8_t len = 0;

        ad_context_t context;
        for (ad_iterator_init(&context, report.len, report.data);
             ad_iterator_has_more(&context);
             ad_iterator_next(&context))
        {
            uint8_t dataType = ad_iterator_get_data_type(&context);

            if (dataType == type)
            {
                retVal = ad_iterator_get_data(&context);
                len    = ad_iterator_get_data_len(&context);

                break;
            }
        }

        return pair{ retVal, len };
    }

    static bool MatchMfrDataSize(const AdReport &report, uint8_t mfrDataSize)
    {
        static const uint8_t TYPE = 0xFF;

        auto [data, len] = GetDataByType(report, TYPE);

        bool retVal = data && len == mfrDataSize;

        return retVal;
    }

    static bool MatchDeviceAddr(const AdReport &report, const string &deviceAddr)
    {
        bool retVal = false;

        if (deviceAddr.size() == sizeof(report.addrStr) - 1)
        {
            retVal = memcmp(report.addrStr, deviceAddr.c_str(), sizeof(report.addrStr)) == 0;
        }

        return retVal;
    }

    static bool MatchUuid16(const AdReport &report, const UUID &uuid)
    {
        static const uint8_t BT_DATA_UUID16_ALL   = 0x03;
        static const uint8_t BT_DATA_UUID16_SOME  = 0x02;

        bool retVal = false;

        vector<uint8_t> typeList = { BT_DATA_UUID16_ALL, BT_DATA_UUID16_SOME };

        for (auto type : typeList)
        {
            auto [data, len] = GetDataByType(report, type);

            if (data && len >= 2)
            {
                const uint8_t *uuidBuf = uuid.GetBytes();

                for (int i = 0; i < len; i += 2)
                {
                    const uint8_t *p = &data[i];

                    retVal = memcmp(p, uuidBuf, 2) == 0;

                    if (retVal)
                    {
                        break;
                    }
                }
            }
        }

        return retVal;
    }

    static bool MatchDeviceName(const AdReport &report, const string &deviceName)
    {
        static const uint8_t TYPE = 0x09;

        bool retVal = false;

        auto [data, len] = GetDataByType(report, TYPE);

        if (data && len == (uint8_t)deviceName.length())
        {
            retVal = memcmp(data, deviceName.c_str(), len) == 0;
        }

        return retVal;
    }

    static bool FilterMatch(const AdReport &report, const Filter &filter)
    {
        size_t countMatch = 0;

        for (const auto &attr : filter.attrList)
        {
            if (attr.type == Filter::Attr::Type::AD_TYPE)
            {
                if ((uint8_t)attr.eventType == report.eventType)
                {
                    ++countMatch;
                }
            }
            else if (attr.type == Filter::Attr::Type::MFR_DATA_SIZE)
            {
                if (MatchMfrDataSize(report, attr.mfrDataSize))
                {
                    ++countMatch;
                }
            }
            else if (attr.type == Filter::Attr::Type::DEVICE_ADDR)
            {
                if (MatchDeviceAddr(report, attr.deviceAddr))
                {
                    ++countMatch;
                }
            }
            else if (attr.type == Filter::Attr::Type::UUID16)
            {
                if (MatchUuid16(report, attr.uuid))
                {
                    ++countMatch;
                }
            }
            else if (attr.type == Filter::Attr::Type::DEVICE_NAME)
            {
                if (MatchDeviceName(report, attr.deviceName))
                {
                    ++countMatch;
                }
            }
        }

        return countMatch == filter.attrList.size();
    }

    static bool FilterMatch(const AdReport &report, const vector<Filter> &filterList)
    {
        bool retVal = false;

        if (filterList.size())
        {
            for (const auto &filter : filterList)
            {
                if (FilterMatch(report, filter))
                {
                    retVal = true;

                    break;
                }
            }
        }
        else
        {
            retVal = true;
        }

        return retVal;
    }


    /////////////////////////////////////////////////////////////////
    // Misc
    /////////////////////////////////////////////////////////////////

    static void SetupShell()
    {
        Shell::AddCommand("ble.scan.start", [](vector<string> argList){
            Start([](const AdReport &report){
                Log("Ad: addrType: ", report.addressType,
                    ", addr: ", report.addrStr,
                    ", rssi: ", report.rssi,
                    ", len: ", report.len,
                    " eventType: ", report.eventType);
            });
        }, { .argCount = 0, .help = "Scan start and report seen" });

        Shell::AddCommand("ble.scan.stop", [](vector<string> argList){
            Stop();
        }, { .argCount = 0, .help = "Scan stop" });
    }


private:

    constexpr static const char *LOW_PRIO_WORK_LABEL = "BleObserver Low Priority Work";

    inline static unordered_map<uint8_t, string> typeMap_ = {
        { 0b0000, "ADV_IND"                       },
        { 0b0001, "ADV_DIRECT_IND"                },
        { 0b0010, "ADV_NONCONN_IND"               },
        { 0b0011, "SCAN_REQ | AUX_SCAN_REQ"       },
        { 0b0100, "SCAN_RSP"                      },
        { 0b0101, "CONNECT_IND | AUX_CONNECT_REQ" },
        { 0b0110, "ADV_SCAN_IND"                  },
        { 0b0111, "LOTS"                          },
        { 0b1000, "AUX_CONNECT_RSP"               },
    };

    static const uint8_t MAX_EVENTS = 8;
    inline static KMessagePipe<AdReport, MAX_EVENTS> pipe_;

    // callbacks
    inline static function<void(const AdReport &report)> cbFn_ = [](const AdReport &){};

    // filter
    inline static vector<Filter> filterList_;

    inline static TimedEventHandlerDelegate ted_;

    inline static bool started_ = false;
};













static const char * ad_types[] = {
    "", 
    "Flags",
    "Incomplete List of 16-bit Service Class UUIDs",
    "Complete List of 16-bit Service Class UUIDs",
    "Incomplete List of 32-bit Service Class UUIDs",
    "Complete List of 32-bit Service Class UUIDs",
    "Incomplete List of 128-bit Service Class UUIDs",
    "Complete List of 128-bit Service Class UUIDs",
    "Shortened Local Name",
    "Complete Local Name",
    "Tx Power Level",
    "", 
    "", 
    "Class of Device",
    "Simple Pairing Hash C",
    "Simple Pairing Randomizer R",
    "Device ID",
    "Security Manager TK Value",
    "Slave Connection Interval Range",
    "",
    "List of 16-bit Service Solicitation UUIDs",
    "List of 128-bit Service Solicitation UUIDs",
    "Service Data",
    "Public Target Address",
    "Random Target Address",
    "Appearance",
    "Advertising Interval"
};

static const char * flags[] = {
    "LE Limited Discoverable Mode",
    "LE General Discoverable Mode",
    "BR/EDR Not Supported",
    "Simultaneous LE and BR/EDR to Same Device Capable (Controller)",
    "Simultaneous LE and BR/EDR to Same Device Capable (Host)",
    "Reserved",
    "Reserved",
    "Reserved"
};
/* LISTING_END */

/* @text BTstack offers an iterator for parsing sequence of advertising data (AD) structures, 
 * see [BLE advertisements parser API](../appendix/apis/#ble-advertisements-parser-api).
 * After initializing the iterator, each AD structure is dumped according to its type.
 */

/* LISTING_START(GAPLEAdvDataParsing): Parsing advertising data */
[[maybe_unused]]
static void dump_advertisement_data(const uint8_t * adv_data, uint8_t adv_size){
    ad_context_t context;
    bd_addr_t address;
    uint8_t uuid_128[16];
    for (ad_iterator_init(&context, adv_size, (uint8_t *)adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type    = ad_iterator_get_data_type(&context);
        uint8_t size         = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        
        if (data_type > 0 && data_type < 0x1B){
            // printf("    %s: ", ad_types[data_type]);
            Log("    ", ad_types[data_type]);
        } 
        int i;
        // Assigned Numbers GAP
    
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_FLAGS:
                // show only first octet, ignore rest
                for (i=0; i<8;i++){
                    if (data[0] & (1<<i)){
                        // printf("%s; ", flags[i]);
                        LogNNL(flags[i]);
                    }

                }
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_16_BIT_SERVICE_SOLICITATION_UUIDS:
                for (i=0; i<size;i+=2){
                    // printf("%02X ", little_endian_read_16(data, i));
                    LogNNL(ToHex(little_endian_read_16(data, i)));
                }
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_32_BIT_SERVICE_SOLICITATION_UUIDS:
                for (i=0; i<size;i+=4){
                    // printf("%04X", (unsigned int)little_endian_read_32(data, i));
                    LogNNL(ToHex(little_endian_read_32(data, i)));
                }
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_128_BIT_SERVICE_SOLICITATION_UUIDS:
                reverse_128(data, uuid_128);
                // printf("%s", uuid128_to_str(uuid_128));
                LogNNL(uuid128_to_str(uuid_128));
                break;
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                for (i=0; i<size;i++){
                    // printf("%c", (char)(data[i]));
                    LogNNL(data[i]);
                }
                break;
            case BLUETOOTH_DATA_TYPE_TX_POWER_LEVEL:
                // printf("%d dBm", *(int8_t*)data);
                LogNNL(*(int8_t*)data, " dBm");
                break;
            case BLUETOOTH_DATA_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE:
                // printf("Connection Interval Min = %u ms, Max = %u ms", little_endian_read_16(data, 0) * 5/4, little_endian_read_16(data, 2) * 5/4);
                LogNNL("Connection Interval Min = ", little_endian_read_16(data, 0) * 5/4, " ms, Max = ", little_endian_read_16(data, 2) * 5/4, " ms");
                break;
            case BLUETOOTH_DATA_TYPE_SERVICE_DATA:
                // printf_hexdump(data, size);
                LogBlob(data, size);
                break;
            case BLUETOOTH_DATA_TYPE_PUBLIC_TARGET_ADDRESS:
            case BLUETOOTH_DATA_TYPE_RANDOM_TARGET_ADDRESS:
                reverse_bd_addr(data, address);
                // printf("%s", bd_addr_to_str(address));
                LogNNL(bd_addr_to_str(address));
                break;
            case BLUETOOTH_DATA_TYPE_APPEARANCE: 
                // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
                // printf("%02X", little_endian_read_16(data, 0) );
                LogNNL(ToHex(little_endian_read_16(data, 0)));
                break;
            case BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL:
                // printf("%u ms", little_endian_read_16(data, 0) * 5/8 );
                LogNNL(little_endian_read_16(data, 0) * 5/8, " ms");
                break;
            case BLUETOOTH_DATA_TYPE_3D_INFORMATION_DATA:
                // printf_hexdump(data, size);
                LogBlob(data, size);
                break;
            case BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA: // Manufacturer Specific Data 
                break;
            case BLUETOOTH_DATA_TYPE_CLASS_OF_DEVICE:
            case BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_HASH_C:
            case BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_RANDOMIZER_R:
            case BLUETOOTH_DATA_TYPE_DEVICE_ID: 
            case BLUETOOTH_DATA_TYPE_SECURITY_MANAGER_OUT_OF_BAND_FLAGS:
            default:
                // printf("Advertising Data Type 0x%2x not handled yet", data_type); 
                LogNNL("Advertising Data Type ", ToHex(data_type), " not handled yet");
                break;
        }        
        // printf("\n");
        LogNL();
    }
    // printf("\n");
    LogNL();
}

