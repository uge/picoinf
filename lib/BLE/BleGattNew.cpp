#include "BleGattNew.h"
#include "Log.h"
#include "Timeline.h"
#include "Utl.h"

#include "btstack.h"

#include <unordered_map>
using namespace std;


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
    // 0x0003 VALUE CHARACTERISTIC-GAP_DEVICE_NAME - READ -picow_temp
    // READ_ANYBODY
    0x12, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x2a, 0x70, 0x69, 0x63, 0x6f, 0x77, 0x5f, 0x74, 0x65, 0x6d, 0x70, 
    // 0x0004 PRIMARY_SERVICE-GATT_SERVICE
    0x0a, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x28, 0x01, 0x18, 
    // 0x0005 CHARACTERISTIC-GATT_DATABASE_HASH - READ
    0x0d, 0x00, 0x02, 0x00, 0x05, 0x00, 0x03, 0x28, 0x02, 0x06, 0x00, 0x2a, 0x2b, 
    // 0x0006 VALUE CHARACTERISTIC-GATT_DATABASE_HASH - READ -
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
    static const uint8_t BUF_SIZE = sizeof(profile_data);
    static uint8_t bufOrig[BUF_SIZE];
    memcpy(bufOrig, profile_data, BUF_SIZE);

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
    }

    Log("Data intact? ", memcmp(bufOrig, profile_data, BUF_SIZE) == 0 ? "YES" : "NO");
}


#include "UUID.h"
#include "Utl.h"
class AttDatabase
{
public:

    uint16_t AddEntry(uint16_t flags, string uuidTypeStr, vector<uint8_t> valueByteList)
    {
        vector<uint8_t> rowByteList;

        UUID uuidType(uuidTypeStr);
        bool is128Bit = uuidType.GetBitCount() != 16;

        // fill out size
        uint16_t size = 0;
        size += 2;                      // for itself
        size += 2;                      // for flags
        size += 2;                      // for handle
        size += is128Bit ? 16 : 2;      // for uuid
        size += valueByteList.size();   // for value
        Append(rowByteList, ToByteList(size));
        // Log("Size: ", size, ": ", ToByteList(size));

        // fill out flags
        Append(rowByteList, ToByteList(flags));
        // Log("Flags: ", flags, ": ", ToByteList(flags));

        // fill out handle
        uint16_t handle = nextHandle_;
        ++nextHandle_;
        Append(rowByteList, ToByteList(handle));
        // Log("Handle: ", handle, ": ", ToByteList(handle));

        // fill out uuid
        uuidType.ReverseBytes();
        Append(rowByteList, uuidType.GetByteList());
        // Log("UUID: ", uuidStr, ": ", uuid.GetByteList());
        uuidType.ReverseBytes();

        // fill out value
        Append(rowByteList, valueByteList);

        // capture this row for later
        rowByteListList_.push_back(rowByteList);

        return handle;
    }

    static const uint16_t DEFAULT_FLAGS = 0x0002;

    uint16_t AddPrimaryService(string uuidStr)
    {
        string uuidType = "0x2800";
        uint16_t flags = DEFAULT_FLAGS;

        UUID uuid = uuidStr;
        uuid.ReverseBytes();
        vector<uint8_t> byteList = uuid.GetByteList();
        
        return AddEntry(flags, uuidType, byteList);
    }


    inline static unordered_map<string, uint32_t> propMap_ = {
        // # GATT Characteristic Properties
        { "BROADCAST" ,                   0x01 },
        { "READ" ,                        0x02 },
        { "WRITE_WITHOUT_RESPONSE" ,      0x04 },
        { "WRITE" ,                       0x08 },
        { "NOTIFY",                       0x10 },
        { "INDICATE" ,                    0x20 },
        { "AUTHENTICATED_SIGNED_WRITE" ,  0x40 },
        { "EXTENDED_PROPERTIES" ,         0x80 },
        // # custom BTstack extension
        { "DYNAMIC",                      0x100 },
        { "LONG_UUID",                    0x200 },

        // # read permissions
        { "READ_PERMISSION_BIT_0",        0x400 },
        { "READ_PERMISSION_BIT_1",        0x800 },

        // #
        { "ENCRYPTION_KEY_SIZE_7",       0x6000 },
        { "ENCRYPTION_KEY_SIZE_8",       0x7000 },
        { "ENCRYPTION_KEY_SIZE_9",       0x8000 },
        { "ENCRYPTION_KEY_SIZE_10",      0x9000 },
        { "ENCRYPTION_KEY_SIZE_11",      0xa000 },
        { "ENCRYPTION_KEY_SIZE_12",      0xb000 },
        { "ENCRYPTION_KEY_SIZE_13",      0xc000 },
        { "ENCRYPTION_KEY_SIZE_14",      0xd000 },
        { "ENCRYPTION_KEY_SIZE_15",      0xe000 },
        { "ENCRYPTION_KEY_SIZE_16",      0xf000 },
        { "ENCRYPTION_KEY_SIZE_MASK",    0xf000 },
        
        // # only used by gatt compiler >= 0xffff
        // # Extended Properties
        { "RELIABLE_WRITE",              0x00010000 },
        { "AUTHENTICATION_REQUIRED",     0x00020000 },
        { "AUTHORIZATION_REQUIRED",      0x00040000 },
        { "READ_ANYBODY",                0x00080000 },
        { "READ_ENCRYPTED",              0x00100000 },
        { "READ_AUTHENTICATED",          0x00200000 },
        { "READ_AUTHENTICATED_SC",       0x00400000 },
        { "READ_AUTHORIZED",             0x00800000 },
        { "WRITE_ANYBODY",               0x01000000 },
        { "WRITE_ENCRYPTED",             0x02000000 },
        { "WRITE_AUTHENTICATED",         0x04000000 },
        { "WRITE_AUTHENTICATED_SC",      0x08000000 },
        { "WRITE_AUTHORIZED",            0x10000000 },

        // # Broadcast Notify Indicate Extended Properties are only used to describe a GATT Characteristic but are free to use with att_db
        // # - write permissions
        { "WRITE_PERMISSION_BIT_0",      0x01 },
        { "WRITE_PERMISSION_BIT_1",      0x10 },
        // # - SC required
        { "READ_PERMISSION_SC",          0x20 },
        { "WRITE_PERMISSION_SC",         0x80 },
    };

    static uint32_t ParseProperties(string &properties)
    {
        uint32_t retVal = 0;

        vector<string> propList = Split(properties, "|");

        for (const auto &prop : propList)
        {
            if (propMap_.contains(prop))
            {
                retVal |= propMap_[prop];
            }
            else
            {
                Log("ERR: ATT Property \"", prop, "\" not supported");
            }
        }

        return retVal;
    }


    static uint16_t att_flags(uint32_t propertiesIn)
    {
        // # drop Broadcast (0x01), Notify (0x10), Indicate (0x20), Extended Properties (0x80) - not used for flags 
        uint32_t properties = (propertiesIn & 0xffffff4e);

        // # rw permissions distinct
        bool distinct_permissions_used = properties & (
            propMap_["READ_AUTHORIZED"] |
            propMap_["READ_AUTHENTICATED_SC"] |
            propMap_["READ_AUTHENTICATED"] |
            propMap_["READ_ENCRYPTED"] |
            propMap_["READ_ANYBODY"] |
            propMap_["WRITE_AUTHORIZED"] |
            propMap_["WRITE_AUTHENTICATED"] |
            propMap_["WRITE_AUTHENTICATED_SC"] |
            propMap_["WRITE_ENCRYPTED"] |
            propMap_["WRITE_ANYBODY"]
        ) != 0;

        // # post process properties
        bool encryption_key_size_specified = (properties & propMap_["ENCRYPTION_KEY_SIZE_MASK"]) != 0;

        // # if distinct permissions not used and encyrption key size specified -> set READ/WRITE Encrypted
        if (encryption_key_size_specified && !distinct_permissions_used)
            properties |= propMap_["READ_ENCRYPTED"] | propMap_["WRITE_ENCRYPTED"];

        // # if distinct permissions not used and authentication is requires -> set READ/WRITE Authenticated
        if ((properties & propMap_["AUTHENTICATION_REQUIRED"]) && !distinct_permissions_used)
            properties |= propMap_["READ_AUTHENTICATED"] | propMap_["WRITE_AUTHENTICATED"];

        // # if distinct permissions not used and authorized is requires -> set READ/WRITE Authorized
        if ((properties & propMap_["AUTHORIZATION_REQUIRED"]) && !distinct_permissions_used)
            properties |= propMap_["READ_AUTHORIZED"] | propMap_["WRITE_AUTHORIZED"];

        // # determine read/write security requirements
        uint8_t read_security_level  = 0;
        uint8_t write_security_level = 0;
        bool read_requires_sc     = false;
        bool write_requires_sc    = false;

        if (properties & propMap_["READ_AUTHORIZED"])
            read_security_level = 3;
        else if (properties & propMap_["READ_AUTHENTICATED"])
            read_security_level = 2;
        else if (properties & propMap_["READ_AUTHENTICATED_SC"]){
            read_security_level = 2;
            read_requires_sc = true;
        }
        else if (properties & propMap_["READ_ENCRYPTED"])
            read_security_level = 1;

        if (properties & propMap_["WRITE_AUTHORIZED"])
            write_security_level = 3;
        else if (properties & propMap_["WRITE_AUTHENTICATED"])
            write_security_level = 2;
        else if (properties & propMap_["WRITE_AUTHENTICATED_SC"]){
            write_security_level = 2;
            write_requires_sc = true;
        }
        else if (properties & propMap_["WRITE_ENCRYPTED"])
            write_security_level = 1;

        // # map security requirements to flags
        if (read_security_level & 2)
            properties |= propMap_["READ_PERMISSION_BIT_1"];
        if (read_security_level & 1)
            properties |= propMap_["READ_PERMISSION_BIT_0"];
        if (read_requires_sc)
            properties |= propMap_["READ_PERMISSION_SC"];
        if (write_security_level & 2)
            properties |= propMap_["WRITE_PERMISSION_BIT_1"];
        if (write_security_level & 1)
            properties |= propMap_["WRITE_PERMISSION_BIT_0"];
        if (write_requires_sc)
            properties |= propMap_["WRITE_PERMISSION_SC"];

        return properties;
    }

    uint32_t write_permissions_and_key_size_flags_from_properties(uint32_t properties)
    {
        return att_flags(properties) & (propMap_["ENCRYPTION_KEY_SIZE_MASK"] | propMap_["WRITE_PERMISSION_BIT_0"] | propMap_["WRITE_PERMISSION_BIT_1"]);
    }


    // more or less copying the implementation of the compile_gatt.py script
    void AddCharacteristic(string uuidStr, string propertiesStr, string value = "")
    {
        // used in multiple places
        uint16_t readOnlyAnybodyFlags = propMap_["READ"];
        UUID uuid(uuidStr);
        uuid.ReverseBytes();
        vector<uint8_t> byteListUuid = uuid.GetByteList();
        uuid.ReverseBytes();

        // convert string of | separated properties into a bitmap
        uint32_t properties = ParseProperties(propertiesStr);

        // # reliable writes is defined in an extended properties
        if (properties & propMap_["RELIABLE_WRITE"])
        {
            properties |= propMap_["EXTENDED_PROPERTIES"];
        }

        ///////////////////////////////////////////////////
        // add entry for "theres a characteristic here"
        ///////////////////////////////////////////////////
        
        // gather values needed for data portion
        const string UUID_TYPE_CHARACTERISTIC = "0x2803";
        uint8_t ctcProperties = (uint8_t)properties & 0xFF;
        Log("CTC Prop: ", ToHex(ctcProperties));
        uint16_t handleOfSubsequent = nextHandle_ + 1;

        // create the total byte list for sending
        vector<uint8_t> byteListCtc;

        Append(byteListCtc, ToByteList(ctcProperties));
        Append(byteListCtc, ToByteList(handleOfSubsequent));
        Append(byteListCtc, byteListUuid);

        AddEntry(readOnlyAnybodyFlags, UUID_TYPE_CHARACTERISTIC, byteListCtc);

        ///////////////////////////////////////////////////
        // add entry for dynamic
        ///////////////////////////////////////////////////

        uint16_t valueFlags = att_flags(properties);

        if (uuid.GetBitCount() == 128)
        {
            valueFlags |= propMap_["LONG_UUID"];
        }

        AddEntry(valueFlags, uuidStr, ToByteList(value));

        ///////////////////////////////////////////////////
        // add entry for (optional) CCC
        ///////////////////////////////////////////////////

        if (properties & (propMap_["NOTIFY"] | propMap_["INDICATE"]))
        {
            const string UUID_TYPE_CCC = "0x2902";

            // # use write permissions and encryption key size from attribute value and set READ_ANYBODY | READ | WRITE | DYNAMIC
            uint16_t ccdFlags  = write_permissions_and_key_size_flags_from_properties(properties);
            ccdFlags |= propMap_["READ"];
            ccdFlags |= propMap_["WRITE"];
            ccdFlags |= propMap_["WRITE_WITHOUT_RESPONSE"];
            ccdFlags |= propMap_["DYNAMIC"];

            uint16_t cccData = 0;

            AddEntry(ccdFlags, UUID_TYPE_CCC, ToByteList(cccData));
        }

        ///////////////////////////////////////////////////
        // add entry for (optional) Reliable Write
        ///////////////////////////////////////////////////

        const string UUID_TYPE_RELIABLE_WRITE = "0x2900";

        uint16_t rwData = 1;

        if (properties & propMap_["RELIABLE_WRITE"])
        {
            AddEntry(readOnlyAnybodyFlags, UUID_TYPE_RELIABLE_WRITE, ToByteList(rwData));
        }

        ///////////////////////////////////////////////////
        // Return
        ///////////////////////////////////////////////////

        // return the handles?
    }







    vector<uint8_t> GetDatabaseData()
    {
        vector<uint8_t> byteList;

        byteList.push_back(1);

        // fill in Generic Access Service
        // fill in Generic Access characteristics (device name, etc)
        // fill in Generic Attribute Service
        // fill in Generic Attribute characteristics (database hash, indicate change)?
            // also deal with the characteristic matching the hash (or something?)

        auto FnLog = [](uint8_t row, vector<uint8_t> &byteList){
            Log("Row ", row);
            // LogNNL("Bytes: [");
            string sep = "";
            for (auto &b : byteList)
            {
                LogNNL(sep);
                LogNNL(ToHex(b));

                sep = ", ";
            }
            // LogNNL("]");
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
            // context->counter = A;
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
    Timeline::Global().Event("BleGatt::Init");

    // Read database
    // ReadGatt((uint8_t *)profile_data);

    // register for ATT event
    att_server_register_packet_handler(PacketHandlerATT);

    // https://bluekitchen-gmbh.com/btstack/#appendix/att_server/
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // Generate database
    AttDatabase attDb;
    // attDb.AddPrimaryService("0x1800");
    // attDb.AddPrimaryService("0x1801");
    // attDb.AddPrimaryService("0x181A");
    

    attDb.AddCharacteristic("0xABCD", "WRITE | ENCRYPTION_KEY_SIZE_16");

    vector<uint8_t> byteList = attDb.GetDatabaseData();

    // att_server_init(byteList.data(), att_read_callback, att_write_callback);



    

    // static TimedEventHandlerDelegate ted;
    // ted.SetCallback([]{
    //     if (le_notification_enabled) {
    //         att_server_request_can_send_now_event(con_handle);
    //     }
    // });
    // ted.RegisterForTimedEventInterval(5000, 0);

    Log("BLE Services Started");
}