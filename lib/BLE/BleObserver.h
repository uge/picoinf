#pragma once

#include "BleAdvertisingDataFormatter.h"
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

private:

    struct AdReport
    {
        AdReport()
        {
            // make sure any holes in the structure are zero for hashing
            memset(this, 0, sizeof(AdReport));
        }

        bd_addr_t address;
        uint8_t   addressType;
        uint8_t   eventType;
        int8_t    rssi;
        uint8_t   len;
        uint8_t   data[BleAdvertisingDataFormatter::ADV_BYTES];
    };



public:

    /////////////////////////////////////////////////////////////////
    // Public Interface
    /////////////////////////////////////////////////////////////////

    static void Start()
    {
        if (started_) return;

        // scan_type – 0 = passive, 1 = active
        // scan_interval – range 0x0004..0x4000, unit 0.625 ms
        // scan_window – range 0x0004..0x4000, unit 0.625 ms
        // scanning_filter_policy – 0 = all devices, 1 = all from whitelist
        // uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window

        // Active scanning, 100% (scan interval = scan window)
        gap_set_scan_parameters(1, 48, 48);

        gap_start_scan();

        ted_.SetCallback([]{
            ClearCache();
        });
        ted_.RegisterForTimedEventInterval(CACHE_EXPIRY_SECS * 1000);
    }

    static void Stop()
    {
        if (!started_) return;

        gap_stop_scan();

        ted_.DeRegisterForTimedEvent();
        ClearCache();
    }

    static void ClearCache()
    {
        IrqLock lock;
        seenSet_.clear();
        seenAddrSet_.clear();
    }


    /////////////////////////////////////////////////////////////////
    // Report Handling
    /////////////////////////////////////////////////////////////////

    static void OnAdvertisingReportEvm()
    {
        AdReport report;

        while (pipe_.Get(report, 0))
        {
            // commit hash so ISR can pre-filter
            uint32_t hash = HashReport(report);
            {
                IrqLock lock;
                seenSet_.insert(hash);
            }
            Log("Hash: ", ToHex(hash), ", now ", seenSet_.size(), " seen");

            // check if this is changed data for a particular address.
            // not functionality which is required, just investigating.
            string addrStr = bd_addr_to_str(report.address);

            if (seenAddrSet_.contains(addrStr) == false)
            {
                seenAddrSet_.insert(addrStr);
                Log("New: ", seenAddrSet_.size(), " addresses so far");
            }
            else
            {
                Log("Mod: Address ", addrStr, " seen before, changed");
            }

            // print out report
            string eventTypeStr = typeMap_.contains(report.eventType) ?
                                  typeMap_.at(report.eventType)       :
                                  to_string(report.eventType);

            Log("Ad: addrType: ", report.addressType,
                ", addr: ", addrStr,
                ", rssi: ", report.rssi,
                ", len: ", report.len,
                " eventType: ", eventTypeStr);

            // dump_advertisement_data(report.data, report.len);

            LogNL();
        }
    }


    static void OnAdvertisingReport(uint8_t *packet)
    {
        static Pin p(18);

        uint8_t type = hci_event_packet_get_type(packet);

        if (type == GAP_EVENT_ADVERTISING_REPORT)
        {
            AdReport report;

            gap_event_advertising_report_get_address(packet, report.address);

            report.eventType    = gap_event_advertising_report_get_advertising_event_type(packet);
            report.addressType  = gap_event_advertising_report_get_address_type(packet);
            report.rssi         = gap_event_advertising_report_get_rssi(packet);
            report.len          = gap_event_advertising_report_get_data_length(packet);

            memcpy(report.data, gap_event_advertising_report_get_data(packet), report.len);

            bool ourGuy = (strcmp("28:CD:C1:08:7D:74", bd_addr_to_str(report.address)) == 0) &&
                          (report.eventType == 0 || report.eventType == 0b0010);

            uint32_t hash = HashReport(report);

            if (seenSet_.contains(hash) == false && ourGuy)
            {
                p.DigitalToggle();
            }

            if (ourGuy)
            {
                static uint64_t timeLast = 0;

                uint64_t timeNow = PAL.Micros();

                if (timeLast != 0)
                {
                    uint64_t timeDiff = timeNow - timeLast;
                    Log(Commas(timeDiff / 1000), " ms since last");
                }

                // p.DigitalToggle();
                Log("[", TimestampFromUs(PAL.Micros()), "]: Our guy, hash (", ToHex(hash), ") seen? ", seenSet_.contains(hash) ? "yes" : "no");
                LogBlob(report.data, report.len);
                LogNL();

                timeLast = timeNow;
            }

            if (seenSet_.contains(hash) == false && ourGuy)
            {
                auto count = pipe_.Count();

                if (pipe_.Put(report, 0))
                {
                    // send an event if there isn't one already pending
                    if (count == 0)
                    {
                        Evm::QueueWork("BleObserver::OnAdvertisingReport", OnAdvertisingReportEvm);
                    }
                }
            }
        }
    }


private:

    static uint32_t HashReport(const AdReport &report)
    {
        XXHash32 hashBuilder(0);

        hashBuilder.add(report.address, sizeof(report.address));
        hashBuilder.add(&report.eventType, sizeof(report.eventType));
        hashBuilder.add(report.data, report.len);

        return hashBuilder.hash();
    }

    static void SetupShell()
    {
        Shell::AddCommand("ble.scan.cache.report", [](vector<string> argList){
            IrqLock lock;
            Log("Seen Set: ", seenSet_.size());
            Log("Addr Set: ", seenAddrSet_.size());
        }, { .argCount = 0, .help = "Report on count of entries in cache" });

        Shell::AddCommand("ble.scan.cache.clear", [](vector<string> argList){
            ClearCache();
        }, { .argCount = 0, .help = "Clear entries in cache" });
    }


private:

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

    // must not allow unbounded growth, reset these periodically
    inline static const uint64_t CACHE_EXPIRY_SECS = 5 * 60;   // 5 min
    inline static TimedEventHandlerDelegate ted_;
    inline static unordered_set<uint32_t> seenSet_;
    inline static unordered_set<string> seenAddrSet_;

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

