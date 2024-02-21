#include "BleNew.h"

#include "Evm.h"
#include "Log.h"
#include "PAL.h"
#include "Timeline.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"


static btstack_packet_callback_registration_t hciEventCallbackRegistration;
static volatile bool initDone = false;


static void OnHciReady()
{
    // in interrupt context still, but should be fine
    BleGap::Init();
    BleGatt::Init();

    initDone = true;
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
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
        {
            // Log("Not yet working, early return");
        }
        else
        {
            bd_addr_t local_addr;
            gap_local_bd_addr(local_addr);

            LogNL();
            Log("BTstack up and running on ", bd_addr_to_str(local_addr));

            OnHciReady();
        }
    }
    else if (eventType == HCI_EVENT_CONNECTION_REQUEST)
    {
        Log("HCI_EVENT_CONNECTION_REQUEST");
    }
    else if (eventType == HCI_EVENT_CONNECTION_COMPLETE)
    {
        Log("HCI_EVENT_CONNECTION_REQUEST");
    }
    else if (eventType == HCI_EVENT_DISCONNECTION_COMPLETE)
    {
        Log("HCI_EVENT_DISCONNECTION_COMPLETE");
        // le_notification_enabled = 0;

        hci_con_handle_t con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
        printf("- LE Connection 0x%04x: disconnect, reason %02x\n", con_handle, hci_event_disconnection_complete_get_reason(packet));                    
    }
    else if (eventType == ATT_EVENT_CAN_SEND_NOW)
    {
        Log("ATT_EVENT_CAN_SEND_NOW");
        // att_server_notify(con_handle, ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE, (uint8_t*)&current_temp, sizeof(current_temp));
    }
    else if (eventType == HCI_EVENT_LE_META)
    {
        Log("HCI_EVENT_LE_META");

        static const char * const phy_names[] = {
            "1 M",
            "2 M",
            "Codec",
        };

        uint8_t code = hci_event_le_meta_get_subevent_code(packet);

        if (code == HCI_SUBEVENT_LE_CONNECTION_COMPLETE)
        {
            Log("- HCI_SUBEVENT_LE_CONNECTION_COMPLETE");

            // print connection parameters (without using float operations)
            hci_con_handle_t con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet); 
            uint16_t conn_interval      = hci_subevent_le_connection_complete_get_conn_interval(packet);

            uint32_t intervalWhole = conn_interval * 125 / 100;
            uint32_t intervalFrac  = 25 * (conn_interval & 3);
            float interval = conn_interval * 1.25;

            uint16_t latency = hci_subevent_le_connection_complete_get_conn_latency(packet);

            Log("- LE Connection ", con_handle, " connected - connection interval (", conn_interval, ") ", interval, " ms, latency ", latency);

            printf("- LE Connection 0x%04x: connected - connection interval %u.%02u ms, latency %u\n", con_handle, conn_interval * 125 / 100,
                25 * (conn_interval & 3), hci_subevent_le_connection_complete_get_conn_latency(packet));

            // request min con interval 15 ms for iOS 11+ 
            // printf("- LE Connection 0x%04x: request 15 ms connection interval\n", con_handle);
            Log("- LE Connection ", con_handle, ": request 15 ms connection interval");
            gap_request_connection_parameter_update(con_handle, 12, 12, 4, 0x0048);
        }
        else if (code == HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE)
        {
            Log("- HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE");

            // print connection parameters (without using float operations)
            hci_con_handle_t con_handle    = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
            uint16_t conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);

            uint32_t intervalWhole = conn_interval * 125 / 100;
            uint32_t intervalFrac  = 25 * (conn_interval & 3);
            float interval = conn_interval * 1.25;

            Log("interval: ", interval);

            printf("- LE Connection 0x%04x: connection update - connection interval %u.%02u ms, latency %u\n", con_handle, conn_interval * 125 / 100,
                25 * (conn_interval & 3), hci_subevent_le_connection_update_complete_get_conn_latency(packet));
        }
        else if (code == HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE)
        {
            Log("- HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE");

            hci_con_handle_t con_handle = hci_subevent_le_data_length_change_get_connection_handle(packet);
            printf("- LE Connection 0x%04x: data length change - max %u bytes per packet\n", con_handle,
                    hci_subevent_le_data_length_change_get_max_tx_octets(packet));
        }
        else if (code == HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE)
        {
            Log("- HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE");

            hci_con_handle_t con_handle = hci_subevent_le_phy_update_complete_get_connection_handle(packet);
            printf("- LE Connection 0x%04x: PHY update - using LE %s PHY now\n", con_handle,
                    phy_names[hci_subevent_le_phy_update_complete_get_tx_phy(packet)]);
        }
        else
        {
            Log("- Default");
        }
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
    Timeline::Global().Event("Ble::Init");

    Log("BLE Init Starting");
    if (cyw43_arch_init())
    {
        Log("ERR: CYW43 Init Failed");
    }
    LogNL();
    hci_power_control(HCI_POWER_ON);

    l2cap_init();
    sm_init();

    hciEventCallbackRegistration.callback = &PacketHandlerHCI;
    hci_add_event_handler(&hciEventCallbackRegistration);

    // make this function synchronously wait for init
    while (initDone == false)
    {
        PAL.Delay(10);
    }

    LogNL();
    Log("Ble Init Complete");
}
