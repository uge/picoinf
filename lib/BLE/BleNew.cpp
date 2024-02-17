#include "BleNew.h"
#include "BleAdvertisement.h"
#include "Log.h"
#include "PAL.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"


inline static btstack_packet_callback_registration_t hciEventCallbackRegistration;


static void OnHciOperational()
{
    // setup advertisements
    uint16_t adv_int_min = 800;
    uint16_t adv_int_max = 800;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);

    // fill out actual advertisement data
    BleAdvertisement adv;
    adv.SetName("GLOW_BIKE");
    adv.SetAdvertisingUuidList({
        "0x181A",
    });
    static const vector<uint8_t> byteList = adv.GetRawAdvertisingDataStructure();
    gap_advertisements_set_data(byteList.size(), (uint8_t *)byteList.data());

    // enable
    gap_advertisements_enable(1);
}

// HCI, GAP, and general BTstack events
static void PacketHandlerHCI(uint8_t   packet_type,
                                uint16_t  channel,
                                uint8_t  *packet,
                                uint16_t  size)
{
    if (packet_type != HCI_EVENT_PACKET)
    {
        Log("ERR(?): Non-HCI packet received");
        return;
    }

    uint8_t eventType = hci_event_packet_get_type(packet);

    if (eventType == BTSTACK_EVENT_STATE)
    {
        Log("BTSTACK_EVENT_STATE - ", PAL.InIsr() ? "in ISR" : "");

        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
        {
            Log("Not yet working, early return");
        }
        else
        {
            bd_addr_t local_addr;
            gap_local_bd_addr(local_addr);

            printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));

            OnHciOperational();
        }
    }
    else if (eventType == HCI_EVENT_DISCONNECTION_COMPLETE)
    {
        Log("HCI_EVENT_DISCONNECTION_COMPLETE");
        // le_notification_enabled = 0;
    }
    else if (eventType == ATT_EVENT_CAN_SEND_NOW)
    {
        // Log("  ATT_EVENT_CAN_SEND_NOW");
        // att_server_notify(con_handle, ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE, (uint8_t*)&current_temp, sizeof(current_temp));
    }
    else
    {   
        // Log("  default - ", eventType);
    }
}



/////////////////////////////////////////////////////////////////////
// Interface
/////////////////////////////////////////////////////////////////////

void Ble::Init()
{
    Log("BLE Init");
    if (cyw43_arch_init())
    {
        Log("ERR: CYW43 Init Failed");
    }
    LogNL();

    hciEventCallbackRegistration.callback = &PacketHandlerHCI;
    hci_add_event_handler(&hciEventCallbackRegistration);

    hci_power_control(HCI_POWER_ON);
}
