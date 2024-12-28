#pragma once


// inline static le_advertising_set_t le_advertising_set;
inline le_advertising_set_t le_advertising_set;

#include "BleAdvertisingDataFormatter.h"

class BleGapExtended
{
public:

    void Init()
    {



// watch for GAP_SUBEVENT_ADVERTISING_SET_INSTALLED
// typedef struct {
//     uint16_t        advertising_event_properties;
//     uint16_t        primary_advertising_interval_min;
//     uint16_t        primary_advertising_interval_max;
//     uint8_t         primary_advertising_channel_map;
//     bd_addr_type_t  own_address_type;
//     bd_addr_type_t  peer_address_type;
//     bd_addr_t       peer_address;
//     uint8_t         advertising_filter_policy;
//     int8_t          advertising_tx_power;
//     uint8_t         primary_advertising_phy;
//     uint8_t         secondary_advertising_max_skip;
//     uint8_t         secondary_advertising_phy;
//     uint8_t         advertising_sid;
//     uint8_t         scan_request_notification_enable;
// } le_extended_advertising_parameters_t;



// from le_audio_broadcast_sink.c (14 of 14 params set)
//
// static const le_extended_advertising_parameters_t extended_params = {
//         .advertising_event_properties = 1,  // connectable
//         .primary_advertising_interval_min = 0x4b0, // 750 ms
//         .primary_advertising_interval_max = 0x4b0, // 750 ms
//         .primary_advertising_channel_map = 7,
//         .own_address_type = 0,
//         .peer_address_type = 0,
//         .peer_address =  { 0 },
//         .advertising_filter_policy = 0,
//         .advertising_tx_power = 10, // 10 dBm
//         .primary_advertising_phy = 1, // LE 1M PHY
//         .secondary_advertising_max_skip = 0,
//         .secondary_advertising_phy = 1, // LE 1M PHY
//         .advertising_sid = adv_sid,
//         .scan_request_notification_enable = 0,
// };

// from le_audio_broadcast_source.c (14 of 14 params set)
//
// static const le_extended_advertising_parameters_t extended_params = {
//         .advertising_event_properties = 0,
//         .primary_advertising_interval_min = 0x4b0, // 750 ms
//         .primary_advertising_interval_max = 0x4b0, // 750 ms
//         .primary_advertising_channel_map = 7,
//         .own_address_type = 0,
//         .peer_address_type = 0,
//         .peer_address =  { 0 },
//         .advertising_filter_policy = 0,
//         .advertising_tx_power = 10, // 10 dBm
//         .primary_advertising_phy = 1, // LE 1M PHY
//         .secondary_advertising_max_skip = 0,
//         .secondary_advertising_phy = 1, // LE 1M PHY
//         .advertising_sid = adv_sid,
//         .scan_request_notification_enable = 0,
// };

static const le_extended_advertising_parameters_t params = {
        .advertising_event_properties = 0,
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = BD_ADDR_TYPE_LE_PUBLIC,
        .peer_address_type = BD_ADDR_TYPE_LE_PUBLIC,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = 0,
        .scan_request_notification_enable = 0,
};





        static le_advertising_set_t storage;
        uint8_t handle;
        uint8_t retCreate = gap_extended_advertising_setup(&storage, &params, &handle);
        if (retCreate != ERROR_CODE_SUCCESS)
        {
            Log("ERR Creating extended advertising - ", retCreate);
        }

        // make stv structure
        BleAdvertisingDataFormatter f;
        f.AddFlags();
        f.AddName("TEST_EXTENDED");
        vector<uint8_t> mfrDataByteList;
        mfrDataByteList.resize(50);
        for (uint8_t i = 0; auto &b : mfrDataByteList)
        {
            b = i++;
        }
        f.AddMfrData(mfrDataByteList);
        
        static vector<uint8_t> byteList = f.GetData();




        uint8_t retSet =
            gap_extended_advertising_set_adv_data(handle,
                                                  (uint16_t)byteList.size(),
                                                  byteList.data());
        if (retSet != ERROR_CODE_SUCCESS)
        {
            Log("ERR setting data - ", retSet);
        }







// #define BROADCAST_ID (0x112233u)



// static const uint8_t extended_adv_data[] = {
//         // 16 bit service data, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE, Broadcast ID
//         6, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x52, 0x18,
//         BROADCAST_ID >> 16,
//         (BROADCAST_ID >> 8) & 0xff,
//         BROADCAST_ID & 0xff,
//         7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e'
// };

//     gap_extended_advertising_set_adv_data(handle, sizeof(extended_adv_data), extended_adv_data);






        uint8_t retStart = gap_extended_advertising_start(handle, 0, 0);
        if (retStart != ERROR_CODE_SUCCESS)
        {
            Log("ERR Starting - ", retStart);
        }
    }

    void Init2()
    {

static uint8_t adv_handle = 0;


static const le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 0,
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = BD_ADDR_TYPE_LE_PUBLIC,
        .peer_address_type = BD_ADDR_TYPE_LE_PUBLIC,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = 0,
        .scan_request_notification_enable = 0,
};

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

static const uint8_t extended_adv_data[] = {
        // 16 bit service data, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE, Broadcast ID
        6, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x52, 0x18,
        BROADCAST_ID >> 16,
        (BROADCAST_ID >> 8) & 0xff,
        BROADCAST_ID & 0xff,
        // name
#ifdef PTS_MODE
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P', 'T', 'S', '-', 'x', 'x'
#elif defined(COUNT_MODE)
        6, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'C', 'O', 'U', 'N', 'T'
#else
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e'
#endif
};

static const le_periodic_advertising_parameters_t periodic_params = {
        .periodic_advertising_interval_min = 0x258, // 375 ms
        .periodic_advertising_interval_max = 0x258, // 375 ms
        .periodic_advertising_properties = 0
};
static uint8_t period_adv_data[255];
static uint16_t period_adv_data_len;


    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(extended_adv_data), extended_adv_data);
    gap_periodic_advertising_set_params(adv_handle, &periodic_params);
    gap_periodic_advertising_set_data(adv_handle, period_adv_data_len, period_adv_data);
    gap_periodic_advertising_start(adv_handle, 0);
    gap_extended_advertising_start(adv_handle, 0, 0);




    }



private:

    // if you do this, you must reserve in advance, as the data can't move later, they keep the pointer
    vector<le_advertising_set_t> advSetList_;


};

