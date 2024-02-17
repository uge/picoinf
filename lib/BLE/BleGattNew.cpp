#include "BleGattNew.h"
#include "Log.h"

#include "btstack.h"


// format of each entry is NOT(?) the same as advertising data
// format is:
// <size of entire entry including this 1-byte size field><data>

const uint8_t profile_data[] =
{
    // ATT DB Version
    1,

    // 0x0001 PRIMARY_SERVICE-GAP_SERVICE
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18, 
    // 0x0002 CHARACTERISTIC-GAP_DEVICE_NAME - READ
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a, 
    // 0x0003 VALUE CHARACTERISTIC-GAP_DEVICE_NAME - READ -'picow_temp'
    // READ_ANYBODY
    0x12, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x2a, 0x70, 0x69, 0x63, 0x6f, 0x77, 0x5f, 0x74, 0x65, 0x6d, 0x70, 
    // 0x0004 PRIMARY_SERVICE-GATT_SERVICE
    0x0a, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x28, 0x01, 0x18, 
    // 0x0005 CHARACTERISTIC-GATT_DATABASE_HASH - READ
    0x0d, 0x00, 0x02, 0x00, 0x05, 0x00, 0x03, 0x28, 0x02, 0x06, 0x00, 0x2a, 0x2b, 
    // 0x0006 VALUE CHARACTERISTIC-GATT_DATABASE_HASH - READ -''
    // READ_ANYBODY
    0x18, 0x00, 0x02, 0x00, 0x06, 0x00, 0x2a, 0x2b, 0x30, 0x2e, 0x9d, 0x1e, 0x20, 0x9a, 0x0a, 0xfe, 0x9b, 0xb7, 0xd8, 0x2e, 0x19, 0x38, 0xa8, 0xca, 
    // 0x0007 PRIMARY_SERVICE-ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING
    0x0a, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x28, 0x1a, 0x18, 
    // 0x0008 CHARACTERISTIC-ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE - READ | NOTIFY | INDICATE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x08, 0x00, 0x03, 0x28, 0x32, 0x09, 0x00, 0x6e, 0x2a, 
    // 0x0009 VALUE CHARACTERISTIC-ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE - READ | NOTIFY | INDICATE | DYNAMIC
    // READ_ANYBODY
    0x08, 0x00, 0x02, 0x01, 0x09, 0x00, 0x6e, 0x2a, 
    // 0x000a CLIENT_CHARACTERISTIC_CONFIGURATION
    // READ_ANYBODY, WRITE_ANYBODY
    0x0a, 0x00, 0x0e, 0x01, 0x0a, 0x00, 0x02, 0x29, 0x00, 0x00, 
    // END
    0x00, 0x00, 
}; // total size 71 bytes 


//
// list service handle ranges
//
#define ATT_SERVICE_GAP_SERVICE_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_END_HANDLE 0x0003
#define ATT_SERVICE_GAP_SERVICE_01_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_01_END_HANDLE 0x0003
#define ATT_SERVICE_GATT_SERVICE_START_HANDLE 0x0004
#define ATT_SERVICE_GATT_SERVICE_END_HANDLE 0x0006
#define ATT_SERVICE_GATT_SERVICE_01_START_HANDLE 0x0004
#define ATT_SERVICE_GATT_SERVICE_01_END_HANDLE 0x0006
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_START_HANDLE 0x0007
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_END_HANDLE 0x000a
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_01_START_HANDLE 0x0007
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_01_END_HANDLE 0x000a

//
// list mapping between characteristics and handles
//
#define ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_GATT_DATABASE_HASH_01_VALUE_HANDLE 0x0006
#define ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE 0x0009
#define ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_CLIENT_CONFIGURATION_HANDLE 0x000a




int le_notification_enabled;
hci_con_handle_t con_handle;




uint16_t current_temp = 42;













uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    Log("att_read_callback");
    UNUSED(connection_handle);

    if (att_handle == ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE){
        return att_read_callback_handle_blob((const uint8_t *)&current_temp, sizeof(current_temp), offset, buffer, buffer_size);
    }
    return 0;
}

int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    Log("att_write_callback");
    
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    if (att_handle != ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    con_handle = connection_handle;
    if (le_notification_enabled) {
        att_server_request_can_send_now_event(con_handle);
    }
    return 0;
}



/////////////////////////////////////////////////////////////////////
// Interface
/////////////////////////////////////////////////////////////////////

void BleGatt::Init()
{
    // https://bluekitchen-gmbh.com/btstack/#appendix/att_server/
    // att_server_init(profile_data, att_read_callback, att_write_callback);    


    // inform about BTstack state

    // register for ATT event
    // att_server_register_packet_handler(packet_handler);

    // static TimedEventHandlerDelegate ted;
    // ted.SetCallback([]{
    //     if (le_notification_enabled) {
    //         att_server_request_can_send_now_event(con_handle);
    //     }
    // });
    // ted.RegisterForTimedEventInterval(5000, 0);


}