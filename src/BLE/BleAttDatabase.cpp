#include "BleAttDatabase.h"
#include "Log.h"
#include "Utl.h"
#include "UUID.h"

#include <cstring>
#include <unordered_map>
using namespace std;

#include "StrictMode.h"


// https://bluekitchen-gmbh.com/btstack/#profiles/#gatt-server
// https://github.com/bluekitchen/btstack/blob/master/tool/compile_gatt.py

/////////////////////////////////////////////////////////////////////
// implementation basically lifted verbatim from compile_gatt.py
/////////////////////////////////////////////////////////////////////

static unordered_map<string, uint32_t> property_flags = {
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

static uint32_t parseProperties(string &properties)
{
    uint32_t retVal = 0;

    vector<string> propList = Split(properties, "|");

    for (const auto &prop : propList)
    {
        if (property_flags.contains(prop))
        {
            retVal |= property_flags[prop];
        }
        else
        {
            Log("ERR: ATT Property \"", prop, "\" not supported");
        }
    }

    return retVal;
}

static uint32_t att_flags(uint32_t propertiesIn)
{
    // # drop Broadcast (0x01), Notify (0x10), Indicate (0x20), Extended Properties (0x80) - not used for flags 
    uint32_t properties = (propertiesIn & 0xffffff4e);

    // # rw permissions distinct
    bool distinct_permissions_used = (properties & (
        property_flags["READ_AUTHORIZED"] |
        property_flags["READ_AUTHENTICATED_SC"] |
        property_flags["READ_AUTHENTICATED"] |
        property_flags["READ_ENCRYPTED"] |
        property_flags["READ_ANYBODY"] |
        property_flags["WRITE_AUTHORIZED"] |
        property_flags["WRITE_AUTHENTICATED"] |
        property_flags["WRITE_AUTHENTICATED_SC"] |
        property_flags["WRITE_ENCRYPTED"] |
        property_flags["WRITE_ANYBODY"]
    )) != 0;

    // # post process properties
    bool encryption_key_size_specified = (properties & property_flags["ENCRYPTION_KEY_SIZE_MASK"]) != 0;

    // # if distinct permissions not used and encyrption key size specified -> set READ/WRITE Encrypted
    if (encryption_key_size_specified && !distinct_permissions_used)
        properties |= property_flags["READ_ENCRYPTED"] | property_flags["WRITE_ENCRYPTED"];

    // # if distinct permissions not used and authentication is requires -> set READ/WRITE Authenticated
    if ((properties & property_flags["AUTHENTICATION_REQUIRED"]) && !distinct_permissions_used)
        properties |= property_flags["READ_AUTHENTICATED"] | property_flags["WRITE_AUTHENTICATED"];

    // # if distinct permissions not used and authorized is requires -> set READ/WRITE Authorized
    if ((properties & property_flags["AUTHORIZATION_REQUIRED"]) && !distinct_permissions_used)
        properties |= property_flags["READ_AUTHORIZED"] | property_flags["WRITE_AUTHORIZED"];

    // # determine read/write security requirements
    uint8_t read_security_level  = 0;
    uint8_t write_security_level = 0;
    bool read_requires_sc     = false;
    bool write_requires_sc    = false;

    if (properties & property_flags["READ_AUTHORIZED"])
        read_security_level = 3;
    else if (properties & property_flags["READ_AUTHENTICATED"])
        read_security_level = 2;
    else if (properties & property_flags["READ_AUTHENTICATED_SC"]){
        read_security_level = 2;
        read_requires_sc = true;
    }
    else if (properties & property_flags["READ_ENCRYPTED"])
        read_security_level = 1;

    if (properties & property_flags["WRITE_AUTHORIZED"])
        write_security_level = 3;
    else if (properties & property_flags["WRITE_AUTHENTICATED"])
        write_security_level = 2;
    else if (properties & property_flags["WRITE_AUTHENTICATED_SC"]){
        write_security_level = 2;
        write_requires_sc = true;
    }
    else if (properties & property_flags["WRITE_ENCRYPTED"])
        write_security_level = 1;

    // # map security requirements to flags
    if (read_security_level & 2)
        properties |= property_flags["READ_PERMISSION_BIT_1"];
    if (read_security_level & 1)
        properties |= property_flags["READ_PERMISSION_BIT_0"];
    if (read_requires_sc)
        properties |= property_flags["READ_PERMISSION_SC"];
    if (write_security_level & 2)
        properties |= property_flags["WRITE_PERMISSION_BIT_1"];
    if (write_security_level & 1)
        properties |= property_flags["WRITE_PERMISSION_BIT_0"];
    if (write_requires_sc)
        properties |= property_flags["WRITE_PERMISSION_SC"];

    return properties;
}

uint32_t write_permissions_and_key_size_flags_from_properties(uint32_t properties)
{
    return att_flags(properties) & (property_flags["ENCRYPTION_KEY_SIZE_MASK"] | property_flags["WRITE_PERMISSION_BIT_0"] | property_flags["WRITE_PERMISSION_BIT_1"]);
}


/////////////////////////////////////////////////////////////////////
//
// BleAttDatabase Design
//
// The last operation using this class is to "compile" the database entries
// made up until that point.
//
// I want this class to take care of handling entering the standard services of
// - Generic Access (0x1800)
//   - Device Name (0x2A00)
// - Generic Attribute (0x1801)
//   - Database Hash (0x2B2A)
// 
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
// BleAttDatabase Public Interface
/////////////////////////////////////////////////////////////////////

BleAttDatabase::BleAttDatabase(string deviceName)
{
    // Generic Access
    AddPrimaryService("0x1800");
    AddCharacteristic("0x2A00", "READ", deviceName);

    // Generic Attribute
    AddPrimaryService("0x1801");
    vector<uint8_t> byteList;
    for (int i = 0; i < 16; ++i)
    {
        byteList.push_back((uint8_t)RandInRange(0, 255));
    }
    AddCharacteristic("0x2B2A", "READ", byteList);
}

uint16_t BleAttDatabase::AddPrimaryService(string uuidStr)
{
    static const uint16_t DEFAULT_FLAGS = 0x0002;

    string uuidTypeStr = "0x2800";
    uint16_t flags = DEFAULT_FLAGS;

    UUID uuid = uuidStr;
    uuid.ReverseBytes();
    vector<uint8_t> byteList = uuid.GetByteList();
    uuid.ReverseBytes();

    uint16_t retVal = AddEntry(flags, uuidTypeStr, byteList);

    return retVal;
}

// more or less copying the implementation of the compile_gatt.py script
vector<uint16_t> BleAttDatabase::AddCharacteristic(string uuidStr, string propertiesStr, string value)
{
    vector<uint8_t> byteList;

    for (const auto &c : value)
    {
        byteList.push_back(c);
    }

    return AddCharacteristic(uuidStr, propertiesStr, byteList);
}

vector<uint16_t> BleAttDatabase::AddCharacteristic(string uuidStr, string propertiesStr, vector<uint8_t> value)
{
    vector<uint16_t> retVal;

    // used in multiple places
    uint16_t readOnlyAnybodyFlags = (uint16_t)property_flags["READ"];
    UUID uuid(uuidStr);
    uuid.ReverseBytes();
    vector<uint8_t> byteListUuid = uuid.GetByteList();
    uuid.ReverseBytes();

    // convert string of | separated properties into a bitmap
    uint32_t properties = parseProperties(propertiesStr);

    // # reliable writes is defined in an extended properties
    if (properties & property_flags["RELIABLE_WRITE"])
    {
        properties |= property_flags["EXTENDED_PROPERTIES"];
    }

    ///////////////////////////////////////////////////
    // add entry for "theres a characteristic here"
    ///////////////////////////////////////////////////
    
    // gather values needed for data portion
    const string UUID_TYPE_CHARACTERISTIC = "0x2803";
    uint8_t ctcProperties = (uint8_t)properties & 0xFF;
    uint16_t handleOfSubsequent = nextHandle_ + 1;

    // create the total byte list for sending
    vector<uint8_t> byteListCtc;

    Append(byteListCtc, ToByteList(ctcProperties));
    Append(byteListCtc, ToByteList(handleOfSubsequent));
    Append(byteListCtc, byteListUuid);

    uint16_t handle1 = AddEntry(readOnlyAnybodyFlags, UUID_TYPE_CHARACTERISTIC, byteListCtc);

    retVal.push_back(handle1);

    ///////////////////////////////////////////////////
    // add entry for dynamic
    ///////////////////////////////////////////////////

    uint16_t valueFlags = (uint16_t)att_flags(properties);

    if (uuid.GetBitCount() == 128)
    {
        valueFlags |= (uint16_t)property_flags["LONG_UUID"];
    }

    uint16_t handle2 = AddEntry(valueFlags, uuidStr, value);

    retVal.push_back(handle2);

    ///////////////////////////////////////////////////
    // add entry for (optional) CCC
    ///////////////////////////////////////////////////

    if (properties & (property_flags["NOTIFY"] | property_flags["INDICATE"]))
    {
        const string UUID_TYPE_CCC = "0x2902";

        // # use write permissions and encryption key size from attribute value and set READ_ANYBODY | READ | WRITE | DYNAMIC
        uint16_t ccdFlags = (uint16_t)write_permissions_and_key_size_flags_from_properties(properties);
        ccdFlags |= (uint16_t)property_flags["READ"];
        ccdFlags |= (uint16_t)property_flags["WRITE"];
        ccdFlags |= (uint16_t)property_flags["WRITE_WITHOUT_RESPONSE"];
        ccdFlags |= (uint16_t)property_flags["DYNAMIC"];

        uint16_t cccData = 0;

        uint16_t handle3 = AddEntry(ccdFlags, UUID_TYPE_CCC, ToByteList(cccData));

        retVal.push_back(handle3);
    }

    ///////////////////////////////////////////////////
    // add entry for (optional) Reliable Write
    ///////////////////////////////////////////////////

    const string UUID_TYPE_RELIABLE_WRITE = "0x2900";

    uint16_t rwData = 1;

    if (properties & property_flags["RELIABLE_WRITE"])
    {
        uint16_t handle4 = AddEntry(readOnlyAnybodyFlags, UUID_TYPE_RELIABLE_WRITE, ToByteList(rwData));

        retVal.push_back(handle4);
    }

    ///////////////////////////////////////////////////
    // Return
    ///////////////////////////////////////////////////

    return retVal;
}


/////////////////////////////////////////////////////////////////////
// BleAttDatabase Private Interface
/////////////////////////////////////////////////////////////////////

uint16_t BleAttDatabase::AddEntry(uint16_t flags, string uuidTypeStr, vector<uint8_t> valueByteList)
{
    vector<uint8_t> rowByteList;

    UUID uuidType(uuidTypeStr);
    bool is128Bit = uuidType.GetBitCount() != 16;
    uint16_t uuidSize = is128Bit ? 16u : 2u;

    // fill out size
    uint16_t size = 0;
    size += 2;                                  // for itself
    size += 2;                                  // for flags
    size += 2;                                  // for handle
    size += uuidSize;                           // for uuid
    size += (uint16_t)valueByteList.size();     // for value
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

vector<uint8_t> BleAttDatabase::GetDatabaseData()
{
    vector<uint8_t> byteList;

    // add version number
    byteList.push_back(1);

    // combine captured rows
    for (const auto &rowByteList : rowByteListList_)
    {
        Append(byteList, rowByteList);
    }

    // finish final row
    byteList.push_back(0);
    byteList.push_back(0);

    // debug output (making validation against btstacks compiler doable)
    // auto FnLog = [](uint8_t row, vector<uint8_t> &byteList){
    //     Log("Row ", row);
    //     string sep = "";
    //     for (auto &b : byteList)
    //     {
    //         LogNNL(sep);
    //         LogNNL(ToHex(b));

    //         sep = ", ";
    //     }
    //     LogNL();
    // };

    // for (int i = 0; auto &rowByteList : rowByteListList_)
    // {
    //     FnLog(++i, rowByteList);
    // }

    // return raw bytes
    return byteList;
}

void BleAttDatabase::ReadDatabaseData(uint8_t *buf)
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
        const static uint32_t ATT_PROPERTY_UUID128 = 0x200u;

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
        uint8_t valueBytesToConsume = (uint8_t)((int)rowSize - ((int)uuidByteList.size() + 2 + 2 + 2));
        for (int i = 0; i < valueBytesToConsume; ++i)
        {
            valueByteList.push_back(row[6 + (int)uuidByteList.size() + i]);
        }

        UUID uuid;
        uuid.SetBytesReversed(uuidByteList.data(), (uint8_t)uuidByteList.size());    // I think reversed?

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
}


