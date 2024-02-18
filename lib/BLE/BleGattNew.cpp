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


#include "UUID.h"
static void ReadGatt(uint8_t *buf)
{
    uint8_t *row = (uint8_t *)&buf[1];

    struct Row
    {
        uint16_t size = 0;
        uint16_t flags = 0;
        uint16_t handle = 0;
        vector<uint8_t> uuidByteList;
        UUID uuid;
        vector<uint8_t> valueByteList;
        string value;
    };

    auto Get16 = [](uint8_t *buf, uint8_t idx)
    {
        uint16_t retVal;

        memcpy(&retVal, &buf[idx], 2);

        return retVal;
    };

    vector<Row> rowList;

    uint16_t rowSize = Get16(row, 0);    // little endian
    while (rowSize)
    {
        uint16_t flags = Get16(row, 2);  // little endian
        uint16_t handle = Get16(row, 4); // little endian
        vector<uint8_t> uuidByteList;
        uint8_t uuidBytesToConsume = 2;
        if (flags & (uint16_t)ATT_PROPERTY_UUID128)
        {
            uuidBytesToConsume = 16;
        }
        for (int i = 0; i < uuidBytesToConsume; ++i)
        {
            uuidByteList.push_back(row[6 + i]);
        }

        vector<uint8_t> valueByteList;
        uint8_t valueBytesToConsume = rowSize - (uuidByteList.size() + 2 + 2 + 2);
        for (int i = 0; i < valueBytesToConsume; ++i)
        {
            valueByteList.push_back(row[6 + uuidByteList.size() + i]);
        }

        UUID uuid;
        uuid.SetBytesReversed(uuidByteList.data(), uuidByteList.size());    // I think reversed?

        string value;
        for (auto &b : valueByteList)
        {
            value.push_back(isprint(b) ? b : '.');
        }

        rowList.push_back({
            rowSize,
            flags, 
            handle,
            uuidByteList,
            uuid,
            valueByteList,
            value
        });

        row += rowSize;
        rowSize = Get16(row, 0);    // little endian
    }

    Log("Found ", rowList.size(), " rows");
    for (int i = 0; auto &row : rowList)
    {
        Log("Row ", ++i);
        Log("  Size  : ", row.size, " / ", ToHex(row.size));
        Log("  Flags : ", ToHex(row.flags));
        Log("  Handle: ", row.handle);
        Log("  UUID  : ", row.uuid.GetUUID());
        Log("  Value : \"", row.value, "\"");
        Log("  Value : ", row.valueByteList);

        LogNNL("  Value : [");
        string sep = "";
        for (auto &b : row.valueByteList)
        {
            LogNNL(sep);
            LogNNL(ToHex(b, false));

            sep = ", ";
        }
        LogNNL("]");
        LogNL();
    }
}


#include "UUID.h"
#include "Utl.h"
class AttDatabase
{
public:

    uint16_t AddEntry(uint16_t flags, string uuidStr, vector<uint8_t> valueByteList)
    {
        vector<uint8_t> rowByteList;

        UUID uuid(uuidStr);
        bool is128Bit = uuid.GetBitCount() != 16;

        // fill out size
        uint16_t size = 0;
        size += 2;                      // for itself
        size += 2;                      // for flags
        size += 2;                      // for handle
        size += is128Bit ? 16 : 2;      // for uuid
        size += valueByteList.size();   // for value
        Append(rowByteList, ToByteList(size));

        // fill out flags
        Append(rowByteList, ToByteList(flags));

        // fill out handle
        uint16_t handle = nextHandle_;
        ++nextHandle_;
        Append(rowByteList, ToByteList(handle));

        // fill out uuid
        uuid.ReverseBytes();
        Append(rowByteList, uuid.GetByteList());
        uuid.ReverseBytes();

        // fill out value
        Append(rowByteList, valueByteList);

        // capture this row for later
        rowByteListList_.push_back(rowByteList);

        return handle;
    }

    static const uint16_t DEFAULT_FLAGS = 0x0002;

    uint16_t AddPrimaryService(string uuid)
    {
        string uuidType = "0x2800";
        uint16_t flags = DEFAULT_FLAGS;
        
        return AddEntry(flags, uuidType, UUID{uuid}.GetByteList());
    }


// 0x0006 VALUE CHARACTERISTIC-GATT_DATABASE_HASH - READ -''
// READ_ANYBODY
// 0x18, 0x00, 0x02, 0x00, 0x06, 0x00, 0x2a, 0x2b, 0x30, 0x2e, 0x9d, 0x1e, 0x20, 0x9a, 0x0a, 0xfe, 0x9b, 0xb7, 0xd8, 0x2e, 0x19, 0x38, 0xa8, 0xca, 

// 0x0008 CHARACTERISTIC-ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE - READ | NOTIFY | INDICATE | DYNAMIC
// 0x0d, 0x00, 0x02, 0x00, 0x08, 0x00, 0x03, 0x28, 0x32, 0x09, 0x00, 0x6e, 0x2a, 

// 0x0002 CHARACTERISTIC-GAP_DEVICE_NAME - READ
// 0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a, 
// 0x0d, 0x00 - size
//             0x02, 0x00 - flags
//                         0x02, 0x00 - handle
//                                     0x03, 0x28 - UUID type
//                                                 0x02, 0x03, 0x00 - ??
//                                                 0x02, 0x03 - ?? (flags?)
//                                                             0x00 - ?? (len where 0 = dynamic??)
//                                                                   0x00, 0x2a - visible UUID

    uint16_t AddCharacteristic(uint16_t flags, string uuid, vector<uint8_t> valueByteList)
    {
        return AddEntry(flags, uuid, valueByteList);
    }














    vector<uint8_t> GetDatabaseData()
    {
        vector<uint8_t> byteList;

        byteList.push_back(1);

        // fill in Generic Access Service
        // fill in Generic Access characteristics (device name, etc)
        // fill in Generic Attribute Service
        // fill in Generic Attribute characteristics (database hash, indicate change)?

        auto FnLog = [](uint8_t row, vector<uint8_t> &byteList){
            Log("Row ", row);
            LogNNL("Bytes: [");
            string sep = "";
            for (auto &b : byteList)
            {
                LogNNL(sep);
                LogNNL(ToHex(b, false));

                sep = ", ";
            }
            LogNNL("]");
            LogNL();
        };

        for (int i = 0; auto &rowByteList : rowByteListList_)
        {
            Append(byteList, rowByteList);

            FnLog(++i, rowByteList);
        }

        byteList.push_back(0);
        byteList.push_back(0);

        return byteList;
    }


private:

    void AddGenericAccessService()
    {
        {
            AddPrimaryService("0x1800");
        }

        if (deviceName_ != "")
        {
            string uuid = "";
            // AddCharacteristic();
        }
    }





private:

    uint16_t nextHandle_ = 1;

    vector<vector<uint8_t>> rowByteListList_;

    string deviceName_;
};



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


/*
#define ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS            0x90 
#define ATT_HANDLE_VALUE_INDICATION_TIMEOUT                0x91
#define ATT_HANDLE_VALUE_INDICATION_DISCONNECT             0x92

#define ATT_PROPERTY_BROADCAST           0x01
#define ATT_PROPERTY_READ                0x02
#define ATT_PROPERTY_WRITE_WITHOUT_RESPONSE 0x04
#define ATT_PROPERTY_WRITE               0x08
#define ATT_PROPERTY_NOTIFY              0x10
#define ATT_PROPERTY_INDICATE            0x20
#define ATT_PROPERTY_AUTHENTICATED_SIGNED_WRITE 0x40
#define ATT_PROPERTY_EXTENDED_PROPERTIES 0x80
*/

// HCI, GAP, and general BTstack events
static void PacketHandlerATT(uint8_t   packet_type,
                             uint16_t  channel,
                             uint8_t  *packet,
                             uint16_t  size)
{
    Log("ATT: type: ", packet_type, ", channel: ", channel, ", size: ", size);

    int mtu;

    if (packet_type == HCI_EVENT_PACKET)
    {
        uint8_t eventType = hci_event_packet_get_type(packet);

        if (eventType == ATT_EVENT_CONNECTED)
        {
            hci_con_handle_t handle = att_event_connected_get_handle(packet);
            uint16_t mtu = att_server_get_mtu(handle);

            Log("ATT_EVENT_CONNECTED - ", handle, " at mtu ", mtu);

            // context = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
            // if (!context) break;
            // context->counter = 'A';
            // context->connection_handle = att_event_connected_get_handle(packet);
            // context->test_data_len = btstack_min(att_server_get_mtu(context->connection_handle) - 3, sizeof(context->test_data));
            // printf("%c: ATT connected, handle 0x%04x, test data len %u\n", context->name, context->connection_handle, context->test_data_len);
        }
        else if (eventType == ATT_EVENT_MTU_EXCHANGE_COMPLETE)
        {
            hci_con_handle_t handle = att_event_mtu_exchange_complete_get_handle(packet);
            uint16_t mtu = att_event_mtu_exchange_complete_get_MTU(packet);

            Log("ATT_EVENT_MTU_EXCHANGE_COMPLETE - ", handle, " at mtu ", mtu);

            // mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            // context = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));
            // if (!context) break;
            // context->test_data_len = btstack_min(mtu - 3, sizeof(context->test_data));
            // printf("%c: ATT MTU = %u => use test data of len %u\n", context->name, mtu, context->test_data_len);
        }
        else if (eventType == ATT_EVENT_CAN_SEND_NOW)
        {
            Log("ATT_EVENT_CAN_SEND_NOW");

            // streamer();
        }
        else if (eventType == ATT_EVENT_DISCONNECTED)
        {
            hci_con_handle_t handle = att_event_disconnected_get_handle(packet);

            Log("ATT_EVENT_DISCONNECTED - ", handle);

            // context = connection_for_conn_handle(att_event_disconnected_get_handle(packet));
            // if (!context) break;
            // // free connection
            // printf("%c: ATT disconnected, handle 0x%04x\n", context->name, context->connection_handle);                    
            // context->le_notification_enabled = 0;
            // context->connection_handle = HCI_CON_HANDLE_INVALID;
        }
        else
        {
            Log("ATT: default event");
        }
    }
    else
    {
        Log("ATT: Not HCI_EVENT_PACKET!!!!!");
    }
}

/////////////////////////////////////////////////////////////////////
// Interface
/////////////////////////////////////////////////////////////////////

void BleGatt::Init()
{
    // https://bluekitchen-gmbh.com/btstack/#appendix/att_server/
    att_server_init(profile_data, att_read_callback, att_write_callback);


    ReadGatt((uint8_t *)profile_data);

    AttDatabase attDb;
    attDb.AddPrimaryService("0x1800");
    attDb.AddPrimaryService("0x1801");
    attDb.AddPrimaryService("0x181A");
    attDb.GetDatabaseData();


    // register for ATT event
    att_server_register_packet_handler(PacketHandlerATT);

    // static TimedEventHandlerDelegate ted;
    // ted.SetCallback([]{
    //     if (le_notification_enabled) {
    //         att_server_request_can_send_now_event(con_handle);
    //     }
    // });
    // ted.RegisterForTimedEventInterval(5000, 0);


}