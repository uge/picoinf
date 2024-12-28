#include "Ble.h"
#include "Evm.h"
#include "Log.h"
#include "PAL.h"
#include "Shell.h"

#include "btstack.h"

#include "StrictMode.h"



// When events come in, their code is found in btstack_defines.h

// When you want to extract data, functions are in btstack_event.h




// set it when connections come in
static hci_con_handle_t con_handle_global;


static vector<gatt_client_service_t> svcList;
static vector<gatt_client_characteristic_t> ctcList;

static void PacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    uint8_t eventType = hci_event_packet_get_type(packet);
    // Log("EventType: ", ToHex(eventType));

    if (eventType == GATT_EVENT_MTU)
    {
        Log("GATT MTU");
    }
    else if (eventType == GATT_EVENT_SERVICE_QUERY_RESULT)
    {
        Log("GATT_EVENT_SERVICE_QUERY_RESULT");

        /*
        typedef struct {
            uint16_t start_group_handle;
            uint16_t end_group_handle;
            uint16_t uuid16;
            uint8_t  uuid128[16];
        } gatt_client_service_t;
        */

        gatt_client_service_t svc;
        gatt_event_service_query_result_get_service(packet, &svc);
        svcList.push_back(svc);

        Log("Adding svc ", ToHex(svc.uuid16), " at idx ", svcList.size() - 1);
    }
    else if (eventType == GATT_EVENT_CHARACTERISTIC_QUERY_RESULT)
    {
        Log("GATT_EVENT_CHARACTERISTIC_QUERY_RESULT");

        /*
        typedef struct {
            uint16_t start_handle;
            uint16_t value_handle;
            uint16_t end_handle;
            uint16_t properties;
            uint16_t uuid16;
            uint8_t  uuid128[16];
        } gatt_client_characteristic_t;
        */
        gatt_client_characteristic_t ctc;
        gatt_event_characteristic_query_result_get_characteristic(packet, &ctc);
        ctcList.push_back(ctc);

        Log("Adding ctc at idx ", ctcList.size() - 1);
        Log("  start_handle: ", ToHex(ctc.start_handle));
        Log("  value_handle: ", ToHex(ctc.value_handle));
        Log("  end_handle  : ", ToHex(ctc.end_handle));
        Log("  properties  : ", ToBin(ctc.properties));
        Log("  uuid16      : ", ToHex(ctc.uuid16));
    }
    else if (eventType == GATT_EVENT_NOTIFICATION)
    {
        static Pin p(18);

        p.DigitalToggle();

        return;

        Log("GATT_EVENT_NOTIFICATION");

        uint16_t       handle = gatt_event_notification_get_value_handle(packet);
        uint16_t       len    = gatt_event_notification_get_value_length(packet);
        const uint8_t *data   = gatt_event_notification_get_value(packet);

        Log("handle: ", ToHex(handle), ", ", len, " bytes: ", ToHex(data, (uint8_t)len));
    }
    else if (eventType == GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT)
    {
        Log("GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT");

        uint16_t       len  = gatt_event_characteristic_descriptor_query_result_get_descriptor_length(packet);
        const uint8_t *data = gatt_event_characteristic_descriptor_query_result_get_descriptor(packet);

        Log("Got ", len, " byte response");
        LogHex(data, len);
    }
    else if (eventType == GATT_EVENT_QUERY_COMPLETE)
    {
        Log("Query complete");
    }


}


void SetupShell()
{
    Shell::AddCommand("connect", [](vector<string> argList){
        uint8_t ret;

        string addrStr = "28:CD:C1:08:7D:74";

        bd_addr_t addr;
        ret = (uint8_t)sscanf_bd_addr(addrStr.c_str(), addr);
        if (ret != 1)
        {
            Log("Scan fail");
        }

        gatt_client_init();

        ret = gap_connect(addr, BD_ADDR_TYPE_LE_PUBLIC);
        if (ret != ERROR_CODE_SUCCESS)
        {
            Log("Connect fail - ", ret);
        }

        Log("Connecting...");
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("svc", [](vector<string> argList){
        gatt_client_discover_primary_services(PacketHandler, con_handle_global);
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("ctc", [](vector<string> argList){
        gatt_client_discover_characteristics_for_service(PacketHandler, con_handle_global, &svcList[2]);
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("sub", [](vector<string> argList){
        uint16_t configuration = GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
        gatt_client_write_client_characteristic_configuration(PacketHandler, con_handle_global, &ctcList[0], configuration);
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("listen", [](vector<string> argList){
        static gatt_client_notification_t storage;
        gatt_client_listen_for_characteristic_value_updates(&storage, PacketHandler, con_handle_global, &ctcList[0]);
    }, { .argCount = 0, .help = "" });



    Shell::AddCommand("read", [](vector<string> argList){
        uint16_t handle = (uint16_t)atoi(argList[0].c_str());

        uint8_t ret;

        Log("Reading");
        
        ret = 
            gatt_client_read_characteristic_descriptor_using_descriptor_handle(PacketHandler, con_handle_global, handle);

        if (ret != ERROR_CODE_SUCCESS)
        {
            Log("Read Failed, ", ret);
        }
    }, { .argCount = 1, .help = "" });


    Shell::AddCommand("", [](vector<string> argList){
    }, { .argCount = 1, .help = "Read Descriptor <x>" });








}

