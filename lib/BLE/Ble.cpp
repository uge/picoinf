#include "Ble.h"

#include "Evm.h"
#include "Log.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"

#include "btstack.h"
#include "platform/embedded/hci_dump_embedded_stdout.h"

#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"

#include "StrictMode.h"


static btstack_packet_callback_registration_t hciEventCallbackRegistration;
static volatile bool initDone = false;


static void OnHciReady()
{
    // in interrupt context still, but should be fine
    BleGap::OnReady();
    BleGatt::OnReady();

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
        uint8_t code = btstack_event_state_get_state(packet);

        if (code == HCI_STATE_WORKING)
        {
            bd_addr_t local_addr;
            gap_local_bd_addr(local_addr);

            LogNL();
            Log("BLE BTstack up and running on ", bd_addr_to_str(local_addr));
            LogNL();

            OnHciReady();
        }
        else
        {
            // Log("Not yet working, early return");
            Log("BTSTACK_EVENT_STATE DEFAULT - ", ToHex(code));
        }
    }
    else if (eventType == BTSTACK_EVENT_NR_CONNECTIONS_CHANGED)
    {
        // Log("BTSTACK_EVENT_NR_CONNECTIONS_CHANGED");
    }
    else if (eventType == HCI_EVENT_CONNECTION_REQUEST)
    {
        Log("HCI_EVENT_CONNECTION_REQUEST");
    }
    else if (eventType == HCI_EVENT_CONNECTION_COMPLETE)
    {
        Log("HCI_EVENT_CONNECTION_COMPLETE");
    }
    else if (eventType == HCI_EVENT_DISCONNECTION_COMPLETE)
    {
        Log("HCI_EVENT_DISCONNECTION_COMPLETE");

        BleGap::OnDisconnection();

        hci_con_handle_t con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
        printf("- LE Connection 0x%04x: disconnect, reason %02x\n", con_handle, hci_event_disconnection_complete_get_reason(packet));

        LogNL();
    }
    else if (eventType == HCI_EVENT_LE_META)
    {
        // Log("HCI_EVENT_LE_META");

        static const char * const phy_names[] = {
            "1 M",
            "2 M",
            "Codec",
        };

        uint8_t code = hci_event_le_meta_get_subevent_code(packet);

        if (code == HCI_SUBEVENT_LE_CONNECTION_COMPLETE)
        {
            Log("HCI_SUBEVENT_LE_CONNECTION_COMPLETE");

            BleGap::OnConnection();

            hci_con_handle_t con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet); 
            double interval = hci_subevent_le_connection_complete_get_conn_interval(packet) * 1.25;
            uint16_t latency = hci_subevent_le_connection_complete_get_conn_latency(packet);

            Log("- ", ToHex(con_handle), " connected - connection interval ", interval, " ms, latency ", latency);
            Log("  - Requesting 15 ms connection interval");
            LogNL();

            // request min con interval 15 ms for iOS 11+ 
            gap_request_connection_parameter_update(con_handle, 12, 12, 4, 0x0048);
        }
        else if (code == HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE)
        {
            Log("HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE");

            hci_con_handle_t con_handle = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
            double interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet) * 1.25;
            uint16_t latency = hci_subevent_le_connection_update_complete_get_conn_latency(packet);

            Log("- ", ToHex(con_handle), " interval updated - connection interval ", interval, " ms, latency ", latency);
            LogNL();
        }
        else if (code == HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE)
        {
            Log("HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE");

            hci_con_handle_t con_handle = hci_subevent_le_data_length_change_get_connection_handle(packet);
            uint16_t maxSize = hci_subevent_le_data_length_change_get_max_tx_octets(packet);

            Log("- ", ToHex(con_handle), " mtu updated - max ", maxSize, " bytes per packet");
            LogNL();
        }
        else if (code == HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE)
        {
            Log("HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE");

            hci_con_handle_t con_handle = hci_subevent_le_phy_update_complete_get_connection_handle(packet);
            printf("- LE Connection 0x%04x: PHY update - using LE %s PHY now\n", con_handle,
                    phy_names[hci_subevent_le_phy_update_complete_get_tx_phy(packet)]);

            LogNL();
        }
        else if (code == HCI_SUBEVENT_LE_CHANNEL_SELECTION_ALGORITHM)
        {
            // Log("HCI_SUBEVENT_LE_CHANNEL_SELECTION_ALGORITHM");
        }
        else if (code == HCI_SUBEVENT_LE_READ_REMOTE_FEATURES_COMPLETE)
        {
            // Log("HCI_SUBEVENT_LE_READ_REMOTE_FEATURES_COMPLETE");
        }
        else if (code == HCI_SUBEVENT_LE_REMOTE_CONNECTION_PARAMETER_REQUEST)
        {
            // Log("HCI_SUBEVENT_LE_REMOTE_CONNECTION_PARAMETER_REQUEST");
        }
        else if (code == HCI_SUBEVENT_LE_ADVERTISING_REPORT)
        {
            // Log("HCI_SUBEVENT_LE_ADVERTISING_REPORT");
        }
        else
        {
            Log("HCI_EVENT_LE_META Default ", ToHex(code));
            LogNL();
        }
    }
    else if (eventType == HCI_EVENT_TRANSPORT_PACKET_SENT)
    {
        // Log("HCI_EVENT_TRANSPORT_PACKET_SENT");
    }
    else if (eventType == HCI_EVENT_COMMAND_COMPLETE)
    {
        // Log("HCI_EVENT_COMMAND_COMPLETE");
    }
    else if (eventType == HCI_EVENT_COMMAND_STATUS)
    {
        // Log("HCI_EVENT_COMMAND_STATUS");
    }
    else if (eventType == HCI_EVENT_VENDOR_SPECIFIC)
    {
        // Log("HCI_EVENT_VENDOR_SPECIFIC");
    }
    else if (eventType == HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS)
    {
        // Log("HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS");
    }
    else if (eventType == GAP_EVENT_ADVERTISING_REPORT ||
             eventType == GAP_EVENT_EXTENDED_ADVERTISING_REPORT)
    {
        BleObserver::OnAdvertisingReport(packet);
    }
    // else if (eventType == )
    // {
    //     Log("");
    // }
    else
    {
        Log("HCI_DEFAULT - ", ToHex(eventType));
        LogNL();
    }
}



/////////////////////////////////////////////////////////////////////
// Interface
/////////////////////////////////////////////////////////////////////

void Ble::Init()
{
    Timeline::Global().Event("Ble::Init");

    Log("BLE Init Starting");

    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // init underlying drivers
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
    Log("BLE Init Complete");
}

void Ble::SetupShell()
{
    Shell::AddCommand("ble.init", [](vector<string> argList){
        Log("Ble::Init");
        Init();
    }, { .argCount = 0, .help = "Init" });

    Ble::Gap::SetupShell();
    BleObserver::SetupShell();
    BleGatt::SetupShell();
}